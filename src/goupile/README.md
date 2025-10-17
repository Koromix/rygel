# Introduction

Goupile is an open-source electronic data capture application that strives to make form creation and data entry both powerful and easy.

It is licensed under the [GPL 3 license](https://www.gnu.org/licenses/#GPL). You can freely download and use the goupile source code. Everyone is granted the right to copy, modify and redistribute it.

This project is sponsored by NLnet: https://nlnet.nl/project/Goupile/

<div align="center">
    <a href="https://nlnet.nl/" style="border-bottom-color: transparent;"><img src="https://codeberg.org/Koromix/rygel/raw/branch/master/web/goupile.org/static/nlnet/nlnet.svg" height="60" alt="NLnet Foundation"/></a>&nbsp;&nbsp;&nbsp;
    <a href="https://nlnet.nl/" style="border-bottom-color: transparent;"><img src="https://codeberg.org/Koromix/rygel/raw/branch/master/web/goupile.org/static/nlnet/ngi0core.svg" height="60" alt="NGI Zero Core"/></a>
    <br><br>
</div>

# Build from source

This repository uses a dedicated build tool called felix. To get started, you need to build this tool. You can use the bootstrap scripts at the root of the repository to bootstrap it:

* Run `./bootstrap.sh` on Linux and macOS
* Run `bootstrap.bat` on Windows

This will create a felix binary at the root of the source tree. You can then start it to build all projects defined in *FelixBuild.ini*: `felix` on Windows or `./felix` on Linux and macOS.

The following compilers are supported: GCC, Clang and MSVC (on Windows).

Use `./felix --help` for more information.

# Documentation

You can find more information on [the official website](https://goupile.org/), in french and english.
