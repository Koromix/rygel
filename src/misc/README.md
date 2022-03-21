# SeatSH

SeatSH is a Windows service that enables you to launch graphical applications from an SSH connection on Windows 10. As an example, try to start `notepad` from an SSH connection: it won't work, because this session does not have access to the active console (desktop) session.

SeatSH runs as a service, waits for a client to connect, and launches the command line specified by the client on the active console session. Once the service is installed, launching a graphical application from the command line is as simple as `seatsh notepad`.

It will transparently forward standard streams (stdin, stdout, stderr) and report the application exit code as its own.

It is mainly useful for automated test systems, **do not install it** on production machines because it is a security risk. SeatSH only makes sure that the user connected to the SSH matches the one on the active console session, nothing more.

# Meestic

Meestic is a small command-line Linux only (at the moment) tool to control the keyboard lights of MSI Delta 15 laptops. It was made by looking at the HID packets sent by the Windows tool with Wireshark. It is provided "as is", I don't make any guarantee about this tool.

In order to build Meestic on Linux, clone the repository and run these commands from the root directory:

```sh
# Install required dependencies on Debian or Ubuntu:
sudo apt install git build-essential libudev-dev

# Build the meestic binary:
./bootstrap.sh
./felix -pFast meestic
```

After that, the binary will be available in the `bin/Fast` directory.

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
