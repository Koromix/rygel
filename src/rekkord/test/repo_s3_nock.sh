#!/bin/bash -e

cd "$(dirname $0)"

ROOT=$(realpath "$PWD/../../..")

$ROOT/bootstrap.sh
$ROOT/felix -pFast rekkord

mkdir -p /tmp/rekkord/repo_s3
install $ROOT/bin/Fast/rekkord /tmp/rekkord/repo_s3/rekkord
cd /tmp/rekkord/repo_s3

curl -L -o garage https://garagehq.deuxfleurs.fr/_releases/v2.1.0/x86_64-unknown-linux-musl/garage
chmod +x garage

export LANG=C
export REKKORD_CONFIG_FILE=rekkord.ini
export GIT_DISCOVERY_ACROSS_FILESYSTEM=1

cat > garage.toml <<EOF
metadata_dir = "repo/meta"
data_dir = "repo/data"
db_engine = "sqlite"

replication_factor = 1

rpc_bind_addr = "[::]:3901"
rpc_public_addr = "127.0.0.1:3901"
rpc_secret = "$(openssl rand -hex 32)"

[s3_api]
s3_region = "garage"
api_bind_addr = "[::]:3900"
root_domain = ".s3.garage.localhost"

[s3_web]
bind_addr = "[::]:3902"
root_domain = ".web.garage.localhost"
index = "index.html"

[k2v_api]
api_bind_addr = "[::]:3904"

[admin]
api_bind_addr = "[::]:3903"
admin_token = "$(openssl rand -base64 32)"
metrics_token = "$(openssl rand -base64 32)"
EOF

trap 'kill $(jobs -p) 2>/dev/null' EXIT
trap 'kill $(jobs -p) 2>/dev/null' SIGINT

while true; do
    rm -rf repo git src dest rekkord.key

    mkdir repo

    RUST_LOG=garage=warn ./garage -c garage.toml server &
    sleep 5

    node=$(./garage -c garage.toml status | awk '/NO ROLE/ {print $1}')
    ./garage -c garage.toml layout assign -c 16G -z rygel $node
    ./garage -c garage.toml layout apply --version=1
    ./garage -c garage.toml bucket create rygel
    secret=$(./garage -c garage.toml key create rygel | awk -F ': +' '/Secret key/ {print $2}')
    key=$(./garage -c garage.toml key info rygel | awk -F ': +' '/Key ID/ {print $2}')
    ./garage -c garage.toml bucket allow rygel --key $key --read --write

    echo "[Repository]" > rekkord.ini
    echo "URL = http://localhost:3900/rygel" >> rekkord.ini
    echo "KeyFile = rekkord.key" >> rekkord.ini
    echo "[S3]" >> rekkord.ini
    echo "KeyID = $key" >> rekkord.ini
    echo "SecretKey = $secret" >> rekkord.ini
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
