# Build Goupile

The Goupile server is cross-platform, but it is **recommended to use it on Linux** for maximum security.

> [!NOTE]
> Indeed, on Linux, Goupile can run in a sandboxed mode thanks to seccomp and Landlock. Support for sandboxing on other operating systems is considered a long-term goal but is not currently available.
>
> Using Debian 12 or a later distribution is recommended.

Goupile relies on C++ (server-side) and HTML/CSS/JS (client-side). Compiling Goupile uses a dedicated tool included directly in the repository.

Start by fetching the code from the Git repository: https://framagit.org/interhop/goupile

```sh
git clone https://framagit.org/interhop/goupile
cd goupile
```

## Linux

To compile a **development and test version**, proceed as follows from the root of the repository:

```sh
# Prepare the felix tool used to compile Goupile
./bootstrap.sh

# The executable will be placed in the bin/Debug folder
./felix
```

For **production use**, it is recommended to compile Goupile [in Paranoid mode](technical/architecture.md#compilation-options) using Clang 18+ and the LLD 18+ linker. On Debian 12, you can do the following:

```sh
# Prepare the felix tool used to compile Goupile
./bootstrap.sh

# Installation of LLVM described here and copied below: https://apt.llvm.org/
sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
sudo apt install clang-18 lld-18

# The executable will be placed in the bin/Paranoid folder
./felix -pParanoid --host=,clang-18,lld-18
```

## Other Systems

To compile a development version, proceed as follows from the root of the repository:

### POSIX Systems (macOS, WSL, etc.)

```sh
# Prepare the felix tool used to compile Goupile
./bootstrap.sh

# The executable will be placed in the bin/Debug folder
./felix
```

### Windows

```sh
# Prepare the felix tool used to compile Goupile
# It may be necessary to use the Visual Studio command prompt environment before continuing.
bootstrap.bat

# The executable will be placed in the bin/Debug folder
felix
```

It is not recommended to use Goupile in production on another system, as sandboxing mode and Paranoid mode compilation are not currently available there.

However, you can use the command `./felix --help` (or `felix --help` on Windows) to check the compilation modes and options available for your system.

# Run Goupile

Once the Goupile executable is compiled, you can create a Goupile domain using the following command:

```sh
# For this example, we will create this domain in a tmp subfolder of the repository.
# But you can create it wherever you wish!
mkdir -p tmp/test
touch tmp/test/goupile.ini
```

Initializing this domain will create an archive recovery key that you must store in order to restore an archive created in the Goupile domain's admin panel. If this key is lost, it can be changed, but archives created with the previous key will not be recoverable!

To access this domain via a web browser, you can start it using the following command:

```sh
# With this command, Goupile will be accessible via http://localhost:8889/
bin/Debug/goupile -C tmp/test/goupile.ini
```

For production deployment, it is recommended to run Goupile behind an HTTPS reverse proxy such as NGINX.

To automate the deployment of a complete production server (with multiple Goupile domains and NGINX configured automatically), we provide ready-to-use Ansible playbooks and roles that you can use as-is or adapt to your needs.
