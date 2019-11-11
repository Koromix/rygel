// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include "../../libcc/libcc.hh"
#include "config.hh"
#include "files.hh"
#include "goupil.hh"
#ifdef _WIN32
    #include <io.h>
#endif
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>

namespace RG {

HeapArray<FileEntry> app_files;
HashTable<const char *, const FileEntry *> app_files_map;
static BlockAllocator files_alloc;

static bool ComputeFileHash(const char *filename, char out_sha256[65])
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

    // Represent hash as string
    Fmt(MakeSpan(out_sha256, 65), "%1",
        FmtSpan(hash, FmtType::Hexadecimal, "").Pad0(-2));

    return true;
}

static bool ListRecurse(const char *directory, Size offset)
{
    EnumStatus status = EnumerateDirectory(directory, nullptr, 1024,
                                           [&](const char *name, FileType file_type) {
        // Always use '/' for URL mapping
        const char *filename = Fmt(&files_alloc, "%1/%2", directory, name).ptr;

        switch (file_type) {
            case FileType::Directory: { return ListRecurse(filename, offset); } break;
            case FileType::File: {
                FileEntry file = {};

                if (GetPathExtension(filename).len) {
                    file.filename = DuplicateString(filename, &files_alloc).ptr;
                    if (!StatFile(filename, &file.info))
                        return false;
                    file.url = file.filename + offset;

                    app_files.Append(file);
                } else {
                    LogError("Ignoring file '%1' without extension", filename);
                }
            } break;

            case FileType::Unknown: {} break;
        }

        return true;
    });

    return status != EnumStatus::Error;
}

bool InitFiles()
{
    Size url_offset = strlen(goupil_config.file_directory);
    if (!ListRecurse(goupil_config.file_directory, url_offset))
        return false;

    Async async;

    // Map and compute hashes
    for (FileEntry &file: app_files) {
        async.Run([&]() { return ComputeFileHash(file.filename, file.sha256); });
        app_files_map.Set(&file);
    }

    return async.Sync();
}

void HandleFileList(const http_RequestInfo &request, http_IO *io)
{
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    for (const FileEntry &file: app_files) {
        json.StartObject();
        json.Key("path"); json.String(file.url + 1);
        json.Key("sha256"); json.String(file.sha256);
        json.EndObject();
    }
    json.EndArray();

    json.Finish(io);
}

void HandleFileLoad(const http_RequestInfo &request, const FileEntry &file, http_IO *io)
{
    if (request.compression_type == CompressionType::None) {
#ifdef _WIN32
        int fd = open(file.filename, O_RDONLY | O_BINARY);
#else
        int fd = open(file.filename, O_RDONLY | O_BINARY | O_CLOEXEC);
#endif
        if (fd < 0) {
            LogError("Failed to open '%1': %2", file.filename, strerror(errno));
            return;
        }

        // libmicrohttpd wants to know the file size
        struct stat sb;
        if (fstat(fd, &sb) < 0) {
            LogError("Failed to stat '%1': %2", file.filename, strerror(errno));
            return;
        }

        // Let libmicrohttpd handle the rest, and maybe use sendfile
        MHD_Response *response = MHD_create_response_from_fd(sb.st_size, fd);
        io->AttachResponse(200, response);
    } else {
        // Open source file
        StreamReader reader(file.filename);
        if (!reader.IsValid())
            return;

        // Send to browser
        StreamWriter writer;
        if (!io->OpenForWrite(200, &writer))
            return;
        if (!SpliceStream(&reader, Megabytes(8), &writer))
            return;

        // Done!
        writer.Close();
    }
}

}
