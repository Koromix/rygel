// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../../web/core/base.js';
import * as UI from '../../../web/flat/ui.js';
import { computeAge, dateToString, loadTexture } from '../lib/util.js';
import { PROXIMITY_LEVELS, LINK_KINDS, PERSON_KINDS } from './constants.js';
import { ASSETS } from '../../assets/assets.js';

const TOOLS = {
    move: { title: 'Déplacer', icon: 'ui/move' },
    rotate: { title: 'Pivoter', icon: 'ui/rotate' },

    link: {
        title: 'Relier',
        icon: 'ui/link',

        modes: LINK_KINDS.reduce((acc, kind, idx) => {
            if (kind.width)
                acc[kind.text] = idx;
            return acc;
        }, {})
    },
};

const PERSON_RADIUS = 0.05;
const LINK_RADIUS = 0.03;
const CIRCLE_MARGIN = 1.8 * PERSON_RADIUS;

const ORIGIN = { x: 0, y: 0 };

function NetworkWidget(mod, world) {
    let self = this;

    // Data
    let persons = world.persons;
    let links = world.links;

    // Platform
    let runner = mod.runner;
    let canvas = runner.canvas;
    let ctx = canvas.getContext('2d');
    let mouse_state = runner.mouseState;
    let pressed_keys = runner.pressedKeys;

    // Cursor position in world space
    let cursor = { x: null, y: null };

    // UI state
    let textures = {};
    let state = {
        pos: { x: 0, y: 0 },
        zoom: 0
    };
    let new_kind = 'family';

    // Interactions
    let active_grab = null;
    let active_tool = 'move';
    let active_edit = null;

    // Selection
    let select_mode = false;
    let select_points = [];
    let select_persons = [];

    // Misc
    let last_link = {
        a2b: 2,
        b2a: 2
    };

    this.init = async function() {
        await Promise.all([
            ...Object.keys(TOOLS).map(async tool => {
                let info = TOOLS[tool];
                textures[info.icon] = await loadTexture(ASSETS[info.icon]);
            }),
            ...Object.keys(PERSON_KINDS).map(async kind => {
                let info = PERSON_KINDS[kind];
                textures[info.icon] = await loadTexture(ASSETS[info.icon]);
            })
        ]);
    };

    this.update = function() {
        // Convert mouse coordinates
        {
            let scale = computeScale(state.zoom);

            cursor.x = (mouse_state.x - canvas.width / 2) / scale - state.pos.x;
            cursor.y = (mouse_state.y - canvas.height / 2) / scale - state.pos.y;
        }

        // Global shortcuts
        if (pressed_keys.ctrl > 0 && pressed_keys.a == -1) {
            select_persons.length = 0;
            select_persons.push(...persons);
        }
        if (pressed_keys.delete == -1)
            deletePersons(select_persons);

        let insert = false;
        for (let i = 1; i < world.subjects.length; i++) {
            if (!world.persons.find(p => p.subject.id == world.subjects[i].id)) {
                insert = true;
                break;
            }
        }

        render(html`
            <button @click=${UI.wrap(createPersons)}>
                <img src=${ASSETS['ui/create']} alt="" />
                <span>Insérer</span>
            </button>
            ${insert ? html`<button @click=${UI.wrap(insertPersons)}>
                <img src=${ASSETS['ui/insert']} alt="" />
                <span>Réutiliser</span>
            </button>` : ''}

            <div style="flex: 1;"></div>
            ${Object.keys(TOOLS).map(tool => {
                let info = TOOLS[tool];
                let active = (active_tool == tool && !select_mode);

                return html`
                    <button class=${active ? 'active' : ''}
                            @click=${UI.wrap(e => switchTool(tool))}>
                        <img src=${ASSETS[info.icon]} alt="" />
                        <span>${info.title}</span>
                    </button>
                `;
            })}

            <div style="height: 30px;"></div>
            <button class=${select_mode && !select_persons.length ? 'active' : ''}
                    title=${select_persons.length >= 2 ? '' : 'Outil de sélection multiple au lasso'}
                    @click=${UI.wrap(toggleSelection)}>
                <img src=${select_persons.length < 2 ? ASSETS['ui/select'] : ASSETS['ui/unselect']} alt="" />
                <span>${select_persons.length < 2 ? 'Sélectionner' : 'Déselectionner'}</span>
            </button>

            <div style="flex: 1;"></div>
            <button class=${mod.anonymous ? 'active' : ''}
                    @click=${mod.anonymous ? UI.confirm('Voulez-vous quitter le mode anonyme ?', e => { mod.anonymous = false; })
                                           : UI.wrap(e => { mod.anonymous = true; })}>
                <img src=${ASSETS['ui/anonymous']} alt="" />
                <span>Mode anonyme</span>
            </button>
        `, mod.toolbox);

        // User is moving around
        if (active_grab != null) {
            if (mouse_state.left > 0) {
                state.pos.x += cursor.x - active_grab.x;
                state.pos.y += cursor.y - active_grab.y;

                if (mouse_state.moving)
                    mouse_state.left = 0;

                runner.cursor = 'grabbing';

                return;
            } else {
                active_grab = null;
            }
        }

        // Handle selection tool
        if (select_mode && active_edit == null) {
            if (mouse_state.left > 0) {
                let p = { x: cursor.x, y: cursor.y };
                let prev = select_points[select_points.length - 1];

                if (prev == null || computeDistance(prev, p) >= 0.01)
                    select_points.push(p);

                if (mouse_state.moving)
                    mouse_state.left = 0;
            } else {
                if (select_points.length > 4) {
                    select_mode = false;
                    selectInPolygon(select_points);
                }
                select_points.length = 0;
            }

            runner.cursor = 'default';
        }

        // Handle active move or rotation
        if (active_edit != null && (active_edit.type == 'move' || active_edit.type == 'rotate')) {
            if (mouse_state.left > 0) {
                if (mouse_state.moving) {
                    mod.registerChange(select_persons, false, true);

                    if (active_edit.type == 'move') {
                        for (let p of select_persons) {
                            if (p == persons[0])
                                continue;

                            p.x += cursor.x - active_edit.x;
                            p.y += cursor.y - active_edit.y;
                        }
                    } else if (active_edit.type == 'rotate') {
                        let rotate = Math.atan2(-cursor.y, cursor.x) - Math.atan2(-active_edit.y, active_edit.x);
                        let expand = 1.0;

                        if (pressed_keys.ctrl)
                            expand = computeDistance(ORIGIN, cursor) / computeDistance(ORIGIN, active_edit);

                        for (let p of select_persons) {
                            if (p == persons[0])
                                continue;

                            let angle = Math.atan2(-p.y, p.x);
                            let distance = computeDistance(ORIGIN, p);

                            p.x = distance * expand * Math.cos(angle + rotate);
                            p.y = distance * expand * -Math.sin(angle + rotate);
                        }
                    }

                    active_edit.x = cursor.x;
                    active_edit.y = cursor.y;
                    active_edit.moved = true;

                    mouse_state.left = 0;
                }

                runner.cursor = 'grabbing';
            } else {
                if (active_edit.moved) {
                    mod.registerChange(select_persons, false, true);

                    for (let p of select_persons) {
                        if (p == persons[0])
                            continue;

                        snapPerson(p);
                    }
                }

                active_edit = null;
            }
        }

        // Handle active proximity resize
        if (active_edit != null && active_edit.type == 'proximity') {
            if (mouse_state.left > 0) {
                if (mouse_state.moving) {
                    let levels = world.levels.slice(1, active_edit.proximity + 1);
                    let min = Math.max(
                        world.levels[(active_edit.proximity + 1) % world.levels.length].radius + CIRCLE_MARGIN,
                        ...world.persons.filter(p => p.proximity == active_edit.proximity).map(p => computeDistance(ORIGIN, p) + CIRCLE_MARGIN)
                    );

                    let distance = computeDistance(ORIGIN, cursor);
                    let change = distance - active_edit.radius;

                    let allow = levels.every(level => level.radius + change >= min);

                    if (allow) {
                        let persons = world.persons.filter(p => p.proximity < active_edit.proximity && p != world.persons[0]);

                        mod.registerChange([...levels, ...persons], false, true);

                        for (let level of levels)
                            level.radius += change;

                        for (let p of persons) {
                            let distance = computeDistance(ORIGIN, p);
                            let angle = Math.atan2(-p.y, p.x);

                            p.x = (distance + change) * Math.cos(angle);
                            p.y = (distance + change) * -Math.sin(angle);
                        }

                        active_edit.radius = distance;
                        active_edit.moved = true;

                        mouse_state.left = 0;
                    }
                }

                runner.cursor = 'grabbing';
            } else {
                active_edit = null;
            }
        }

        // Handle active link creation
        if (active_edit != null && active_edit.type == 'link') {
            if (mouse_state.left > 0) {
                active_edit.x = cursor.x;
                active_edit.y = cursor.y;
            } else {
                let [target, idx] = findPerson(cursor);

                if (idx >= 0) {
                    let auto = false;

                    for (let p of select_persons) {
                        if (p == target)
                            continue;

                        let link = {
                            id: Util.makeULID(),

                            a: p.id,
                            b: target.id,
                            a2b: active_edit.a2b,
                            b2a: active_edit.b2a
                        };

                        if (link.a > link.b)
                            [link.a, link.b] = [link.b, link.a];

                        let prev_link = links.find(it => it.a == link.a && it.b == link.b);

                        if (prev_link != null) {
                            mod.registerChange(prev_link, auto);

                            prev_link.a2b = link.a2b;
                            prev_link.b2a = link.b2a;
                        } else {
                            links.push(link);
                            mod.registerPush(links, link, auto);
                        }

                        auto = true;
                    }
                }

                active_edit = null;
            }

            if (mouse_state.moving)
                mouse_state.left = 0;
        }

        // Start move or rotation interaction
        if (active_edit == null && (active_tool == 'move' || active_tool == 'rotate')) {
            let [target, idx] = findPerson(cursor);

            if (idx > 0) {
                if (mouse_state.left > 0) {
                    if (!select_persons.includes(target)) {
                        select_persons.length = 0;
                        select_persons.push(target);
                    }

                    active_edit = {
                        type: active_tool,
                        x: cursor.x,
                        y: cursor.y,
                        moved: false
                    };
                }

                runner.cursor = 'grab';
            }
        }

        // Start proximity resize interaction
        if (active_edit == null) {
            let [target, idx] = findLevel(cursor);

            if (idx > 0) {
                if (mouse_state.left > 0) {
                    active_edit = {
                        type: 'proximity',
                        proximity: idx,
                        radius: target.radius,
                        moved: false
                    };
                }

                let angle = Math.atan2(-cursor.y, cursor.x);
                let align = angleToAlign(angle);

                switch (align) {
                    case 4:
                    case 6: { runner.cursor = 'ew-resize'; } break;
                    case 2:
                    case 8: { runner.cursor = 'ns-resize'; } break;
                    case 1:
                    case 9: { runner.cursor = 'nesw-resize'; } break;
                    case 3:
                    case 7: { runner.cursor = 'nwse-resize'; } break;
                }
            }
        }

        // Start link interaction
        if (active_edit == null && active_tool == 'link') {
            let [target, idx] = findPerson(cursor);

            if (idx >= 0) {
                if (mouse_state.left > 0) {
                    if (!select_persons.includes(target)) {
                        select_persons.length = 0;
                        select_persons.push(target);
                    }

                    active_edit = {
                        type: 'link',
                        x: cursor.x,
                        y: cursor.y,
                        a2b: last_link.a2b,
                        b2a: last_link.b2a
                    };
                }

                if (mouse_state.moving)
                    mouse_state.left = 0;
            }
        }

        // Click on stuff to edit
        {
            let target = null;
            let idx = null;

            if ([target, idx] = findLink(cursor), idx >= 0) {
                if (!mod.anonymous) {
                    let a = persons.find(it => it.id == target.a);
                    let b = persons.find(it => it.id == target.b);

                    let tooltip = `${namePerson(a, true)} et ${namePerson(b, true)}`;
                    mod.tooltip(tooltip);
                }

                if (mouse_state.left == -1) {
                    changeLink(target);
                } else if (mouse_state.right == -1) {
                    deleteLink(target);
                }
            } else if ([target, idx] = findPerson(cursor), idx >= 0) {
                let tooltip = namePerson(target, true);

                if (tooltip)
                    mod.tooltip(tooltip);

                if (mouse_state.left == -1) {
                    if (idx > 0)
                        changePerson(target);
                } else if (idx > 0 && mouse_state.right == -1) {
                    deletePersons(target);
                }
            }

            if (target != null)
                runner.cursor = 'pointer';
        }

        // Allow user to roam around
        if (mouse_state.left > 0 && mouse_state.moving && active_edit == null) {
            active_grab = {
                x: cursor.x,
                y: cursor.y
            };
        } else {
            active_grab = null;
        }
        if (mouse_state.left == -1)
            select_persons.length = 0;

        // Handle zoom
        if (mouse_state.wheel < 0) {
            zoom(1, cursor);
        } else if (mouse_state.wheel > 0) {
            zoom(-1, cursor);
        }
    };

    function switchTool(tool) {
        active_tool = tool;
        active_edit = null;

        select_mode = false;
    }

    function toggleSelection() {
        if (select_persons.length < 2)
            select_mode = !select_mode;

        select_points.length = 0;
        select_persons.length = 0;
    }

    function selectInPolygon(points) {
        select_persons.length = 0;

        for (let p of persons) {
            if (isInsidePolygon(p, points))
                select_persons.push(p);
        }
    }

    async function createPersons() {
        let new_subjects = [createSubject(new_kind)];

        await UI.dialog({
            run: (render, close) => {
                let disabled = !new_subjects.length;

                return html`
                    <div class="title">
                        Ajout de personnes au sociogramme
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                    </div>

                    <div class="main">
                        <div class="header">
                            <select @change=${UI.wrap(e => { new_kind = e.target.value; render(); })}>
                                ${Object.keys(PERSON_KINDS).map(kind => {
                                    let info = PERSON_KINDS[kind];
                                    let active = (kind == new_kind);

                                    return html`<option value=${kind} ?selected=${active}>${info.text}</option>`;
                                })}
                            </select>
                            <div style="flex: 1;"></div>
                            <button type="button" class="secondary small" @click=${UI.wrap(add_subject)}>Ajouter</button>
                        </div>
                        <table style="table-layout: fixed;">
                            <colgroup>
                                <col/>
                                <col class="check"/>
                            </colgroup>

                            <thead>
                                <th>Identité</th>
                                <th></th>
                            <thead>

                            <tbody>
                                ${new_subjects.map(subject => html`
                                    <tr>
                                        <td><input type="text" .value=${subject.name} @change=${UI.wrap(e => { subject.name = e.target.value.trim(); render() })} /></td>
                                        <td><button type="button" class="secondary small"
                                                    @click=${UI.insist(e => { new_subjects = new_subjects.filter(it => it !== subject); render(); })}><img src=${ASSETS['ui/delete']} alt="Supprimer" /></button></td>
                                    </tr>
                                `)}
                                ${!new_subjects.length ? html`<td colspan="5" class="center">Cliquez sur le bouton d'ajout ci-dessus.</td>` : ''}
                            </tbody>
                        </table>
                    </div>

                    <div class="footer">
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                        <button type="submit" ?disabled=${disabled}>Ajouter</button>
                    </div>
                `;

                function add_subject() {
                    let subject = createSubject(null);
                    new_subjects.push(subject);
                    render();
                }
            },

            submit: (elements) => {
                let auto = false;

                if (new_subjects.some(subject => !subject.name))
                    throw new Error('Identités manquantes');

                for (let subject of new_subjects) {
                    subject.last_kind = new_kind;

                    world.subjects.push(subject);

                    mod.registerPush(world.subjects, subject);
                    auto = true;

                    createPerson(subject, auto);
                }
            }
        });
    }

    function createSubject(kind) {
        let subject = {
            id: Util.makeULID(),

            name: '',
            last_kind: kind
        };

        return subject;
    }

    async function insertPersons() {
        let reuse_subjects = world.subjects.filter(subject => {
            if (!subject.name)
                return false;
            if (persons.some(p => p.subject == subject.id))
                return false;

            return true;
        });
        let reuse_map = new Map;
        let reuse_changes = new WeakMap;

        await UI.dialog({
            run: (render, close) => {
                let disabled = !reuse_subjects.length;

                return html`
                    <div class="title">
                        Ajout de personnes au sociogramme
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                    </div>

                    <div class="main">
                        <div class="header">Sujets existants</div>
                        <table style="table-layout: fixed;">
                            <colgroup>
                                <col class="check"/>
                                <col/>
                                <col/>
                                <col/>
                                <col/>
                            </colgroup>

                            <thead>
                                <tr>
                                    <th></th>
                                    <th>Identité</th>
                                    <th>Type</th>
                                    <th>Âge</th>
                                </tr>
                            </thead>

                            <tbody>
                                ${reuse_subjects.map(subject => {
                                    let changes = reuse_changes.get(subject);

                                    if (changes == null) {
                                        changes = {};
                                        reuse_changes.set(subject, changes);
                                    }

                                    let values = {
                                        kind: (changes.kind != null) ? changes.kind : subject.last_kind
                                    };

                                    return html`
                                        <tr class="item" @click=${UI.wrap(e => toggle_subject(subject))}>
                                            <td><input type="checkbox" style="pointer-events: none;"
                                                       ?checked=${reuse_map.has(subject.id)} /></td>
                                            <td>${subject.name}</td>
                                            <td>
                                                <select @change=${UI.wrap(e => { changes.kind = e.target.value; render(); })}
                                                        @click=${e => e.stopPropagation()}>
                                                    ${Object.keys(PERSON_KINDS).map(kind => {
                                                        let info = PERSON_KINDS[kind];
                                                        return html`<option value=${kind} .selected=${kind == values.kind}>${info.text}</option>`;
                                                    })}
                                                </select>
                                            </td>
                                            <td class="missing">Inconnu</td>
                                        </tr>
                                    `;
                                })}
                                ${!reuse_subjects.length ? html`<td colspan="5" class="center">Les personnes connues et nommées auparavant sont déjà dans ce sociogramme.</td>` : ''}
                            </tbody>
                        </table>
                    </div>

                    <div class="footer">
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                        <button type="submit" ?disabled=${disabled}>Ajouter</button>
                    </div>
                `;

                function toggle_subject(subject) {
                    if (!reuse_map.delete(subject.id))
                        reuse_map.set(subject.id, subject);

                    render();
                }
            },

            submit: (elements) => {
                let auto = false;

                for (let subject of reuse_map.values()) {
                    let changes = reuse_changes.get(subject);

                    let p = createPerson(subject, auto);
                    Object.assign(p, changes);

                    auto = true;
                }
            }
        });
    }

    function createPerson(subject, auto = false) {
        let p = {
            id: Util.makeULID(),

            subject: subject.id,
            kind: subject.last_kind,

            x: null,
            y: null,
            proximity: 0
        };

        // Find a good position
        {
            let scale = computeScale(state.zoom);

            for (let i = 0; i < 100; i++) {
                let angle = Util.getRandomFloat(0, Math.PI * 2);
                let distance = world.levels[1].radius + CIRCLE_MARGIN + Util.getRandomFloat(0, 0.3);

                p.x = distance * Math.cos(angle);
                p.y = distance * -Math.sin(angle);

                let mx = scale * (state.pos.x + p.x) + canvas.width / 2;
                let my = scale * (state.pos.y + p.y) + canvas.height / 2;

                if (mx >= PERSON_RADIUS && mx <= canvas.width - PERSON_RADIUS &&
                        my >= PERSON_RADIUS && my <= canvas.height - PERSON_RADIUS)
                    break;
            }
        }

        persons.push(p);

        mod.registerPush(persons, p, auto);

        return p;
    }

    function snapPerson(p) {
        let distance = computeDistance(ORIGIN, p);
        let angle = Math.atan2(-p.y, p.x);

        for (let i = PROXIMITY_LEVELS.length - 1; i >= 1; i--) {
            let min_distance = world.levels[(i + 1) % PROXIMITY_LEVELS.length].radius;
            let max_distance = world.levels[i].radius;

            if (distance <= max_distance) {
                distance = Util.clamp(distance, min_distance + CIRCLE_MARGIN, max_distance - CIRCLE_MARGIN);

                p.x = distance * Math.cos(angle);
                p.y = distance * -Math.sin(angle);
                p.proximity = i;

                return;
            }
        }

        let min_distance = world.levels[0].radius + CIRCLE_MARGIN;

        if (distance < min_distance) {
            p.x = min_distance * Math.cos(angle);
            p.y = min_distance * -Math.sin(angle);
        }
        p.proximity = 0;
    }

    async function changePerson(p) {
        let subject = world.subjects.find(subject => subject.id == p.subject);

        await UI.dialog({
            run: (render, close) => html`
                <div class="title">
                    Modifier la personne ${namePerson(p, false)}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>Identité</span>
                        <input name="name" value=${subject.name} />
                    </label>
                    <label>
                        <span>Catégorie</span>
                        <select name="kind">
                            ${Object.keys(PERSON_KINDS).map(kind => {
                                let info = PERSON_KINDS[kind];
                                let active = (kind == p.kind);

                                return html`<option value=${kind} ?selected=${active}>${info.text}</option>`;
                            })}
                        </select>
                    </label>
                </div>

                <div class="footer">
                    <button type="button" class="danger" @click=${UI.insist(e => { deletePersons(p); close(); })}>Supprimer</button>
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                    <button type="submit">Appliquer</button>
                </div>
            `,

            submit: (elements) => {
                mod.registerChange([p, subject]);

                p.kind = elements.kind.value;

                subject.name = elements.name.value.trim();
                subject.last_kind = p.kind;
            }
        });
    }

    function deletePersons(targets) {
        if (!Array.isArray(targets))
            targets = [targets];

        let auto = false;

        for (let p of targets) {
            let idx = persons.indexOf(p);

            console.log(p, idx);

            if (idx <= 0)
                continue;

            persons.splice(idx, 1);
            mod.registerRemove(persons, idx, p, auto);

            auto = true;
        }

        if (auto)
            cleanUpLinks(true);
    }

    async function changeLink(link) {
        let a = persons.find(it => it.id == link.a);
        let b = persons.find(it => it.id == link.b);

        await UI.dialog({
            run: (render, close) => html`
                <div class="title">
                    Modifier le lien entre ${namePerson(a, false)} et ${namePerson(b, false)}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>${namePerson(a, false)} à ${namePerson(b, false)}</span>
                        <select name="a2b">
                            ${LINK_KINDS.map((kind, idx) => html`<option value=${idx} ?selected=${link.a2b == idx}>${kind.text}</option>`)}
                        </select>
                    </label>

                    <label>
                        <span>${namePerson(b, false)} à ${namePerson(a, false)}</span>
                        <select name="b2a">
                            ${LINK_KINDS.map((kind, idx) => html`<option value=${idx} ?selected=${link.b2a == idx}>${kind.text}</option>`)}
                        </select>
                    </label>
                </div>

                <div class="footer">
                    <button type="button" class="danger" @click=${UI.insist(e => { deleteLink(link); close(); })}>Supprimer</button>
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                    <button type="submit">Appliquer</button>
                </div>
            `,

            submit: (elements) => {
                let a2b = parseInt(elements.a2b.value, 10);
                let b2a = parseInt(elements.b2a.value, 10);

                if (LINK_KINDS[a2b].width || LINK_KINDS[b2a].width) {
                    mod.registerChange(link);

                    link.a2b = a2b;
                    link.b2a = b2a;

                    Object.assign(last_link, link);
                } else {
                    deleteLink(link);
                }
            }
        });
    }

    function deleteLink(link) {
        let idx = links.indexOf(link);

        links.splice(idx, 1);
        mod.registerRemove(links, idx, link);
    }

    function namePerson(p, details) {
        if (mod.anonymous)
            return '';

        if (p == persons[0]) {
            return 'vous';
        } else {
            let subject = world.subjects.find(subject => subject.id == p.subject);
            let kind = PERSON_KINDS[p.kind];

            let text = subject.name + (details ? ` [${kind.text}]` : '');
            return text;
        }
    }

    // Ray-casting algorithm
    function isInsidePolygon(at, points) {
        let collisions = 0;

        let x = points.reduce((acc, p) => Math.min(acc, p.x), Infinity) - 2;
        let y = points.reduce((acc, p) => Math.min(acc, p.y), Infinity) - 2;

        for (let i = 0; i < points.length; i++) {
            let s1 = points[i];
            let s2 = points[(i + 1) % points.length];

            collisions += testIntersect(s1.x, s1.y, s2.x, s2.y, x, y, at.x, at.y);
        }

        return (collisions & 1) ? true : false;
    }

    function testIntersect(x0, y0, x1, y1, x2, y2, x3, y3) {
        let s1x = x1 - x0;
        let s1y = y1 - y0;
        let s2x = x3 - x2;
        let s2y = y3 - y2;

        let s = (-s1y * (x0 - x2) + s1x * (y0 - y2)) / (-s2x * s1y + s1x * s2y);
        let t = (s2x * (y0 - y2) - s2y * (x0 - x2)) / (-s2x * s1y + s1x * s2y);

        if (isNaN(s) || isNaN(t))
            return false;

        return (s >= 0 && s <= 1 && t >= 0 && t <= 1);
    }

    function zoom(delta, at) {
        if (state.zoom + delta < -16 || state.zoom + delta > 16)
            return;

        let scale1 = computeScale(state.zoom);
        let scale2 = computeScale(state.zoom + delta);

        let transformed = {
            x: at.x * scale1 + state.pos.x * scale1,
            y: at.y * scale1 + state.pos.y * scale1
        };

        state.pos.x = state.pos.x - transformed.x / scale1 + transformed.x / scale2;
        state.pos.y = state.pos.y - transformed.y / scale1 + transformed.y / scale2;

        state.zoom += delta;
    }

    function findPerson(at) {
        let idx = persons.findIndex(p => {
            let distance = computeDistance(at, p);
            let radius = computeRadius(p);

            return distance <= radius;
        });

        if (idx >= 0) {
            return [persons[idx], idx];
        } else {
            return [null, -1];
        }
    }

    function findLink(at) {
        let idx = links.findIndex(link => {
            let a = persons.find(it => it.id == link.a);
            let b = persons.find(it => it.id == link.b);

            let [from, to] = computeLinkCoordinates(a, b, 0);
            let p = { x: (from.x + to.x) / 2, y: (from.y + to.y) / 2 };
            let distance = computeDistance(at, p);

            return distance <= LINK_RADIUS;
        });

        if (idx >= 0) {
            return [links[idx], idx];
        } else {
            return [null, -1];
        }
    }

    function findLevel(at) {
        let distance = computeDistance(ORIGIN, at);

        for (let i = 1; i < PROXIMITY_LEVELS.length; i++) {
            let level = world.levels[i];
            let delta = Math.abs(distance - level.radius);

            if (delta <= 0.02)
                return [level, i];
        }

        return [null, -1];
    }

    function cleanUpLinks(auto) {
        let removed = [];

        let j = 0;
        for (let i = 0; i < links.length; i++) {
            let link = links[i];

            let a = persons.find(p => p.id == link.a);
            let b = persons.find(p => p.id == link.b);

            links[j] = links[i];

            if (a == null || b == null) {
                let remove = {
                    idx: i,
                    link: link
                };

                removed.push(remove);
            } else {
                j++;
            }
        }
        links.length = j;

        for (let i = removed.length - 1; i >= 0; i--) {
            let remove = removed[i];
            mod.registerRemove(links, remove.idx, remove.link, auto);
        }
    }

    this.draw = function() {
        ctx.save();

        // Transform from world space to screen space
        {
            let scale = computeScale(state.zoom);

            ctx.translate(canvas.width / 2, canvas.height / 2);
            ctx.scale(scale, scale);
            ctx.translate(state.pos.x, state.pos.y);
        }

        // Draw concentric proximity circles
        {
            ctx.save();

            ctx.fillStyle = '#0000001a';
            ctx.strokeStyle = '#00000033';
            ctx.lineWidth = 0.01;
            ctx.setLineDash([0.02, 0.02]);

            for (let i = 1; i < PROXIMITY_LEVELS.length; i++) {
                let level = PROXIMITY_LEVELS[i];
                let radius = world.levels[i].radius;

                ctx.beginPath();
                ctx.arc(0, 0, radius, 0, Math.PI * 2, false);
                ctx.fill();
                ctx.stroke();
            }

            ctx.font = '13px Open Sans';
            ctx.fillStyle = '#777777';

            for (let i = 1; i < PROXIMITY_LEVELS.length; i++) {
                let level = PROXIMITY_LEVELS[i];
                let radius = world.levels[i].radius;

                runner.text(0, radius - 0.03, level.text, { align: 2 });
            }

            ctx.restore();
        }

        // Draw links
        {
            ctx.save();

            for (let link of links) {
                let a = persons.find(p => p.id == link.a);
                let b = persons.find(p => p.id == link.b);
                let a2b = LINK_KINDS[link.a2b];
                let b2a = LINK_KINDS[link.b2a];

                let offset = (b2a.width - a2b.width) / 2;

                if (a2b.width) {
                    ctx.lineWidth = a2b.width;
                    ctx.fillStyle = a2b.color;
                    ctx.strokeStyle = a2b.color + 'dd';

                    drawHalfLink(a, b, a2b.width / 1.6 + offset);
                }
                if (b2a.width) {
                    ctx.lineWidth = b2a.width;
                    ctx.fillStyle = b2a.color;
                    ctx.strokeStyle = b2a.color + 'dd';

                    drawHalfLink(b, a, b2a.width / 1.6 - offset);
                }

                // Draw center
                {
                    let [from, to] = computeLinkCoordinates(a, b, 0);
                    let angle = Math.atan2(from.y - to.y, to.x - from.x);

                    let p = { x: (from.x + to.x) / 2, y: (from.y + to.y) / 2 };
                    let split = (b2a.width && b2a != a2b);

                    ctx.fillStyle = a2b.color;

                    ctx.beginPath();
                    ctx.arc(p.x, p.y, LINK_RADIUS, Math.PI - angle, Math.PI - angle + (split ? Math.PI : Math.PI * 2), false);
                    ctx.fill();

                    if (split) {
                        ctx.fillStyle = b2a.color;

                        ctx.beginPath();
                        ctx.arc(p.x, p.y, LINK_RADIUS, -angle, -angle + Math.PI, false);
                        ctx.fill();
                    }
                }
            }

            ctx.restore();
        }

        // Draw persons
        for (let i = persons.length - 1; i >= 0; i--) {
            let p = persons[i];

            let radius = computeRadius(p);
            let color = computeColor(p);

            ctx.fillStyle = color;

            if (select_persons.includes(p)) {
                ctx.strokeStyle = '#00000088';
                ctx.lineWidth = 0.01;

                ctx.setLineDash([0.01, 0.01])
                ctx.lineDashOffset = runner.drawCounter / 1200;
            } else {
                ctx.strokeStyle = color + '22';
                ctx.lineWidth = 0.005;

                ctx.setLineDash([]);
            }

            ctx.beginPath();
            ctx.arc(p.x, p.y, radius + 0.005, 0, Math.PI * 2, false);
            ctx.stroke();
            ctx.fill();

            if (i && p.proximity) {
                let info = PERSON_KINDS[p.kind];

                let icon = textures[info.icon];
                let size = radius + 0.03;

                ctx.drawImage(icon, p.x - size / 2, p.y - size / 2, size, size);
            }
        }

        // Name persons
        if (!mod.anonymous) {
            ctx.font = '12px Open Sans';
            ctx.fillStyle = '#000000';

            for (let i = persons.length - 1; i >= 0; i--) {
                let p = persons[i];
                let name = namePerson(p, false);

                if (!name)
                    continue;

                if (i) {
                    let radius = computeRadius(p);
                    let angle = i ? Math.atan2(-p.y, p.x) : Math.PI / 2;

                    let x = p.x + (1.2 * radius + 0.02) * Math.cos(angle);
                    let y = p.y + (1.2 * radius + 0.02) * -Math.sin(angle);
                    let align = angleToAlign(angle);

                    runner.text(x, y, name, { align: align, background: '#ffffffbb' });
                } else {
                    runner.text(p.x, p.y, name, { align: 5 });
                }
            }
        }

        // Draw partial links
        if (active_edit != null && active_edit.type == 'link') {
            ctx.save();

            let kind = LINK_KINDS[active_edit.a2b];

            ctx.strokeStyle = kind.color + '88';
            ctx.lineWidth = kind.width;
            ctx.setLineDash([0.01, 0.01]);

            for (let p of select_persons) {
                ctx.beginPath();
                ctx.moveTo(p.x, p.y);
                ctx.lineTo(active_edit.x, active_edit.y);
                ctx.stroke();
            }

            ctx.restore();
        }

        // Draw selection polygon
        if (select_points.length) {
            ctx.save();

            ctx.strokeStyle = '#000000';
            ctx.lineWidth = 0.005;
            ctx.setLineDash([0.01, 0.01]);
            ctx.lineDashOffset = runner.drawCounter / 1200;

            ctx.beginPath();
            ctx.moveTo(select_points[0].x, select_points[0].y);
            for (let i = 1; i < select_points.length; i++) {
                let p = select_points[i];
                ctx.lineTo(p.x, p.y);
            }
            ctx.stroke();

            ctx.restore();
        }

        ctx.restore();
    };

    function drawHalfLink(a, b, offset, arrow = true) {
        let correct = 1.1 * ctx.lineWidth;
        let [from, to] = computeLinkCoordinates(a, b, offset, 0.02, 0.02 + correct);

        ctx.beginPath();
        ctx.moveTo(from.x, from.y);
        ctx.lineTo(to.x, to.y);
        if (arrow) {
            let angle = Math.atan2(a.y - b.y, b.x - a.x);

            let x = to.x + 0.05 * Math.cos(angle + 3 * Math.PI / 4);
            let y = to.y - 0.05 * Math.sin(angle + 3 * Math.PI / 4);

            ctx.lineTo(x, y);
        }
        ctx.stroke();
    }

    function computeLinkCoordinates(a, b, offset, delta1 = 0, delta2 = 0) {
        let angle = Math.atan2(a.y - b.y, b.x - a.x);

        let radius1 = computeRadius(a) + delta1;
        let radius2 = computeRadius(b) + delta2;

        let adjust = {
            x: offset * -Math.sin(angle),
            y: offset * -Math.cos(angle)
        };
        let from = {
            x: a.x + adjust.x + radius1 * Math.cos(angle),
            y: a.y + adjust.y - radius1 * Math.sin(angle)
        };
        let to = {
            x: b.x + adjust.x - radius2 * Math.cos(angle),
            y: b.y + adjust.y + radius2 * Math.sin(angle),
        };

        return [from, to];
    }

    function angleToAlign(angle) {
        let start = -Math.PI / 8;
        let step = Math.PI / 4;

        if (angle >= 0) {
            if (angle < start + 1 * step) {
                return 4;
            } else if (angle < start + 2 * step) {
                return 1;
            } else if (angle < start + 3 * step) {
                return 2;
            } else if (angle < start + 4 * step) {
                return 3;
            } else {
                return 6;
            }
        } else {
            angle = -angle;

            if (angle < start + 1 * step) {
                return 4;
            } else if (angle < start + 2 * step) {
                return 7;
            } else if (angle < start + 3 * step) {
                return 8;
            } else if (angle < start + 4 * step) {
                return 9;
            } else {
                return 6;
            }
        }
    }

    function computeScale(zoom) {
        zoom += 2;

        let scale = Math.min(canvas.width / 2, canvas.height / 2) * Math.exp(zoom / 10);
        return scale;
    }

    function computeRadius(p) {
        if (p == persons[0]) {
            return world.levels[0].radius;
        } else {
            let level = PROXIMITY_LEVELS[p.proximity];
            return PERSON_RADIUS * level.size;
        }
    }

    function computeColor(p) {
        let self = (p == persons[0]);
        return self ? '#ffffff' : '#5687bb';
    }

    function computeDistance(p1, p2) {
        let dx = p1.x - p2.x;
        let dy = p1.y - p2.y;

        return Math.sqrt(dx * dx + dy * dy);
    }
}

export { NetworkWidget }
