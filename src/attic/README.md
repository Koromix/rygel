# Serf

Serf is a **small HTTP server made for local testing**. It can serve static files, proxy remote websites (GET only), and you can customize the headers with a simple config file.

Here is an example configuration file for serving local files with the required Cross-Origin headers to enabe SharedArrayBuffer support:

```ini
[HTTP]
# Set SocketType to Dual (IPv4 + IPv6), IPv4, IPv6 or Unix
SocketType = Dual
Port = 80
# UnixPath is ignored unless SocketType is set to Unix
UnixPath = /run/serf.sock

[Settings]
# Enable AutoIndex to list content of directories without index.html
AutoIndex = On
# Maximum cache time in seconds
MaxAge = 0
# Generate E-tag based on file modification time and size
ETag = On

[Sources]
# Serve from path relative to INI file
Source = .

[Headers]
# List headers you want to add to all server responses
Cross-Origin-Embedder-Policy = require-corp
Cross-Origin-Opener-Policy = same-origin
```

Once this file exists, run serf with `serf -C serf.ini`. If you don't specify the file explicitly, serf will try to find one from its application directory (i.e. the directory where the executable resides).

Here is another configuration to reverse proxy https://koromix.dev/ unless a local file match in the directory `files/` exists:

```ini
[HTTP]
SocketType = Dual
Port = 80

[Sources]
Source = files
Source = https://koromix.dev/
```

Run `serf --help` for more information.

## Build on Linux and macOS

```bat
git clone --depth 1 https://github.com/Koromix/rygel.git
cd rygel

bootstrap.bat
felix -pFast serf
```

The binaries are then available in the `bin/Fast` directory.

## Build on Windows

```sh
git clone --depth 1 https://github.com/Koromix/rygel.git
cd rygel

./bootstrap.sh
./felix -pFast serf
```

The binaries are then available in the `bin/Fast` directory.

# SeatSH

SeatSH is a Windows service that enables you to launch graphical applications from an SSH connection on Windows 10. As an example, try to start `notepad` from an SSH connection: it won't work, because this session does not have access to the active console (desktop) session.

SeatSH runs as a service, waits for a client to connect, and launches the command line specified by the client on the active console session. Once the service is installed, launching a graphical application from the command line is as simple as `seatsh notepad`.

It will transparently forward standard streams (stdin, stdout, stderr) and report the application exit code as its own.

It is mainly useful for automated test systems, **do not install it** on production machines because it is a security risk. SeatSH only makes sure that the user connected to the SSH matches the one on the active console session, nothing more.

## Build and installation

In order to build SeatSH on Windows, clone the repository and run these commands from the root directory:

```bat
git clone --depth 1 https://github.com/Koromix/rygel.git
cd rygel

bootstrap.bat
felix -pFast seatsh
```

After that, the binary will be available in the `bin/Fast` directory.

Put it somewhere pratical, ideally in the PATH (so you can use it easily), and install the service by running the commands given at the end by `seatsh --help` from a *command prompt with administrative privileges*.

# Authool

Authtool can **generate and check passwords**, and can also **create and check TOTP secrets and codes**. It can also generate a valid QR code PNG file with a TOTP secret.

# Empress

Empress uses the compression algorithms implemented in libcc to provide simple **single-file compression and decompression**, and so far supports Zlib, Gzip, Brotli and LZ4.

Its main purpose is to test compression and decompression implementations against official tools.

# Hodler

Hodler is a simple and **very opinitated static-site builder** that I use to generate the pages on [koromix.dev](https://koromix.dev/) and [demheter.fr](https://demheter.fr/) from Markdown files.

# Snaplite

Snaplite is a support tool to explore and restore SQLite snapshots stream, which is an extension I've made to SQLite WAL, and is used (among others) in goupile.
