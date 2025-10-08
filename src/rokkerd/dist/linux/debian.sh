#!/bin/sh

set -e

PKG_NAME=rokkerd
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Web management app for Rekkord backup tool"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later
PKG_ARCHITECTURES="amd64 arm64"

SCRIPT_PATH=src/rokkerd/dist/linux/debian.sh
VERSION_TARGET=rokkerd
DOCKER_IMAGE=debian11

build() {
    ./bootstrap.sh
    ./felix -pFast --host=$1:clang-18:lld-18 rokkerd

    install -D -m0755 bin/Fast/rokkerd ${ROOT_DIR}/usr/bin/rokkerd
    install -D -m0644 src/rokkerd/dist/linux/rokkerd.ini ${ROOT_DIR}/etc/rokkerd.ini

    echo '/etc/rokkerd.ini' > ${DEBIAN_DIR}/conffiles

    install -D -m0644 src/rokkerd/dist/linux/rokkerd.service ${DEBIAN_DIR}/rokkerd.service

echo '\
#!/bin/sh

set -e

if [ "$1" = "configure" ]; then
    if ! getent group rokkerd >/dev/null; then
        addgroup --quiet --system rokkerd
    fi
    if ! getent passwd rokkerd >/dev/null; then
        adduser --system --ingroup rokkerd --home /var/lib/rokkerd rokkerd \
            --gecos "rokkerd visualizer daemon"
    fi
    mkdir -p /var/lib/rokkerd
fi

exit 0' > ${DEBIAN_DIR}/postinst
    chmod +x ${DEBIAN_DIR}/postinst

    echo '\
#!/bin/sh

set -e

if [ "$1" = "purge" ]; then
    rm -rf /var/lib/rokkerd
fi

exit 0' > ${DEBIAN_DIR}/postrm
    chmod +x ${DEBIAN_DIR}/postrm
}

cd "$(dirname $0)/../../../.."
. tools/package/build/debian/package.sh
