#!/bin/sh

set -e

PKG_NAME=meestic
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="CLI and GUI tools to control the keyboard lighting on MSI Delta 15 laptops"
PKG_DEPENDENCIES="libudev1"
PKG_LICENSE=GPL-3.0-or-later
PKG_ARCHITECTURES="amd64 i386 arm64"

SCRIPT_PATH=src/meestic/dist/linux/debian.sh
VERSION_TARGET=meestic
DOCKER_IMAGE=debian11

build() {
    apt install -y libudev-dev libudev-dev:$1

    ./bootstrap.sh
    ./felix -pFast --host=$1 meestic MeesticTray

    install -D -m0755 bin/Fast/meestic ${ROOT_DIR}/usr/bin/meestic
    install -D -m0755 bin/Fast/MeesticTray ${ROOT_DIR}/usr/bin/MeesticTray

    install -D -m0644 src/meestic/MeesticTray.ini ${ROOT_DIR}/etc/meestic.ini
    install -D -m0644 src/meestic/dist/linux/MeesticTray.desktop ${ROOT_DIR}/usr/share/applications/MeesticTray.desktop
    install -D -m0644 src/meestic/dist/linux/MeesticTray.desktop ${ROOT_DIR}/etc/xdg/autostart/MeesticTray.desktop
    install -D -m0644 src/meestic/images/meestic.png ${ROOT_DIR}/usr/share/icons/hicolor/512x512/apps/MeesticTray.png

    echo '/etc/meestic.ini' > ${DEBIAN_DIR}/conffiles

    install -D -m0644 src/meestic/dist/linux/meestic.service ${DEBIAN_DIR}/meestic.service
}

cd "$(dirname $0)/../../../.."
. tools/package/build/debian/package.sh
