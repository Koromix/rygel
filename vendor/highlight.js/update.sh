#!/bin/sh -e

cd "$(dirname $0)"

VERSION1=$1
VERSION2=$2

if [ -z "$VERSION1" -o -z "$VERSION2" ]; then
    echo "Missing version arguments" >&2
    exit 1
fi

git clone https://github.com/highlightjs/highlight.js repo
npm install "highlightjs-copy@$VERSION2"

cd repo
git checkout "$VERSION1"
npm install
npm run build-browser :common

cd ..
npx esbuild --bundle --platform=browser --format=esm ./highlight.js --outfile=highlight.bundle.js
rsync -rtvp repo/build/demo/styles/ styles/ --delete

rm -rf copy
mkdir copy
cp node_modules/highlightjs-copy/index.js copy/highlightjs-copy.js
cp node_modules/highlightjs-copy/styles/highlightjs-copy.css copy/highlightjs-copy.css

rm -rf repo
rm -rf package.json package-lock.json node_modules
