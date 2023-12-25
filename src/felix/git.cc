// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "git.hh"

namespace RG {

#pragma pack(push, 1)
struct IdxHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t fanout[256];
};

struct PackHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t objects;
};
#pragma pack(pop)

struct DeltaInfo {
    Size base_len;
    Size final_len;
    HeapArray<uint8_t> code;
};

static inline int ParseHexadecimalChar(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return -1;
    }
}

static inline char FormatHexadecimalChar(int value)
{
    if (value < 10) {
        return (char)('0' + value);
    } else {
        return (char)(('a' - 10) + value);
    }
}

static bool EncodeHash(Span<const char> id, GitHash *out_hash)
{
    if (id.len != 40) [[unlikely]] {
        LogError("Malformed Git Hash");
        return false;
    }

    for (int i = 0, j = 0; i < 20; i++, j += 2) {
        int high = ParseHexadecimalChar(id[j + 0]);
        int low = ParseHexadecimalChar(id[j + 1]);

        if (high < 0 || low < 0) [[unlikely]] {
            LogError("Invalid Git Hash");
            return false;
        }

        out_hash->raw[i] = (uint8_t)((high << 4) | low);
    }

    return true;
}

// Make sure there's enough space!
static void DecodeHash(Span<const uint8_t> raw, char *out_id)
{
    int j = 0;
    for (int i = 0; i < raw.len; i++, j += 2) {
        out_id[j + 0] = FormatHexadecimalChar(raw[i] >> 4);
        out_id[j + 1] = FormatHexadecimalChar(raw[i] & 0xF);
    }
    out_id[j] = 0;
}

static inline void CloseSafe(FILE *fp)
{
    if (fp) {
        fclose(fp);
    }
}

GitVersioneer::~GitVersioneer()
{
    for (FILE *fp: idx_files) {
        CloseSafe(fp);
    }
    for (FILE *fp: pack_files) {
        CloseSafe(fp);
    }
}

bool GitVersioneer::Prepare(const char *root_directory)
{
    RG_ASSERT(!repo_directory);

    if (!IsDecompressorAvailable(CompressionType::Zlib)) {
        LogError("Cannot use git versioning without zlib support");
        return false;
    }

    repo_directory = Fmt(&str_alloc, "%1%/.git", root_directory).ptr;

    const char *packed_filename = Fmt(&str_alloc, "%1%/packed-refs", repo_directory).ptr;
    const char *head_filename = Fmt(&str_alloc, "%1%/HEAD", repo_directory).ptr;
    const char *unpacked_directory = Fmt(&str_alloc, "%1%/refs/tags", repo_directory).ptr;

    // Prepare base object directories
    HeapArray<const char *> object_directories;
    {
        const char *obj_directory = Fmt(&str_alloc, "%1%/objects", repo_directory).ptr;
        object_directories.Append(obj_directory);
    }

    // Load alternate object directories (if any)
    for (Size i = 0; i < object_directories.len; i++) {
        const char *obj_directory = object_directories[i];
        const char *alternate_filename = Fmt(&str_alloc, "%1%/info/alternates", obj_directory).ptr;

        if (TestFile(alternate_filename)) {
            StreamReader st(alternate_filename);
            LineReader reader(&st);

            Span<const char> line;
            while (reader.Next(&line)) {
                const char *obj_directory = DuplicateString(line, &str_alloc).ptr;
                object_directories.Append(obj_directory);
            }
            if (!reader.IsValid())
                return false;
        }
    }

    // List IDX and pack files
    for (const char *obj_directory: object_directories) {
        const char *pack_directory = Fmt(&str_alloc, "%1%/pack", obj_directory).ptr;

        if (!EnumerateFiles(pack_directory, "*.idx", 0, 1024, &str_alloc, &idx_filenames))
            return false;

        for (const char *idx_filename: idx_filenames) {
            const char *pack_filename = Fmt(&str_alloc, "%1.pack", MakeSpan(idx_filename, strlen(idx_filename) - 4)).ptr;
            pack_filenames.Append(pack_filename);
        }

        idx_files.AppendDefault(idx_filenames.len);
        pack_files.AppendDefault(pack_filenames.len);

        Span<char> loose_filename = Fmt(&str_alloc, "%1%/_________________________________________", obj_directory);
        loose_filenames.Append(loose_filename);
    }

    // First, read packed references
    {
        StreamReader st(packed_filename);
        LineReader reader(&st);

        Span<const char> line = {};
        while (reader.Next(&line)) {
            if (!line.len || ParseHexadecimalChar(line[0]) < 0)
                continue;

            Span<const char> ref = {};
            Span<const char> id = SplitStr(line, ' ', &ref);

            if (StartsWith(ref, "refs/tags/")) {
                Span<const char> tag = DuplicateString(ref, &str_alloc);

                // Only take annotated tags
                GitHash hash = {};
                switch (ReadAttribute(id, "object", &hash)) {
                    case AttributeResult::Success: {} break;
                    case AttributeResult::Missing: continue;
                    case AttributeResult::Error: return false;
                }

                ref_map.Set(tag.ptr, hash);
                hash_map.Set(hash, tag.ptr);

                Span<const char> prefix = SplitStr(tag.Take(10, tag.len - 10), '/');
                prefix_set.Set(prefix);
            } else if (StartsWith(ref, "refs/heads/")) {
                const char *head = DuplicateString(ref, &str_alloc).ptr;

                GitHash hash = {};
                if (!EncodeHash(id, &hash))
                    return false;

                ref_map.Set(head, hash);
                hash_map.Set(hash, head);
            }

        }

        if (!reader.IsValid())
            return false;
    }

    // Read unpacked tags (no need for others)
    if (TestFile(unpacked_directory)) {
        HeapArray<const char *> filenames;
        if (!EnumerateFiles(unpacked_directory, nullptr, 3, 4096, &str_alloc, &filenames))
            return false;

        Size prefix_len = strlen(repo_directory) + 1;

        for (const char *filename: filenames) {
            LocalArray<char, 512> buf;
            buf.len = ReadFile(filename, buf.data);
            if (buf.len < 0)
                return true;

            Span<const char> id = TrimStr(buf.As());

            // Only take annotated tags
            GitHash hash = {};
            switch (ReadAttribute(id, "object", &hash)) {
                case AttributeResult::Success: {} break;
                case AttributeResult::Missing: continue;
                case AttributeResult::Error: return false;
            }

#ifdef _WIN32
            Span<char> tag = DuplicateString(filename + prefix_len, &str_alloc);

            for (char &c: tag) {
                c = (c == '\\' ? '/' : c);
            }
#else
            Span<const char> tag = filename + prefix_len;
#endif

            if (StartsWith(tag, "refs/tags/")) {
                ref_map.Set(tag.ptr, hash);
                hash_map.Set(hash, tag.ptr);

                Span<const char> prefix = SplitStr(tag.Take(10, tag.len - 10), '/');
                prefix_set.Set(prefix);
            }
        }
    }

    // Find HEAD commit
    {
        LocalArray<char, 512> buf;
        buf.len = ReadFile(head_filename, buf.data);
        if (buf.len < 0)
            return false;

        if (StartsWith(buf, "ref: ")) {
            Span<const char> ref = TrimStr(buf.Take(4, buf.len - 4));

            const char *filename = Fmt(&str_alloc, ".git%/%1", TrimStr(buf.Take(4, buf.len - 4))).ptr;

            if (TestFile(filename)) {
                buf.len = ReadFile(filename, buf.data);
                if (buf.len < 0)
                    return false;

                GitHash hash = {};
                if (!EncodeHash(TrimStr(buf.As()), &hash))
                    return false;
                commits.Append(hash);
            } else {
                const GitHash *hash = ref_map.Find(ref);

                if (!hash) {
                    LogError("Cannot resolve reference '%1'", ref);
                    return false;
                }

                commits.Append(*hash);
            }
        } else {
            GitHash hash = {};
            if (!EncodeHash(TrimStr(buf.As()), &hash))
                return false;
            commits.Append(hash);
        }
    }

    return true;
}

const char *GitVersioneer::Version(Span<const char> key)
{
    RG_ASSERT(commits.len);

    // Mimic git short hashes
    char id[12];
    DecodeHash(MakeSpan(commits[0].raw, 5), id + 1);
    id[0] = 'g';
    id[10] = 0;

    if (prefix_set.Find(key)) {
        Span<const char> prefix = Fmt(&str_alloc, "refs/tags/%1/", key);

        Size idx = 0;

        for (;;) {
            const char *tag = hash_map.FindValue(commits[idx], nullptr);

            if (tag && StartsWith(tag, prefix) && tag[prefix.len]) {
                if (idx) {
                    const char *version = Fmt(&str_alloc, "%1-%2_%3", tag + prefix.len, idx, id).ptr;
                    return version;
                } else {
                    const char *version = DuplicateString(tag + prefix.len, &str_alloc).ptr;
                    return version;
                }
            }

            Size next = idx + 1;
            RG_ASSERT(next <= commits.len);

            if (next == commits.len) {
                GitHash parent = {};
                AttributeResult ret = ReadAttribute(commits[idx], "parent", &parent);

                if (ret == AttributeResult::Error) {
                    return nullptr;
                } else if (ret == AttributeResult::Missing) {
                    break;
                }

                commits.Append(parent);
            }

            idx = next;
        }
    }

    const char *version = Fmt(&str_alloc, "dev_%1", id).ptr;
    return version;
}

GitVersioneer::AttributeResult GitVersioneer::ReadAttribute(Span<const char> id, const char *attr, GitHash *out_hash)
{
    GitHash hash = {};
    if (!EncodeHash(id, &hash))
        return AttributeResult::Error;

    return ReadAttribute(hash, attr, out_hash);
}

GitVersioneer::AttributeResult GitVersioneer::ReadAttribute(const GitHash &hash, const char *attr, GitHash *out_hash)
{
    // Try loose files
    for (Span<char> loose_filename: loose_filenames) {
        DecodeHash(hash.raw[0], loose_filename.end() - 41);
        loose_filename[loose_filename.len - 39] = *RG_PATH_SEPARATORS;
        DecodeHash(MakeSpan(hash.raw + 1, 19), loose_filename.end() - 38);

        if (!TestFile(loose_filename.ptr))
            continue;

        return ReadLooseAttribute(loose_filename.ptr, attr, out_hash);
    }

    // Try packed files
    PackLocation location = {};
    if (!FindInIndexes(0, hash, &location))
        return AttributeResult::Error;

    return ReadPackAttribute(location.idx, location.offset, attr, out_hash);
}

GitVersioneer::AttributeResult GitVersioneer::ReadLooseAttribute(const char *filename, const char *attr, GitHash *out_hash)
{
    StreamReader st(filename, CompressionType::Zlib);
    LineReader reader(&st);

    Span<char> line = {};

    // Skip NUL character in first line
    if (reader.Next(&line)) {
        Size nul = strnlen(line.ptr, line.len);

        if (nul < line.len) {
            line.ptr += nul + 1;
            line.len -= nul - 1;
        }
    }

    do {
        line.len = strnlen(line.ptr, line.len);

        Span<const char> value;
        Span<const char> key = TrimStr(SplitStr(line, ' ', &value));
        value = TrimStr(value);

        if (key == attr) {
            if (!EncodeHash(value, out_hash))
                return AttributeResult::Error;
            return AttributeResult::Success;
        }
    } while (reader.Next(&line));

    return AttributeResult::Missing;
}

static bool SeekFile(FILE *fp, int64_t offset)
{
#ifdef _WIN32
    int ret = _fseeki64(fp, offset, SEEK_SET);
#else
    int ret = fseeko(fp, (off_t)offset, SEEK_SET);
#endif

    if (ret < 0) {
        LogError("Failed to seek IDX or PACK file: %1", strerror(errno));
        return false;
    }

    return true;
}

static bool ReadSection(FILE *fp, int64_t offset, Size len, void *out_ptr)
{
    if (!SeekFile(fp, offset))
        return false;

    Size read = fread(out_ptr, 1, len, fp);
    if (read < 0)
        return false;
    if (read < len) {
        LogError("Truncated data in IDX or PACK file %1 %2 %3", offset, len, read);
        return false;
    }

    return true;
}

bool GitVersioneer::FindInIndexes(Size start_idx, const GitHash &hash, PackLocation *out_location)
{
    for (Size i = 0; i < idx_filenames.len; i++) {
        Size idx = (start_idx + i) % idx_filenames.len;
        FILE *fp = idx_files[idx];

        if (!fp) {
            const char *idx_filename = idx_filenames[idx];

            fp = OpenFile(idx_filename, (int)OpenFlag::Read);
            if (!fp) 
                return false;
            idx_files[idx] = fp;
        }

        IdxHeader header = {};
        if (!ReadSection(fp, 0, RG_SIZE(header), &header))
            return false;
        if (BigEndian(header.magic) != 0xFF744F63 || BigEndian(header.version) != 2) {
            LogError("Invalid or unsupported IDX file");
            return false;
        }

        int64_t from = 20 * (hash.raw[0] ? BigEndian(header.fanout[hash.raw[0] - 1]) : 0);
        int64_t to = 20 * BigEndian(header.fanout[hash.raw[0]]);
        int64_t total = BigEndian(header.fanout[255]);

        // Load compact names
        HeapArray<uint8_t> names;
        {
            if (to < from) {
                LogError("Invalid IDX file");
                return false;
            } else if (to == from) {
                continue;
            }

            names.AppendDefault(to - from);

            if (!ReadSection(fp, RG_SIZE(header) + from, names.len, names.ptr))
                return false;
        }

        // XXX: Switch to binary search
        int64_t offset1 = -1;
        for (int64_t j = 0; j < names.len; j += 20) {
            if (!memcmp(names.ptr + j, hash.raw, 20)) {
                offset1 = (from + j) / 20;
                break;
            }
        }
        if (offset1 < 0)
            continue;
        offset1 = RG_SIZE(header) + 24 * total + 4 * offset1;

        // Read offset into PACK file
        int32_t offset2 = 0;
        if (!ReadSection(fp, offset1, 4, &offset2))
            return false;
        offset2 = BigEndian(offset2);

        if (offset2 < 0) {
            LogError("8-byte IDX offsets are not supported");
            return false;
        }

        out_location->idx = idx;
        out_location->offset = offset2;

        return true;
    }

    char id[41];
    DecodeHash(hash.raw, id);

    return false;
}

static Span<const uint8_t> ParseLength(Span<const uint8_t> buf, Size *out_len)
{
    if (!buf.len) [[unlikely]]
        return 0;

    Size len = buf[0] & 0x7F;

    Size used = 1;
    if (buf[0] & 0x80) {
        int shift = 7;
        do {
            len |= (Size)(buf[used] << shift);
            shift += 7;
        } while ((buf[used++] & 0x80) && used < buf.len);
    }

    *out_len = len;
    return buf.Take(used, buf.len - used);
}

static Span<const uint8_t> ParseOffset(Span<const uint8_t> buf, int64_t *out_offset)
{
    if (!buf.len) [[unlikely]]
        return 0;

    Size max = std::min(buf.len, (Size)6);
    Size used = 1;
    int64_t offset = buf[0] & 0x7F;

    if (buf[0] & 0x80) {
        do {
            offset = ((offset + 1) << 7) + (buf[used] & 0x7F);
        } while (buf[used++] & 0x80 && used < max);
    }

    *out_offset = offset;
    return buf.Take(used, buf.len - used);
}

GitVersioneer::AttributeResult GitVersioneer::ReadPackAttribute(Size idx, Size offset, const char *attr, GitHash *out_hash)
{
    FILE *fp = pack_files[idx];

    if (!fp) {
        const char *pack_filename = pack_filenames[idx];

        fp = OpenFile(pack_filename, (int)OpenFlag::Read);
        if (!fp)
            return AttributeResult::Error;
        pack_files[idx] = fp;

        // Check PACK header
        PackHeader header = {};
        if (!ReadSection(fp, 0, RG_SIZE(header), &header))
            return AttributeResult::Error;
        if (BigEndian(header.magic) != 0x5041434B || BigEndian(header.version) != 2) {
            LogError("Invalid or unsupported PACK file");
            return AttributeResult::Error;
        }
    }

    int type = -1;
    HeapArray<uint8_t> obj;
    HeapArray<DeltaInfo> deltas;

    // Stack up deltas until we find a base object
    for (;;) {
        obj.RemoveFrom(0);

        if (!ReadPackObject(fp, offset, &type, &obj))
            return AttributeResult::Error;

        if (type != 6 && type != 7)
            break;

        Span<const uint8_t> remain = {};

        if (type == 6) { // OBJ_OFS_DELTA
            memcpy(&offset, obj.ptr, RG_SIZE(offset));
            remain = obj.Take(RG_SIZE(offset), obj.len - RG_SIZE(offset));
        } else if (type == 7) { // OBJ_REF_DELTA
            GitHash hash = {};
            memcpy(hash.raw, obj.ptr, 20);

            PackLocation location = {};
            if (!FindInIndexes(idx, hash, &location))
                return AttributeResult::Error;
            if (location.idx != idx) [[unlikely]] {
                LogError("Cannot resolve delta with other file");
                return AttributeResult::Error;
            }
            offset = location.offset;

            remain = obj.Take(20, obj.len - 20);
        }

        DeltaInfo *delta = deltas.AppendDefault();

        remain = ParseLength(remain, &delta->base_len);
        remain = ParseLength(remain, &delta->final_len);
        if (!remain.len) [[unlikely]] {
            LogError("Corrupt object delta");
            return AttributeResult::Error;
        }
        delta->code.Append(remain);
    }

    // Apply delta instructions
    for (Size i = deltas.len - 1; i >= 0; i--) {
        const DeltaInfo &delta = deltas[i];

        HeapArray<uint8_t> base;
        std::swap(base, obj);

        if (delta.base_len != base.len) [[unlikely]] {
            LogError("Size mismatch in delta object %1 %2 %3", delta.base_len, delta.final_len, base.len);
            return AttributeResult::Error;
        }

        for (Size j = 0; j < delta.code.len;) {
            uint8_t cmd = delta.code[j++];

            if (cmd & 0x80) {
                int64_t offset = 0;
                Size len = 0;

                if (delta.code.len - i < PopCount((uint32_t)(cmd & 0x7F))) [[unlikely]] {
                    LogError("Corrupt object delta");
                    return AttributeResult::Error;
                }

                offset |= (cmd & 0x01) ? (delta.code[j++] << 0) : 0;
                offset |= (cmd & 0x02) ? (delta.code[j++] << 8) : 0;
                offset |= (cmd & 0x04) ? (delta.code[j++] << 16) : 0;
                offset |= (cmd & 0x08) ? (delta.code[j++] << 24) : 0;
                len |= (cmd & 0x10) ? (delta.code[j++] << 0) : 0;
                len |= (cmd & 0x20) ? (delta.code[j++] << 8) : 0;
                len |= (cmd & 0x40) ? (delta.code[j++] << 16) : 0;
                len = len ? len : 0x10000;

                if (offset < 0 || offset + len > base.len) [[unlikely]] {
                    LogError("Corrupt object delta");
                    return AttributeResult::Error;
                }

                obj.Append(base.Take((Size)offset, len));
            } else if (cmd) {
                if (cmd > delta.code.len - j) [[unlikely]] {
                    LogError("Corrupt object delta");
                    return AttributeResult::Error;
                }

                obj.Append(delta.code.Take(j, cmd));
                j += cmd;
            } else {
                LogError("Invalid delta command");
                return AttributeResult::Error;
            }
        }

        if (delta.final_len != obj.len) [[unlikely]] {
            LogError("Size mismatch in delta object %1 %2 %3", delta.base_len, delta.final_len, base.len);
            return AttributeResult::Error;
        }
    }

    if (type >= 1 && type <= 4) {
        Span<const char> remain = obj.As<char>();

        while (remain.len) {
            Span<const char> line = SplitStrLine(remain, &remain);

            Span<const char> value;
            Span<const char> key = TrimStr(SplitStr(line, ' ', &value));
            value = TrimStr(value);

            if (key == attr) {
                if (!EncodeHash(value, out_hash))
                    return AttributeResult::Error;
                return AttributeResult::Success;
            }
        }

        return AttributeResult::Missing;
    } else {
        LogError("Expect commit object, unexpected object 0x%1", FmtHex(type));
        return AttributeResult::Error;
    }
}

bool GitVersioneer::ReadPackObject(FILE *fp, int64_t offset, int *out_type, HeapArray<uint8_t> *out_obj)
{
    int64_t base = offset;

    int type = 0;
    Size len = 0;
    {
        uint8_t chunk[6] = {};
        if (!ReadSection(fp, offset, RG_SIZE(chunk), chunk))
            return false;

        type = (chunk[0] >> 4) & 0x7;
        len = chunk[0] & 0xF;

        Size used = 1;
        if (chunk[0] & 0x80) {
            int shift = 4;
            do {
                len |= (Size)(chunk[used] << shift);
                shift += 7;
            } while ((chunk[used++] & 0x80) && used < RG_LEN(chunk));
        }
        offset += used;

        if (used != RG_SIZE(chunk) && !SeekFile(fp, offset))
            return false;
    }

    // Deal with delta encoding
    if (type == 6) { // OBJ_OFS_DELTA
        uint8_t chunk[6] = {};
        if (!ReadSection(fp, offset, RG_SIZE(chunk), chunk))
            return false;

        int64_t negative = 0;
        offset += ParseOffset(chunk, &negative).ptr - chunk;
        negative = base - negative;

        out_obj->Append(MakeSpan((const uint8_t *)&negative, RG_SIZE(negative)));

        if (!SeekFile(fp, offset))
            return false;
    } else if (type == 7) { // OBJ_REF_DELTA
        uint8_t hash[20];
        if (!ReadSection(fp, offset, RG_SIZE(hash), hash))
            return false;
        offset += 20;

        out_obj->Append(hash);
    }

    StreamReader reader(fp, "<pack>", CompressionType::Zlib);
    {
        Size prev_len = out_obj->len;
        if (reader.ReadAll(Mebibytes(2), out_obj) < 0)
            return false;
        if (len != out_obj->len - prev_len) [[unlikely]] {
            LogError("Packed object size mismatch");
            return false;
        }
    }
    *out_type = type;

    return true;
}

}
