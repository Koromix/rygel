#!/bin/sh -e

cd "$(dirname $0)"

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

git clone https://github.com/highlightjs/highlight.js repo
cd repo
git checkout "$VERSION"

npm install
npm run build-browser :common

cp build/highlight.js ../highlight.js
rsync -rtvp build/demo/styles/ ../styles/ --delete

cd ..
rm -rf repo
