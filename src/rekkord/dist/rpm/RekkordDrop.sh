#!/bin/sh

set -e

PKG_NAME=rekkorddrop
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Rekkord Drop file send service"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later

SCRIPT_PATH=src/rekkord/dist/rpm/RekkordDrop.sh
VERSION_TARGET=RekkordDrop
DOCKER_IMAGE=rocky9

build() {
    ./bootstrap.sh
    ./felix -pFast RekkordDrop

    install -D -m0755 bin/Fast/RekkordDrop ${ROOT_DIR}/usr/bin/RekkordDrop

    install -D -m0644 src/rekkord/dist/rpm/RekkordDrop.ini ${ROOT_DIR}/etc/rekkord/RekkordDrop.ini
    install -D -m0644 src/rekkord/dist/rpm/RekkordDrop.service ${ROOT_DIR}/usr/lib/systemd/system/rekkorddrop.service

    echo "
%post
%systemd_post rekkorddrop.service

if ! getent group rekkorddrop >/dev/null; then
    groupadd --system rekkorddrop
fi
if ! getent passwd rekkorddrop >/dev/null; then
    adduser --system --gid rekkorddrop --home-dir /var/lib/rekkorddrop rekkorddrop
fi
mkdir -p /var/lib/rekkorddrop
chown rekkorddrop:rekkorddrop /var/lib/rekkorddrop

%preun
%systemd_preun rekkorddrop.service

%postun
%systemd_postun_with_restart rekkorddrop.service" >> ${RPM_DIR}/${PKG_NAME}.spec
}

cd "$(dirname $0)/../../../.."
. tools/package/build/rpm/package.sh
