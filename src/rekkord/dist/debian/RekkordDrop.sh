#!/bin/sh

set -e

PKG_NAME=rekkorddrop
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Rekkord Drop file send service"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later
PKG_ARCHITECTURES="amd64 arm64"

SCRIPT_PATH=src/rekkord/dist/debian/RekkordDrop.sh
VERSION_TARGET=RekkordDrop
DOCKER_IMAGE=debian11

build() {
    ./bootstrap.sh
    ./felix -pFast --host=$1:clang-18:lld-18 RekkordDrop

    install -D -m0755 bin/Fast/RekkordDrop ${ROOT_DIR}/usr/bin/RekkordDrop
    install -D -m0644 src/rekkord/dist/debian/RekkordDrop.ini ${ROOT_DIR}/etc/rekkord/RekkordDrop.ini

    echo '/etc/rekkord/RekkordDrop.ini' > ${DEBIAN_DIR}/conffiles

    install -D -m0644 src/rekkord/dist/debian/RekkordDrop.service ${DEBIAN_DIR}/rekkorddrop.service

echo '\
#!/bin/sh

set -e

if [ "$1" = "configure" ]; then
    if ! getent group rekkorddrop >/dev/null; then
        addgroup --quiet --system rekkorddrop
    fi
    if ! getent passwd rekkorddrop >/dev/null; then
        adduser --system --ingroup rekkorddrop --home /var/lib/rekkorddrop rekkorddrop \
            --gecos "Rekkord Drop daemon"
    fi
    mkdir -p /var/lib/rekkorddrop
    chown rekkorddrop:rekkorddrop /var/lib/rekkorddrop
fi

exit 0' > ${DEBIAN_DIR}/postinst
    chmod +x ${DEBIAN_DIR}/postinst

    echo '\
#!/bin/sh

set -e

if [ "$1" = "purge" ]; then
    rm -rf /var/lib/rekkorddrop
fi

exit 0' > ${DEBIAN_DIR}/postrm
    chmod +x ${DEBIAN_DIR}/postrm
}

cd "$(dirname $0)/../../../.."
. tools/package/build/debian/package.sh
