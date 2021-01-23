#!/bin/sh -e

# Dependencies

sudo apt -y install build-essential curl python3 git
sudo useradd -r -d /nonexistent -s /usr/sbin/nologin goupile

# Install manage.py

curl -O https://framagit.org/interhop/goupile/-/raw/master/build/goupile/server/manage.py
curl -O https://framagit.org/interhop/goupile/-/raw/master/build/goupile/server/manage.ini
chmod +x manage.py

# Instructions

echo "------------------------------------------------"
echo ""
echo "You need to adjust section [Users] and the list of domains in manage.ini"
echo "Once this is done, continue with: sudo ./manage.py -bs"
