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

My personal preference goes to a rather C-like C++ style:

- Careful use of templates (mainly for containers)
- Little object-oriented programming, tagged unions and code locality are preferred over inheritance and virtual methods
- Use of RAII where natural and defer-like statements otherwise
- No exceptions, errors are logged and signaled with a simple return value (bool when possible, a dedicated enum when more information is relevant)
- Memory regions are used when many allocations are needed, when building many strings for example
- Heavy use of array and string views for function arguments
- Avoid dependencies unless a strong argument can be made for one, and vendorize them aggressively (with patches if needed)

I keep watch of all dependencies using GitHub notifications, RSS feeds, mailing lists and visual web page change detection (depending on the project), in order to update them as needed.
