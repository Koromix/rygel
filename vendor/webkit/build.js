#!/usr/bin/env node

const COMMIT = 'WebKit-7616.1.14.11.1';

const fs = require('fs');
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

    // Clone repository
    if (fs.existsSync('webkit'))
        fs.rmSync('webkit', { recursive: true, force: true });
    run('git', ['clone', '--branch', COMMIT, '--depth', '1', 'https://github.com/WebKit/WebKit.git', 'webkit'])
    fs.rmSync('webkit/.git', { recursive: true, force: true });

    // Apply patch files
    for (let basename of fs.readdirSync('../_patches')) {
        if (!basename.startsWith('webkit_'))
            continue;

        let filename = '../_patches/' + basename;
        run('git', ['apply', filename]);
    }

    // Build JSCore
    {
        if (fs.existsSync('build'))
            fs.rmSync('build', { recursive: true, force: true });
        fs.mkdirSync('build', { mode: 0o755 });

        process.chdir('build');

        run('cmake', ['-G', 'Ninja', '-DPORT=JSCOnly', '-DCMAKE_BUILD_TYPE=Release', '-DDEVELOPER_MODE=OFF', '-DENABLE_FTL_JIT=ON', '-DENABLE_STATIC_JSC=ON', '-DUSE_THIN_ARCHIVES=OFF', '../webkit']);
        run('ninja');
    }

    // Copy library files
    {
        let lib_dir = '../lib/' + platform + '/' + architecture;

        if (fs.existsSync(lib_dir))
            fs.rmSync(lib_dir, { recursive: true, force: true });
        fs.mkdirSync(lib_dir, { recursive: true, mode: 0o755 });

        for (let basename of fs.readdirSync('lib')) {
            if (!basename.endsWith('.a'))
                continue;

            let src_filename = 'lib/' + basename;
            let dest_filename = lib_dir + '/' + basename;

            fs.copyFileSync(src_filename, dest_filename);
        }
    }

    // Copy and patch header files
    {
        if (fs.existsSync('../include/JavaScriptCore'))
            fs.rmSync('../include/JavaScriptCore', { recursive: true, force: true });
        fs.mkdirSync('../include/JavaScriptCore', { recursive: true, mode: 0o755 });

        for (let basename of fs.readdirSync('JavaScriptCore/Headers/JavaScriptCore')) {
            if (!basename.endsWith('.h'))
                continue;

            let src_filename = 'JavaScriptCore/Headers/JavaScriptCore/' + basename;
            let dest_filename = '../include/JavaScriptCore/' + basename;

            let header = fs.readFileSync(src_filename, { encoding: 'utf8' });
            let patched = header.replace(/<JavaScriptCore\/([A-Za-z0-9_\.]+)>/g, '"$1"');

            fs.writeFileSync(dest_filename, patched);
        }
    }
}

function run(cmd, args) {
    let proc = spawnSync(cmd, args, { stdio: 'inherit' });
    if (proc.status !== 0)
        throw new Error('Command failed');
}
