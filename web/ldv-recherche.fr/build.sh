#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast hodler

mkdir -p ../../bin/Web/ldv-recherche.fr
../../bin/Fast/hodler . -O ../../bin/Web/ldv-recherche.fr --gzip
