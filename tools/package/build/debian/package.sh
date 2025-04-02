PKG_DIR=bin/Packages
DEST_DIR=${PKG_DIR}/${PKG_NAME}/debian
DEBIAN_DIR=${DEST_DIR}/pkg/debian
ROOT_DIR=${DEST_DIR}/pkg/root
CLIENT_DIR=${DEST_DIR}/client

if [ "$1" = "" -o "$1" = "package" ]; then
    ./bootstrap.sh

    VERSION=$(./felix -pDebug --run ${VERSION_TARGET} --version | awk -F'[ _]' "/^${VERSION_TARGET}/ {print \$2}")
    DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)

    sudo rm -rf ${DEST_DIR}
    mkdir -p ${DEBIAN_DIR} ${ROOT_DIR}
    mkdir -p ${CLIENT_DIR}/upper ${CLIENT_DIR}/work

    install -D -m0755 tools/package/build/debian/rules ${DEBIAN_DIR}/rules
    install -D -m0644 tools/package/build/debian/compat ${DEBIAN_DIR}/compat
    install -D -m0644 tools/package/build/debian/format ${DEBIAN_DIR}/source/format

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

    echo "\
License: ${PKG_LICENSE}" > ${DEBIAN_DIR}/copyright

    docker build -t rygel/${DOCKER_IMAGE} tools/docker/${DOCKER_IMAGE}
    if echo "${PKG_ARCHITECTURES}" | grep -q -w amd64; then
        docker run --privileged -t -i --rm -v $(pwd):/io/host -v $(pwd)/${CLIENT_DIR}:/io/client rygel/${DOCKER_IMAGE} /io/host/${SCRIPT_PATH} build x86_64-linux-gnu amd64
    fi
    if echo "${PKG_ARCHITECTURES}" | grep -q -w arm64; then
        docker run --privileged -t -i --rm -v $(pwd):/io/host -v $(pwd)/${CLIENT_DIR}:/io/client rygel/${DOCKER_IMAGE} /io/host/${SCRIPT_PATH} build aarch64-linux-gnu arm64
    fi

    cp ${CLIENT_DIR}/upper/*/${DEST_DIR}/${PKG_NAME}_*.deb ${PKG_DIR}/
elif [ "$1" = "build" ]; then
    # Fix git error about dubious repository ownership
    git config --global safe.directory '*'

    mkdir -p /repo /io/client/upper/$3 /io/client/work/$3
    mount -t overlay overlay -o lowerdir=/io/host,upperdir=/io/client/upper/$3,workdir=/io/client/work/$3 /repo
    rm -f /repo/FelixBuild.ini.user

    cd /repo
    build "$3"

    (cd ${ROOT_DIR} && find \( -type f -o -type l \) -printf "%h/%f %h\n" | awk -F ' ' '{print "root/" substr($1, 3) " " substr($2, 3)}') | sort > ${DEBIAN_DIR}/install
    (cd ${DEBIAN_DIR}/.. && dpkg-buildpackage -uc -us -b -t "$2" -a "$3")

    cd /
    umount /repo
    rm -rf /io/client/work
else
    echo "Unknown command '$1'" >&2
    exit 1
fi
