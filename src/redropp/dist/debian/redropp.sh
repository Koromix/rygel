#!/bin/sh

set -e

PKG_NAME=redropp
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Redropp file send service"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later
PKG_ARCHITECTURES="amd64 arm64"

SCRIPT_PATH=src/redropp/dist/debian/redropp.sh
VERSION_TARGET=redropp
DOCKER_IMAGE=debian11

build() {
    ./bootstrap.sh
    ./felix -pFast --host=$1:clang-18:lld-18 redropp

    install -D -m0755 bin/Fast/redropp ${ROOT_DIR}/usr/bin/redropp

    install -D -m0644 src/redropp/dist/debian/redropp.ini ${ROOT_DIR}/etc/redropp/redropp.ini
    echo "" >> ${ROOT_DIR}/etc/redropp/redropp.ini
    cat src/redropp/server/config.ini >> ${ROOT_DIR}/etc/redropp/redropp.ini

    echo '/etc/redropp/redropp.ini' > ${DEBIAN_DIR}/conffiles

    install -D -m0644 src/redropp/dist/debian/redropp.service ${DEBIAN_DIR}/redropp.service

echo '\
#!/bin/sh

set -e

if [ "$1" = "configure" ]; then
    if ! getent group redropp >/dev/null; then
        addgroup --quiet --system redropp
    fi
    if ! getent passwd redropp >/dev/null; then
        adduser --system --ingroup redropp --home /var/lib/redropp redropp \
            --gecos "Redropp daemon"
    fi
    mkdir -p /var/lib/redropp
    chown redropp:redropp /var/lib/redropp
fi

exit 0' > ${DEBIAN_DIR}/postinst
    chmod +x ${DEBIAN_DIR}/postinst

    echo '\
#!/bin/sh

set -e

if [ "$1" = "purge" ]; then
    rm -rf /var/lib/redropp
fi

exit 0' > ${DEBIAN_DIR}/postrm
    chmod +x ${DEBIAN_DIR}/postrm
}

cd "$(dirname $0)/../../../.."
. tools/package/build/debian/package.sh
