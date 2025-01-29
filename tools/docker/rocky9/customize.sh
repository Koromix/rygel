#!/bin/bash -e

dnf groupinstall -y "Development Tools"
dnf --enablerepo=crb install -y rpmdevtools libstdc++-static clang lld

rpmdev-setuptree
