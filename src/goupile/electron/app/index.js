'use strict';

const settings = require('./package.json');
const path = require('path');
const fs = require('fs');
const { app, BrowserWindow, dialog, ipcMain, Menu, MenuItem } = require('electron');
const { autoUpdater } = require('electron-updater');

if (app.requestSingleInstanceLock()) {
    let win = null;

    ipcMain.on('get_usb_key', (e, id) => {
        let drives = 'DEFGHIJKLMNOPQRSTUVWXYZ';

        for (let drive of drives) {
            let filename = `${drive}:/Goupile/${id}`;

            if (fs.existsSync(filename)) {
                let key = fs.readFileSync(filename, { encoding: 'ascii' });
                key = key.trim();

                e.returnValue = key;
                return;
            }
        }

        e.returnValue = null;
    });

    ipcMain.on('has_usb_key', (e, id) => {
        let drives = 'DEFGHIJKLMNOPQRSTUVWXYZ';

        for (let drive of drives) {
            let filename = `${drive}:/Goupile/${id}`;

            if (fs.existsSync(filename)) {
                e.returnValue = true;
                return;
            }
        }

        e.returnValue = false;
    })

    app.on('ready', async () => {
        if (isPackaged())
            autoUpdater.checkForUpdatesAndNotify();

        win = new BrowserWindow({
            width: 1600,
            height: 900,
            webPreferences: {
                nodeIntegration: true
            }
        });

        let menu = Menu.buildFromTemplate(Menu.getApplicationMenu().items);
        menu.insert(menu.items.length - 1, new MenuItem({
            label: 'Print',
            click: () => win.webContents.print({
                silent: false,
                printBackground: true,
            })
        }));
        win.setMenu(menu);

        win.loadURL(settings.homepage);
    });

    app.on('second-instance', (e, cmdline, cwd) => {
        if (win) {
            if (win.isMinimized())
                win.restore();
            win.focus();
        }
    });
} else {
    app.quit()
}

function isPackaged() {
    let isPackaged = false;

    if (process.mainModule && process.mainModule.filename.indexOf('app.asar') !== -1)
        return true;
    if (process.argv.filter(a => a.indexOf('app.asar') !== -1).length > 0)
        return true;

    return false;
}
