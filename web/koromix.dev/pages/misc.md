# SaneBB

SaneBB is a **small and fast BBCode parser in Java**, which I made as an alternative to the horribly overengineered and [inner-platform effect](https://en.wikipedia.org/wiki/Inner-platform_effect) victim [KefirBB library](https://github.com/kefirfromperm/kefirbb).

It's a single Java file, the documentation is included at the top of the file.

You can find it here [on GitHub](https://github.com/Koromix/libraries/blob/master/SaneBB.java).

# Serf

Serf is a **small HTTP server made for local testing**. It can serve static file, proxy remote websites (GET only), and you can customize the headers with a simple config file.

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

You can find out more in the [code repository](https://github.com/Koromix/rygel/tree/master/src/attic#serf).

# SeatSH

SeatSH is a Windows service that enables you to **launch graphical applications from an SSH connection on Windows 10**. As an example, try to start `notepad` from an SSH connection: it won't work, because this session does not have access to the active console (desktop) session.

SeatSH runs as a service, waits for a client to connect, and launches the command line specified by the client on the active console session. Once the service is installed, launching a graphical application from the command line is as simple as `seatsh notepad`.

It will transparently forward standard streams (stdin, stdout, stderr) and report the application exit code as its own.

It is mainly useful for automated test systems, do not install it on production machines because it is a security risk. SeatSH only makes sure that the user connected to the SSH matches the one on the active console session, nothing more.

You can find out more in the [code repository](https://github.com/Koromix/rygel/tree/master/src/attic#seatsh).

# DRD

The DRD project strives to provide a **fully-featured and up-to-date alternative french PMSI MCO classifier**.

Using the data files (.tab) provided with GenRSA, it can classify MCO stays from 2013 to 2022 *several hundred times faster* than the official classifier (on a 4 core computer from 2015, it can classify around 4 million stays per second).

It can be used in three ways:

- *libdrd*: C++ library, usable as a single-header file.
- *drdc*: command-line tool.
- *drdR*: R package (load stays as data.frames, and classify them).

The classifier was carefully assembled from the documentation and through retro-engineering of the official classifier (GenRSA), without source code access.

You can find out more [in the code repository](https://github.com/Koromix/rygel/tree/master/src/drd).
