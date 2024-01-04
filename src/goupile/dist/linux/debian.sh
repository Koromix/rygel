#!/bin/sh -e

PKG_NAME=goupile
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Programmable electronic data capture application"
PKG_DEPENDENCIES="python3"
PKG_LICENSE=AGPL-3.0-or-later

SCRIPT_PATH=src/goupile/dist/linux/debian.sh
VERSION_TARGET=goupile
DOCKER_IMAGE=debian11

build() {
    ./bootstrap.sh
    ./felix -pParanoid --host=,clang-15,lld-15 goupile

    install -D -m0755 bin/Paranoid/goupile ${ROOT_DIR}/bin/goupile

    install -D -m0755 src/goupile/dist/linux/generator.py ${ROOT_DIR}/lib/systemd/system-generators/goupile-systemd-generator
    install -D -m0755 src/goupile/dist/linux/create_domain.py ${ROOT_DIR}/usr/lib/goupile/create_domain
    install -D -m0644 src/goupile/dist/linux/README.md ${ROOT_DIR}/etc/goupile/domains.d/README.md
    install -D -m0644 src/goupile/dist/linux/template.ini ${ROOT_DIR}/etc/goupile/template.ini
    install -D -m0644 src/goupile/dist/linux/goupile@.service ${ROOT_DIR}/lib/systemd/system/goupile@.service

echo '\
#!/bin/sh

set -e

if [ "$1" = "configure" ]; then
    if ! getent group goupile >/dev/null; then
        addgroup --quiet --system goupile
    fi
    if ! getent passwd goupile >/dev/null; then
        adduser --system --ingroup goupile --home /var/lib/goupile goupile \
            --gecos "goupile eCRF daemon"
    fi
    if [ "$2" = "3.0" ]; then
        public_key=$(awk -F " = " "/PublicKey/ { print \$2 }" /var/lib/goupile/default/goupile.ini | xargs)
        echo > /etc/goupile/domains.d/default.ini
        echo "[Domain]" >> /etc/goupile/domains.d/default.ini
        echo "ArchiveKey = ${public_key}" >> /etc/goupile/domains.d/default.ini
        echo "Port = 8888" >> /etc/goupile/domains.d/default.ini
    fi
    if [ -d /run/systemd/system ]; then
        /lib/systemd/system-generators/goupile-systemd-generator /run/systemd/generator
        systemctl --system daemon-reload >/dev/null || true
    fi
fi

exit 0' > ${DEBIAN_DIR}/postinst
    chmod +x ${DEBIAN_DIR}/postinst

    echo '\
#!/bin/sh

set -e

if [ "$1" = "purge" ]; then
    rm -rf /etc/goupile
fi
if [ -d /run/systemd/system ]; then
    rm -f /run/systemd/generator/multi-user.target.wants/goupile@*.service || true
    systemctl --system daemon-reload >/dev/null || true
fi

exit 0' > ${DEBIAN_DIR}/postrm
    chmod +x ${DEBIAN_DIR}/postrm
}

cd "$(dirname $0)/../../../.."
. deploy/debian/package/package.sh
