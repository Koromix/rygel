#!/bin/sh -e

sudo systemctl stop meestic
sudo systemctl disable meestic

sudo rm /usr/local/bin/meestic /usr/local/bin/MeesticGui
sudo rm /etc/systemd/system/meestic.service
# sudo rm /etc/meestic.ini
sudo rm /usr/local/share/icons/hicolor/512x512/apps/MeesticGui.png
sudo rm  /usr/local/share/applications/MeesticGui.desktop

read -r -p "Press enter to continue..." dummy
