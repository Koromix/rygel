<!-- Title: koromix.dev — Meestic
     Menu: Meestic -->

# Overview

Meestic is a small utility made to control **the lights of MSI Delta 15 laptop keyboards** on Windows and Linux. It provides both a CLI and a system-tray GUI, and can fully replace the official MSI tool (which is not even available for Linux).

You can use the command-line or the GUI version. Download ready-to-use binaries from the release section of the [ dedicated repository](https://github.com/Koromix/meestic/releases/latest).

It was made by looking at the HID packets sent by the Windows tool with Wireshark. It is provided "as is", I don't make any guarantee about this tool.

# Graphical interface

MeesticGui runs in the **system tray**, and provides customizable profiles that you can cycle through with the tray icon or with the keyboard light function key.

## Windows

Simply download and run `MeesticGui.exe` manually, or set it up to run when Windows starts. It provides a system tray icon, you can left click on it to switch to the next profile or right click to get a list of configured profiles.

Create a file named `MeesticGui.ini` to customize the profiles, and put it either:

- Next to `MeesticGui.exe`
- Or at `<ProfileDir>\AppData\MeesticGui.ini` (e.g. _C:\Users\JohnDoe\AppData\MeesticGui.ini_)

Restart MeesticGui to apply changes made to the file.

## Linux

Install the Debian package to configure the system daemon (which is necessary to manage access to the HID device) and the GUI application. Both will start automatically but you may need to restart your session to see the system tray icon.

If you are using GNOME, please install an extension such as `Tray Icons: Reloaded` or the MeesticGui tray icon may not be visible.

Customize `/etc/meestic.ini` to edit default profiles. Restart the daemon after each change to load the new profiles with:

    sudo systemctl restart meestic

## Example configuration

Here is an example:

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

# Command-line version

Here are a few examples on how to use it:

    # Disable lighting
    meestic -m Disabled

    # Set default static MSI blue
    meestic -m Static MsiBlue

    # Slowly breathe between Orange and MsiBlue
    meestic -m Breathe -s 0 "#FFA100" MsiBlue

    # Quickly transition between Magenta, Orange and MsiBlue colors
    meestic -m Transition -s 2 Magenta Orange MsiBlue

Use `meestic --help` for a list of available options.

Be careful, color names and most options are **case-sensitive**.

# Build from source

## Windows

In order to build Meestic on Windows, clone the repository and run these commands from the root directory in a *Visual Studio command prompt* (x64 or x86, as you prefer):

    bootstrap.bat
    felix -pFast meestic MeesticGui

After that, the binaries will be available in the `bin/Fast` directory.

## Linux

In order to build Meestic on Linux, clone the repository and run these commands from the root directory:

    # Install required dependencies on Debian or Ubuntu:
    sudo apt install git build-essential libudev-dev

    # Build binaries
    ./bootstrap.sh
    ./felix -pFast meestic MeesticGui

After that, the binaries will be available in the `bin/Fast` directory.

# License

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU Affero General Public License** as published by the Free Software Foundation, either **version 3 of the License**, or (at your option) any later version.

Find more information here: [https://www.gnu.org/licenses/](https://www.gnu.org/licenses/)
