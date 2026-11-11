#!/bin/sh -e

cd "$(dirname $0)"

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

find .  -depth -not -name 'update.sh' -delete
curl -L https://github.com/aws/aws-lc/archive/refs/tags/v$VERSION.tar.gz | tar xz --strip-components=1 --exclude=".*"

find -size +1M -not -name '*.c' -not -name '*.cpp' -not -name '*.S' -not -name '*.asm' -delete
find -depth -path '*/kat/*.txt' -delete
find -depth -path '*/kat/*.json' -delete
find -depth -path '*/test*/*' -delete
find -depth -path '*/fuzz/*' -delete
find -depth -path '*/vectors/*' -delete
find -name '*.pdf' -delete
find -type d -empty -delete
rm -rf util/fipstools
rm -rf third_party/googletest
rm -rf third_party/googletest

ls ../_patches/aws-lc_*.patch | xargs -n1 git apply
