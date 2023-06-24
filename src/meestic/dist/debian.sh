#!/bin/sh -e

cd $(dirname $0)
./debian/package.sh $*
