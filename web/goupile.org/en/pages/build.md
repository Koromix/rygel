# Build Goupile

The Goupile server is **Linux-only** for the moment, and uses the **Landlock LSM** (and seccomp) which requires Linux 5.13 or more recent.

> [!NOTE]
> Support for sandboxing on other operating systems is considered a long-term goal but is not currently available.
>
> Using Debian 12 or a later distribution is recommended.

Goupile relies on C++ (server-side) and HTML/CSS/JS (client-side). Compiling Goupile uses a dedicated tool included directly in the repository.

Start by fetching the code from the Git repository: https://codeberg.org/Koromix/rygel

```sh
git clone https://codeberg.org/Koromix/rygel
cd rygel
```

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

> [!NOTE]
> For production deployment, it is recommended to run Goupile behind an HTTPS reverse proxy such as NGINX.
