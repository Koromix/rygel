# Bugs and ideas

Use the official repository for bugs, ideas and features requests: https://codeberg.org/Koromix/rekkord

Please note that the source code is not in this repository, instead it lives in a monorepo: https://codeberg.org/Koromix/rygel/ (in the *src/rekkord* subdirectory).

# Build from source

Start by cloning the [git repository](https://codeberg.org/Koromix/rygel):

```sh
git clone https://codeberg.org/Koromix/rygel
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
