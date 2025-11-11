#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast hodler

mkdir -p ../../bin/Web/goupile.org/en
../../bin/Fast/hodler fr -O ../../bin/Web/goupile.org --gzip
../../bin/Fast/hodler en -b /en/ -O ../../bin/Web/goupile.org/en --gzip

ln -s en/index.html ../../bin/Web/goupile.org/en.html
