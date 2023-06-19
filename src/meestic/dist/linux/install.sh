#!/bin/sh -e

sudo install -m0755 meestic MeesticGui /usr/local/bin/
sudo install -m0644 meestic.service /etc/systemd/system/
sudo install -m0644 meestic.ini /etc/
sudo install -D -m0755 MeesticGui.png /usr/local/share/icons/hicolor/512x512/apps/MeesticGui.png
sudo install -m0755 MeesticGui.desktop /usr/local/share/applications

sudo systemctl enable meestic
sudo systemctl start meestic

echo ""
echo "The system daemon is running so the keyboard shortcut should work."
echo "To change profiles, edit /etc/meestic.ini and restart with:"
echo "    sudo systemctl restart meestic"
echo "For the tray icon, start MeesticGui client in your user session."
echo ""
