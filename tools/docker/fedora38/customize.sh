#!/bin/bash -e

dnf groupinstall -y "Development Tools"
dnf install -y rpmdevtools libstdc++-static clang lld cmake ninja-build perl ruby nodejs

rpmdev-setuptree
