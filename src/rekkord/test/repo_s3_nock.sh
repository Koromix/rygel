#!/bin/bash -e

cd "$(dirname $0)"

ROOT=$(realpath "$PWD/../../..")

$ROOT/bootstrap.sh
$ROOT/felix -pFast rekkord

mkdir -p /tmp/rekkord/repo_s3nock
install $ROOT/bin/Fast/rekkord /tmp/rekkord/repo_s3nock/rekkord
cd /tmp/rekkord/repo_s3nock

curl -L versitygw.tgz https://github.com/versity/versitygw/releases/download/v1.8.0/versitygw_v1.8.0_Linux_x86_64.tar.gz | tar xzO versitygw_v1.8.0_Linux_x86_64/versitygw > versitygw
chmod +x versitygw

export LANG=C
export REKKORD_CONFIG_FILE=rekkord.ini
export XDG_CACHE_HOME=$PWD/cache
export GIT_DISCOVERY_ACROSS_FILESYSTEM=1

export ROOT_ACCESS_KEY=root
export ROOT_SECRET_KEY=root

trap 'kill $(jobs -p) 2>/dev/null' EXIT
trap 'kill $(jobs -p) 2>/dev/null' SIGINT

while true; do
    rm -rf cache repo git src dest rekkord.key

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
    echo "ChecksumType = None" >> rekkord.ini

    git clone --bare "$ROOT" git
    git -C git worktree add "$PWD/src"
    commits=$(git -C src log --since=120.days --format=%H | shuf | head -n10)

    ./rekkord init -g

    for commit in $commits; do
        git -C src checkout $commit
        ./rekkord save $commit src
    done

    for commit in $commits; do
        rm -rf dest

        git -C src checkout $commit
        ./rekkord restore $commit:${PWD:1}/src dest

        if test -z "$(rsync -rplinc src/* dest/)"; then
            echo "================== SUCCESS ($commit) =================="
        else
            echo "================== ERROR ($commit) =================="
            exit 1
        fi
    done

    kill $(jobs -p) 2>/dev/null
    sleep 3
done
