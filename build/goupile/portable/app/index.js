'use strict';

const settings = require('./package.json');
const path = require('path');
const { app, BrowserWindow } = require('electron');

app.on('ready', async () => {
    let win = new BrowserWindow({
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

function isPackaged() {
    let isPackaged = false;

    if (process.mainModule && process.mainModule.filename.indexOf('app.asar') !== -1)
        return true;
    if (process.argv.filter(a => a.indexOf('app.asar') !== -1).length > 0)
        return true;

    return false;
}
