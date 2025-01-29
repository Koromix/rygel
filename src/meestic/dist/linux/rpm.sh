#!/bin/sh

set -e

PKG_NAME=meestic
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="CLI and GUI tools to control the keyboard lighting on MSI Delta 15 laptops"
PKG_DEPENDENCIES="libudev"
PKG_LICENSE=GPL-3.0-or-later

SCRIPT_PATH=src/meestic/dist/linux/rpm.sh
VERSION_TARGET=meestic
DOCKER_IMAGE=rocky9

build() {
    dnf install -y libudev-devel

    ./bootstrap.sh
    ./felix -pFast meestic MeesticTray

    install -D -m0755 bin/Fast/meestic ${ROOT_DIR}/usr/bin/meestic
    install -D -m0755 bin/Fast/MeesticTray ${ROOT_DIR}/usr/bin/MeesticTray

    install -D -m0644 src/meestic/MeesticTray.ini ${ROOT_DIR}/etc/meestic.ini
    install -D -m0644 src/meestic/dist/linux/MeesticTray.desktop ${ROOT_DIR}/usr/share/applications/MeesticTray.desktop
    install -D -m0644 src/meestic/dist/linux/MeesticTray.desktop ${ROOT_DIR}/etc/xdg/autostart/MeesticTray.desktop
    install -D -m0644 src/meestic/images/meestic.png ${ROOT_DIR}/usr/share/icons/hicolor/512x512/apps/MeesticTray.png
    install -D -m0644 src/meestic/dist/linux/meestic.service ${ROOT_DIR}/usr/lib/systemd/system/meestic.service

    echo "
%post
%systemd_post meestic.service

%preun
%systemd_preun meestic.service

%postun
%systemd_postun_with_restart meestic.service" >> ${RPM_DIR}/${PKG_NAME}.spec
}

cd "$(dirname $0)/../../../.."
. tools/package/build/rpm/package.sh
