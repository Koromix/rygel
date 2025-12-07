#!/bin/env node

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { Readable } = require('node:stream');
const zlib = require('zlib');

main();

async function main() {
    try {
        await run();
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function run() {
    process.chdir(__dirname);

    // Clean up
    fs.rmSync('assets', { recursive: true, force: true });
    fs.rmSync('src', { recursive: true, force: true });

    // Fetch source
    runCommand('git', 'clone', '--depth', 1, 'https://github.com/mayandev/notion-avatar', 'src');
    fs.cpSync('src/public/avatar/preview', 'assets', { recursive: true });

    // List assets
    let assets = {
        face: listAssets('assets/face'),
        beard: listAssets('assets/beard'),
        eyes: listAssets('assets/eyes'),
        eyebrows: listAssets('assets/eyebrows'),
        nose: listAssets('assets/nose'),
        mouth: listAssets('assets/mouth'),
        glasses: listAssets('assets/glasses'),
        hair: listAssets('assets/hair'),
        accessories: listAssets('assets/accessories')
    };
    fs.writeFileSync('notion.json', JSON.stringify(assets, null, 4));

    // Clean up
    fs.rmSync('src', { recursive: true, force: true });
}

function runCommand(cmd, ...args) {
    let proc = spawnSync(cmd, args, { stdio: 'inherit' });

    if (proc.status === null) {
        throw new Error(`Failed to start '${cmd}'`);
    } else if (proc.status != 0) {
        throw new Error(`Failed to run '${cmd}'`);
    }
}

function listAssets(dir) {
    let files = fs.readdirSync(dir);

    let parts = [];

    for (let name of files) {
        let filename = dir + '/' + name;
        let svg = fs.readFileSync(filename).toString();

        if (!svg.startsWith('<?xml'))
            svg = '<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n' + svg;

        parts.push(svg);
    }

    return parts;
}
