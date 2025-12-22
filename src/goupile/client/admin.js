// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, Mutex, LocalDate, LocalTime } from 'lib/web/base/base.js';
import { Base64 } from 'lib/web/base/mixer.js';
import * as goupile from './goupile.js';
import { profile } from './goupile.js';
import * as UI from './ui.js';

import './admin.css';

let domain;
let instances;
let users;
let archives;

let selected_instance;
let all_users;
let selected_permissions;

// Explicit mutex to serialize (global) state-changing functions
let mutex = new Mutex;

async function init(fallback) {
    UI.setMenu(renderMenu);

    UI.createPanel('domain', 0, renderDomain);
    UI.createPanel('users', 1, renderUsers);
    if (profile.root)
        UI.createPanel('archives', 1, renderArchives);

    UI.setPanels(['domain','users']);
}

function renderMenu() {
    return html`
        <nav class="ui_toolbar" id="ui_top" style="z-index: 999999;">
            <button class="icon admin" @click=${UI.wrap(e => go(e, '/admin/'))}>${T.admin}</button>
            <button class=${'icon domain' + (UI.isPanelActive('domain') ? ' active' : '')}
                    @click=${UI.wrap(e => togglePanel('domain'))}>${T.projects}</button>
            <button class=${'icon users' + (UI.isPanelActive('users') ? ' active' : '')}
                    @click=${UI.wrap(e => togglePanel('users'))}>${T.users}</button>
            ${profile.root ? html`
                <button class=${'icon archives' + (UI.isPanelActive('archives') ? ' active' : '')}
                        @click=${UI.wrap(e => togglePanel('archives'))}>${T.archives}</button>
            ` : ''}
            <div style="flex: 1;"></div>
            <div class="drop right" @click=${UI.deployMenu}>
                <button class=${goupile.isLoggedOnline() ? 'icon online' : 'icon offline'}>${profile.username}</button>
                <div>
                    <button @click=${UI.wrap(goupile.runChangePasswordDialog)}>${T.change_my_password}</button>
                    <button @click=${UI.wrap(goupile.runResetTOTP)}>${T.configure_my_totp}</button>
                    <hr/>
                    <button @click=${UI.wrap(goupile.logout)}>${T.logout}</button>
                </div>
            </div>
        </nav>
    `;
}

function togglePanel(key, enable = null) {
    UI.togglePanel(key, enable);
    return go();
}

function renderDomain() {
    return html`
        <div class="padded" style="background: #f8f8f8;">
            <div class="ui_quick">
                ${profile.root ? html`<a @click=${UI.wrap(runCreateInstanceDialog)}>${T.create_project}</a>` : ''}
                <div style="flex: 1;"></div>
                ${T.projects} (<a @click=${UI.wrap(e => { instances = null; return go(); })}>${T.refresh.toLowerCase()}</a>)
            </div>

            <table class="ui_table fixed">
                <colgroup>
                    <col/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                </colgroup>

                <tbody>
                    ${instances.map(instance => html`
                        <tr class=${instance === selected_instance ? 'active' : ''}>
                            <td style="text-align: left;" class=${instance.master != null ? 'child' : ''}
                                colspan=${instance.master != null ? 2 : 1}>
                                ${instance.master == null && instance.legacy ? html`<span class="ui_tag" style="background: #bbb;">v2</span>`  : ''}
                                ${instance.master != null ? html`<span style="color: #ccc;">${instance.master} / </span>${instance.key.replace(/^.*\//, '')}` : ''}
                                ${instance.master == null ? instance.key : ''}
                                (<a href=${'/' + instance.key} target="_blank">${T.access.toLowerCase()}</a>)
                            </td>
                            ${instance.master == null ?
                                html`<td><a role="button" tabindex="0" @click=${UI.wrap(e => runSplitInstanceDialog(e, instance.key))}>${T.split}</a></td>` : ''}
                            <td><a role="button" tabindex="0" href=${Util.pasteURL('/admin/', { select: instance != selected_instance ? instance.key : null })}>${T.permissions}</a></td>
                            <td><a role="button" tabindex="0" @click=${UI.wrap(e => runConfigureInstanceDialog(e, instance))}>${T.configure}</a></td>
                            <td>${profile.root || instance.master != null ? html`<a role="button" tabindex="0" @click=${UI.wrap(e => runDeleteInstanceDialog(e, instance))}>${T.delete}</a>` : ''}</td>
                        </tr>
                    `)}
                    ${!instances.length ? html`<tr><td colspan="5">${T.no_project}</td></tr>` : ''}
                </tbody>
            </table>

            ${profile.root ? html`
                <div class="ui_quick">
                    <div style="flex: 1;"></div>
                    <a @click=${UI.wrap(runConfigureDomainDialog)}>${T.configure_domain}</a>
                    <div style="flex: 1;"></div>
                </div>
            ` : ''}
        </div>
    `;
}

function renderUsers() {
    let visible_users = users.filter(user => selected_instance == null || all_users || selected_permissions.permissions[user.userid]);

    return html`
        <div class="padded" style="flex-grow: 1.5;">
            <div class="ui_quick">
                <a @click=${UI.wrap(runCreateUserDialog)}>${T.create_user}</a>
                <div style="flex: 1;"></div>
                ${selected_instance != null ? html`
                    <div class="fm_check">
                        <input id="all_users" type="checkbox" .checked=${all_users}
                               @change=${UI.wrap(e => { all_users = e.target.checked; return go(); })} />
                        <label for="all_users">${T.show_all_users}</label>
                    </div>
                    <div style="flex: 1;"></div>
                ` : ''}
                ${T.users} (<a @click=${UI.wrap(e => { users = null; selected_permissions = null; return go(); })}>${T.refresh.toLowerCase()}</a>)
            </div>

            <table class="ui_table fixed">
                <colgroup>
                    ${selected_instance == null ? html`
                        <col style="width: 160px;"/>
                        <col/>
                        <col style="width: 160px;"/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    ` : ''}
                    ${selected_instance != null ? html`
                        <col style="width: 160px;"/>
                        <col style="width: 100px;"/>
                        <col/>
                        <col style="width: 100px;"/>
                    ` : ''}
                </colgroup>

                <tbody>
                    ${!visible_users.length ? html`<tr><td colspan=${selected_instance != null ? 5 : 4}>${T.no_user}</td></tr>` : ''}
                    ${visible_users.map(user => {
                        let permissions;
                        if (selected_instance != null) {
                            permissions = selected_permissions.permissions[user.userid] || [];
                        } else {
                            permissions = [];
                        }

                        return html`
                            <tr>
                                <td style=${'text-align: left;' + (user.root ? ' color: #db0a0a;' : '')}>
                                    ${user.username}
                                    ${user.root ? html`<span title=${T.root}>♛\uFE0E</span>` : ''}
                                </td>
                                ${selected_instance == null ? html`
                                    <td style="text-align: left;">${user.email != null ? html`<a href=${'mailto:' + user.email}>${user.email}</a>` : ''}</td>
                                    <td style="text-align: left;">${user.phone != null ? html`<a href=${'tel:' + user.phone}>${user.phone}</a>` : ''}</td>
                                ` : ''}
                                <td><a role="button" tabindex="0"
                                       @click=${UI.wrap(e => runEditUserDialog(e, user))}>${T.edit}</a></td>
                                ${selected_instance != null ? html`
                                    <td class=${!permissions.length ? 'ui_sub' : ''}
                                        style="white-space: normal;">
                                        ${selected_instance.master == null ? makePermissionsTag(permissions, 'build_', '#b518bf') : ''}
                                        ${!selected_instance.slaves ? makePermissionsTag(permissions, 'data_', '#258264') : ''}
                                        ${!selected_instance.slaves ? makePermissionsTag(permissions, 'bulk_', '#3364bd') : ''}
                                        ${!selected_instance.slaves ? makePermissionsTag(permissions, 'message_', '#c97f1a') : ''}
                                        ${!permissions.length ? T.unassigned : ''}
                                    </td>
                                    <td><a role="button" tabindex="0"
                                           @click=${UI.wrap(e => runAssignUserDialog(e, selected_instance,
                                                                                     user, permissions))}>${T.assign}</a></td>
                                ` : ''}
                                ${selected_instance == null ?
                                    html`<td><a role="button" tabindex="0"
                                                @click=${UI.wrap(e => runDeleteUserDialog(e, user))}>${T.delete}</a></td>` : ''}
                            </tr>
                        `;
                    })}
                </tbody>
            </table>
        </div>
    `;
}

function makePermissionsTag(permissions, prefix, background) {
    permissions = permissions.filter(perm => perm.startsWith(prefix));

    if (permissions.length) {
        let names = permissions.map(perm => Util.capitalize(perm.substr(prefix.length)));
        return html`<span class="ui_tag" style=${'background: ' + background + ';'}>${names.join('|')}</span>`;
    } else {
        return '';
    }
}

function renderArchives() {
    return html`
        <div class="padded">
            <div style="margin-bottom: 2em;">
                <p>${unsafeHTML(T.format(T.archive_retention_period, domain.archive.retention))}</p>
                <p>${unsafeHTML(T.remember_restore_key)}</p>
            </div>

            <div class="ui_quick">
                <a @click=${UI.wrap(createBackup)}>${T.create_archive}</a>
                <div style="flex: 1;"></div>
                Archives (<a @click=${UI.wrap(e => { archives = null; return go(); })}>${T.refresh.toLowerCase()}</a>)
            </div>

            <table class="ui_table fixed">
                <colgroup>
                    <col/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                </colgroup>

                <tbody>
                    ${!archives.length ? html`<tr><td colspan="4">${T.no_archive}</td></tr>` : ''}
                    ${archives.map(archive => html`
                        <tr>
                            <td style="text-align: left;"><a href=${'/admin/api/archives/files/' + archive.filename}
                                                             download>${archive.filename}</a></td>
                            <td>${Util.formatDiskSize(archive.size)}</td>
                            <td><a @click=${UI.wrap(e => runRestoreBackupDialog(e, archive.filename))}>${T.restore}</a></td>
                            <td><a @click=${UI.wrap(e => runDeleteBackupDialog(e, archive.filename))}>${T.delete}</a></td>
                        </tr>
                    `)}
                </tbody>
            </table>

            <div class="ui_quick">
                <div style="flex: 1;"></div>
                <a @click=${UI.wrap(runUploadBackupDialog)}>${T.upload_archive}</a>
            </div>
        </div>
    `;
}

async function createBackup() {
    let progress = Log.progress(T.archive_in_progress);

    try {
        await Net.post('/admin/api/archives/create', null, { timeout: 180000 });

        progress.success(T.archive_done);
        archives = null;

        go();
    } catch (err) {
        progress.close();
        throw err;
    }
}

async function runUploadBackupDialog(e) {
    return UI.dialog(e, T.upload_archive, {}, (d, resolve, reject) => {
        d.file('*archive', T.archive);

        d.action(T.send, { disabled: !d.isValid() }, async () => {
            let progress = Log.progress(T.sending_in_progress);

            try {
                let url = '/admin/api/archives/files/' + d.values.archive.name;

                let response = await Net.fetch(url, {
                    method: 'PUT',
                    body: d.values.archive,
                    timeout: null
                });

                if (response.ok) {
                    resolve();
                    progress.success(T.sending_done);

                    archives = null;

                    go();
                } else {
                    let err = await Net.readError(response);
                    throw new Error(err);
                }
            } catch (err) {
                progress.close();

                Log.error(err);
                d.refresh();
            }
        });
    });
}

async function runRestoreBackupDialog(e, filename) {
    return UI.dialog(e, T.format(T.restore_x, filename), {}, (d, resolve, reject) => {
        d.password('*key', T.restore_key);
        d.boolean('*restore_users', T.restore_users_and_permissions, { value: false, untoggle: false });

        d.action(T.restore, { disabled: !d.isValid() }, async () => {
            let progress = Log.progress(T.restore_in_progress);

            try {
                await Net.post('/admin/api/domain/restore', {
                    filename: filename,
                    key: d.values.key,
                    users: d.values.restore_users
                });

                resolve();
                progress.success(T.format(T.archive_x_restored, filename));

                instances = null;
                users = null;
                archives = null;

                go();
            } catch (err) {
                progress.close();

                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runDeleteBackupDialog(e, filename) {
    return UI.confirm(e, T.format(T.confirm_archive_deletion, filename),
                         T.delete, async () => {
        await Net.post('/admin/api/archives/delete', { filename: filename });

        Log.success(T.format(T.archive_x_deleted, filename));
        archives = null;

        go();
    });
}

async function go(e, url = null, options = {}) {
    await mutex.run(async () => {
        options = Object.assign({ push_history: true }, options);

        let new_domain = domain;
        let new_instances = instances;
        let new_users = users;
        let new_archives = archives;
        let new_selected = selected_instance;
        let new_all_users = all_users;
        let new_permissions = selected_permissions;
        let explicit_panels = false;

        if (new_domain == null)
            new_domain = await Net.get('/admin/api/domain/info');
        if (new_instances == null)
            new_instances = await Net.get('/admin/api/instances/list');
        if (new_users == null)
            new_users = await Net.get('/admin/api/users/list');

        if (url != null) {
            if (!(url instanceof URL))
                url = new URL(url, window.location.href);
            goupile.setCurrentHash(url.hash);

            let panels = url.searchParams.get('p');

            if (panels) {
                panels = panels.split('|').filter(panel => panel);

                if (panels.length) {
                    UI.setPanels(panels);
                    explicit_panels = true;
                }
            }

            if (url.searchParams.has('select')) {
                let select = url.searchParams.get('select');
                new_selected = new_instances.find(instance => instance.key === select);
            } else {
                new_selected = null;
            }

            if (url.searchParams.has('all')) {
                new_all_users = (url.searchParams.get('all') == 1);
            } else if (new_all_users == null) {
                new_all_users = true;
            }
        }

        if (UI.isPanelActive('archives') && new_archives == null) {
            new_archives = await Net.get('/admin/api/archives/list');
            new_archives.sort(Util.makeComparator(archive => archive.filename));
        }

        if (new_selected != null)
            new_selected = new_instances.find(instance => instance.key === new_selected.key);
        if (new_selected != null) {
            if (new_permissions == null || new_permissions.key != new_selected.key) {
                let url = Util.pasteURL('/admin/api/instances/permissions', { instance: new_selected.key });
                let permissions = await Net.get(url);

                new_permissions = {
                    key: new_selected.key,
                    permissions: permissions
                };
            }

            if (!explicit_panels && (selected_instance == null ||
                                     new_selected.key !== selected_instance.key))
                UI.togglePanel('users', true);
        } else {
            new_permissions = null;
        }

        // Commit
        domain = new_domain;
        instances = new_instances;
        users = new_users;
        archives = new_archives;
        selected_instance = new_selected;
        selected_permissions = new_permissions;
        all_users = new_all_users;

        // Update browser URL
        {
            let params = {};

            if (selected_instance != null)
                params.select = selected_instance.key;
            params.all = 0 + all_users;
            params.p = UI.getPanels().join('|') || null;

            let url = Util.pasteURL('/admin/', params);
            goupile.syncHistory(url, options.push_history);
        }

        UI.draw();
    });
}

function runCreateInstanceDialog(e) {
    return UI.dialog(e, T.create_project, {}, (d, resolve, reject) => {
        d.text('*key', T.name, {
            help: [
                T.project_key_length,
                T.project_key_characters
            ]
        });

        d.text('name', T.title, { value: d.values.key });

        let langs = Object.keys(goupile.languages).map(lang => [lang, lang.toUpperCase()]);
        d.enumDrop('lang', T.project_language, langs, { value: domain.default_lang });

        d.boolean('populate', T.populate_demo, { value: true, untoggle: false });

        d.action(T.create, { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/instances/create', {
                    key: d.values.key,
                    name: d.values.name,
                    populate: d.values.populate,
                    lang: d.values.lang
                });

                resolve();
                Log.success(T.format(T.project_x_created, d.values.key));

                instances = null;
                selected_permissions = null;

                let url = Util.pasteURL('/admin/', { select: d.values.key });
                go(null, url);
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runConfigureInstanceDialog(e, instance) {
    return UI.dialog(e, T.format(T.configure_x, instance.key), {}, (d, resolve, reject) => {
        d.pushOptions({ untoggle: false });

        if (instance.master == null) {
            d.tabs('tabs', () => {
                d.tab(T.basic, () => {
                    d.text('*name', T.name, { value: instance.config.name });

                    if (!instance.legacy) {
                        let langs = Object.keys(goupile.languages).map(lang => [lang, lang.toUpperCase()]);
                        d.enumDrop('lang', T.language, langs, { value: instance.config.lang });
                    }

                    d.boolean('*use_offline', T.use_offline, { value: instance.config.use_offline });
                    d.boolean('*allow_guests', T.allow_guests, { value: instance.config.allow_guests });
                });

                d.tab(T.advanced, () => {
                    d.boolean('*data_remote', T.online_data, { value: instance.config.data_remote });

                    d.text('token_key', T.session_by_token, { value: instance.config.token_key });
                    if (d.values.token_key != null && !checkCryptoKey(d.values.token_key))
                        d.error('token_key', T.invalid_key_format);

                    d.number('fs_version', T.fs_version, {
                        suffix: T.format(T.current_fs_x, instance.config.fs_version),
                        help: T.fs_version_warning
                    });
                });

                d.tab(T.exports, () => {
                    let export_days = [0, 1, 2, 3, 4, 5, 6].filter(day => instance.config.export_days & (1 << day));
                    let export_time = new LocalTime(Math.floor(instance.config.export_time / 100), instance.config.export_time % 100);

                    // Beautiful JS API
                    let time = new Date;
                    let offset = ENV.timezone + time.getTimezoneOffset();
                    time.setMinutes(time.getMinutes() + offset);

                    d.multiCheck('export_days', T.export_days, [
                        [0, T.day_names.monday],
                        [1, T.day_names.tuesday],
                        [2, T.day_names.wednesday],
                        [3, T.day_names.thursday],
                        [4, T.day_names.friday],
                        [5, T.day_names.saturday],
                        [6, T.day_names.sunday]
                    ], { value: export_days });

                    d.time('export_time', T.export_time, {
                        value: export_time,
                        disabled: !d.values.export_days?.length,
                        help: T.format(T.local_time_x, time.toLocaleTimeString())
                    });
                    d.boolean('*export_all', T.export_all, {
                        value: instance.config.export_all,
                        disabled: !d.values.export_days?.length
                    });
                })
            });

            d.action(T.configure, { disabled: !d.isValid() }, async () => {
                try {
                    let export_days = (d.values.export_days ?? []).reduce((acc, day) => acc | (1 << day), 0);
                    let export_time = (d.values.export_time != null) ? (d.values.export_time.hour * 100 + d.values.export_time.minute) : null;

                    await Net.post('/admin/api/instances/configure', {
                        instance: instance.key,
                        name: d.values.name,
                        lang: d.values.lang,
                        use_offline: d.values.use_offline,
                        data_remote: d.values.data_remote,
                        token_key: d.values.token_key,
                        allow_guests: d.values.allow_guests,
                        fs_version: d.values.fs_version,
                        export_days: export_days,
                        export_time: export_time,
                        export_all: d.values.export_all
                    });

                    resolve();
                    Log.success(T.format(T.project_x_edited, instance.key));

                    instances = null;

                    go();
                } catch (err) {
                    Log.error(err);
                    d.refresh();
                }
            });
        } else {
            d.text('*name', T.name, { value: instance.config.name });

            d.action(T.configure, { disabled: !d.isValid() }, async () => {
                try {
                    await Net.post('/admin/api/instances/configure', {
                        instance: instance.key,
                        name: d.values.name
                    });

                    resolve();
                    Log.success(T.format(T.project_x_edited, instance.key));

                    instances = null;

                    go();
                } catch (err) {
                    Log.error(err);
                    d.refresh();
                }
            });
        }
    });
}

function checkCryptoKey(str) {
    try {
        let key = Base64.toBytes(str);
        return key.length === 32;
    } catch (err) {
        return false;
    }
}

async function runMigrateDialog(e, instance) {
    return UI.dialog(e, T.format(T.migrate_x, instance.key), {}, (d, resolve, reject) => {
        d.output(T.format(T.confirm_migration_of_x, instance.key));

        d.action(T.migrate, {}, async () => {
            try {
                await Net.post('/admin/api/instances/migrate', {
                    instance: instance.key
                }, { timeout: 180000 });

                resolve();
                Log.success(T.format(T.project_x_migrated, instance.key));

                instances = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runDeleteInstanceDialog(e, instance) {
    return UI.confirm(e, T.format(T.confirm_project_deletion, instance.key),
                         T.delete, async () => {
        await Net.post('/admin/api/instances/delete', {
            instance: instance.key
        }, { timeout: 180000 });

        Log.success(T.format(T.project_x_deleted, instance.key));

        instances = null;
        archives = null;

        go();
    });
}

function runSplitInstanceDialog(e, master) {
    return UI.dialog(e, T.format(T.split_x, master), {}, (d, resolve, reject) => {
        d.calc('instance', T.project, master);
        d.text('*key', T.slave_key);
        d.text('name', T.name, { value: d.values.key });

        d.action(T.create, { disabled: !d.isValid() }, async () => {
            let full_key = master + '/' + d.values.key;

            try {
                await Net.post('/admin/api/instances/create', {
                    key: full_key,
                    name: d.values.name
                });

                resolve();
                Log.success(T.format(T.slave_x_created, full_key));

                instances = null;
                selected_permissions = null;

                let url = Util.pasteURL('/admin/', { select: full_key });
                go(null, url);
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runConfigureDomainDialog(e) {
    return UI.dialog(e, T.format(T.configure_x, domain.name), {}, (d, resolve, reject) => {
        d.pushOptions({ untoggle: false });

        d.tabs('tabs', () => {
            d.tab(T.domain, () => {
                d.text('*name', T.domain_name, { value: domain.name });
                d.text('*title', T.domain_title, { value: domain.title });

                let langs = Object.keys(goupile.languages).map(lang => [lang, lang.toUpperCase()]);
                d.enumDrop('default_lang', T.default_language, langs, {
                    value: domain.default_lang,
                    help: T.default_project_language
                });

                d.text('*archive_key', T.archive_key, { value: domain.archive.key });
            });

            d.tab(T.smtp, () => {
                if (domain.smtp.provisioned) {
                    d.output(T.provisioned_settings);
                    d.pushOptions({ disabled: true });
                }

                d.text('smtp_url', T.url, {
                    value: domain.smtp.url,
                    help: html`${T.smtp_url_help} <a href="https://everything.curl.dev/usingcurl/smtp.html" target="_blank">CURL SMTP</a>`
                });
                d.text('smtp_user', T.username, {
                    value: domain.smtp.user,
                    help: T.smtp_auth_help
                });
                d.password('smtp_password', T.password, { value: domain.smtp.password });
                d.text('smtp_from', T.smtp_from, { value: domain.smtp.from });
            });

            d.tab(T.security, () => {
                if (domain.security.provisioned) {
                    d.output(T.provisioned_settings);
                    d.pushOptions({ disabled: true });
                }

                let props = [
                    ['easy', T.password_complexity.easy],
                    ['moderate', T.password_complexity.moderate],
                    ['hard', T.password_complexity.hard]
                ];

                d.enumDrop('user_password', T.user_passwords, props, { value: domain.security.user_password });
                d.enumDrop('admin_password', T.admin_passwords, props, { value: domain.security.admin_password });
                d.enumDrop('root_password', T.root_passwords, props, { value: domain.security.root_password });
            });
        });

        d.action(T.configure, { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/domain/configure', {
                    name: d.values.name,
                    title: d.values.title,
                    default_lang: d.values.default_lang,
                    archive_key: d.values.archive_key,

                    smtp: !domain.smtp.provisioned ? {
                        url: d.values.smtp_url,
                        user: d.values.smtp_user,
                        password: d.values.smtp_password,
                        from: d.values.smtp_from
                    } : null,

                    security: !domain.security.provisioned ? {
                        user_password: d.values.user_password,
                        admin_password: d.values.admin_password,
                        root_password: d.values.root_password
                    } : null
                });

                resolve();
                Log.success(T.domain_edited);

                domain = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runCreateUserDialog(e) {
    return UI.dialog(e, T.create_user, {}, (d, resolve, reject) => {
        let username = d.text('*username', T.username);

        d.password('*password', T.password);
        d.password('*password2', null, { placeholder: T.confirmation });
        d.boolean('*change_password', T.require_password_change, {
            value: true, untoggle: false
        });
        if (d.values.password != null && d.values.password2 != null) {
            if (d.values.password !== d.values.password2) {
                d.error('password2', T.password_mismatch);
            } else if (d.values.password.length < 8) {
                d.error('password2', T.password_too_short);
            }
        }
        d.boolean('*confirm', T.use_totp, {
            value: false, untoggle: false
        });

        d.text('email', T.email);
        if (d.values.email != null && !d.values.email.includes('@'))
            d.error('email', T.invalid_format);
        d.text('phone', T.phone);
        if (d.values.phone != null && !d.values.phone.startsWith('+'))
            d.error('phone', T.invalid_format_use_prefix);

        if (profile.root)
            d.boolean('*root', T.root, { value: false, untoggle: false });

        d.action(T.create, { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/users/create', {
                    username: d.values.username,
                    password: d.values.password,
                    change_password: d.values.change_password,
                    confirm: d.values.confirm,
                    email: d.values.email,
                    phone: d.values.phone,
                    root: profile.root ? d.values.root : false
                });

                resolve();
                Log.success(T.format(T.user_x_created, d.values.username));

                users = null;
                selected_permissions = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runAssignUserDialog(e, instance, user, prev_permissions) {
    return UI.dialog(e, T.format(T.permissions_of_x_on_x, user.username, instance.key), {}, (d, resolve, reject) => {
        d.section(T.project, () => {
            let props = listPermissions('build_', instance.legacy);
            let value = (instance.master == null) ? prev_permissions.filter(perm => perm.startsWith('build_')) : null;

            d.multiCheck('build_permissions', null, props, {
                value: value,
                disabled: instance.master != null
            });
        }, { color: '#b518bf' });
        d.sameLine(true); d.section(T.data_entry, () => {
            let props = listPermissions('data_', instance.legacy);
            let value = !instance.slaves ? prev_permissions.filter(perm => perm.startsWith('data_')) : null;

            d.multiCheck('data_permissions', null, props, {
                value: value,
                disabled: instance.slaves > 0
            });
        }, { color: '#258264' });
        d.sameLine(true); d.section(T.exports, () => {
            let props = listPermissions('bulk_', instance.legacy);
            let value = !instance.slaves ? prev_permissions.filter(perm => perm.startsWith('bulk_')) : null;

            d.multiCheck('bulk_permissions', null, props, {
                value: value,
                disabled: instance.slaves > 0
            });
        }, { color: '#3364bd' });
        d.sameLine(true); d.section(T.messages, () => {
            let props = listPermissions('message_', instance.legacy);
            let value = !instance.slaves ? prev_permissions.filter(perm => perm.startsWith('message_')) : null;

            d.multiCheck('message_permissions', null, props, {
                value: value,
                disabled: instance.slaves > 0
            });
        }, { color: '#c97f1a' });

        // Now regroup permissions
        let permissions = [
            ...(d.values.build_permissions || []),
            ...(d.values.data_permissions || []),
            ...(d.values.bulk_permissions || []),
            ...(d.values.message_permissions || [])
        ];

        d.action(T.edit, { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/instances/assign', {
                    instance: instance.key,
                    userid: user.userid,
                    permissions: permissions
                });

                resolve();
                if (permissions.length) {
                    Log.success(T.format(T.permissions_of_x_on_x_changed, user.username, instance.key));
                } else {
                    Log.success(T.format(T.permissions_of_x_on_x_deleted, user.username, instance.key));
                }

                selected_permissions = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function listPermissions(prefix, legacy) {
    let props = [];

    for (let perm in ENV.permissions) {
        if (legacy && !ENV.permissions[perm])
            continue;
        if (!perm.startsWith(prefix))
            continue;

        let name = Util.capitalize(perm.substr(prefix.length));
        let prop = [perm, name];

        props.push(prop);
    }

    return props;
}

function runEditUserDialog(e, user) {
    return UI.dialog(e, T.format(T.edit_x, user.username), {}, (d, resolve, reject) => {
        d.pushOptions({ untoggle: false });

        d.tabs('tabs', () => {
            d.tab('Identité', () => {
                d.text('username', T.username, { value: user.username });

                d.text('email', T.email, { value: user.email });
                if (d.values.email != null && !d.values.email.includes('@'))
                    d.error('email', T.invalid_format);
                d.text('phone', T.phone, { value: user.phone });
                if (d.values.phone != null && !d.values.phone.startsWith('+'))
                    d.error('phone', T.invalid_format_use_prefix);

                if (profile.root)
                    d.boolean('*root', T.root, { value: user.root });
            });

            d.tab('Sécurité', () => {
                d.password('password', T.password);
                d.password('password2', null, {
                    placeholder: T.confirmation,
                    help: T.leave_empty_to_ignore,
                    mandatory: d.values.password != null
                });
                d.boolean('change_password', T.require_password_change, {
                    value: d.values.password != null,
                    untoggle: false
                });
                if (d.values.password != null && d.values.password2 != null) {
                    if (d.values.password !== d.values.password2) {
                        d.error('password2', T.password_mismatch);
                    } else if (d.values.password.length < 8) {
                        d.error('password2', T.password_too_short);
                    }
                }
                d.boolean('confirm', T.use_totp, {
                    value: user.confirm, untoggle: false
                });
                d.boolean('reset_secret', T.reset_totp, {
                    value: !user.confirm,
                    disabled: !d.values.confirm,
                    untoggle: false
                });
            });
        });

        d.action(T.edit, { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/users/edit', {
                    userid: user.userid,
                    username: d.values.username,
                    password: d.values.password,
                    change_password: d.values.change_password,
                    confirm: d.values.confirm,
                    reset_secret: d.values.reset_secret,
                    email: d.values.email ?? '',
                    phone: d.values.phone ?? '',
                    root: profile.root ? d.values.root : false
                });

                resolve();
                Log.success(T.format(T.user_x_edited, d.values.username));

                users = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runDeleteUserDialog(e, user) {
    return UI.confirm(e, T.format(T.confirm_user_deletion, user.username),
                         T.delete, async () => {
        await Net.post('/admin/api/users/delete', { userid: user.userid });

        Log.success(T.format(T.user_x_deleted, user.username));

        users = null;

        go();
    });
}

export {
    init,
    go
}
