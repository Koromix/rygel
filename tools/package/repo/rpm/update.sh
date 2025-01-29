#!/bin/bash -e

# Dependencies: rpm createrepo-c

if [ $# -lt 2 ]; then
    echo 1>&2 "Usage: $0 <name> <GPG user> [directory]"
    exit 1
fi

SCRIPT_DIR="$(realpath $(dirname $0))"

REPO_NAME="$1"
GPG_USER="$2"

if [ $# -gt 2 ]; then
    cd "$3"
fi

gpg --local-user "$GPG_USER" --export --armour >"$REPO_NAME-repo.asc"
find *.rpm -exec rpmsign --addsign {} --define='%_gpgbin /usr/bin/gpg' --define='%__gpg /usr/bin/gpg' --key-id "$GPG_USER" \;

createrepo_c .
gpg --local-user "$GPG_USER" --detach-sign --armor --yes repodata/repomd.xml
