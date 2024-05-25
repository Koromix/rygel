#!/bin/sh

set -e

cd "$(dirname $0)"

if [ ! -d spidermonkey ]; then
    git clone https://github.com/bytecodealliance/spidermonkey-wasi-embedding.git spidermonkey
fi
cd spidermonkey

git reset --hard
git pull
ls -1 ../../_patches/spidermonkey_*.patch | xargs -n1 git apply -p4
./build-engine.sh
ar x release/lib/libjsrust.a --output release/lib
ar x release/lib/libjs_static.a --output release/lib
rm release/lib/*.a

rsync -rtv --delete release/include/ ../include/
rsync -rtv --delete release/lib/ ../lib/
