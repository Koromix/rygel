PKG_DIR=bin/Packages
DEST_DIR=${PKG_DIR}/${PKG_NAME}/src

./bootstrap.sh

mkdir -p "${DEST_DIR}"

rm -f "${DEST_DIR}/bin/Log/commands.txt"
./felix -pDebug -O "${DEST_DIR}/bin" felix ${BUILD_TARGETS}

directories=$(grep -E -o '[|"](lib/native|lib/web|src|vendor)/[a-zA-Z0-9_\-]+/' < "${DEST_DIR}/bin/Log/commands.txt" | cut -c 2- | sort | uniq)
sources="bootstrap.sh bootstrap.bat FelixBuild.ini.presets ${directories}"
version=$(./felix -pDebug -O "${DEST_DIR}/bin" --run "${VERSION_TARGET}" --version | awk -F'[ _]' "/^${VERSION_TARGET}/ {print \$2}")
imports=$(ls "${DEST_DIR}/bin/Log/"*.json | xargs jq -r '.imports | .[]')

rm -rf "${DEST_DIR}/tmp"
mkdir -p "${DEST_DIR}/tmp/${PKG_NAME}-${version}/"

cp -r --parents $sources "${DEST_DIR}/tmp/${PKG_NAME}-${version}/"
adjust "${DEST_DIR}/tmp/${PKG_NAME}-${version}/"

tools/package/build/source/rewrite_felix.py FelixBuild.ini -O "${DEST_DIR}/tmp/${PKG_NAME}-${version}/FelixBuild.ini" -t felix $BUILD_TARGETS $imports
for target in $BUILD_TARGETS; do echo "$target = $version" >> "${DEST_DIR}/tmp/${PKG_NAME}-${version}/FelixVersions.ini"; done
