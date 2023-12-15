# Overview

Meestic is a small utility made to control **the lights of MSI Delta 15 laptop keyboards** on Windows and Linux. It provides both a CLI and a system-tray GUI, and can fully replace the official MSI tool (which is not even available for Linux).

You can use the command-line or the GUI version. Download ready-to-use binaries from the release section of the [dedicated repository](https://github.com/Koromix/meestic/releases/latest).

It was made by looking at the HID packets sent by the Windows tool with Wireshark. It is provided "as is", I don't make any guarantee about this tool.

# Install

## Windows

Download ready-to-use binaries from the release section of the [dedicated repository](https://github.com/Koromix/meestic/releases/latest).

## Linux

A signed Debian repository is provided, and should work with Debian 11 and Debian derivatives (such as Ubuntu).

Execute the following commands (as root) to add the repository to your system:

```sh
mkdir -p -m0755 /etc/apt/keyrings
curl https://download.koromix.dev/debian/koromix-archive-keyring.gpg -o /etc/apt/keyrings/koromix-archive-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/koromix-archive-keyring.gpg] https://download.koromix.dev/debian stable main" > /etc/apt/sources.list.d/koromix.dev-stable.list
```

Once this is done, refresh the repository cache and install the package:

```sh
apt update
apt install meestic
```

For other distributions, you can [build the code from source](#build-from-source) as documented below.

# Graphical interface

MeesticTray runs in the **system tray**, and provides customizable profiles that you can cycle through with the tray icon or with the keyboard light function key.

## Windows

Simply download and run `MeesticTray.exe` manually, or set it up to run when Windows starts. It provides a system tray icon, you can left click on it to switch to the next profile or right click to get a list of configured profiles.

Create a file named `MeesticTray.ini` (or `MeesticGui.ini`) to customize the profiles, and put it either:

- Next to `MeesticTray.exe`
- Or at `<ProfileDir>\AppData\MeesticTray.ini` (e.g. _C:\Users\JohnDoe\AppData\MeesticTray.ini_)

Restart MeesticTray to apply changes made to the file.

## Linux

Install the Debian package to configure the system daemon (which is necessary to manage access to the HID device) and the GUI application. Both will start automatically but you may need to restart your session to see the system tray icon.

```sh
mkdir -p -m0755 /etc/apt/keyrings
curl https://download.koromix.dev/debian/koromix-archive-keyring.gpg -o /etc/apt/keyrings/koromix-archive-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/koromix-archive-keyring.gpg] https://download.koromix.dev/debian stable main" > /etc/apt/sources.list.d/koromix.dev-stable.list
```

Once this is done, refresh the repository cache and install the package:

```sh
apt update
apt install meestic
```

If you are using GNOME, please install an extension such as `Tray Icons: Reloaded` or the MeesticTray tray icon may not be visible.

Customize `/etc/meestic.ini` to edit default profiles. Restart the daemon after each change to load the new profiles with:

```sh
sudo systemctl restart meestic
```

## Example configuration

Here is an example:

```ini
# The profile set with the "Default" will run when the GUI starts
# Read the commented out section at the end for possible profile values

Default = Disabled

[Static Blue]
Mode = Static
Colors = MsiBlue

[Breathe Slow]
Mode = Breathe
Speed = 0
Colors = #FFA100 MsiBlue

[Epilepsy]
Mode = Transition
Speed = 2
Colors = #ff0000 #ffa500 #ffff00 #008000 #0000ff #4b0082 #ee82ee

[Disabled]
Mode = Disabled

# [Example]
# Intensity = 0 to 10
# Speed = 0 to 2
# Mode = Disabled, Static, Breathe or Transition
# Colors = list of Colors (only one for Static, 1 to 7 for Breathe or Transition), use name or CSS-like hexadecimal
# ManualOnly = Yes or No (if Yes, the option must be used from the context menu and won't be used when cycling modes with the function keys)
```

# Command-line version

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

# Build from source

## Windows

In order to build Meestic on Windows, clone the repository and run these commands from the root directory in a *Visual Studio command prompt* (x64 or x86, as you prefer):

```batch
bootstrap.bat
felix -pFast meestic MeesticTray
```

After that, the binaries will be available in the `bin/Fast` directory.

## Linux

In order to build Meestic on Linux, clone the repository and run these commands from the root directory:

```sh
# Install required dependencies on Debian or Ubuntu:
sudo apt install git build-essential libudev-dev

# Build binaries
./bootstrap.sh
./felix -pFast meestic MeesticTray
```

After that, the binaries will be available in the `bin/Fast` directory.
