PKG_DIR=bin/Packages
DEST_DIR="${PKG_DIR}/${PKG_NAME}/src"

./bootstrap.sh

mkdir -p "${DEST_DIR}"

rm -f "${DEST_DIR}/bin/Log/commands.txt"
./felix -pDebug -O "${DEST_DIR}/bin" felix ${BUILD_TARGETS}

directories=$(grep -E -o '[|"](lib/native|lib/web|src|vendor)/[a-zA-Z0-9_\-]+/' < "${DEST_DIR}/bin/Log/commands.txt" | cut -c 2- | sort | uniq)
sources="bootstrap.sh bootstrap.bat FelixBuild.ini.presets ${directories}"
imports=$(ls "${DEST_DIR}/bin/Log/"*.json | xargs jq -r '.imports | .[]')

VERSION=$(./felix -pDebug -O "${DEST_DIR}/bin" --run "${VERSION_TARGET}" --version | awk -F'[ ]' "/^${VERSION_TARGET}/ {print \$2}")
TMP_DIR="${DEST_DIR}/${PKG_NAME}-${VERSION}"
DEST_FILE="../../${PKG_NAME}-${VERSION}.src.tar.xz"

rm -rf "${TMP_DIR}"
mkdir -p "${TMP_DIR}"
cp -r --parents $sources "${TMP_DIR}/"

# Clean up files that are not in version control
find "${TMP_DIR}/" -type f | sort > "${DEST_DIR}/files"
git ls-tree -r HEAD --name-only | sort | awk "{ print \"${TMP_DIR}/\" \$1 }" > "${DEST_DIR}/keeps"
comm -2 -3 "${DEST_DIR}/files" "${DEST_DIR}/keeps" | tail +2 | xargs rm -f
find "${TMP_DIR}/" -type d -empty -delete

# Rewrite FelixBuild.ini with only relevant targets
tools/package/build/source/rewrite_felix.py FelixBuild.ini -O "${TMP_DIR}/FelixBuild.ini" -t felix $BUILD_TARGETS $imports
for target in $BUILD_TARGETS; do echo "${target} = ${VERSION}" >> "${TMP_DIR}/FelixVersions.ini"; done

# Add build script and preset
echo "./bootstrap.sh" > "${TMP_DIR}/build.sh"
echo "./felix" >> "${TMP_DIR}/build.sh"
echo "Preset = ${FELIX_PRESET}" > "${TMP_DIR}/FelixBuild.ini.user"
chmod +x "${TMP_DIR}/build.sh"

# Run project-specific adjust function
adjust "${TMP_DIR}/"

cd "${DEST_DIR}"

echo "Assembling ${PKG_NAME}-${VERSION}.src.tar.gz..."
tar -c $(basename "${TMP_DIR}") | gzip -9 - > "../../${PKG_NAME}-${VERSION}.src.tar.gz"

echo "Assembling ${PKG_NAME}-${VERSION}.src.tar.xz..."
tar -cJf "../../${PKG_NAME}-${VERSION}.src.tar.xz" $(basename "${TMP_DIR}")
