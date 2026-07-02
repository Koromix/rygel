#!/bin/sh

set -e

PKG_NAME=redropp
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Redropp file send service"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later

SCRIPT_PATH=src/redropp/dist/rpm/redropp.sh
VERSION_TARGET=redropp
DOCKER_IMAGE=rocky9

build() {
    ./bootstrap.sh
    ./felix -pFast redropp

    install -D -m0755 bin/Fast/redropp ${ROOT_DIR}/usr/bin/redropp

    install -D -m0644 src/redropp/dist/rpm/redropp.ini ${ROOT_DIR}/etc/redropp/redropp.ini
    install -D -m0644 src/redropp/dist/rpm/redropp.service ${ROOT_DIR}/usr/lib/systemd/system/redropp.service

    echo "
%post
%systemd_post redropp.service

if ! getent group redropp >/dev/null; then
    groupadd --system redropp
fi
if ! getent passwd redropp >/dev/null; then
    adduser --system --gid redropp --home-dir /var/lib/redropp redropp
fi
mkdir -p /var/lib/redropp
chown redropp:redropp /var/lib/redropp

%preun
%systemd_preun redropp.service

%postun
%systemd_postun_with_restart redropp.service" >> ${RPM_DIR}/${PKG_NAME}.spec
}

cd "$(dirname $0)/../../../.."
. tools/package/build/rpm/package.sh
