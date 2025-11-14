// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import dayjs from 'vendor/dayjs/dayjs.bundle.js';
import { Util, Log, Net } from 'lib/web/base/base.js';
import * as UI from 'lib/web/base/ui.js';
import * as app from './main.js';
import { route, cache } from './main.js';
import { ASSETS } from '../assets/assets.js';

async function runRepositories() {
    let repositories = UI.tableValues('repositories', cache.repositories, 'name');

    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.repositories}</a>
            ${cache.repository != null ? html`<a href=${app.makeURL({ mode: 'repository' })}>${cache.repository.name}</a>` : ''}
        </div>

        <div class="tab">
            <div class="header">${T.repositories}</div>
            <table style="table-layout: fixed; width: 100%;">
                <colgroup>
                    <col style="width: 30%;"></col>
                    <col></col>
                    <col style="width: 140px;"></col>
                </colgroup>
                <thead>
                    <tr>
                        ${UI.tableHeader('repositories', 'name', 'Name')}
                        ${UI.tableHeader('repositories', 'url', 'URL')}
                        ${UI.tableHeader('repositories', repo => {
                            if (repo.errors) {
                                return 1;
                            } else {
                                return repo.checked ?? 0;
                            }
                        }, 'Status')}
                    </tr>
                </thead>
                <tbody>
                    ${repositories.map(repo => {
                        let url = app.makeURL({ mode: 'repository', repository: repo.id });

                        return html`
                            <tr style="cursor: pointer;" @click=${UI.wrap(e => app.go(url))}>
                                <td><a href=${url}>${repo.name}</a></td>
                                <td>${repo.url}</td>
                                <td style="text-align: right;">
                                    ${!repo.checked ? T.valid : ''}
                                    ${repo.checked && !repo.errors ? T.success : ''}
                                    ${repo.errors ? html`<span style="color: red;">${repo.failed || T.unknown_error}</span>` : ''}
                                    <br><span class="sub">${repo.checked ? dayjs(repo.checked).format('lll') : T.check_pending}</span>
                                </td>
                            </tr>
                        `;
                    })}
                    ${!repositories.length ? html`<tr><td colspan="3" style="text-align: center;">${T.no_repository}</td></tr>` : ''}
                </tbody>
            </table>
            <div class="actions">
                <button type="button" @click=${UI.wrap(e => configureRepository(null))}>${T.add_repository}</button>
            </div>
        </div>
    `);
}

async function runRepository() {
    if (route.repository != null) {
        try {
            let url = Util.pasteURL('/api/repository/get', { id: route.repository });
            cache.repository = await Net.cache('repository', url);
        } catch (err) {
            if (err.status != 404 && err.status != 422)
                throw err;

            cache.repository = null;
        }
    } else {
        cache.repository = null;
    }

    route.repository = cache.repository?.id;

    if (cache.repository == null) {
        app.go('/repositories');
        return;
    }

    let channels = UI.tableValues('channels', cache.repository.channels, 'name');

    UI.main(html`
        <div class="tabbar">
            <a href="/repositories">${T.repositories}</a>
            <a class="active">${cache.repository.name}</a>
        </div>

        <div class="tab">
            <div class="row">
                <div class="box" style="min-width: 250px;">
                    <div class="header">${T.repository}</div>
                    <div class="info">
                        ${cache.repository.name}
                        <div class="sub">${cache.repository.url}</div>
                    </div>
                    <button type="button" @click=${UI.wrap(e => configureRepository(cache.repository))}>${T.configure}</button>
                </div>

                <div class="box">
                    <div class="header">${T.channels}</div>
                    <table style="table-layout: fixed; width: 100%;">
                        <colgroup>
                            <col></col>
                            <col></col>
                            <col></col>
                        </colgroup>
                        <thead>
                            <tr>
                                ${UI.tableHeader('channels', 'name', T.name)}
                                ${UI.tableHeader('channels', 'size', T.size)}
                                ${UI.tableHeader('channels', 'time', T.timestamp)}
                            </tr>
                        </thead>
                        <tbody>
                            ${channels.map(channel => html`
                                <tr style="cursor: pointer;" @click=${UI.wrap(e => runChannel(cache.repository, channel.name))}>
                                    <td>${channel.name} <span class="sub">(${channel.count})</span></td>
                                    <td style="text-align: right;">${formatSize(channel.size)}</td>
                                    <td style="text-align: right;">${dayjs(channel.time).format('lll')}</td>
                                </tr>
                            `)}
                            ${!channels.length ? html`<tr><td colspan="3" style="text-align: center;">${T.no_channel}</td></tr>` : ''}
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    `);
}

async function configureRepository(repo) {
    let ptr = repo;

    if (repo == null) {
        repo = {
            name: '',
            url: ''
        };
    } else {
        repo = Object.assign({}, repo);
    }

    let url = repo.url;

    await UI.dialog({
        run: (render, close) => {
            return html`
                <div class="title">
                    ${ptr != null ? T.edit_repository : T.create_repository}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>${T.name}</span>
                        <input type="text" name="name" required value=${repo.name} />
                    </label>
                    <label>
                        <span>${T.url}</span>
                        <input type="text" name="url" required value=${url}
                               @input=${e => { url = e.target.value; render(); }} />
                    </label>
                </div>

                <div class="footer">
                    ${ptr != null ? html`
                        <button type="button" class="danger"
                                @click=${UI.wrap(e => deleteRepository(repo.id).then(close))}>${T.delete}</button>
                        <div style="flex: 1;"></div>
                    ` : ''}
                    <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                    <button type="submit">${ptr != null ? T.save : T.create}</button>
                </div>
            `
        },

        submit: async (elements) => {
            let obj = {
                id: repo.id,
                name: elements.name.value.trim() || null,
                url: elements.url.value.trim() || null
            };

            let json = await Net.post('/api/repository/save', obj);

            Net.invalidate('repositories');
            Net.invalidate('repository');

            let url = app.makeURL({ mode: 'repository', repository: json.id });
            await app.go(url);
        }
    });
}

async function deleteRepository(id) {
    await UI.dialog((render, close) => html`
        <div class="title">${T.delete_repository}</div>
        <div class="main">${T.confirm_not_reversible}</div>
        <div class="footer">
            <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.cancel}</button>
            <button type="submit">${T.confirm}</button>
        </div>
    `);

    await Net.post('/api/repository/delete', { id: id });
    Net.invalidate('repositories');

    if (route.repository == id)
        route.repository = null;
}

async function runChannel(repo, channel) {
    let url = Util.pasteURL('/api/repository/snapshots', { id: repo.id, channel: channel });
    let snapshots = await Net.cache('snapshots', url);

    // Make sure it is sorted by time
    snapshots.sort((snapshot1, snapshot2) => snapshot1.time - snapshot2.time);

    let canvas = null;

    if (snapshots.length >= 2) {
        const { Chart, adaptDayjs } = await import(BUNDLES['chart.bundle.js']).then(obj => {
            obj.adaptDayjs(dayjs);
            return obj;
        });

        let sets = {
            size: { label: T.size, data: [], fill: false },
            stored: { label: T.stored, data: [], fill: false },
            added: { label: T.added, data: [], fill: false }
        };

        for (let snapshot of snapshots) {
            sets.size.data.push({ x: dayjs(snapshot.time), y: snapshot.size });
            sets.stored.data.push({ x: dayjs(snapshot.time), y: snapshot.stored });
            if (snapshot.added != null)
                sets.added.data.push({ x: dayjs(snapshot.time), y: snapshot.added });
        }

        canvas = document.createElement('canvas');
        let ctx = canvas.getContext('2d');

        new Chart(ctx, {
            type: 'line',
            data: {
                datasets: Object.values(sets)
            },
            options: {
                responsive: true,
                plugins: {
                    legend: {
                        position: 'right',
                    },
                    tooltip: {
                        callbacks: {
                            label: item => `${item.dataset.label}: ${formatSize(item.parsed.y)}`
                        }
                    }
                },
                scales: {
                    x: {
                        type: 'time',
                        ticks: {
                        }
                    },
                    y: {
                        type: 'linear',
                        min: 0,
                        ticks: {
                            callback: formatSize
                        }
                    }
                }
            }
        });
    }

    await UI.dialog({
        run: (render, close) => {
            snapshots = UI.tableValues('snapshots', snapshots, 'time', false);

            return html`
                <div class="title">
                    ${T.format(T.x_snapshots, channel)}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    ${canvas != null ? html`<div style="width: 80%; margin: 0 auto;">${canvas}</div>` : ''}

                    <table style="table-layout: fixed;">
                        <colgroup>
                            <col></col>
                            <col></col>
                            <col></col>
                            <col></col>
                            <col></col>
                        </colgroup>
                        <thead>
                            <tr>
                                ${UI.tableHeader('snapshots', 'time', T.timestamp)}
                                ${UI.tableHeader('snapshots', 'oid', T.object)}
                                ${UI.tableHeader('snapshots', 'size', T.size)}
                                ${UI.tableHeader('snapshots', 'stored', T.stored)}
                                ${UI.tableHeader('snapshots', 'added', T.added)}
                            </tr>
                        </thead>
                        <tbody>
                            ${snapshots.map(snapshot => html`
                                <tr>
                                    <td>${dayjs(snapshot.time).format('lll')}</td>
                                    <td><span class="sub">${snapshot.oid}</span></td>
                                    <td style="text-align: right;">${formatSize(snapshot.size)}</td>
                                    <td style="text-align: right;">${formatSize(snapshot.stored)}</td>
                                    <td style="text-align: right;">
                                        ${snapshot.added != null ? formatSize(snapshot.added) : ''}
                                        ${snapshot.added == null ? html`<span class="sub">(${T.unknown.toLowerCase()})</span>` : ''}
                                    </td>
                                </tr>
                            `)}
                            ${!snapshots.length ? html`<tr><td colspan="5" style="text-align: center;">${T.no_snapshot}</td></tr>` : ''}
                        </tbody>
                    </table>
                </div>
            `;
        }
    });
}

function formatSize(size) {
    if (size >= 999950000) {
        let value = size / 1000000000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.size_units.gb;
    } else if (size >= 999950) {
        let value = size / 1000000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.size_units.mb;
    } else if (size >= 999.95) {
        let value = size / 1000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.size_units.kb;
    } else if (size > 1) {
        return T.format(T.size_units.x_b, size);
    } else {
        return size ? T.size_units.one : T.size_units.zero;
    }
}

export {
    runRepositories,
    runRepository
}
