#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast hodler

mkdir -p ../../bin/Web/koffi.dev
../../bin/Fast/hodler . -O ../../bin/Web/koffi.dev --gzip
