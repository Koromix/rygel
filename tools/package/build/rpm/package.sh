PKG_DIR=bin/Packages
DEST_DIR=${PKG_DIR}/${PKG_NAME}/rpm
RPM_DIR=${DEST_DIR}/pkg
ROOT_DIR=${DEST_DIR}/pkg/root
CLIENT_DIR=${DEST_DIR}/client

if [ "$1" = "" -o "$1" = "package" ]; then
    ./bootstrap.sh

    VERSION=$(./felix -pDebug --run ${VERSION_TARGET} --version | awk -F'[ _]' "/^${VERSION_TARGET}/ {print \$2}")
    DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)

    RELEASE=$(echo $VERSION | sed 's/^.*-//')
    VERSION=$(echo $VERSION | sed 's/-.*$//')
    [ "$RELEASE" = "$VERSION" ] && RELEASE=1

    sudo rm -rf ${DEST_DIR}
    mkdir -p ${RPM_DIR} ${ROOT_DIR}
    mkdir -p ${CLIENT_DIR}/upper

    echo "\
Name: ${PKG_NAME}
Version: ${VERSION}
Release: ${RELEASE}
Summary: ${PKG_DESCRIPTION}
License: ${PKG_LICENSE}

%description
${PKG_DESCRIPTION}

%prep
# Not needed

%build
# Not needed" > ${RPM_DIR}/${PKG_NAME}.spec

    podman build -t rygel/${DOCKER_IMAGE} tools/docker/${DOCKER_IMAGE}
    podman run --privileged -t -i --rm -v $(pwd):/io/host -v $(pwd)/${CLIENT_DIR}:/io/client rygel/${DOCKER_IMAGE} /io/host/${SCRIPT_PATH} build

    cp ${CLIENT_DIR}/upper/${DEST_DIR}/${PKG_NAME}-*.rpm ${PKG_DIR}/
elif [ "$1" = "build" ]; then
    # Fix git error about dubious repository ownership
    git config --global safe.directory '*'

    mkdir -p /repo /io/client/work
    mount -t overlay overlay -o lowerdir=/io/host,upperdir=/io/client/upper,workdir=/io/client/work /repo
    rm -f /repo/FelixBuild.ini.user

    cd /repo
    build

echo "
%install" >> ${RPM_DIR}/${PKG_NAME}.spec
    (cd ${ROOT_DIR} && find -mindepth 1 -type d | awk -F ' ' "{print \"mkdir %{buildroot}/\" substr(\$1, 3)}") | sort >> ${RPM_DIR}/${PKG_NAME}.spec
    (cd ${ROOT_DIR} && find -type f -printf "%#m %p\n" | awk -F ' ' "{print \"install -m\" \$1 \" /repo/${ROOT_DIR}/\" substr(\$2, 3) \" %{buildroot}/\" substr(\$2, 3)}") | sort >> ${RPM_DIR}/${PKG_NAME}.spec
    (cd ${ROOT_DIR} && find -type l -printf "%l %p\n" | awk -F ' ' "{print \"ln -s \" \$1 \" %{buildroot}/\" substr(\$2, 3)}") | sort >> ${RPM_DIR}/${PKG_NAME}.spec

echo "
%files" >> ${RPM_DIR}/${PKG_NAME}.spec
    (cd ${ROOT_DIR} && find \( -type f -o -type l \) | cut -c '2-') | sort >> ${RPM_DIR}/${PKG_NAME}.spec

echo "
%changelog
# Skip for now" >> ${RPM_DIR}/${PKG_NAME}.spec

    (cd ${RPM_DIR} && rpmbuild -ba ${PKG_NAME}.spec)
    (cd ${DEST_DIR} && find $HOME/rpmbuild/RPMS/ -name "${PKG_NAME}-*.rpm" -exec cp {} ./ \;)

    cd /
    umount /repo
    rm -rf /io/client/work
else
    echo "Unknown command '$1'" >&2
    exit 1
fi
