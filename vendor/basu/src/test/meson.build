# SPDX-License-Identifier: LGPL-2.1-or-later

libtest = static_library(
        'libtest',
        ['tests.c', 'tests.h'],
        install: false,
        include_directories : includes,
        dependencies : [threads, librt, libcap])

############################################################

tests += [
        [['src/libsystemd/sd-bus/test-bus-marshal.c'],
         [libtest, libsystemd_static],
         []],

        [['src/libsystemd/sd-bus/test-bus-signature.c'],
         [libtest, libsystemd_static],
         [threads]],

        [['src/libsystemd/sd-bus/test-bus-chat.c'],
         [libtest, libsystemd_static],
         [threads]],

        [['src/libsystemd/sd-bus/test-bus-cleanup.c'],
         [libtest, libsystemd_static],
         [threads]],

        [['src/libsystemd/sd-bus/test-bus-error.c'],
         [libtest, libsystemd_static],
         []],

        [['src/libsystemd/sd-bus/test-bus-server.c'],
         [libtest, libsystemd_static],
         [threads]],

        [['src/libsystemd/sd-bus/test-bus-gvariant.c'],
         [libtest, libsystemd_static],
         []],

        [['src/libsystemd/sd-bus/test-bus-objects.c'],
         [libtest, libsystemd_static],
         [threads]],

        [['src/libsystemd/sd-bus/test-bus-vtable.c',
          'src/libsystemd/sd-bus/test-vtable-data.h'],
         [libtest, libsystemd_static],
         []],

        [['src/libsystemd/sd-bus/test-bus-creds.c'],
         [libtest, libsystemd_static],
         []],

        [['src/libsystemd/sd-bus/test-bus-match.c'],
         [libtest, libsystemd_static],
         []],

        #        [['src/libsystemd/sd-bus/test-bus-benchmark.c'],
        #         [libtest, libsystemd_static],
        #         [threads],
        #         '', 'manual'],


        [['src/libsystemd/sd-bus/test-bus-introspect.c',
          'src/libsystemd/sd-bus/test-vtable-data.h'],
         [libtest, libsystemd_static],
         []],
]

if cxx_cmd != ''
        tests += [
                [['src/libsystemd/sd-bus/test-bus-vtable-cc.cc'],
                 [libtest, libsystemd_static],
                 []]
        ]
endif

