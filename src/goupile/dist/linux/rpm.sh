#!/bin/sh

set -e

PKG_NAME=goupile
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Programmable electronic data capture application"
PKG_DEPENDENCIES="python3"
PKG_LICENSE=GPL-3.0-or-later

SCRIPT_PATH=src/goupile/dist/linux/rpm.sh
VERSION_TARGET=goupile
DOCKER_IMAGE=rocky9

build() {
    dnf install -y clang lld gcc-toolset-14-libatomic-devel

    ./bootstrap.sh
    ./felix -pParanoid --host=:clang:lld goupile

    install -D -m0755 bin/Paranoid/goupile ${ROOT_DIR}/bin/goupile

    install -D -m0755 src/goupile/dist/linux/manage.py ${ROOT_DIR}/usr/lib/goupile/manage.py
    install -D -m0755 src/goupile/dist/linux/generator.py ${ROOT_DIR}/usr/lib/goupile/generator.py
    mkdir -p ${ROOT_DIR}/lib/systemd/system-generators
    ln -s /usr/lib/goupile/generator.py ${ROOT_DIR}/lib/systemd/system-generators/goupile-systemd-generator
    install -D -m0644 src/goupile/dist/linux/README.md ${ROOT_DIR}/etc/goupile/domains.d/README.md
    install -D -m0644 src/goupile/server/config.ini ${ROOT_DIR}/etc/goupile/template.ini
    install -D -m0644 src/goupile/dist/linux/goupile@.service ${ROOT_DIR}/usr/lib/systemd/system/goupile@.service

echo '
%post
if ! getent group goupile >/dev/null; then
    groupadd --system goupile
fi
if ! getent passwd goupile >/dev/null; then
    adduser --system --gid goupile --home-dir /var/lib/goupile goupile
fi
if [ -d /run/systemd/system ]; then
    /usr/lib/goupile/manage.py sync
fi

%postun
if [ "$1" = "0" ]; then
    if [ -d /run/systemd/system ]; then
        /lib/systemd/system-generators/goupile-systemd-generator /run/systemd/generator
        systemctl --system daemon-reload >/dev/null || true
        systemctl stop goupile@* >/dev/null 2>&1 || true
    fi
fi' >> ${RPM_DIR}/${PKG_NAME}.spec
}

cd "$(dirname $0)/../../../.."
. tools/package/build/rpm/package.sh
