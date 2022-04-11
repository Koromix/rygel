# Overview

Meestic is a small command-line tool to control the keyboard lights of MSI Delta 15 laptops. It was made by looking at the HID packets sent by the Windows tool with Wireshark. It is provided "as is", I don't make any guarantee about this tool.

For ease of use, pre-built x64 binaries (built statically with musl-libc) are available here: https://koromix.dev/files/meestic/

# Build

## Windows

In order to build Meestic on Windows, clone the repository and run these commands from the root directory in a *Visual Studio command prompt* (x64 or x86, as you prefer):

```sh
bootstrap.bat
felix -pFast meestic
```

After that, the binary will be available in the `bin/Fast` directory.

## Linux

In order to build Meestic on Linux, clone the repository and run these commands from the root directory:

```sh
# Install required dependencies on Debian or Ubuntu:
sudo apt install git build-essential libudev-dev

# Build the meestic binary:
./bootstrap.sh
./felix -pFast meestic
```

After that, the binary will be available in the `bin/Fast` directory.

# Usage

Here are a few examples on how to use it:

```sh
# Disable lighting
meestic -m Disabled

# Set default static MSI blue
meestic -m Static MsiBlue

# Slowly breathe between Orange and MsiBlue
meestic -m Breathe -s 0 "#FFA100" MsiBlue

# Quickly transition between Magenta, Orange and MsiBlue colors
meestic -m Transition -s 2 Magenta Orange MsiBlue
```

Be careful, color names and most options are **case-sensitive**.
