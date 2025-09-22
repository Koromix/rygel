PKG_DIR=bin/Packages
DEST_DIR=${PKG_DIR}/${TARGET}/docker
DOCKER_IMAGE=debian12

docker build -t rygel/${DOCKER_IMAGE} tools/docker/${DOCKER_IMAGE}

./bootstrap.sh

VERSION=$(./felix -pDebug --run ${TARGET} --version | awk -F' ' "/^${TARGET}/ {print \$2}")

rm -rf $DEST_DIR
mkdir -p $DEST_DIR

git ls-files -co --exclude-standard | grep -E -v '^tools/' > $DEST_DIR/files.txt
tar -cf $DEST_DIR/repo.tar -T $DEST_DIR/files.txt
cp $DOCKER_PATH $DEST_DIR/Dockerfile
cp tools/package/build/docker/isolate.sh $DEST_DIR/isolate.sh
echo "$TARGET = $VERSION" > $DEST_DIR/versions.txt

cd $DEST_DIR
docker build -t $TARGET:$VERSION .
docker image tag $TARGET:$VERSION $TARGET:dev

