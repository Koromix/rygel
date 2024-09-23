# Install required dependencies

On Debian and Ubuntu use the following commands:

```sh
sudo apt update
sudo apt install build-essential
```

On RPM-based distributions such as Fedora or Rocky Linux, use:

```sh
sudo dnf groupinstall "Development Tools"
sudo dnf install clang
```

# Run the develop script

Run the development script from the website directory:

```sh
./develop.sh
```

Open the proposed link at `http://localhost:8000/`.

Change page content at will and builds will happen automatically. You just need to refresh the page.
