# Overview

Meestic is a small utility made to control **the lights of MSI Delta 15 laptop keyboards** on Windows and Linux. It provides both a CLI and a system-tray GUI, and can fully replace the official MSI tool (which is not even available for Linux).

You can use the command-line or the GUI version. Download ready-to-use binaries from the release section of the [dedicated repository](https://github.com/Koromix/meestic/releases/latest).

It was made by looking at the HID packets sent by the Windows tool with Wireshark. It is provided "as is", I don't make any guarantee about this tool.

# Usage

## Command-line version

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

Use `meestic --help` for a list of available options.

Be careful, color names and most options are **case-sensitive**.

## Configure GUI profiles

### Windows

Create a file named `MeesticTray.ini` (or `MeesticGui.ini`) to customize the profiles, and put it either:

- Next to `MeesticTray.exe`
- Or at `<ProfileDir>\AppData\MeesticTray.ini` (e.g. _C:\Users\JohnDoe\AppData\MeesticTray.ini_)

Restart the GUI to apply each change.

### Linux

After installing the Debian package, edit `/etc/meestic.ini`. Once done, restart the daemon with `sudo systemctl restart meestic`.

### Example

Here is an example:

```ini
# The profile set with Default will run when the GUI starts
Default = Disabled

[Static Blue]
Mode = Static
Colors = MsiBlue

[Breathe Slow]
Mode = Breathe
Speed = 0
Colors = #FFA100 MsiBlue

[Disabled]
Mode = Disabled

# [Example]
# Intensity = 0 to 10
# Speed = 0 to 2
# Mode = Disabled, Static, Breathe or Transition
# Colors = list of Colors (only one for Static, 1 to 7 for Breathe or Transition), use name or CSS-like hexadecimal
# ManualOnly = Yes or No (if Yes, the option must be used from the context menu and won't be used when cycling modes with the function keys)
```

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
