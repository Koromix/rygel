#!/usr/bin/env node

const fs = require('fs');
const os = require('os');
const { spawnSync } = require('child_process');

const ICU = process.env.ICU;
const WEBKIT = process.env.WEBKIT;
const PLATFORM = process.env.PLATFORM;
const ARCHITECTURE = process.env.ARCHITECTURE;

main();

function main() {
    process.chdir(__dirname)

    // Cleanup
    if (fs.existsSync('cmake'))
        fs.rmdirSync('cmake', { recursive: true });
    if (fs.existsSync('icu'))
        fs.rmdirSync('icu', { recursive: true });
    if (fs.existsSync('webkit'))
        fs.rmdirSync('webkit', { recursive: true });

    // Get recent CMake build
    fs.mkdirSync('cmake');
    switch (ARCHITECTURE) {
        case 'x86_64': { run('.', 'curl', ['-L', '-o', 'cmake.tar.gz', 'https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-linux-x86_64.tar.gz']); } break;
        case 'ARM64': { run('.', 'curl', ['-L', '-o', 'cmake.tar.gz', 'https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-linux-aarch64.tar.gz']); } break;
    }
    run('cmake', '/bin/tar', ['-x', '--strip-components=1', '-f', '../cmake.tar.gz']);

    // Clone ICU repository
    run('.', 'git', ['clone', '--branch', ICU, '--depth', '1', 'https://github.com/unicode-org/icu.git', 'icu']);
    fs.rmdirSync('icu/.git', { recursive: true });

    // Clone WebKit repository
    run('.', 'git', ['clone', '--branch', WEBKIT, '--depth', '1', 'https://github.com/WebKit/WebKit.git', 'webkit']);
    fs.rmdirSync('webkit/.git', { recursive: true });

    buildICU();
    buildWebKit();
}

function buildICU() {
    // Apply patch files
    for (let basename of fs.readdirSync('patches')) {
        if (!basename.startsWith('icu_'))
            continue;

        let filename = 'patches/' + basename;
        run('.', 'patch', ['-f', '-p3'], { input: filename });
    }

    // Build ICU
    {
        if (fs.existsSync('icu/build'))
            fs.rmdirSync('icu/build', { recursive: true });
        fs.mkdirSync('icu/build', { mode: 0o755 });

        run('icu/build', '../icu4c/source/runConfigureICU', [
            'Linux',
            '--enable-shared', '--enable-static'
        ], { CC: 'clang-18', CXX: 'clang++-18', CFLAGS: '-fPIC', CXXFLAGS: '-fPIC' });
        run('icu/build', 'make', ['-j' + (os.cpus().length + 1)]);
    }

    // Copy library files
    {
        let src_dir = 'icu/build/lib';
        let lib_dir = 'lib/' + PLATFORM + '/' + ARCHITECTURE;

        fs.mkdirSync(lib_dir, { recursive: true, mode: 0o755 });

        fs.copyFileSync(src_dir + '/libicudata.a', lib_dir + '/libicudata.a');
        fs.copyFileSync(src_dir + '/libicui18n.a', lib_dir + '/libicui18n.a');
        fs.copyFileSync(src_dir + '/libicuuc.a', lib_dir + '/libicuuc.a');
    }

    // Regroup header files
    {
        let source_dirs = ['icu/icu4c/source/common/unicode', 'icu/icu4c/source/i18n/unicode'];
        let include_dir = 'icu/build/include/unicode';

        if (fs.existsSync(include_dir))
            fs.rmdirSync(include_dir, { recursive: true });
        fs.mkdirSync(include_dir, { recursive: true, mode: 0o755 });

        for (let src_dir of source_dirs) {
            for (let basename of fs.readdirSync(src_dir)) {
                if (!basename.endsWith('.h'))
                    continue;

                let src_filename = src_dir + '/' + basename;
                let dest_filename = include_dir + '/' + basename;

                fs.copyFileSync(src_filename, dest_filename);
            }
        }
    }
}

function buildWebKit() {
    // Apply patch files
    for (let basename of fs.readdirSync('patches')) {
        if (!basename.startsWith('webkit_'))
            continue;

        let filename = 'patches/' + basename;
        run('.', 'patch', ['-f', '-p3'], { input: filename });
    }

    // Build JSCore
    {
        if (fs.existsSync('webkit/build'))
            fs.rmdirSync('webkit/build', { recursive: true });
        fs.mkdirSync('webkit/build', { mode: 0o755 });

        run('webkit/build', '../../cmake/bin/cmake', [
            '-G', 'Ninja',
            '-DPORT=JSCOnly',
            '-DICU_INCLUDE_DIR=../../icu/build/include',
            '-DICU_DATA_LIBRARY_RELEASE=../../icu/build/lib/libicudata.a',
            '-DICU_I18N_LIBRARY_RELEASE=../../icu/build/lib/libicui18n.a',
            '-DICU_UC_LIBRARY_RELEASE=../../icu/build/lib/libicuuc.a',
            '-DCMAKE_CXX_FLAGS=-DU_STATIC_IMPLEMENTATION',
            '-DCMAKE_BUILD_TYPE=Release',
            '-DDEVELOPER_MODE=OFF', '-DENABLE_FTL_JIT=ON',
            '-DENABLE_STATIC_JSC=ON', '-DUSE_THIN_ARCHIVES=OFF',
            '..'
        ], {
            env: { CC: 'clang-18', CXX: 'clang++-18' }
        });
        run('webkit/build', 'ninja');
    }

    // Copy library files
    {
        let src_dir = 'webkit/build/lib'
        let lib_dir = 'lib/' + PLATFORM + '/' + ARCHITECTURE;

        fs.mkdirSync(lib_dir, { recursive: true, mode: 0o755 });

        for (let basename of fs.readdirSync(src_dir)) {
            if (!basename.endsWith('.a'))
                continue;

            let src_filename = src_dir + '/' + basename;
            let dest_filename = lib_dir + '/' + basename;

            fs.copyFileSync(src_filename, dest_filename);
        }
    }

    // Copy and patch header files
    {
        let src_dir = 'webkit/build/JavaScriptCore/Headers/JavaScriptCore';
        let include_dir = 'include/JavaScriptCore'

        if (fs.existsSync(include_dir))
            fs.rmdirSync(include_dir, { recursive: true });
        fs.mkdirSync(include_dir, { recursive: true, mode: 0o755 });

        for (let basename of fs.readdirSync(src_dir)) {
            if (!basename.endsWith('.h'))
                continue;

            let src_filename = src_dir + '/' + basename;
            let dest_filename = include_dir + '/' + basename;

            let header = fs.readFileSync(src_filename, { encoding: 'utf8' });
            let patched = header.replace(/<JavaScriptCore\/([A-Za-z0-9_\.]+)>/g, '"$1"');

            fs.writeFileSync(dest_filename, patched);
        }
    }
}

function run(cwd, cmd, args = [], options = {}) {
    let stdio = ['inherit', 'inherit', 'inherit'];
    let env = Object.assign({}, process.env, options.env);

    if (options.input != null) {
        let fd = fs.openSync(options.input, 'r');
        let stdin = fs.createReadStream(null, { fd: fd });

        stdio[0] = stdin;
    }

    let proc = spawnSync(cmd, args, {
        cwd: cwd,
        env: env,
        stdio: stdio
    });

    if (proc.status !== 0)
        throw new Error(`Command failed: ${cmd}`);
}
