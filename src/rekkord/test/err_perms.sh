#!/bin/bash -e

cd "$(dirname $0)"

../../../bootstrap.sh
../../../felix -pFast rekkord

mkdir -p /tmp/rekkord/error_perm
install ../../../bin/Fast/rekkord /tmp/rekkord/error_perm/rekkord
cd /tmp/rekkord/error_perm

echo "[Repository]
URL = repo
KeyFile = rekkord.key
" > rekkord.ini

export LANG=C
export REKKORD_CONFIG_FILE=rekkord.ini

while true; do
    chmod -R 700 src 2>/dev/null || true
    rm -rf repo src dest rekkord.key

    seed=$RANDOM
    ftzz -n10000 -b100000000 --seed $seed src/small
    ftzz -n8 -b200000000 --seed $seed src/big

    files=$(find src/ -mindepth 2 | shuf | head -n20 | sort)
    for file in $files; do
        chmod 0000 "$file" 2>/dev/null || true
    done

    ./rekkord init -g
    ./rekkord save src src
    ./rekkord restore src:${PWD:1}/src dest

    for file in $files; do
        chmod 700 "$file" 2>/dev/null || true
        rm -rf "$file" 2>/dev/null || true
    done

    if test -z "$(rsync -rplinc src/* dest/ 2>/dev/null)"; then
        echo "================== SUCCESS ($seed) =================="
     else
        echo "================== ERROR ($seed) =================="
        exit 1
    fi
done
