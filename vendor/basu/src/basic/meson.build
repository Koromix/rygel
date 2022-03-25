# SPDX-License-Identifier: LGPL-2.1+

basic_sources = files('''
        alloc-util.c
        alloc-util.h
        audit-util.c
        audit-util.h
        bus-label.c
        bus-label.h
        def.h
        env-util.c
        env-util.h
        errno-list.c
        errno-list.h
        escape.c
        escape.h
        fd-util.c
        fd-util.h
        fileio.c
        fileio.h
        format-util.h
        fs-util.c
        fs-util.h
        gunicode.c
        gunicode.h
        hash-funcs.c
        hash-funcs.h
        hashmap.c
        hashmap.h
        hexdecoct.c
        hexdecoct.h
        io-util.c
        io-util.h
        json-internal.h
        json.c
        json.h
        list.h
        locale-util.c
        locale-util.h
        log.c
        log.h
        macro.h
        memfd-util.c
        memfd-util.h
        parse-util.c
        parse-util.h
        path-util.c
        path-util.h
        prioq.c
        prioq.h
        process-util.c
        process-util.h
        random-util.c
        random-util.h
        refcnt.h
        set.h
        siphash24.c
        siphash24.h
        socket-util.c
        socket-util.h
        stdio-util.h
        string-table.h
        string-util.c
        string-util.h
        strv.c
        strv.h
        syslog-util.c
        syslog-util.h
        terminal-util.c
        terminal-util.h
        time-util.c
        time-util.h
        unaligned.h
        user-util.c
        user-util.h
        utf8.c
        utf8.h
        util.c
        util.h
        verbs.c
        verbs.h
        xml.c
        xml.h
'''.split())

if have_libcap
        basic_sources += files('''
                cap-list.c
                cap-list.h
                capability-util.c
                capability-util.h
        '''.split())
endif

missing_h = files('missing.h')

generate_gperfs = find_program('generate-gperfs.py')

if have_libcap
        generate_cap_list = find_program('generate-cap-list.sh')
        cap_list_txt = custom_target(
                'cap-list.txt',
                output : 'cap-list.txt',
                command : [generate_cap_list, cpp, config_h, missing_h],
                capture : true)
else
        cap_list_txt = disabler()
endif

generate_errno_list = find_program('generate-errno-list.sh')
errno_list_txt = custom_target(
        'errno-list.txt',
        output : 'errno-list.txt',
        command : [generate_errno_list, cpp],
        capture : true)

generated_gperf_headers = []
foreach item : [['cap',    cap_list_txt,    'capability', ''],
                ['errno',  errno_list_txt,  'errno',      '']]

        if is_disabler(item[1])
                continue
        endif

        fname = '@0@-from-name.gperf'.format(item[0])
        gperf_file = custom_target(
                fname,
                input : item[1],
                output : fname,
                command : [generate_gperfs, item[2], item[3], '@INPUT@'],
                capture : true)

        fname = '@0@-from-name.h'.format(item[0])
        target1 = custom_target(
                fname,
                input : gperf_file,
                output : fname,
                command : [gperf,
                           '-L', 'ANSI-C', '-t', '--ignore-case',
                           '-N', 'lookup_@0@'.format(item[2]),
                           '-H', 'hash_@0@_name'.format(item[2]),
                           '-p', '-C',
                           '@INPUT@'],
                capture : true)

        fname = '@0@-to-name.h'.format(item[0])
        awkscript = '@0@-to-name.awk'.format(item[0])
        target2 = custom_target(
                fname,
                input : [awkscript, item[1]],
                output : fname,
                command : [awk, '-f', '@INPUT0@', '@INPUT1@'],
                capture : true)

        generated_gperf_headers += [target1, target2]
endforeach

basic_sources += [missing_h] + generated_gperf_headers

libbasic = static_library(
        'basic',
        basic_sources,
        include_directories : includes,
        dependencies : [threads,
                        libcap,
                        libm],
        c_args : ['-fvisibility=default'],
        install : false)
