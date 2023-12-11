#!/bin/bash -e

if [ $# -lt 1 ]; then
    echo 1>&2 "Usage: $0 <GPG user> [directory]"
    exit 1
fi

SCRIPT_DIR="$(realpath $(dirname $0))"
GPG_USER="$1"

if [ $# -gt 1 ]; then
    cd "$2"
fi

rm -rf keys
mkdir -p keys
gpg --local-user "$GPG_USER" --export >"keys/$GPG_USER.gpg"

# Keep old content
mkdir -p pool

rm -rf dists
mkdir -p dists/stable/main/binary-amd64

apt-ftparchive --arch amd64 packages pool >dists/stable/main/binary-amd64/Packages
gzip -f -k -9 dists/stable/main/binary-amd64/Packages

apt-ftparchive -c "$SCRIPT_DIR/release.conf" release dists/stable/ >dists/stable/Release
rm -f dists/stable/Release.gpg dists/stable/InRelease
gpg --local-user "$GPG_USER" -abs -o dists/stable/Release.gpg dists/stable/Release
gpg --local-user "$GPG_USER" --clearsign -o dists/stable/InRelease dists/stable/Release
