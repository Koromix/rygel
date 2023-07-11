# Authool

Authtool can generate and check passwords, and can also create and check TOTP secrets and codes. It can also generate a valid QR code PNG file with a TOTP secret.

# Empress

Empress uses the compression algorithms implemented in libcc to provide simple single-file compression and decompression, and so far supports Zlib, Gzip, Brotli and LZ4.

# Hodler

Hodler is a simple and limited static-site builder that I use to generate the pages on [koromix.dev](https://koromix.dev/) and [goupile.fr](https://goupile.fr/) from Markdown files.

# SeatSH

SeatSH is a Windows service that enables you to launch graphical applications from an SSH connection on Windows 10. As an example, try to start `notepad` from an SSH connection: it won't work, because this session does not have access to the active console (desktop) session.

SeatSH runs as a service, waits for a client to connect, and launches the command line specified by the client on the active console session. Once the service is installed, launching a graphical application from the command line is as simple as `seatsh notepad`.

It will transparently forward standard streams (stdin, stdout, stderr) and report the application exit code as its own.

It is mainly useful for automated test systems, **do not install it** on production machines because it is a security risk. SeatSH only makes sure that the user connected to the SSH matches the one on the active console session, nothing more.

# Serf

Small HTTP server made for localhost testing, with support for custom HTTP headers.

## Build and installation

In order to build SeatSH on Windows, clone the repository and run these commands from the root directory:

```sh
bootstrap.bat
felix -pFast seatsh
```

After that, the binary will be available in the `bin/Fast` directory.

Put it somewhere pratical, ideally in the PATH (so you can use it easily), and install the service by running the commands given at the end by `seatsh --help` from a *command prompt with administrative privileges*.
