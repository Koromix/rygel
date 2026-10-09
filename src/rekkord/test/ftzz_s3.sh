#!/bin/bash -e

cd "$(dirname $0)"

ROOT=$(realpath "$PWD/../../..")
PRESET=${PRESET:-UBSan}

$ROOT/bootstrap.sh
$ROOT/felix -p$PRESET rekkord

mkdir -p /tmp/rekkord/ftzz_s3
install $ROOT/bin/$PRESET/rekkord /tmp/rekkord/ftzz_s3/rekkord
cd /tmp/rekkord/ftzz_s3

curl -L -o linux.tar.xz https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.17.8.tar.xz
curl -L versitygw.tgz https://github.com/versity/versitygw/releases/download/v1.8.0/versitygw_v1.8.0_$(uname -sm | sed -e 's/ /_/').tar.gz | tar xzO versitygw_v1.8.0_$(uname -sm | sed -e 's/ /_/')/versitygw > versitygw
case $(uname -sm | sed -e 's/ /_/') in
    Darwin_arm64)
        curl -L -o ftzz https://github.com/SUPERCILEX/ftzz/releases/download/4.0.0/aarch64-apple-darwin-ftzz
        ;;
    Darwin_x86_64)
        curl -L -o ftzz https://github.com/SUPERCILEX/ftzz/releases/download/4.0.0/x86_64-apple-darwin-ftzz
        ;;
    Linux_x86_64)
        curl -L -o ftzz https://github.com/SUPERCILEX/ftzz/releases/download/4.0.0/x86_64-unknown-linux-musl-ftzz
        ;;
    Linux_arm64)
        curl -L -o ftzz https://github.com/SUPERCILEX/ftzz/releases/download/4.0.0/aarch64-unknown-linux-musl-ftzz
        ;;
esac
chmod +x versitygw
chmod +x ftzz

export LANG=C
export REKKORD_CONFIG_FILE=rekkord.ini
export XDG_CACHE_HOME=$PWD/cache

export ROOT_ACCESS_KEY=root
export ROOT_SECRET_KEY=root

trap 'kill $(jobs -p) 2>/dev/null' EXIT
trap 'kill $(jobs -p) 2>/dev/null' SIGINT

while true; do
    rm -rf cache repo src dest rekkord.ini rekkord.key

    mkdir repo repo/iam repo/data repo/version

    ./versitygw --quiet --iam-dir $PWD/repo/iam posix --versioning-dir $PWD/repo/version $PWD/repo/data &
    sleep 2
    ./versitygw admin --endpoint-url http://localhost:7070 create-bucket --owner $ROOT_ACCESS_KEY --bucket ftzz

    echo "[Repository]" > rekkord.ini
    echo "URL = http://localhost:7070/ftzz" >> rekkord.ini
    echo "KeyFile = rekkord.key" >> rekkord.ini
    echo "[S3]" >> rekkord.ini
    echo "KeyID = $ROOT_ACCESS_KEY" >> rekkord.ini
    echo "SecretKey = $ROOT_SECRET_KEY" >> rekkord.ini

    seed=$RANDOM
    ./ftzz -n10000 -b1000000000 --seed $seed src/small
    ./ftzz -n8 -b2000000000 --seed $seed src/big
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

    kill $(jobs -p) 2>/dev/null
    sleep 3
done
