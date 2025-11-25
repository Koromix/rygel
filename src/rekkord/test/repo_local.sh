#!/bin/bash -e

cd "$(dirname $0)"

ROOT=$(realpath "$PWD/../../..")

$ROOT/bootstrap.sh
$ROOT/felix -pFast rekkord

mkdir -p /tmp/rekkord/repo_local
install $ROOT/bin/Fast/rekkord /tmp/rekkord/repo_local/rekkord
cd /tmp/rekkord/repo_local

echo "[Repository]
URL = repo
KeyFile = rekkord.key
" > rekkord.ini

export LANG=C
export REKKORD_CONFIG_FILE=rekkord.ini
export GIT_DISCOVERY_ACROSS_FILESYSTEM=1

while true; do
    rm -rf repo git src dest rekkord.key

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
done
