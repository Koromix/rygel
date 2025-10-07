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

    if [ -d ${DEST_DIR} ]; then
        find ${DEST_DIR} -type d -exec chmod u+rwx {} \;
        rm -rf ${DEST_DIR}
    fi
    mkdir -p ${RPM_DIR} ${ROOT_DIR}

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

    mkdir -p ${CLIENT_DIR}/upper ${CLIENT_DIR}/work
    podman run -t -i --rm -v $(pwd):/repo:O,upperdir=$(pwd)/${CLIENT_DIR}/upper,workdir=$(pwd)/${CLIENT_DIR}/work rygel/${DOCKER_IMAGE} /repo/${SCRIPT_PATH} build

    cp ${CLIENT_DIR}/upper/${DEST_DIR}/${PKG_NAME}-*.rpm ${PKG_DIR}/
elif [ "$1" = "build" ]; then
    # Fix git error about dubious repository ownership
    git config --global safe.directory '*'

    cd /repo
    rm -f FelixBuild.ini.user

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
else
    echo "Unknown command '$1'" >&2
    exit 1
fi
