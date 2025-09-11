#!/bin/sh

set -e

PKG_NAME=heimdall
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Medical timeline visualization (proof-of-concept) "
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later
PKG_ARCHITECTURES="amd64 arm64"

SCRIPT_PATH=src/heimdall/dist/linux/debian.sh
VERSION_TARGET=heimdall
DOCKER_IMAGE=debian11

build() {
    ./bootstrap.sh
    ./felix -pFast --host=$1:clang-18:lld-18 heimdall

    install -D -m0755 bin/Fast/heimdall ${ROOT_DIR}/usr/bin/heimdall
    install -D -m0644 src/heimdall/dist/linux/heimdall.ini ${ROOT_DIR}/etc/heimdall.ini

    echo '/etc/heimdall.ini' > ${DEBIAN_DIR}/conffiles

    install -D -m0644 src/heimdall/dist/linux/heimdall.service ${DEBIAN_DIR}/heimdall.service

echo '\
#!/bin/sh

set -e

if [ "$1" = "configure" ]; then
    if ! getent group heimdall >/dev/null; then
        addgroup --quiet --system heimdall
    fi
    if ! getent passwd heimdall >/dev/null; then
        adduser --system --ingroup heimdall --home /var/lib/heimdall heimdall \
            --gecos "heimdall visualizer daemon"
    fi

    mkdir -p /var/lib/heimdall
    chmod 0775 /var/lib/heimdall
fi

exit 0' > ${DEBIAN_DIR}/postinst
    chmod +x ${DEBIAN_DIR}/postinst

    echo '\
#!/bin/sh

set -e

if [ "$1" = "purge" ]; then
    rm -rf /var/lib/heimdall
fi

exit 0' > ${DEBIAN_DIR}/postrm
    chmod +x ${DEBIAN_DIR}/postrm
}

cd "$(dirname $0)/../../../.."
. tools/package/build/debian/package.sh
