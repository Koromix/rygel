#!/bin/bash -e

cd "$(dirname $0)"

../../../bootstrap.sh
../../../felix -pFast rekkord

mkdir -p /tmp/rekkord/ftzz_s3
install ../../../bin/Fast/rekkord /tmp/rekkord/ftzz_s3/rekkord
cd /tmp/rekkord/ftzz_s3

rm -f linux.tar.xz garage
curl -L -o linux.tar.xz https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.17.8.tar.xz
curl -L -o garage https://garagehq.deuxfleurs.fr/_releases/v2.1.0/x86_64-unknown-linux-musl/garage
chmod +x garage

export LANG=C
export REKKORD_CONFIG_FILE=rekkord.ini

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
    rm -rf repo src dest rekkord.ini rekkord.key

    mkdir repo

    RUST_LOG=garage=warn ./garage -c garage.toml server &
    sleep 5

    node=$(./garage -c garage.toml status | awk '/NO ROLE/ {print $1}')
    ./garage -c garage.toml layout assign -c 16G -z ftzz $node
    ./garage -c garage.toml layout apply --version=1
    ./garage -c garage.toml bucket create ftzz
    secret=$(./garage -c garage.toml key create ftzz | awk -F ': +' '/Secret key/ {print $2}')
    key=$(./garage -c garage.toml key info ftzz | awk -F ': +' '/Key ID/ {print $2}')
    ./garage -c garage.toml bucket allow ftzz --key $key --read --write

    echo "[Repository]" > rekkord.ini
    echo "URL = http://localhost:3900/ftzz" >> rekkord.ini
    echo "KeyFile = rekkord.key" >> rekkord.ini
    echo "[S3]" >> rekkord.ini
    echo "KeyID = $key" >> rekkord.ini
    echo "SecretKey = $secret" >> rekkord.ini
    echo "ChecksumType = None" >> rekkord.ini

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

    kill $(jobs -p) 2>/dev/null
    sleep 3
done
