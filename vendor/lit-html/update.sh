#!/bin/sh -e

cd "$(dirname $0)"

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

git clone --branch=lit@${VERSION} --depth=1 https://github.com/lit/lit.git
cd lit

for filename in ../../_patches/lit-html_*.patch; do
    patch -p4 < $filename
done

npm install
npm run build

cd ..
ln -sf lit/node_modules node_modules
npx esbuild --bundle --preserve-symlinks --platform=browser --format=esm lit-html.js --outfile=lit-html.bundle.js

rm -rf lit
rm -f node_modules
