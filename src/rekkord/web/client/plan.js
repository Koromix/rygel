// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import dayjs from 'vendor/dayjs/dayjs.bundle.js';
import { Util, Log, Net } from 'lib/web/base/base.js';
import * as UI from 'lib/web/ui/ui.js';
import * as app from './main.js';
import { route, cache } from './main.js';
import { ASSETS } from '../assets/assets.js';

const DAYS = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];

async function runPlans() {
    cache.plans = await Net.cache('plans', '/api/plan/list');

    let plans = UI.tableValues('plans', cache.plans, 'name');

    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.plans}</a>
            ${cache.plan != null ? html`<a href=${app.makeURL({ mode: 'plan' })}>${cache.plan.name}</a>` : ''}
        </div>

        <div class="tab">
            <div class="header">${T.plans}</div>
            <table style="table-layout: fixed; width: 100%;">
                <colgroup>
                    <col/>
                    <col/>
                    <col/>
                    <col/>
                </colgroup>
                <thead>
                    <tr>
                        ${UI.tableHeader('plans', 'name', T.name)}
                        ${UI.tableHeader('plans', 'repository', T.repository)}
                        <th>${T.key_prefix}</th>
                        ${UI.tableHeader('plans', makePlanTasks, T.tasks)}
                    </tr>
                </thead>
                <tbody>
                    ${plans.map(plan => {
                        let url = app.makeURL({ mode: 'plan', plan: plan.id });
                        let repo = cache.repositories.find(repo => repo.id == plan.repository);

                        return html`
                            <tr style="cursor: pointer;" @click=${UI.wrap(e => app.go(url))}>
                                <td><a href=${url}>${plan.name}</a></td>
                                <td><a href=${app.makeURL({ mode: 'repository', repository: plan.repository })}>${repo?.name}</a></td>
                                <td><span class="sub">${plan.key}</sub></td>
                                <td>${makePlanTasks(plan)}</td>
                            </tr>
                        `;
                    })}
                    ${!plans.length ? html`<tr><td colspan="4" style="text-align: center;">${T.no_plan}</td></tr>` : ''}
                </tbody>
            </table>
            <div class="actions">
                <button type="button" @click=${UI.wrap(e => configurePlan(null))}>${T.add_plan}</button>
            </div>
        </div>
    `);
}

function makePlanTasks(plan) {
    let parts = [];

    if (plan.items) {
        let text = (plan.items > 1) ? T.format(T.run_x_snapshot_items, plan.items) : T.run_snapshot_item;
        parts.push(text);
    }
    if (plan.scan != null) {
        let text = T.format(T.scan_repository_at_x, formatClock(plan.scan));
        parts.push(text);
    }

    if (!parts.length) {
        let empty = html`<span class="sub">(${T.nothing_to_do.toLowerCase()})</span>`;
        parts.push(empty);
    }

    return parts.map((part, idx) => html`${idx ? html`<br>` : ''}${part}`);
}

async function runPlan() {
    if (route.plan != null) {
        try {
            let url = Util.pasteURL('/api/plan/get', { id: route.plan });
            cache.plan = await Net.cache('plan', url);
        } catch (err) {
            if (err.status != 404 && err.status != 422)
                throw err;

            cache.plan = null;
        }
    } else {
        cache.plan = null;
    }

    route.plan = cache.plan?.id;

    if (cache.plan == null) {
        app.go('/plans');
        return;
    }

    let repo = cache.repositories.find(repo => repo.id == cache.plan.repository);

    UI.main(html`
        <div class="tabbar">
            <a href="/plans">${T.plans}</a>
            <a class="active">${cache.plan.name}</a>
        </div>

        <div class="tab">
            <div class="row">
                <div class="box" style="min-width: 250px;">
                    <div class="header">${T.plan}</div>
                    <div class="info">
                        ${cache.plan.name} (${repo?.name ?? T.unassigned.toLowerCase()})
                        <div class="sub">${cache.plan.key}</div>
                    </div>
                    <button type="button" @click=${UI.wrap(e => configurePlan(cache.plan))}>${T.configure}</button>
                    ${cache.plan.scan != null ?
                        html`<div style="text-align: center;">${T.format(T.scan_repository_at_x, formatClock(cache.plan.scan))}</div>` : ''}
                </div>

                <div class="box">
                    <div class="header">${T.snapshot_items}</div>
                    <table style="table-layout: fixed; width: 100%;">
                        <colgroup>
                            <col style="width: 150px;"></col>
                            <col></col>
                            <col style="width: 100px;"></col>
                            <col></col>
                            <col></col>
                        </colgroup>
                        <thead>
                            <tr>
                                <th>${T.channel}</th>
                                <th>${T.days}</th>
                                <th>${T.clock_time}</th>
                                <th>${T.paths}</th>
                                <th>${T.last_run}</th>
                            </tr>
                        </thead>
                        <tbody>
                            ${cache.plan.items.map(item => html`
                                <tr>
                                    <td>${item.channel}</td>
                                    <td>${formatDays(item.days)}</td>
                                    <td style="text-align: center;">${formatClock(item.clock)}</td>
                                    <td class="nowrap">${item.paths.map(path => html`${path}<br>`)}</td>
                                    <td style=${'text-align: right;' + (item.error != null ? ' color: var(--color, red);' : '')}>
                                        ${item.timestamp == null ? T.never : ''}
                                        ${item.timestamp != null ? dayjs(item.timestamp).format('lll') : ''}
                                        <br>${item.error || ''}
                                    </td>
                                </tr>
                            `)}
                            ${!cache.plan.items.length ? html`<tr><td colspan="5" style="text-align: center;">${T.no_snapshot_item}</td></tr>` : ''}
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    `);
}

async function configurePlan(plan) {
    let ptr = plan;

    if (plan == null) {
        if (!cache.repositories.length)
            throw new Error(T.message(`Create a repository before you create a plan`));

        plan = {
            name: '',
            repository: route.repository ?? cache.repositories[0].id,
            scan: null,
            items: []
        };
    } else {
        plan = Object.assign({}, plan);
        plan.items = plan.items.slice();
    }

    let scan = true;

    if (plan.scan == null) {
        scan = false;
        plan.scan = 1200;
    }

    await UI.dialog({
        run: (render, close) => {
            return html`
                <div class="title">
                    ${ptr != null ? T.edit_plan : T.create_plan}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>Name</span>
                        <input type="text" name="name" required .value=${live(plan.name)}
                               @change=${UI.wrap(e => { plan.name = e.target.value; render(); })} />
                    </label>
                    <label>
                        <span>Repository</span>
                        <select ?disabled=${ptr != null}
                                @change=${UI.wrap(e => { plan.repository = parseInt(e.target.value, 10); render(); })}>
                            ${cache.repositories.map(repo =>
                                html`<option value=${repo.id} ?selected=${repo.id == plan.repository}>${repo.name}</option>`)}
                        </select>
                    </label>

                    <div class="section">${T.periodic_scans}</div>
                    <label>
                        <input type="checkbox" ?checked=${scan} @change=${UI.wrap(e => { scan = e.target.checked; render(); })} />
                        <span>${T.run_daily_scans}</span>
                    </label>
                    ${scan ? html`
                        <label>
                            <span>${T.scan_time}</span>
                            <td><input type="time" .value=${live(formatClock(plan.scan))}
                                       @change=${UI.wrap(e => { plan.scan = parseClock(e.target.value); render(); })} /></td>
                        </label>
                        <div style="color: red; font-style: italic;">${T.scan_safety_warning}</div>
                    ` : ''}

                    <div class="section">
                        Snapshot items
                        <div style="flex: 1;"></div>
                        <button type="button" class="small" @click=${UI.wrap(add_item)}>${T.add_item}</button>
                    </div>
                    <table style="table-layout: fixed;">
                        <colgroup>
                            <col class="check"/>
                            <col style="width: 200px;"/>
                            <col style="width: 200px;"/>
                            <col style="width: 150px;"/>
                            <col style="width: 200px;"/>
                            <col style="width: 60px;"/>
                        </colgroup>

                        <thead>
                            <tr>
                                <th></th>
                                <th>${T.channel}</th>
                                <th>${T.days}</th>
                                <th>${T.clock_time}</th>
                                <th>${T.paths}</th>
                                <th></th>
                            </tr>
                        </thead>

                        <tbody>
                            ${plan.items.map(item => {
                                let paths = [...item.paths, ''];

                                return html`
                                    <tr ${UI.reorderItems(plan.items, item)}>
                                        <td class="grab"><img src=${ASSETS['ui/move']} width="16" height="16" alt=${T.move} /></td>
                                        <td><input type="text" .value=${live(item.channel)} @change=${UI.wrap(e => { item.channel = e.target.value; render(); })} /></td>
                                        <td>
                                            ${Util.mapRange(0, DAYS.length, idx => {
                                                let active = !!(item.days & (1 << idx));

                                                return html`
                                                    <label>
                                                        <input type="checkbox" .checked=${active} @change=${UI.wrap(e => toggle_day(item, idx))} />
                                                        <span>${T.day_names[DAYS[idx]]}</span>
                                                    </label>
                                                `;
                                            })}
                                        </td>
                                        <td><input type="time" .value=${live(formatClock(item.clock))}
                                                   @change=${UI.wrap(e => { item.clock = parseClock(e.target.value); render(); })} /></td>
                                        <td>${paths.map((path, idx) =>
                                            html`<input type="text" style=${idx ? 'margin-top: 3px;' : ''} .value=${live(path)}
                                                        @input=${UI.wrap(e => edit_path(item, idx, e.target.value))} />`)}</td>
                                        <td class="right">
                                            <button type="button" class="small"
                                                    @click=${UI.insist(e => delete_item(item))}><img src=${ASSETS['ui/delete']} alt=${T.delete} /></button>
                                        </td>
                                    </tr>
                                `;
                            })}
                            ${!plan.items.length ?
                                html`<tr><td colspan="6" style="text-align: center;">${T.no_item}</td></tr>` : ''}
                        </tbody>
                    </table>

                    ${ptr != null ? html`
                        <div class="section">${T.api_key}</div>
                        <label>
                            <span>${T.key_prefix}</span>
                            <div class="sub">${plan.key}</div>
                            <button type="button" class="small secondary" @click=${UI.insist(renew_key)}>${T.renew_key}</button>
                        </label>
                    ` : ''}
                </div>

                <div class="footer">
                    ${ptr != null ? html`
                        <button type="button" class="danger"
                                @click=${UI.wrap(e => deletePlan(plan.id).then(close))}>${T.delete}</button>
                        <div style="flex: 1;"></div>
                    ` : ''}
                    <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                    <button type="submit">${ptr != null ? T.save : T.create}</button>
                </div>
            `;

            function add_item() {
               let item = {
                    channel: '',
                    days: 0b0011111,
                    clock: 0,
                    paths: []
                };
                plan.items.push(item);

                render();
            }

            function delete_item(item) {
                plan.items = plan.items.filter(it => it !== item);
                render();
            }

            function toggle_day(item, idx) {
                item.days = (item.days ^ (1 << idx)) || item.days;
                render();
            }

            function edit_path(item, idx, path) {
                if (idx >= item.paths.length) {
                    if (path)
                        item.paths.push(path);
                } else if (path) {
                    item.paths[idx] = path;
                } else {
                    item.paths.splice(idx, 1);
                }

                render();
            }

            async function renew_key() {
                let json = await Net.post('/api/plan/key', { id: plan.id });

                Net.invalidate('plans');
                Net.invalidate('plan');

                await showKey(plan, json.key, json.secret);
                plan.key = json.key;

                render();
            }
        },

        submit: async (elements) => {
            let obj = {
                id: plan.id,
                name: plan.name,
                repository: plan.repository,
                scan: scan ? plan.scan : null,
                items: plan.items.map(item => ({
                    channel: item.channel,
                    days: item.days,
                    clock: item.clock,
                    paths: item.paths
                }))
            };

            let json = await Net.post('/api/plan/save', obj);

            Net.invalidate('plans');
            Net.invalidate('plan');

            let url = app.makeURL({ mode: 'plan', plan: json.id });
            await app.go(url);

            if (ptr == null)
                await showKey(plan, json.key, json.secret);
        }
    });
}

async function showKey(plan, key, secret) {
    let full = `${key}/${secret}`;

    try {
        await UI.dialog({
            run: (render, close) => html`
                <div class="title">
                    ${T.api_key}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>${T.api_key}</span>
                        <input type="text" style="width: 40em;" readonly value=${full} />
                        <button type="button" class="small" @click=${UI.wrap(e => writeClipboard(T.api_key, full))}>${T.copy}</button>
                    </label>

                    <div style="color: red; font-style: italic; text-align: center">${T.please_copy_api_key}</div>
                </div>

                <div class="footer" style="justify-content: center;">
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.close}</button>
                </div>
            `
        });
    } catch (err) {
        if (err != null)
            throw err;
    }
}

async function deletePlan(id) {
    await UI.dialog((render, close) => html`
        <div class="title">${T.delete_plan}</div>
        <div class="main">${T.confirm_not_reversible}</div>
        <div class="footer">
            <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.cancel}</button>
            <button type="submit">${T.confirm}</button>
        </div>
    `);

    await Net.post('/api/plan/delete', { id: id });
    Net.invalidate('plans');

    if (route.plan == id)
        route.plan = null;
}

function formatDays(days) {
    let parts = [];

    for (let i = 0; i < DAYS.length; i++) {
        if (days & (1 << i)) {
            let prefix = T.day_names[DAYS[i]].substr(0, 3);
            parts.push(prefix);
        }
    }

    return parts.join(', ');
}

function formatClock(clock) {
    let hh = Math.floor(clock / 100).toString().padStart(2, '0');
    let mm = Math.floor(clock % 100).toString().padStart(2, '0');

    return `${hh}:${mm}`;
}

function parseClock(clock) {
    let [hh, mm] = (clock ?? '').split(':').map(value => parseInt(value, 10));
    return hh * 100 + mm;
}

async function writeClipboard(label, text) {
    await navigator.clipboard.writeText(text);

    let msg = T.message(`{1} copied to clipboard`, label);
    Log.info(msg);
}

export {
    runPlans,
    runPlan
}
