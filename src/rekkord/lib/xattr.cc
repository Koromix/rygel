// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "xattr.hh"

#if defined(__linux__)
    #include <sys/xattr.h>
#elif defined(__FreeBSD__)
    #include <sys/acl.h>
    #include <sys/types.h>
    #include <sys/extattr.h>
#endif

namespace K {

static const char *AclAccessKey = "rk.acl1";
static const char *AclDefaultKey = "rk.acl1d";

#if defined(__linux__)

static const char *AclAccessAttribute = "system.posix_acl_access";
static const char *AclDefaultAttribute = "system.posix_acl_default";
static const int AclVersion = 2;

struct AclHeader {
    uint32_t version;
};
static_assert(K_SIZE(AclHeader) == 4);

struct AclEntry {
    uint16_t tag;
    uint16_t perm;
    uint32_t id;
};
static_assert(K_SIZE(AclEntry) == 8);

enum class AclPermission {
    Read = 4,
    Write = 2,
    Execute = 1
};

enum class AclTag {
    Undefined = 0,
    UserObj = 1,
    User = 2,
    GroupObj = 4,
    Group = 8,
    Mask = 16,
    Other = 32
};

static FmtArg FormatPermissions(uint16_t perm)
{
    FmtArg arg = {};

    arg.type = FmtType::Buffer;
    arg.u.buf[0] = (perm & (int)AclPermission::Read) ? 'r' : '-';
    arg.u.buf[1] = (perm & (int)AclPermission::Write) ? 'w' : '-';
    arg.u.buf[2] = (perm & (int)AclPermission::Execute) ? 'x' : '-';
    arg.u.buf[3] = 0;

    return arg;
}

static Span<const char> FormatACLs(const char *filename, Span<const uint8_t> raw, Allocator *alloc)
{
    HeapArray<char> str(alloc);

    // Check size and header
    {
        if (raw.len < K_SIZE(AclHeader) || raw.len % K_SIZE(AclEntry) != K_SIZE(AclHeader)) {
            LogError("Invalid ACL attribute size in '%1'", filename);
            return {};
        }

        AclHeader header = *(const AclHeader *)raw.ptr;

        header.version = LittleEndian(header.version);

        if (header.version != AclVersion) {
            LogError("Unsupported ACL version in '%1'", filename);
            return {};
        }
    }

    for (Size offset = K_SIZE(AclHeader); offset < raw.len; offset += K_SIZE(AclEntry)) {
        AclEntry entry = *(const AclEntry *)(raw.ptr + offset);

        entry.tag = LittleEndian(entry.tag);
        entry.perm = LittleEndian(entry.perm);
        entry.id = LittleEndian(entry.id);

        switch (entry.tag) {
            case (int)AclTag::UserObj: { Fmt(&str, "user::%1\n", FormatPermissions(entry.perm)); } break;
            case (int)AclTag::User: { Fmt(&str, "user:%1:%2\n", entry.id, FormatPermissions(entry.perm)); } break;
            case (int)AclTag::GroupObj: { Fmt(&str, "group::%1\n", FormatPermissions(entry.perm)); } break;
            case (int)AclTag::Group: { Fmt(&str, "group:%1:%2\n", entry.id, FormatPermissions(entry.perm)); } break;
            case (int)AclTag::Mask: { Fmt(&str, "mask::%1\n", FormatPermissions(entry.perm)); } break;
            case (int)AclTag::Other: { Fmt(&str, "other::%1\n", FormatPermissions(entry.perm)); } break;

            default: continue;
        }
    }

    // Strip last LF byte
    str.len = std::max((Size)0, str.len - 1);

    return str.TrimAndLeak();
}

static bool ParsePermissions(Span<const char> str, uint16_t *out_perm)
{
    if (!str.len) {
        LogError("Invalid empty permission set");
        return false;
    }

    uint16_t perm = 0;

    for (char c: str) {
        switch (c) {
            case 'r': { perm |= (int)AclPermission::Read; } break;
            case 'w': { perm |= (int)AclPermission::Write; } break;
            case 'x': { perm |= (int)AclPermission::Execute; } break;
            case '-': {} break;

            default: {
                LogError("Invalid permission set '%1'", str);
                return false;
            } break;
        }
    }

    *out_perm = perm;
    return true;
}

static Size ParseACLs(Span<const char> str, Span<uint8_t> out_buf)
{
    K_ASSERT(out_buf.len >= K_SIZE(AclHeader));

    Size len = 0;

    // Append header
    {
        AclHeader header = {};
        header.version = LittleEndian(AclVersion);

        MemCpy(out_buf.ptr, &header, K_SIZE(header));
        len += K_SIZE(header);

        out_buf.ptr += K_SIZE(header);
        out_buf.len -= K_SIZE(header);
    }

    // Parse entries
    while (str.len) {
        Span<const char> line = TrimStr(SplitStr(str, '\n', &str));

        if (!line.len)
            continue;
        if (line[0] == '#')
            continue;

        AclEntry entry = {};

        Span<const char> tag = TrimStrRight(SplitStr(line, ':', &line));
        Span<const char> id = TrimStr(SplitStr(line, ':', &line));
        Span<const char> perm = TrimStrLeft(line);

        if (TestStr(tag, "u") || TestStr(tag, "user")) {
            entry.tag = id.len ? (int)AclTag::User : (int)AclTag::UserObj;
        } else if (TestStr(tag, "g") || TestStr(tag, "group")) {
            entry.tag = id.len ? (int)AclTag::Group : (int)AclTag::GroupObj;
        } else if (TestStr(tag, "m") || TestStr(tag, "mask")) {
            entry.tag = (int)AclTag::Mask;
        } else if (TestStr(tag, "o") || TestStr(tag, "other")) {
            entry.tag = (int)AclTag::Other;
        } else {
            LogError("Invalid ACL tag '%1'", tag);
            return -1;
        }

        if (id.len && !ParseInt(id, &entry.id))
            return -1;
        if (!ParsePermissions(perm, &entry.perm))
            return -1;

        entry.tag = LittleEndian(entry.tag);
        entry.id = LittleEndian(entry.id);
        entry.perm = LittleEndian(entry.perm);

        if (out_buf.len < K_SIZE(entry)) {
            LogError("Excessive POSIX ACL size");
            return -1;
        }

        MemCpy(out_buf.ptr, &entry, K_SIZE(entry));
        len += K_SIZE(entry);

        out_buf.ptr += K_SIZE(entry);
        out_buf.len -= K_SIZE(entry);
    }

    return len;
}

static Size ReadAttribute(int fd, const char *filename, const char *key, HeapArray<uint8_t> *out_value)
{
    Size size = (fd >= 0) ? fgetxattr(fd, key, nullptr, 0)
                          : lgetxattr(filename, key, nullptr, 0);

    if (size < 0) {
        LogError("Failed to read extended attribute '%1' from '%2': %3", key, filename, strerror(errno));
        return -1;
    }
    if (!size)
        return 0;

retry:
    out_value->Grow(size);

    Size len = (fd >= 0) ? fgetxattr(fd, key, out_value->end(), out_value->capacity - out_value->len)
                         : lgetxattr(filename, key, out_value->end(), out_value->capacity - out_value->len);

    if (len < 0) {
        if (errno == E2BIG) {
            size += Kibibytes(4);
            goto retry;
        }

        LogError("Failed to read extended attribute '%1' from '%2': %3", key, filename, strerror(errno));
        return -1;
    }

    out_value->len += len;
    out_value->Trim();

    return len;
}

bool ReadXAttributes(int fd, const char *filename, FileType, Allocator *alloc, HeapArray<XAttrInfo> *out_xattrs)
{
    K_DEFER_NC(err_guard, len = out_xattrs->len) { out_xattrs->RemoveFrom(len); };

    HeapArray<char> list(alloc);
    {
        Size size = Kibibytes(4);

retry:
        list.Grow(size);

        Size len = (fd >= 0) ? flistxattr(fd, list.ptr, (size_t)list.capacity)
                             : llistxattr(filename, list.ptr, (size_t)list.capacity);

        if (len < 0) {
            if (errno == E2BIG) {
                size += Kibibytes(4);
                goto retry;
            }

            LogError("Failed to list extended attributes of '%1': %2", filename, strerror(errno));
            return false;
        }

        list.len = len;
        list.Trim();
    }

    for (Size offset = 0; offset < list.len;) {
        XAttrInfo xattr = {};

        Span<const char> key = list.ptr + offset;

        // Prepare next iteration
        offset += key.len + 1;

        HeapArray<uint8_t> value(alloc);
        Size len = ReadAttribute(fd, filename, key.ptr, &value);

        if (len < 0)
            continue;

        if (TestStr(key, AclAccessAttribute)) {
            Span<const char> acls = FormatACLs(filename, value, alloc);

            if (acls.len) {
                XAttrInfo xattr = { AclAccessKey, acls.As<uint8_t>() };
                out_xattrs->Append(xattr);
            }
        } else if (TestStr(key, AclDefaultAttribute)) {
            Span<const char> acls = FormatACLs(filename, value, alloc);

            if (acls.len) {
                XAttrInfo xattr = { AclDefaultKey, acls.As<uint8_t>() };
                out_xattrs->Append(xattr);
            }
        } else {
            xattr.key = key.ptr;
            xattr.value = value.TrimAndLeak();

            out_xattrs->Append(xattr);
        }
    }

    list.Leak();

    err_guard.Disable();
    return true;
}

bool WriteXAttributes(int fd, const char *filename, Span<const XAttrInfo> xattrs)
{
    bool success = true;

    // Hold transformed/parsed values (such as ACL)
    uint8_t buf[16384];

    for (XAttrInfo xattr: xattrs) {
        if (TestStr(xattr.key, AclAccessKey)) {
            Size len = ParseACLs(xattr.value.As<const char>(), buf);

            if (len < 0) {
                success = false;
                continue;
            }
            if (!len)
                continue;

            xattr.key = AclAccessAttribute;
            xattr.value = MakeSpan(buf, len);
        } else if (TestStr(xattr.key, AclDefaultKey)) {
            Size len = ParseACLs(xattr.value.As<const char>(), buf);

            if (len < 0) {
                success = false;
                continue;
            }
            if (!len)
                continue;

            xattr.key = AclDefaultAttribute;
            xattr.value = MakeSpan(buf, len);
        }

        int ret = (fd >= 0) ? fsetxattr(fd, xattr.key, xattr.value.ptr, xattr.value.len, 0)
                            : lsetxattr(filename, xattr.key, xattr.value.ptr, xattr.value.len, 0);

        if (ret < 0) {
            LogError("Failed to write extended attribute '%1' to '%2': %3'", xattr.key, filename, strerror(errno));
            success = false;
        }
    }

    return success;
}

#elif defined(__FreeBSD__)

static Span<const char> ReadACLs(int fd, const char *filename, acl_type_t type, Allocator *alloc)
{
    K_ASSERT(type == ACL_TYPE_ACCESS || fd < 0);

    acl_t acl = (fd >= 0) ? acl_get_fd(fd)
                          : acl_get_link_np(filename, type);
    if (!acl) {
        // Most likely not a directory, skip silently
        if (type == ACL_TYPE_DEFAULT && errno == EINVAL)
            return {};

        LogError("Failed to open ACL entries for '%1': %2", filename, strerror(errno));
        return {};
    }
    K_DEFER { acl_free(acl); };

    // Ignore trivial ACLs
    {
        int trivial = 0;
        acl_is_trivial_np(acl, &trivial);

        if (trivial)
            return {};
    }

    char *str = acl_to_text_np(acl, nullptr, ACL_TEXT_NUMERIC_IDS);
    if (!str) {
        LogError("Failed to read ACL entries for '%1': %2", filename, strerror(errno));
        return {};
    }
    K_DEFER { acl_free(str); };

    Span<const char> copy = DuplicateString(str, alloc).ptr;
    return copy;
}

static bool WriteACLs(int fd, const char *filename, acl_type_t type, Span<const char> str)
{
    K_ASSERT(type == ACL_TYPE_ACCESS || fd < 0);

    char buf[32768];
    CopyString(str, buf);

    acl_t acl = acl_from_text(buf);
    if (!acl) {
        LogError("Failed to decode ACL string for '%1': %2", filename, strerror(errno));
        return false;
    }
    K_DEFER { acl_free(acl); };

    if (fd >= 0) {
        if (acl_set_fd(fd, acl) < 0) {
            LogError("Failed to set ACL for '%1': %2", filename, strerror(errno));
            return false;
        }
    } else {
        if (acl_set_link_np(filename, type, acl) < 0) {
            LogError("Failed to set ACL for '%1': %2", filename, strerror(errno));
            return false;
        }
    }

    return true;
}

static Size ReadAttribute(int fd, const char *filename, const char *key, HeapArray<uint8_t> *out_value)
{
    Size size = (fd >= 0) ? extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, key, nullptr, 0)
                          : extattr_get_link(filename, EXTATTR_NAMESPACE_USER, key, nullptr, 0);

    if (size < 0) {
        LogError("Failed to read extended attribute '%1' from '%2': %3", key, filename, strerror(errno));
        return -1;
    }
    if (!size)
        return 0;

retry:
    out_value->Grow(size);

    Size available = out_value->capacity - out_value->len;
    Size len = (fd >= 0) ? extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, key, out_value->end(), available)
                         : extattr_get_link(filename, EXTATTR_NAMESPACE_USER, key, out_value->end(), available);

    if (len == available) {
        size += Kibibytes(4);
        goto retry;
    }
    if (len < 0) {
        LogError("Failed to read extended attribute '%1' from '%2': %3", key, filename, strerror(errno));
        return -1;
    }

    out_value->len += len;
    out_value->Trim();

    return len;
}

bool ReadXAttributes(int fd, const char *filename, FileType type, Allocator *alloc, HeapArray<XAttrInfo> *out_xattrs)
{
    K_DEFER_NC(err_guard, len = out_xattrs->len) { out_xattrs->RemoveFrom(len); };

    // Get access ACLs
    {
        Span<const char> acls = ReadACLs(fd, filename, ACL_TYPE_ACCESS, alloc);

        if (acls.len) {
            XAttrInfo xattr = { AclAccessKey, acls.As<uint8_t>() };
            out_xattrs->Append(xattr);
        }
    }

    // Get default ACLs (only works for directories)
    if (type == FileType::Directory) {
        Span<const char> acls = ReadACLs(-1, filename, ACL_TYPE_DEFAULT, alloc);

        if (acls.len) {
            XAttrInfo xattr = { AclDefaultKey, acls.As<uint8_t>() };
            out_xattrs->Append(xattr);
        }
    }

    HeapArray<char> list;
    {
        Size size = Kibibytes(4);

retry:
        list.Grow(size);

        Size len = (fd >= 0) ? extattr_list_fd(fd, EXTATTR_NAMESPACE_USER, list.ptr, (size_t)list.capacity)
                             : extattr_list_link(filename, EXTATTR_NAMESPACE_USER, list.ptr, (size_t)list.capacity);

        if (len == list.capacity) {
            size += Kibibytes(4);
            goto retry;
        }
        if (len < 0) {
            LogError("Failed to list extended attributes of '%1': %2", filename, strerror(errno));
            return false;
        }

        list.len = len;
    }

    for (Size offset = 0; offset < list.len;) {
        XAttrInfo xattr = {};

        Span<const char> key = MakeSpan(list.ptr + offset + 1, list[offset]);

        // Prepare next iteration
        offset += 1 + key.len;

        // Prefix attribute namespace
        key = Fmt(alloc, "user.%1", key);

        HeapArray<uint8_t> value(alloc);
        Size len = ReadAttribute(fd, filename, key.ptr + 5, &value);

        if (len < 0)
            continue;

        xattr.key = key.ptr;
        xattr.value = value.TrimAndLeak();

        out_xattrs->Append(xattr);
    }

    err_guard.Disable();
    return true;
}

bool WriteXAttributes(int fd, const char *filename, Span<const XAttrInfo> xattrs)
{
    bool success = true;

    for (const XAttrInfo &xattr: xattrs) {
        if (TestStr(xattr.key, AclAccessKey)) {
            success &= WriteACLs(fd, filename, ACL_TYPE_ACCESS, xattr.value.As<char>());
            continue;
        } else if (TestStr(xattr.key, AclDefaultKey)) {
            success &= WriteACLs(-1, filename, ACL_TYPE_DEFAULT, xattr.value.As<char>());
            continue;
        }

        if (!StartsWith(xattr.key, "user.")) {
            LogError("Cannot restore extended attribute '%1' for '%2': unsupported prefix", xattr.key, filename);
            success = false;
            continue;
        }

        if (fd >= 0) {
            if (extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, xattr.key + 5, xattr.value.ptr, xattr.value.len) < 0) {
                LogError("Failed to write extended attribute '%1' to '%2': %3'", xattr.key, filename, strerror(errno));
                success = false;
            }
        } else {
            if (extattr_set_link(filename, EXTATTR_NAMESPACE_USER, xattr.key + 5, xattr.value.ptr, xattr.value.len) < 0) {
                LogError("Failed to write extended attribute '%1' to '%2': %3'", xattr.key, filename, strerror(errno));
                success = false;
            }
        }
    }

    return success;
}

#else

static std::once_flag flag;

bool ReadXAttributes(int, const char *, FileType, Allocator *, HeapArray<XAttrInfo> *)
{
    (void)AclAccessKey;
    (void)AclDefaultKey;

    std::call_once(flag, []() { LogError("Extended attributes (xattrs) are not implemented or supported on this platform"); });
    return false;
}

bool WriteXAttributes(int, const char *, Span<const XAttrInfo>)
{
    (void)AclAccessKey;
    (void)AclDefaultKey;

    std::call_once(flag, []() { LogError("Extended attributes (xattrs) are not implemented or supported on this platform"); });
    return false;
}

#endif

}
