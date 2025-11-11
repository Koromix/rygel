#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast hodler

mkdir -p ../../bin/Web/demheter.fr
../../bin/Fast/hodler . -O ../../bin/Web/demheter.fr --gzip
