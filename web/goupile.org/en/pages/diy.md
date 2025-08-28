Okay, here is the English translation of the provided technical documentation.

# Installation

## Linux (Debian)

A signed Debian repository is available and can be used with Debian 11 and Debian derivatives (e.g., Ubuntu).

Run the following commands (as root) to add the repository to your system:

```sh
mkdir -p -m0755 /etc/apt/keyrings
curl https://download.koromix.dev/debian/koromix-archive-keyring.gpg -o /etc/apt/keyrings/koromix-archive-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/koromix-archive-keyring.gpg] https://download.koromix.dev/debian stable main" > /etc/apt/sources.list.d/koromix.dev-stable.list
```

Once this is done, refresh the package cache and install Goupile:

```sh
apt update
apt install goupile
```

For other distributions, you can [compile the code](#compilation) as indicated below.

## Linux (RPM)

A signed RPM repository is available and can be used with RHEL, Fedora, and Rocky Linux (9+).

Run the following commands (as root) to add the repository to your system:

```sh
curl https://download.koromix.dev/rpm/koromix-repo.asc -o /etc/pki/rpm-gpg/koromix-repo.asc

echo "[koromix]
name=koromix repository
baseurl=https://download.koromix.dev/rpm
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/koromix-repo.asc" > /etc/yum.repos.d/koromix.repo
```

Once this is done, install the package with this command:

```sh
dnf install goupile
```

For other distributions, you can [compile the code](#compilation) as indicated below.

# Domains

## Creating a Domain

To create a new domain, run the following command:

```sh
/usr/lib/goupile/manage.py create <name> [-p <HTTP port>]
```

Remember to **securely store** the decryption key provided to you when the domain is created!

## Deleting a Domain

Delete the corresponding INI file in `/etc/goupile/domains.d` and stop the service:

```sh
rm /etc/goupile/domains.d/<name>.ini
/usr/lib/goupile/manage.py sync
```

# Maintenance

## Updates

You should configure your server for automatic updates:

```sh
apt install unattended-upgrades
dpkg-reconfigure -pmedium unattended-upgrades
```

## Backups

Goupile data is stored in the `/var/lib/goupile` directory. You should regularly back up this folder.

# Reverse Proxy

## NGINX

Edit your NGINX configuration (directly or in a server file in `/etc/nginx/sites-available`) to set it up as a reverse proxy for Goupile.

The server block should look something like this:

```
server {
    # ...

    location / {
        proxy_http_version 1.1;
        proxy_buffering on;
        proxy_read_timeout 180;
        send_timeout 180;

        client_max_body_size 256M;

        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        proxy_pass http://127.0.0.1:8888/;
    }
}
```

## Apache 2

Edit your Apache 2 configuration (directly or in a server file in `/etc/apache2/sites-available`) to set it up as a reverse proxy for Goupile.

The VirtualHost block should look something like this:

```
<VirtualHost *:443>
    # ...

    LimitRequestBody 268435456

    ProxyPreserveHost On
    ProxyPass "/" "http://127.0.0.1:8888/"
    ProxyPassReverse "/" "http://127.0.0.1:8888/"
</VirtualHost>
```

# Compilation

The Goupile server is cross-platform, but it is **recommended to use it on Linux** for maximum security.

> [!NOTE]
> Indeed, on Linux, Goupile can run in a sandboxed mode thanks to seccomp and Linux namespaces (unshare system call). Support for sandboxing on other operating systems is considered a long-term goal but is not currently available. Using the Debian 11 or a later distribution is recommended.

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

# The executable will be placed in the bin/AUBSan folder
./felix
```

For **production use**, it is recommended to compile Goupile [in Paranoid mode](technical/architecture.md#compilation-options) using Clang 18+ and the LLD 18+ linker. On Debian 11, you can do the following:

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

# The executable will be placed in the bin/AUBSan folder
./felix
```

### Windows

```sh
# Prepare the felix tool used to compile Goupile
# It may be necessary to use the Visual Studio command prompt environment before continuing.
bootstrap.bat

# The executable will be placed in the bin/AUBSan folder
felix
```

It is not recommended to use Goupile in production on another system, as sandboxing mode and Paranoid mode compilation are not currently available there.

However, you can use the command `./felix --help` (or `felix --help` on Windows) to check the compilation modes and options available for your system.

# Manual Execution

Once the Goupile executable is compiled, you can create a Goupile domain using the following command:

```sh
# For this example, we will create this domain in a tmp subfolder
# of the repository, but you can create it wherever you wish!
mkdir tmp

# Running this command will prompt you to create a first
# administrator account and set its password.
bin/Paranoid/goupile init tmp/test_domain
```

Initializing this domain will create an archive recovery key that you must store in order to restore an archive created in the Goupile domain's admin panel. If this key is lost, it can be changed, but archives created with the previous key will not be recoverable!

To access this domain via a web browser, you can start it using the following command:

```sh
# With this command, Goupile will be accessible via http://localhost:8889/
bin/Paranoid/goupile -C tmp/test_domain/goupile.ini
```

For production deployment, it is recommended to run Goupile behind an HTTPS reverse proxy such as NGINX.

To automate the deployment of a complete production server (with multiple Goupile domains and NGINX configured automatically), we provide ready-to-use Ansible playbooks and roles that you can use as-is or adapt to your needs.
