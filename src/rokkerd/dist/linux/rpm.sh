#!/bin/sh

set -e

PKG_NAME=rokkerd
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Web management app for Rekkord backup tool"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later

SCRIPT_PATH=src/rokkerd/dist/linux/rpm.sh
VERSION_TARGET=rokkerd
DOCKER_IMAGE=rocky9

build() {
    ./bootstrap.sh
    ./felix -pFast rokkerd

    install -D -m0755 bin/Fast/rokkerd ${ROOT_DIR}/usr/bin/rokkerd

    install -D -m0644 src/rokkerd/dist/linux/rokkerd.ini ${ROOT_DIR}/etc/rokkerd.ini
    install -D -m0644 src/rokkerd/dist/linux/rokkerd.service ${ROOT_DIR}/usr/lib/systemd/system/rokkerd.service

    echo "
%post
%systemd_post rokkerd.service

if ! getent group rokkerd >/dev/null; then
    groupadd --system rokkerd
fi
if ! getent passwd rokkerd >/dev/null; then
    adduser --system --gid rokkerd --home-dir /var/lib/rokkerd rokkerd
fi
mkdir -p /var/lib/rokkerd

%preun
%systemd_preun rokkerd.service

%postun
%systemd_postun_with_restart rokkerd.service" >> ${RPM_DIR}/${PKG_NAME}.spec
}

cd "$(dirname $0)/../../../.."
. tools/package/build/rpm/package.sh
