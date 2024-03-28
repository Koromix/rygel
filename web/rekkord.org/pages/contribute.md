# Bugs and ideas

Use the official repository for bugs, ideas and features requests: https://github.com/Koromix/rekkord

Please note that the source code is not in this repository, instead it lives in a monorepo: https://github.com/Koromix/rygel/ (in the *src/rekkord* subdirectory).

# Build from source

Start by cloning the [git repository](https://github.com/Koromix/rygel):

```sh
git clone https://github.com/Koromix/rygel
cd rygel
```

## Windows

In order to build Rekkord on Windows, clone the repository and run these commands from the root directory in a _Visual Studio command prompt_ (x64 or x86, as you prefer):

```sh
bootstrap.bat
felix -pFast rekkord
```

After that, the binary will be available in the `bin/Fast` directory.

## Linux / macOS

In order to build Rekkord on Linux, clone the repository and run these commands from the root directory:

```sh
# Install required dependencies on Debian or Ubuntu:
sudo apt install build-essential

# Build binaries
./bootstrap.sh
./felix -pFast rekkord
```

After that, the binary will be available in the `bin/Fast` directory.

# Code style

Rekkord is programmed in C++. I don't use the standard library much (with a few exceptions) and instead rely on my own homegrown cross-platform code, containers and abstractions.

My personal preference goes to a rather C-like C++ style, with careful use of templates (mainly for containers) and little object-oriented programming. I strongly prefer tagged unions and code locality over inheritance and virtual methods. Exceptions are disabled.
