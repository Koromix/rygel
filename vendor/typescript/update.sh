#!/bin/sh -e

cd "$(dirname $0)"

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

npm install typescript@$VERSION @types/node
dos2unix node_modules/typescript/*

rm -rf package*.json
rm -rf node_modules/.bin
rm -f node_modules/.*
