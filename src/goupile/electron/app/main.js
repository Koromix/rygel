'use strict';

const settings = require('./package.json');
const path = require('path');
const { app, BrowserWindow, dialog, ipcMain, Menu, MenuItem } = require('electron');
const { autoUpdater } = require('electron-updater');

if (app.requestSingleInstanceLock()) {
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

        customizeMenu(win);

        // For some reason, with some Electron versions/in some cases, loadURL() fails to load a cached
        // offline page. But it works if we use loadFile() and then use JS to redirect to the correct page.
        win.loadFile('index.html', { query: { "url": settings.homepage } });
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

function customizeMenu(win) {
    let menu = Menu.buildFromTemplate(Menu.getApplicationMenu().items);

    menu.insert(menu.items.length - 1, new MenuItem({
        label: 'Print',
        click: () => win.webContents.print({
            silent: false,
            printBackground: true,
        })
    }));
    win.setMenu(menu);

    win.webContents.on('did-create-window', (win, details) => {
        console.log('z');
        customizeMenu(win);
    });
}

function isPackaged() {
    let isPackaged = false;

    if (process.mainModule && process.mainModule.filename.indexOf('app.asar') !== -1)
        return true;
    if (process.argv.filter(a => a.indexOf('app.asar') !== -1).length > 0)
        return true;

    return false;
}
