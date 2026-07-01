PKG_DIR=bin/Packages
DEST_DIR=${PKG_DIR}/${TARGET}/docker
DOCKER_IMAGE=debian12
IMAGE_NAME=${IMAGE_NAME-koromix/$TARGET}

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

podman manifest rm $IMAGE_NAME:$VERSION  2>/dev/null || true
podman manifest create $IMAGE_NAME:$VERSION
podman build --platform "$PLATFORMS" --manifest $IMAGE_NAME:$VERSION .

echo "podman manifest push --all $IMAGE_NAME:$VERSION docker.io/$IMAGE_NAME:$VERSION"

podman image tag $IMAGE_NAME:$VERSION $IMAGE_NAME:dev
echo "podman manifest push --all $IMAGE_NAME:dev docker.io/$IMAGE_NAME:dev"

if echo "$VERSION" | grep -qv '-'; then
    podman image tag $IMAGE_NAME:$VERSION $IMAGE_NAME:latest
    echo "podman manifest push --all $IMAGE_NAME:latest docker.io/$IMAGE_NAME:latest"
fi
