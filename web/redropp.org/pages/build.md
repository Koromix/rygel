# Build Redropp

The Redropp server works **best on Linux** at the moment, because it can use **Landlock and seccomp** to reduce the attack surface. Please note that Landlock requires Linux 5.13 or more recent.

> [!NOTE]
> Even if Linux is preferred, the Redropp server should also work on FreeBSD, OpenBSD, Windows and macOS. Support for sandboxing on other operating systems is considered a long-term goal but is not currently available.
>
> Using **Debian 12 or a later distribution** is recommended.

Redropp relies on C++ (server-side) and HTML/CSS/JS (client-side). Compiling Redropp uses a dedicated tool included directly in the repository.

Start by fetching the code from the Git repository: https://codeberg.org/Koromix/rygel

```sh
git clone https://codeberg.org/Koromix/rygel
cd rygel
```

To compile a **development and test version**, proceed as follows from the root of the repository:

```sh
# Prepare the felix tool used to compile Redropp
./bootstrap.sh

# The executable will be placed in the bin/Debug folder
./felix
```

For **production use**, it is recommended to compile Redropp in Paranoid mode using Clang 18+ and the LLD 18+ linker. On Debian 12, you can do the following:

```sh
# Prepare the felix tool used to compile Redropp
./bootstrap.sh

# Installation of LLVM described here and copied below: https://apt.llvm.org/
sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
sudo apt install clang-18 lld-18

# The executable will be placed in the bin/Paranoid folder
./felix -pParanoid --host=,clang-18,lld-18
```

# Run Redropp

Once the Redropp executable is compiled, you can create a Redropp config using the following command:

```sh
# For this example, we will create this config in a tmp subfolder of the repository.
# But you can create it wherever you wish!

mkdir -p tmp
bin/Debug/redropp init tmp/test
```

To access this instance via a web browser, you can start it using the following command:

```sh
# With this command, Redropp will be accessible via http://localhost:8894/
bin/Debug/redropp -C tmp/test
```

> [!NOTE]
> For production deployment, it is recommended to run Redropp behind an HTTPS reverse proxy such as NGINX.
