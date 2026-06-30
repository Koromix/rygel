// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from 'vendor/lit-html/lit-html.bundle.js';
import * as UI from 'lib/web/ui/ui.js';
import * as App from './m_app.js';
import { route } from './m_app.js';
import * as DropMod from './d_drop.js';
import { initRelay } from './d_relay.js';

const MODES = {
    drops: { run: DropMod.runDrops },
    drop: { run: DropMod.runDrop, path: [{ key: 'drop', type: 'string' }] },
    send: { run: DropMod.runSend }
};
const DEFAULT_MODE = 'drops';

async function start() {
    await App.init(MODES, DEFAULT_MODE, renderMenu);
    await initRelay();

    await App.go(window.location.href, false);

    UI.main();
    document.body.classList.remove('loading');
}

function renderMenu() {
    if (!App.isLogged())
        return '';

    let in_drops = ['drops', 'drop', 'send'].includes(route.mode);

    return html`
        <li><a href="/drops" class=${in_drops ? 'active' : ''}>${T.files}</a></li>
    `;
}

export { start }
