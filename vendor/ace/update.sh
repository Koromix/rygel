#!/bin/sh -e

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

cd "$(dirname $0)"

dest=$(mktemp -d -p /tmp)

git clone --depth 1 --branch "v$VERSION" https://github.com/ajaxorg/ace-builds.git "$dest"
rsync -rtp --exclude update.sh --exclude '.*' --delete "$dest/" ./

rm -rf "$dest"
