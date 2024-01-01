#!/bin/bash -e

dnf groupinstall -y "Development Tools"
dnf install -y rpmdevtools libstdc++-static

rpmdev-setuptree
