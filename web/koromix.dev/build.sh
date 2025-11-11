#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast hodler

mkdir -p ../../bin/Web/koromix.dev
../../bin/Fast/hodler . -O ../../bin/Web/koromix.dev
