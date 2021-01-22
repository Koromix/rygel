#!/bin/sh -e

# Dependencies

sudo apt install build-essential curl python3 git
sudo useradd -r -d /nonexistent -s /usr/sbin/nologin goupile

# Install manage.py

mkdir goupile
cd goupile
curl -O https://git.sr.ht/~koromix/rygel/blob/WIP/build/goupile/server/manage.py
curl -O https://git.sr.ht/~koromix/rygel/blob/WIP/build/goupile/server/manage.ini
chmod +x manage.py
