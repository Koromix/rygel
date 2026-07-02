#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast hodler

mkdir -p ../../bin/Web/redropp.org
../../bin/Fast/hodler . -O ../../bin/Web/redropp.org --gzip
