'use strict';

const settings = require('./package.json');
const path = require('path');
const { app, BrowserWindow } = require('electron');
const { autoUpdater } = require('electron-updater');

let lock = app.requestSingleInstanceLock();

if (lock) {
    let win = null;

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
        //if (isPackaged())
            //win.setMenu(null);
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
