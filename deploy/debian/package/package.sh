#!/bin/sh -e

DEST_DIR=bin/Packages/${PKG_NAME}/debian
DEBIAN_DIR=${DEST_DIR}/debian
ROOT_DIR=${DEST_DIR}/root
BUILD_DIR=${DEST_DIR}/build

if [ "$1" = "" -o "$1" = "package" ]; then
    ./bootstrap.sh
    ./felix -pDebug ${VERSION_TARGET}

    VERSION=$(bin/Debug/${VERSION_TARGET} --version | awk -F'[ _]' "/^${VERSION_TARGET}/ {print \$2}")
    DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)

    rm -rf ${DEST_DIR}
    mkdir -p ${DEBIAN_DIR} ${ROOT_DIR} ${BUILD_DIR}

    docker build -t rygel/${DOCKER_IMAGE} deploy/docker/${DOCKER_IMAGE}
    docker run -t -i --rm -v $(pwd):/io rygel/${DOCKER_IMAGE} /io/${SCRIPT_PATH} build

    package

    install -D -m0755 deploy/debian/package/rules ${DEBIAN_DIR}/rules
    install -D -m0644 deploy/debian/package/compat ${DEBIAN_DIR}/compat
    install -D -m0644 deploy/debian/package/format ${DEBIAN_DIR}/source/format
    (cd ${ROOT_DIR} && find -type f -printf "%h/%f %h\n" | awk -F ' ' '{print "root/" substr($1, 3) " " substr($2, 3)}') | sort > ${DEBIAN_DIR}/install

    echo "\
Source: ${PKG_NAME}
Section: utils
Priority: optional
Maintainer: ${PKG_AUTHOR}
Standards-Version: 4.5.1
Rules-Requires-Root: no

Package: ${PKG_NAME}
Architecture: any
Depends: ${PKG_DEPENDENCIES}
Description: ${PKG_DESCRIPTION}
" > ${DEBIAN_DIR}/control

    echo "\
${PKG_NAME} ($VERSION) unstable; urgency=low

  * Current release.

 -- ${PKG_AUTHOR}  $DATE
" > ${DEBIAN_DIR}/changelog

    (cd ${DEST_DIR} && dpkg-buildpackage -uc -us)
    cp ${DEST_DIR}/${PKG_NAME}_*.deb ${DEST_DIR}/../
elif [ "$1" = "build" ]; then
    cd /io

    # Fix git error about dubious repository ownership
    git config --global safe.directory '*'

    build
else
    echo "Unknown command '$1'" >&2
    exit 1
fi
