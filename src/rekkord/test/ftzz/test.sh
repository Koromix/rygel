#!/bin/bash -e

cd "$(dirname $0)"

felix -pFast rekkord

mkdir -p /tmp/rekkord/ftzz
install ../../../../bin/Fast/rekkord /tmp/rekkord/ftzz/rekkord
cp rekkord.ini /tmp/rekkord/ftzz/rekkord.ini
cd /tmp/rekkord/ftzz

export LANG=C
export REKKORD_CONFIG_FILE=rekkord.ini

rm -f linux.tar.xz
curl -L -o linux.tar.xz https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.17.8.tar.xz

while true; do
    rm -rf repo src dest

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
