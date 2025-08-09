#!/bin/sh -e

cd "$(dirname $0)"

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

npm install typescript@$VERSION
rsync -rt --delete --exclude update.sh --exclude node_modules node_modules/typescript/ ./

rm -rf package-lock.json
rm -rf node_modules

dos2unix *
