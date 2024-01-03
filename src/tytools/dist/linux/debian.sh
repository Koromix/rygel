#!/bin/sh -e

PKG_NAME=tytools
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="GUI and command-line tools to manage Teensy devices"
PKG_DEPENDENCIES="libqt6core6, libudev1"
PKG_LICENSE=Unlicense

SCRIPT_PATH=src/tytools/dist/linux/debian.sh
VERSION_TARGET=tycmd
DOCKER_IMAGE=debian12

build() {
    apt update
    apt install -y qt6-base-dev qt6-base-dev-tools libudev-dev

    ./bootstrap.sh
    ./felix -pFast tycmd tycommander tyuploader

    install -D -m0755 bin/Fast/tycmd ${ROOT_DIR}/usr/bin/tycmd
    install -D -m0755 bin/Fast/tycommander ${ROOT_DIR}/usr/bin/tycommander
    install -D -m0755 bin/Fast/tyuploader ${ROOT_DIR}/usr/bin/tyuploader

    install -D -m0644 src/tytools/tycommander/tycommander_linux.desktop ${ROOT_DIR}/usr/share/applications/TyCommander.desktop
    install -D -m0644 src/tytools/tyuploader/tyuploader_linux.desktop ${ROOT_DIR}/usr/share/applications/TyUploader.desktop
    install -D -m0644 src/tytools/assets/images/tycommander.png ${ROOT_DIR}/usr/share/icons/hicolor/512x512/apps/tycommander.png
    install -D -m0644 src/tytools/assets/images/tyuploader.png ${ROOT_DIR}/usr/share/icons/hicolor/512x512/apps/tyuploader.png
    install -D -m0644 src/tytools/dist/linux/teensy.rules ${ROOT_DIR}/usr/lib/udev/rules.d/00-teensy.rules
}

cd "$(dirname $0)/../../../.."
. deploy/debian/package/package.sh
