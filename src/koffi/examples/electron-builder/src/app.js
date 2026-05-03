import { app, BrowserWindow, ipcMain } from 'electron';
import koffi from 'koffi';

app.whenReady().then(() => {
    ipcMain.handle('koffi:config', (e, ...args) => koffi.config(...args));

    createWindow();
});

function createWindow() {
    const win = new BrowserWindow({
        width: 800,
        height: 600,
        webPreferences: {
            preload: __dirname + '/preload.js'
        }
    });

    win.loadFile(__dirname + '/index.html');
};
