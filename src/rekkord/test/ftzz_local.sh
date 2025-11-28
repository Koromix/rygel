#!/bin/bash -e

cd "$(dirname $0)"

../../../bootstrap.sh
../../../felix -pFast rekkord

mkdir -p /tmp/rekkord/ftzz_local
install ../../../bin/Fast/rekkord /tmp/rekkord/ftzz_local/rekkord
cd /tmp/rekkord/ftzz_local

rm -f linux.tar.xz
curl -L -o linux.tar.xz https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.17.8.tar.xz

echo "[Repository]
URL = repo
KeyFile = rekkord.key
" > rekkord.ini

export LANG=C
export REKKORD_CONFIG_FILE=rekkord.ini
export XDG_CACHE_HOME=$PWD/cache

while true; do
    rm -rf cache repo src dest rekkord.key

    seed=$RANDOM
    ftzz -n10000 -b1000000000 --seed $seed src/small
    ftzz -n8 -b2000000000 --seed $seed src/big
    mkdir src/linux
    tar -Jxf linux.tar.xz -C src/linux

    ./rekkord init -g
    ./rekkord save src src
    ./rekkord restore src:${PWD:1}/src dest

    if test -z "$(rsync -ainc src/* dest/)"; then
        echo "================== SUCCESS ($seed) =================="
     else
        echo "================== ERROR ($seed) =================="
        exit 1
    fi
done
