#!/bin/sh

set -e

PKG_NAME=tytools
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="GUI and command-line tools to manage Teensy devices"
PKG_DEPENDENCIES="libqt6core6, libudev1"
PKG_LICENSE=Unlicense
PKG_ARCHITECTURES="amd64 i386 arm64"

SCRIPT_PATH=src/tytools/dist/linux/debian.sh
VERSION_TARGET=tycmd
DOCKER_IMAGE=debian11

build() {
    apt update
    apt install -y qt6-base-dev qt6-base-dev-tools/bullseye-backports libudev-dev \
                   qt6-base-dev:$1/bullseye-backports libudev-dev:$1 \
                   imagemagick

    ./bootstrap.sh
    ./felix -pFast --host=$1 tycmd tycommander tyuploader

    install -D -m0755 bin/Fast/tycmd ${ROOT_DIR}/usr/bin/tycmd
    install -D -m0755 bin/Fast/tycommander ${ROOT_DIR}/usr/bin/tycommander
    install -D -m0755 bin/Fast/tyuploader ${ROOT_DIR}/usr/bin/tyuploader

    install -D -m0644 src/tytools/tycommander/tycommander_linux.desktop ${ROOT_DIR}/usr/share/applications/TyCommander.desktop
    install -D -m0644 src/tytools/tyuploader/tyuploader_linux.desktop ${ROOT_DIR}/usr/share/applications/TyUploader.desktop
    install -D -m0644 src/tytools/assets/images/tycommander.png ${ROOT_DIR}/usr/share/icons/hicolor/512x512/apps/tycommander.png
    install -D -m0644 src/tytools/assets/images/tyuploader.png ${ROOT_DIR}/usr/share/icons/hicolor/512x512/apps/tyuploader.png
    install -D -m0644 src/tytools/dist/linux/teensy.rules ${ROOT_DIR}/usr/lib/udev/rules.d/00-teensy.rules

    for size in 16 32 48 256; do
        mkdir -p -m0755 "${ROOT_DIR}/usr/share/icons/hicolor/${size}x${size}/apps"
        convert -resize "${size}x${size}" src/tytools/assets/images/tycommander.png \
            "${ROOT_DIR}/usr/share/icons/hicolor/${size}x${size}/apps/tycommander.png"
        convert -resize "${size}x${size}" src/tytools/assets/images/tyuploader.png \
            "${ROOT_DIR}/usr/share/icons/hicolor/${size}x${size}/apps/tyuploader.png"
    done
}

cd "$(dirname $0)/../../../.."
. tools/package/build/debian/package.sh
