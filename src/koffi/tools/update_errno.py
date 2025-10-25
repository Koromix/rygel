#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import os

ERROR_CODES = [
    'E2BIG', 'EACCES', 'EADDRINUSE', 'EADDRNOTAVAIL', 'EADV', 'EAFNOSUPPORT', 'EAGAIN', 'EALREADY', 'EAUTH', 'EBADE',
    'EBADF', 'EBADFD', 'EBADMSG', 'EBADR', 'EBADRPC', 'EBADRQC', 'EBADSLT', 'EBFONT', 'EBUSY', 'ECANCELED', 'ECAPMODE',
    'ECHILD', 'ECHRNG', 'ECOMM', 'ECONNABORTED', 'ECONNREFUSED', 'ECONNRESET', 'EDEADLK', 'EDEADLOCK', 'EDESTADDRREQ',
    'EDOM', 'EDOOFUS', 'EDOTDOT', 'EDQUOT', 'EEXIST', 'EFAULT', 'EFBIG', 'EFTYPE', 'EHOSTDOWN', 'EHOSTUNREACH',
    'EHWPOISON', 'EIDRM', 'EILSEQ', 'EINPROGRESS', 'EINTEGRITY', 'EINTR', 'EINVAL', 'EIO', 'EISCONN', 'EISDIR', 'EISNAM',
    'EKEYEXPIRED', 'EKEYREJECTED', 'EKEYREVOKED', 'EL2HLT', 'EL2NSYNC', 'EL3HLT', 'EL3RST', 'ELIBACC', 'ELIBBAD',
    'ELIBEXEC', 'ELIBMAX', 'ELIBSCN', 'ELNRNG', 'ELOOP', 'EMEDIUMTYPE', 'EMFILE', 'EMLINK', 'EMSGSIZE', 'EMULTIHOP',
    'ENAMETOOLONG', 'ENAVAIL', 'ENEEDAUTH', 'ENETDOWN', 'ENETRESET', 'ENETUNREACH', 'ENFILE', 'ENOANO', 'ENOATTR',
    'ENOBUFS', 'ENOCSI', 'ENODATA', 'ENODEV', 'ENOENT', 'ENOEXEC', 'ENOKEY', 'ENOLCK', 'ENOLINK', 'ENOMEDIUM', 'ENOMEM',
    'ENOMSG', 'ENONET', 'ENOPKG', 'ENOPROTOOPT', 'ENOSPC', 'ENOSR', 'ENOSTR', 'ENOSYS', 'ENOTBLK', 'ENOTCAPABLE',
    'ENOTCONN', 'ENOTDIR', 'ENOTEMPTY', 'ENOTNAM', 'ENOTRECOVERABLE', 'ENOTSOCK', 'ENOTSUP', 'ENOTTY', 'ENOTUNIQ',
    'ENXIO', 'EOPNOTSUPP', 'EOTHER', 'EOVERFLOW', 'EOWNERDEAD', 'EPERM', 'EPFNOSUPPORT', 'EPIPE', 'EPROCLIM', 'EPROCUNAVAIL',
    'EPROGMISMATCH', 'EPROGUNAVAIL', 'EPROTO', 'EPROTONOSUPPORT', 'EPROTOTYPE', 'ERANGE', 'EREMCHG', 'EREMOTE', 'EREMOTEIO',
    'ERESTART', 'ERFKILL', 'EROFS', 'ERPCMISMATCH', 'ESHUTDOWN', 'ESOCKTNOSUPPORT', 'ESPIPE', 'ESRCH', 'ESRMNT', 'ESTALE',
    'ESTRPIPE', 'ETIME', 'ETIMEDOUT', 'ETOOMANYREFS', 'ETXTBSY', 'EUCLEAN', 'EUNATCH', 'EUSERS', 'EWOULDBLOCK', 'EXDEV',
    'EXFULL', 'STRUNCATE'
]

if __name__ == "__main__":
    src_dir = os.path.dirname(__file__) + '/../src'
    filename = src_dir + '/errno.inc'

    with open(filename) as f:
        lines = f.readlines()

    with open(filename, 'w') as f:
        for line in lines:
            if not line.startswith('//'):
                break
            f.write(line)

        print('', file = f)
        print('#include <errno.h>', file = f)

        print('', file = f)
        print('struct ErrnoCodeInfo {', file = f)
        print('    const char *name;', file = f)
        print('    int value;', file = f)
        print('};', file = f)

        print('', file = f)
        print('const ErrnoCodeInfo ErrnoCodes[] = {', file = f)
        for code in ERROR_CODES:
            print(f'#if defined({code})', file = f)
            code_str = '"' + code + '"'
            print('    {' + code_str + ', ' + code + ' },', file = f)
            print('#endif', file = f)
        print('};', file = f)
