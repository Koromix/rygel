# SPDX-License-Identifier: LGPL-2.1+

_systemd_headers = '''
        sd-bus.h
        sd-bus-protocol.h
        sd-bus-vtable.h
        sd-id128.h
'''.split()

# https://github.com/mesonbuild/meson/issues/1633
systemd_headers = files(_systemd_headers)

install_headers(
        systemd_headers,
        '_sd-common.h',
        subdir : 'basu')

