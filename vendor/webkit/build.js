#!/usr/bin/env node

const WEBKIT = 'WebKit-7617.1.17.10.10';
const ICU = 'release-74-1';

const fs = require('fs');
const os = require('os');
const { spawnSync } = require('child_process');

main()

function main() {
    process.chdir(__dirname)

    // Get host information
    let platform;
    let architecture;
    {
        let proc = spawnSync('../../felix', ['--version']);
        if (proc.status !== 0)
            throw new Error('Failed to get host information from felix');
        let output = proc.stdout.toString();

        try {
            platform = output.match(/Host: (?:[a-zA-Z0-9_]+\/)*([a-zA-Z0-9_]+)/)[1];
            architecture = output.match(/Architecture: ([a-zA-Z0-9_]+)/)[1];
        } catch (err) {
            throw new Error('Failed to get host information from felix');
        }
    }

    // Cleanup
    if (fs.existsSync('icu'))
        fs.rmSync('icu', { recursive: true, force: true });
    if (fs.existsSync('webkit'))
        fs.rmSync('webkit', { recursive: true, force: true });

    // Clone ICU repository
    run('.', 'git', ['clone', '--branch', ICU, '--depth', '1', 'https://github.com/unicode-org/icu.git', 'icu'])
    fs.rmSync('webkit/.git', { recursive: true, force: true });

    // Clone WebKit repository
    run('.', 'git', ['clone', '--branch', WEBKIT, '--depth', '1', 'https://github.com/WebKit/WebKit.git', 'webkit'])
    fs.rmSync('webkit/.git', { recursive: true, force: true });

    build_icu(platform, architecture);
    build_webkit(platform, architecture);
}

function build_icu(platform, architecture) {
    // Apply patch files
    for (let basename of fs.readdirSync('../_patches')) {
        if (!basename.startsWith('icu_'))
            continue;

        let filename = '../_patches/' + basename;
        run('git', ['apply', filename]);
    }

    // Build ICU
    {
        if (fs.existsSync('icu/build'))
            fs.rmSync('icu/build', { recursive: true, force: true });
        fs.mkdirSync('icu/build', { mode: 0o755 });

        run('icu/build', '../icu4c/source/runConfigureICU', [
            'Linux',
            '--enable-shared', '--enable-static'
        ]);
        run('icu/build', 'make', ['-j' + (os.cpus().length + 1)]);
    }

    // Copy library files
    {
        let src_dir = 'icu/build/lib';
        let lib_dir = 'lib/' + platform + '/' + architecture;

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
            fs.rmSync(include_dir, { recursive: true, force: true });
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

function build_webkit(platform, architecture) {
    // Apply patch files
    for (let basename of fs.readdirSync('../_patches')) {
        if (!basename.startsWith('webkit_'))
            continue;

        let filename = '../_patches/' + basename;
        run('.', 'git', ['apply', filename]);
    }

    // Build JSCore
    {
        if (fs.existsSync('webkit/build'))
            fs.rmSync('webkit/build', { recursive: true, force: true });
        fs.mkdirSync('webkit/build', { mode: 0o755 });

        run('webkit/build', 'cmake', [
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
        ]);
        run('webkit/build', 'ninja');
    }

    // Copy library files
    {
        let src_dir = 'webkit/build/lib'
        let lib_dir = 'lib/' + platform + '/' + architecture;

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
            fs.rmSync(include_dir, { recursive: true, force: true });
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

function run(cwd, cmd, args = []) {
    let proc = spawnSync(cmd, args, { cwd: cwd, stdio: 'inherit' });
    if (proc.status !== 0)
        throw new Error(`Command failed: ${cmd}`);
}
