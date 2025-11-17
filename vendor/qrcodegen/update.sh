#!/bin/sh -e

cd "$(dirname $0)"

git clone --depth=1 https://github.com/nayuki/QR-Code-generator.git repo

rm -rf c js
cp -a repo/c/ c/
cp -a repo/typescript-javascript/ js/

git apply ../_patches/qrcodegen_*.patch
npx esbuild --bundle --platform=browser --format=esm js/qrcodegen.ts --outfile=js/qrcodegen.js

rm -rf repo
