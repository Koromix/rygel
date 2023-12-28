#!/bin/sh -e

PKG_DIR=bin/Packages
DEST_DIR=${PKG_DIR}/${PKG_NAME}/debian
BUILD_DIR=${DEST_DIR}/build
DEBIAN_DIR=${DEST_DIR}/pkg/debian
ROOT_DIR=${DEST_DIR}/pkg/root
CLIENT_DIR=${DEST_DIR}/client

if [ "$1" = "" -o "$1" = "package" ]; then
    ./bootstrap.sh
    ./felix -pDebug ${VERSION_TARGET}

    VERSION=$(bin/Debug/${VERSION_TARGET} --version | awk -F'[ _]' "/^${VERSION_TARGET}/ {print \$2}")
    DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)

    sudo rm -rf ${DEST_DIR}
    mkdir -p ${DEBIAN_DIR} ${ROOT_DIR}
    mkdir -p ${CLIENT_DIR}/upper

    docker build -t rygel/${DOCKER_IMAGE} deploy/docker/${DOCKER_IMAGE}
    docker run --privileged -t -i --rm -v $(pwd):/io/host -v $(pwd)/${CLIENT_DIR}:/io/client rygel/${DOCKER_IMAGE} /io/host/${SCRIPT_PATH} build
    mv ${CLIENT_DIR}/upper ${BUILD_DIR}

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

    (cd ${DEBIAN_DIR}/.. && dpkg-buildpackage -uc -us)
    cp ${DEBIAN_DIR}/../../${PKG_NAME}_*.deb ${PKG_DIR}/
elif [ "$1" = "build" ]; then
    # Fix git error about dubious repository ownership
    git config --global safe.directory '*'

    mkdir -p /repo /io/client/work
    mount -t overlay overlay -o lowerdir=/io/host,upperdir=/io/client/upper,workdir=/io/client/work /repo
    rm -f /repo/FelixBuild.ini.user

    (cd /repo && build)

    umount /repo
    rm -rf /io/client/work
else
    echo "Unknown command '$1'" >&2
    exit 1
fi
