#!/bin/sh -e

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

cd "$(dirname $0)"

rm -rf asciinema-player
git clone --branch=v$VERSION --depth=1 https://github.com/asciinema/asciinema-player

cd asciinema-player
npm install
npm run build

cd ..
npx esbuild --bundle --platform=browser --format=esm asciinema-player/dist/index.js --outfile=asciinema-player.js
cp asciinema-player/dist/bundle/asciinema-player.css asciinema-player.css

rm -rf asciinema-player
