PKG_DIR=bin/Packages
DEST_DIR=${PKG_DIR}/${TARGET}/docker
DOCKER_IMAGE=debian12
IMAGE_PREFIX=koromix/

podman build -t rygel/${DOCKER_IMAGE} tools/docker/${DOCKER_IMAGE}

./bootstrap.sh

VERSION=$(./felix -pDebug --run ${TARGET} --version | awk -F' ' "/^${TARGET}/ {print \$2}")
PLATFORMS=$(printf "linux/%s," $IMAGE_ARCHITECTURES | head -c -1)

rm -rf $DEST_DIR
mkdir -p $DEST_DIR

git ls-files -co --exclude-standard | grep -E -v '^tools/' > $DEST_DIR/files.txt
tar -cf $DEST_DIR/repo.tar -T $DEST_DIR/files.txt
cp $DOCKER_PATH $DEST_DIR/Dockerfile
cp tools/package/build/docker/isolate.py $DEST_DIR/isolate.py
echo "$TARGET = $VERSION" > $DEST_DIR/versions.txt

cd $DEST_DIR

podman manifest create $IMAGE_PREFIX$TARGET:$VERSION
podman build --platform "$PLATFORMS" --manifest $IMAGE_PREFIX$TARGET:$VERSION .

echo "podman manifest push --all $IMAGE_PREFIX$TARGET:$VERSION docker.io/$IMAGE_PREFIX$TARGET:$VERSION"

podman image tag $IMAGE_PREFIX$TARGET:$VERSION $IMAGE_PREFIX$TARGET:dev
echo "podman manifest push --all $IMAGE_PREFIX$TARGET:dev docker.io/$IMAGE_PREFIX$TARGET:dev"

if echo "$VERSION" | grep -qv '-'; then
    podman image tag $IMAGE_PREFIX$TARGET:$VERSION $IMAGE_PREFIX$TARGET:latest
    echo "podman manifest push --all $IMAGE_PREFIX$TARGET:latest docker.io/$IMAGE_PREFIX$TARGET:latest"
fi
