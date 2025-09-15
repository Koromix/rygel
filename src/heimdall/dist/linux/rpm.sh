#!/bin/sh

set -e

PKG_NAME=heimdall
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Medical timeline visualization (proof-of-concept)"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later

SCRIPT_PATH=src/heimdall/dist/linux/rpm.sh
VERSION_TARGET=heimdall
DOCKER_IMAGE=rocky9

build() {
    ./bootstrap.sh
    ./felix -pFast heimdall

    install -D -m0755 bin/Fast/heimdall ${ROOT_DIR}/usr/bin/heimdall

    install -D -m0644 src/heimdall/dist/linux/heimdall.ini ${ROOT_DIR}/etc/heimdall.ini
    install -D -m0644 src/heimdall/dist/linux/heimdall.service ${ROOT_DIR}/usr/lib/systemd/system/heimdall.service

    echo "
%post
%systemd_post heimdall.service

if ! getent group heimdall >/dev/null; then
    groupadd --system heimdall
fi
if ! getent passwd heimdall >/dev/null; then
    adduser --system --gid heimdall --home-dir /var/lib/heimdall heimdall
fi
mkdir -p /var/lib/heimdall
chmod 0775 /var/lib/heimdall

%preun
%systemd_preun heimdall.service

%postun
%systemd_postun_with_restart heimdall.service" >> ${RPM_DIR}/${PKG_NAME}.spec
}

cd "$(dirname $0)/../../../.."
. tools/package/build/rpm/package.sh
