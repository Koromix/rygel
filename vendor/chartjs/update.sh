#!/bin/sh -e

VERSION1=$1
VERSION2=$2

if [ -z "$VERSION1" -o -z "$VERSION2" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

cd "$(dirname $0)"

npm install "chart.js@$VERSION1"
npm install "chartjs-plugin-datalabels@$VERSION2"

npx esbuild --bundle --platform=browser --format=esm chart.js --outfile=chart.bundle.js

rm -rf package*.json
rm -rf node_modules
