#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

'use strict';

const fs = require('fs');
const cnoke = require('./src/index.js');

const VALID_COMMANDS = ['build', 'configure', 'clean'];

main();

async function main() {
    let config = {};
    let command = 'build';

    // Parse options
    {
        let i = 2;

        if (process.argv.length >= 3 && process.argv[2][0] != '-') {
            let cmd = process.argv[2];

            if (VALID_COMMANDS.includes(cmd)) {
                command = cmd;
                i++;
            }
        }

        for (; i < process.argv.length; i++) {
            let arg = process.argv[i];
            let value = null;

            if (arg[0] == '-') {
                if (arg.length > 2 && arg[1] != '-') {
                    value = arg.substr(2);
                    arg = arg.substr(0, 2);
                } else if (arg[1] == '-') {
                    let offset = arg.indexOf('=');

                    if (offset > 2 && arg.length > offset + 1) {
                        value = arg.substr(offset + 1);
                        arg = arg.substr(0, offset);
                    }
                }
                if (value == null && process.argv[i + 1] != null && process.argv[i + 1][0] != '-') {
                    value = process.argv[i + 1];
                    i++; // Skip this value next iteration
                }
            }

            if (arg == '--help') {
                print_usage();
                return;
            } else if (arg == '-D' || arg == '--directory') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                config.project_dir = fs.realpathSync(value);
            } else if (arg == '-P' || arg == '--package') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                config.package_dir = fs.realpathSync(value);
            } else if (arg == '-O' || arg == '--output') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                config.output_directory = value;
            } else if ((command == 'build' || command == 'configure') && arg == '--runtime') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);
                if (!value.match(/^[0-9]+\.[0-9]+\.[0-9]+$/))
                    throw new Error(`Malformed runtime version '${value}'`);

                config.runtime_version = value;
            } else if ((command == 'build' || command == 'configure') && arg == '--clang') {
                config.prefer_clang = true;
            } else if ((command == 'build' || command == 'configure') && (arg == '-t' || arg == '--toolchain')) {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                config.toolchain = value;
            } else if ((command == 'build' || command == 'configure') && (arg == '-c' || arg == '--config')) {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                switch (value.toLowerCase()) {
                    case 'relwithdebinfo': { config.mode = 'RelWithDebInfo'; } break;
                    case 'debug': { config.mode = 'Debug'; } break;
                    case 'release': { config.mode = 'Release'; } break;

                    default: {
                        throw new Error(`Unknown value '${value}' for ${arg}`);
                    } break;
                }
            } else if ((command == 'build' || command == 'configure') && arg == '--debug') {
                config.mode = 'Debug';
            } else if ((command == 'build' || command == 'configure') && arg == '--release') {
                config.mode = 'Release';
            } else if (command == 'build' && arg == '--prebuild') {
                config.prebuild = true;
            } else if (command == 'build' && arg == '--target') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                config.targets = [value];
            } else if (command == 'build' && (arg == '-v' || arg == '--verbose')) {
                config.verbose = true;
            } else {
                if (arg[0] == '-') {
                    throw new Error(`Unexpected argument '${arg}'`);
                } else {
                    throw new Error(`Unexpected value '${arg}'`);
                }
            }
        }
    }

    try {
        let builder = new cnoke.Builder(config);
        await builder[command]();
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function print_usage() {
    let help = `Usage: cnoke [command] [options...]

Commands:

    configure                            Configure CMake build
    build                                Build project (configure if needed)
    clean                                Clean build files

Options:

    -D, --directory <DIR>                Change source directory
                                         (default: current working directory)
    -P, --package <DIR>                  Change package directory
                                         (default: current working directory)
    -O, --output <DIR>                   Set explicit output directory
                                         (default: ./build)

    -c, --config <CONFIG>                Change build type: RelWithDebInfo, Debug, Release
                                         (default: ${cnoke.DefaultOptions.mode})
        --debug                          Shortcut for --config Debug
        --release                        Shortcut for --config Release

    -t, --toolchain <TOOLCHAIN>          Cross-compile for specific platform
        --runtime <VERSION>              Change node version
                                         (default: ${process.version})
        --clang                          Use Clang instead of default CMake compiler

        --prebuild                       Use prebuilt binary if available
        --target <TARGET>                Only build the specified target

    -v, --verbose                        Show build commands while building

The ARCH value is similar to process.arch, with the following differences:

- arm is changed to arm32hf or arm32sf depending on the floating-point ABI used (hard-float, soft-float)
- riscv32 is changed to riscv32sf, riscv32hf32, riscv32hf64 or riscv32hf128 depending on the floating-point ABI
- riscv64 is changed to riscv64sf, riscv64hf32, riscv64hf64 or riscv64hf128 depending on the floating-point ABI`;

    console.log(help);
}
