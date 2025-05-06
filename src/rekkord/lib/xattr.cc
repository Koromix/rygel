// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "xattr.hh"

#if defined(__linux__)
    #include <sys/xattr.h>
    #include "vendor/acl/include/libacl.h"
#elif defined(__FreeBSD__)
    #include <sys/acl.h>
    #include <sys/types.h>
    #include <sys/extattr.h>
#endif

namespace RG {

#if defined(__linux__)

static Span<const char> ReadACLs(int fd, const char *filename, Allocator *alloc)
{
    acl_t acl = acl_get_fd(fd);
    if (!acl) {
        LogError("Failed to open ACL entries for '%1': %2", filename, strerror(errno));
        return {};
    }
    RG_DEFER { acl_free(acl); };

    if (!acl_equiv_mode(acl, nullptr))
        return {};

    char *str = acl_to_any_text(acl, nullptr, '\n', TEXT_NUMERIC_IDS);
    if (!str) {
        LogError("Failed to read ACL entries for '%1': %2", filename, strerror(errno));
        return {};
    }
    RG_DEFER { acl_free(str); };

    Span<const char> copy = DuplicateString(str, alloc).ptr;
    return copy;
}

static bool WriteACLs(int fd, const char *filename, Span<const char> str)
{
    char buf[32768];
    CopyString(str, buf);

    acl_t acl = acl_from_text(buf);
    if (!acl) {
        LogError("Failed to decode ACL string for '%1': %2", filename, strerror(errno));
        return false;
    }
    RG_DEFER { acl_free(acl); };

    if (acl_set_fd(fd, acl) < 0) {
        LogError("Failed to set ACL for '%1': %2", filename, strerror(errno));
        return false;
    }

    return true;
}

static Size ReadAttribute(int fd, const char *filename, const char *key, HeapArray<uint8_t> *out_value)
{
    Size size = fgetxattr(fd, key, nullptr, 0);

    if (size < 0) {
        LogError("Failed to read extended attribute '%1' from '%2': %3", key, filename, strerror(errno));
        return -1;
    }
    if (!size)
        return 0;

retry:
    out_value->Grow(size);

    Size len = fgetxattr(fd, key, out_value->end(), out_value->capacity - out_value->len);

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

bool ReadXAttributes(int fd, const char *filename, Allocator *alloc, HeapArray<XAttrInfo> *out_xattrs)
{
    RG_DEFER_NC(err_guard, len = out_xattrs->len) { out_xattrs->RemoveFrom(len); };

    Span<const char> acls = ReadACLs(fd, filename, alloc);

    if (acls.len) {
        XAttrInfo xattr = { "rekkord.acl1", acls.As<uint8_t>() };
        out_xattrs->Append(xattr);
    }

    HeapArray<char> list(alloc);
    {
        Size size = Kibibytes(4);

retry:
        list.Grow(size);

        Size len = flistxattr(fd, list.ptr, (size_t)list.capacity);

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

        // Skip ACL xattrs
        if (StartsWith(key, "system."))
            continue;

        HeapArray<uint8_t> value(alloc);
        Size len = ReadAttribute(fd, filename, key.ptr, &value);

        if (len < 0)
            continue;

        xattr.key = key.ptr;
        xattr.value = value.TrimAndLeak();

        out_xattrs->Append(xattr);
    }

    list.Leak();

    err_guard.Disable();
    return true;
}

bool WriteXAttributes(int fd, const char *filename, Span<const XAttrInfo> xattrs)
{
    bool success = true;

    for (const XAttrInfo &xattr: xattrs) {
        if (TestStr(xattr.key, "rekkord.acl1")) {
            success &= WriteACLs(fd, filename, xattr.value.As<char>());
            continue;
        }

        int ret = fsetxattr(fd, xattr.key, xattr.value.ptr, xattr.value.len, 0);

        if (ret < 0) {
            LogError("Failed to write extended attribute '%1' to '%2': %3'", xattr.key, filename, strerror(errno));
            success = false;
        }
    }

    return success;
}

#elif defined(__FreeBSD__)

static Span<const char> ReadACLs(int fd, const char *filename, Allocator *alloc)
{
    acl_t acl = acl_get_fd(fd);
    if (!acl) {
        LogError("Failed to open ACL entries for '%1': %2", filename, strerror(errno));
        return {};
    }
    RG_DEFER { acl_free(acl); };

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
    RG_DEFER { acl_free(str); };

    Span<const char> copy = DuplicateString(str, alloc).ptr;
    return copy;
}

static bool WriteACLs(int fd, const char *filename, Span<const char> str)
{
    char buf[32768];
    CopyString(str, buf);

    acl_t acl = acl_from_text(buf);
    if (!acl) {
        LogError("Failed to decode ACL string for '%1': %2", filename, strerror(errno));
        return false;
    }
    RG_DEFER { acl_free(acl); };

    if (acl_set_fd(fd, acl) < 0) {
        LogError("Failed to set ACL for '%1': %2", filename, strerror(errno));
        return false;
    }

    return true;
}

static Size ReadAttribute(int fd, const char *filename, const char *key, HeapArray<uint8_t> *out_value)
{
    Size size = extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, key, nullptr, 0);

    if (size < 0) {
        LogError("Failed to read extended attribute '%1' from '%2': %3", key, filename, strerror(errno));
        return -1;
    }
    if (!size)
        return 0;

retry:
    out_value->Grow(size);

    Size available = out_value->capacity - out_value->len;
    Size len = extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, key, out_value->end(), available);

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

bool ReadXAttributes(int fd, const char *filename, Allocator *alloc, HeapArray<XAttrInfo> *out_xattrs)
{
    RG_DEFER_NC(err_guard, len = out_xattrs->len) { out_xattrs->RemoveFrom(len); };

    Span<const char> acls = ReadACLs(fd, filename, alloc);

    if (acls.len) {
        XAttrInfo xattr = { "rekkord.acl1", acls.As<uint8_t>() };
        out_xattrs->Append(xattr);
    }

    HeapArray<char> list;
    {
        Size size = Kibibytes(4);

retry:
        list.Grow(size);

        Size len = extattr_list_fd(fd, EXTATTR_NAMESPACE_USER, list.ptr, (size_t)list.capacity);

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
        if (TestStr(xattr.key, "rekkord.acl1")) {
            success &= WriteACLs(fd, filename, xattr.value.As<char>());
            continue;
        }

        if (!StartsWith(xattr.key, "user.")) {
            LogError("Cannot restore extended attribute '%1' for '%2': unsupported prefix", xattr.key, filename);
            success = false;
            continue;
        }

        int ret = extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, xattr.key + 5, xattr.value.ptr, xattr.value.len);

        if (ret < 0) {
            LogError("Failed to write extended attribute '%1' to '%2': %3'", xattr.key, filename, strerror(errno));
            success = false;
        }
    }

    return success;
}

#else

static std::once_flag flag;

bool ReadXAttributes(int, const char *, Allocator *, HeapArray<XAttrInfo> *)
{
    std::call_once(flag, []() { LogError("Extended attributes (xattrs) are not implemented or supported on this platform"); });
    return false;
}

bool WriteXAttributes(int, const char *, Span<const XAttrInfo>)
{
    std::call_once(flag, []() { LogError("Extended attributes (xattrs) are not implemented or supported on this platform"); });
    return false;
}

#endif

}
