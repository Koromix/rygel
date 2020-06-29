// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "files.hh"
#include "goupile.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

#include <shared_mutex>
#ifdef _WIN32
    #include <io.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace RG {

struct FileEntry {
    const char *url;

    const char *filename;
    Size size;
    char sha256[65];

    // Used for garbage collection
    Allocator *allocator;

    // Prevent change and deletion while in use
    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    mutable int readers = 0;

    void LockRead() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        readers++;
    }

    void UnlockRead() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!--readers) {
            cv.notify_one();
        }
    }

    void WaitForReaders()
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (readers) {
            cv.wait(lock);
        }
    }

    RG_HASHTABLE_HANDLER(FileEntry, url);
};

static std::shared_mutex files_mutex;
static BucketArray<FileEntry> files;
static HashTable<const char *, FileEntry *> files_map;

// The caller still needs to compute checksum after this
static FileEntry *AddFileEntry(const char *filename, Size offset, Size size)
{
    FileEntry *file;
    {
        Allocator *alloc;
        file = files.AppendDefault(&alloc);
        file->allocator = alloc;
    }

    file->filename = DuplicateString(filename, file->allocator).ptr;
    file->size = size;

    Span<char> url = Fmt(file->allocator, "/files/%1", file->filename + offset);
#ifdef _WIN32
    for (char &c: url) {
        c = (c == '\\') ? '/' : c;
    }
#endif
    file->url = url.ptr;

    return file;
}

static bool ListRecurse(const char *directory, Size offset)
{
    BlockAllocator temp_alloc;

    EnumStatus status = EnumerateDirectory(directory, nullptr, 1024,
                                           [&](const char *name, FileType file_type) {
        const char *filename = Fmt(&temp_alloc, "%1%/%2", directory, name).ptr;

        switch (file_type) {
            case FileType::Directory: { return ListRecurse(filename, offset); } break;
            case FileType::File: {
                FileInfo file_info;
                if (!StatFile(filename, &file_info))
                    return false;

                AddFileEntry(filename, offset, file_info.size);
                return true;
            } break;
            case FileType::Unknown: {} break;
        }

        return true;
    });

    return status != EnumStatus::Error;
}

static void FormatSha256(Span<const uint8_t> hash, char out_sha256[65])
{
    RG_ASSERT(hash.len == 32);
    Fmt(MakeSpan(out_sha256, 65), "%1", FmtSpan(hash, FmtType::Hexadecimal, "").Pad0(-2));
}

static bool ComputeFileSha256(const char *filename, char out_sha256[65])
{
    // Hash file
    uint8_t hash[crypto_hash_sha256_BYTES];
    {
        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        StreamReader st(filename);
        while (!st.IsEOF()) {
            LocalArray<uint8_t, 16384> buf;
            buf.len = st.Read(buf.data);
            if (buf.len < 0)
                return false;

            crypto_hash_sha256_update(&state, buf.data, buf.len);
        }

        crypto_hash_sha256_final(&state, hash);
    }

    FormatSha256(hash, out_sha256);
    return true;
}

bool InitFiles()
{
    Size url_offset = strlen(goupile_config.files_directory) + 1;
    if (!ListRecurse(goupile_config.files_directory, url_offset))
        return false;

    Async async;

    // Map and compute hashes
    for (FileEntry &file: files) {
        async.Run([&]() { return ComputeFileSha256(file.filename, file.sha256); });
        files_map.Set(&file);
    }

    return async.Sync();
}

void HandleFileList(const http_RequestInfo &request, http_IO *io)
{
    std::shared_lock<std::shared_mutex> lock_files(files_mutex);

    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    for (const FileEntry &file: files) {
        json.StartObject();
        json.Key("path"); json.String(file.url);
        json.Key("size"); json.Int64(file.size);
        json.Key("sha256"); json.String(file.sha256);
        json.EndObject();
    }
    json.EndArray();

    json.Finish(io);
}

// Returns false when file does not exist (and another handler needs to be used)
bool HandleFileGet(const http_RequestInfo &request, http_IO *io)
{
    const char *etag = request.GetHeaderValue("If-None-Match");
    const char *sha256 = request.GetQueryValue("sha256");

    const FileEntry *file;
    {
        std::shared_lock<std::shared_mutex> lock_files(files_mutex);

        if (TestStr(request.url, "/favicon.png")) {
            file = files_map.FindValue("/files/favicon.png", nullptr);
        } else if (TestStr(request.url, "/manifest.json")) {
            file = files_map.FindValue("/files/manifest.json", nullptr);
        } else {
            file = files_map.FindValue(request.url, nullptr);
        }
        if (!file)
            return false;

        file->LockRead();
    }
    io->AddFinalizer([=]() { file->UnlockRead(); });

    if (etag && TestStr(etag, file->sha256)) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        io->AttachResponse(304, response);
        return true;
    }

    if (sha256 && !TestStr(sha256, file->sha256)) {
        LogError("Fetch refused because of sha256 mismatch");
        io->AttachError(409);
        return true;
    }

    io->RunAsync([=]() {
        io->AddCachingHeaders(goupile_config.max_age, file->sha256);

        if (request.compression_type == CompressionType::None) {
#ifdef _WIN32
            int fd = open(file->filename, O_RDONLY | O_BINARY);
#else
            int fd = open(file->filename, O_RDONLY | O_CLOEXEC);
#endif
            if (fd < 0) {
                LogError("Failed to open '%1': %2", file->filename, strerror(errno));
                return;
            }

            // libmicrohttpd wants to know the file size
            struct stat sb;
            if (fstat(fd, &sb) < 0) {
                LogError("Failed to stat '%1': %2", file->filename, strerror(errno));
                return;
            }

            // Let libmicrohttpd handle the rest, and maybe use sendfile
            MHD_Response *response = MHD_create_response_from_fd(sb.st_size, fd);
            io->AttachResponse(200, response);
        } else {
            StreamReader reader(file->filename);
            if (!reader.IsValid())
                return;

            StreamWriter writer;
            if (!io->OpenForWrite(200, &writer))
                return;
            if (!SpliceStream(&reader, goupile_config.max_file_size, &writer))
                return;

            writer.Close();
        }
    });

    return true;
}

void HandleFilePut(const http_RequestInfo &request, http_IO *io)
{
    if (!goupile_config.files_directory) {
        LogError("File upload is disabled");
        io->AttachError(403);
        return;
    }

    const char *sha256 = request.GetQueryValue("sha256");

    // Security checks
    if (strncmp(request.url, "/files/", 7)) {
        LogError("Cannot write to file outside '/files/'");
        io->AttachError(403);
        return;
    }
    if (PathContainsDotDot(request.url)) {
        LogError("Path must not contain any '..' component");
        io->AttachError(403);
        return;
    }

    // Construct filenames
    const char *filename = Fmt(&io->allocator, "%1%/%2", goupile_config.files_directory, request.url + 7).ptr;
    const char *tmp_filename = Fmt(&io->allocator, "%1~", filename).ptr;

    io->RunAsync([=]() {
        RG_DEFER_N(tmp_guard) { UnlinkFile(tmp_filename); };

        if (!EnsureDirectoryExists(filename))
            return;

        // Write new file
        uint8_t hash[crypto_hash_sha256_BYTES];
        Size total_len = 0;
        {
            StreamWriter writer(tmp_filename);
            StreamReader reader;
            if (!io->OpenForRead(&reader))
                return;

            crypto_hash_sha256_state state;
            crypto_hash_sha256_init(&state);

            while (!reader.IsEOF()) {
                LocalArray<uint8_t, 16384> buf;
                buf.len = reader.Read(buf.data);
                if (buf.len < 0)
                    return;

                if (RG_UNLIKELY(buf.len > goupile_config.max_file_size - total_len)) {
                    LogError("File '%1' is too large (limit = %2)",
                             reader.GetFileName(), FmtDiskSize(goupile_config.max_file_size));
                    io->AttachError(413);
                    return;
                }
                total_len += buf.len;

                if (!writer.Write(buf))
                    return;

                crypto_hash_sha256_update(&state, buf.data, buf.len);
            }
            if (!writer.Close())
                return;

            crypto_hash_sha256_final(&state, hash);
        }

        // Nothing has failed so far, the upload is complete. Check and update metadata
        // and if things go well, we'll be able to overwrite and delete the old file.
        std::lock_guard<std::shared_mutex> lock_files(files_mutex);

        FileEntry *file = files_map.FindValue(request.url, nullptr);

        if (file) {
            file->WaitForReaders();

            if (!sha256 || !TestStr(sha256, file->sha256)) {
                LogError("Update refused because of sha256 mismatch");
                io->AttachError(409);
                return;
            }

            if (!RenameFile(tmp_filename, filename))
                return;
            tmp_guard.Disable();

            file->size = total_len;
            FormatSha256(hash, file->sha256);
        } else {
            if (sha256 && sha256[0]) {
                LogError("Update refused because file does not exist");
                io->AttachError(409);
                return;
            }

            if (!RenameFile(tmp_filename, filename))
                return;
            tmp_guard.Disable();

            Size url_offset = strlen(goupile_config.files_directory) + 1;
            file = AddFileEntry(filename, url_offset, total_len);
            FormatSha256(hash, file->sha256);

            files_map.Set(file);
        }

        io->AttachText(200, "Done!");
    });
}

void HandleFileDelete(const http_RequestInfo &request, http_IO *io)
{
    if (!goupile_config.files_directory) {
        LogError("File upload is disabled");
        io->AttachError(403);
        return;
    }

    const char *sha256 = request.GetQueryValue("sha256");

    std::lock_guard<std::shared_mutex> lock_files(files_mutex);

    FileEntry *file = files_map.FindValue(request.url, nullptr);
    if (!file) {
        io->AttachError(404);
        return;
    }
    file->WaitForReaders();

    if (!sha256 || !TestStr(sha256, file->sha256)) {
        LogError("Deletion refused because of sha256 mismatch");
        io->AttachError(409);
        return;
    }

    // Deal with the OS first. The rest can't fail anyway.
    if (!UnlinkFile(file->filename))
        return;

    // Delete file entry
    {
        FileEntry *file0 = &files[0];

        files_map.Remove(file->url);
        if (file != file0) {
            file0->WaitForReaders();

            files_map.Remove(file0->url);
            if (file->allocator != file0->allocator) {
                file->filename = DuplicateString(file0->filename, file->allocator).ptr;
                file->size = file0->size;
                file->url = DuplicateString(file0->url, file->allocator).ptr;
                strcpy(file->sha256, file0->sha256);
            } else {
                file->filename = file0->filename;
                file->size = file0->size;
                file->url = file0->url;
                strcpy(file->sha256, file0->sha256);
            }
            files_map.Set(file);
        }
        files.RemoveFirst(1);
    }

    io->AttachText(200, "Done!");
}

}
