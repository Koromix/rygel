#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast hodler

mkdir -p ../../bin/Web/rekkord.org
../../bin/Fast/hodler . -O ../../bin/Web/rekkord.org --gzip
