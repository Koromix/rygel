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
import { loadTexture } from '../core/misc.js';
import { PROXIMITY_LEVELS, PERSON_KINDS, QUALITY_COLORS, USER_GUIDES } from './constants.js';
import { ASSETS } from '../../assets/assets.js';

const PERSON_RADIUS = 0.05;
const LINK_RADIUS = 0.025;
const CIRCLE_MARGIN = 1.8 * PERSON_RADIUS;
const CIRCLE_HANDLE = 0.015;
const MIN_LINK_WIDTH = 0.003;
const MAX_LINK_WIDTH = 0.01;

const DEFAULT_KIND = 'family';
const DEFAULT_QUALITY = 0;
const DEFAULT_LINK = 0;

const ORIGIN = { x: 0, y: 0 };

function NetworkWidget(app, mod, world) {
    let self = this;

    const UI = app.UI;

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
    let new_persons = new Map;

    // Interactions
    let active_grab = null;
    let active_tool = 'move';
    let active_edit = null;

    // Selection
    let select_lasso = false;
    let select_many = false;
    let select_points = [];
    let select_persons = [];

    // Misc
    let last_link = { quality: DEFAULT_LINK };

    this.init = async function() {
        if (app.identity.picture != null)
            textures.self = await loadTexture(app.identity.picture);

        let promises = Object.keys(PERSON_KINDS).map(async kind => {
            let info = PERSON_KINDS[kind];
            textures[info.icon] = await loadTexture(ASSETS[info.icon]);
        });
        await Promise.all(promises);
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
            select_persons.push(...persons.slice(1));
        }
        if (pressed_keys.ctrl > 0) {
            select_many = true;
        } else if (pressed_keys.ctrl == -1) {
            select_many = false;
        }
        if (pressed_keys.delete == -1)
            deletePersons(select_persons);

        let insert = (active_tool == 'move' || active_tool == 'rotate');

        mod.actions(7, html`
            <button id="net_people" class=${insert ? 'active' : ''}
                    title="Ajouter et déplacer vos relations"
                    @click=${UI.wrap(e => switchTool('move'))}>
                <img src=${ASSETS['ui/people']} alt="" />
                <span>Relations</span>
            </button>
            <button id="net_links" class=${active_tool == 'link' ? 'active' : ''}
                    title="Créer des liens entre les relations"
                    @click=${UI.wrap(e => switchTool('link'))}>
                <img src=${ASSETS['ui/link']} alt="" />
                <span>Liens</span>
            </button>
        `);

        mod.actions(1, html`
            ${insert ? html`
                <button id="net_create" title="Ajouter de nouvelles relations"
                        @click=${UI.wrap(createPersons)}>
                    <img src=${ASSETS['ui/create']} alt="" />
                    <span>Ajouter des personnes</span>
                </button>
            ` : ''}
        `);

        mod.actions(2, html`
            <button class=${'small secondary' + (select_lasso && !select_persons.length ? ' active' : '')}
                    title=${!select_persons.length ? 'Outil de sélection multiple au lasso' : ''}
                    @click=${UI.wrap(toggleLasso)}>
                <img src=${!select_persons.length ? ASSETS['ui/select'] : ASSETS['ui/unselect']} alt="" />
                <span>${!select_persons.length ? 'Sélection au lasso' : 'Déselectionner'}</span>
            </button>
            ${insert ? html`
                <button class=${'small secondary' + (active_tool == 'rotate' ? ' active' : '')}
                        title="Déplacer en pivot plutôt que librement"
                        @click=${UI.wrap(e => switchTool(active_tool == 'move' ? 'rotate' : 'move'))}>
                    <img src=${ASSETS['ui/rotate']} alt="" />
                    <span>Pivoter les éléments</span>
                </button>
            ` : ''}
        `);

        // Help user get started!
        mod.showGuide('addSubjects', USER_GUIDES.addSubjects, { highlight: '#net_create' });

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
        if (select_lasso && active_edit == null) {
            if (mouse_state.left > 0) {
                let p = { x: cursor.x, y: cursor.y };
                let prev = select_points[select_points.length - 1];

                if (prev == null || computeDistance(prev, p) >= 0.01)
                    select_points.push(p);
            } else {
                if (select_points.length > 4) {
                    select_lasso = false;
                    selectInPolygon(select_points);
                }
                select_points.length = 0;
            }

            mouse_state.left = 0;

            runner.cursor = 'default';
        }

        // Handle active move or rotation
        if (active_edit != null && (active_edit.type == 'move' || active_edit.type == 'rotate')) {
            if (mouse_state.left > 0) {
                if (mouse_state.moving) {
                    mod.registerChange(select_persons, false, true);

                    if (active_edit.type == 'move') {
                        for (let p of select_persons) {
                            p.x += cursor.x - active_edit.x;
                            p.y += cursor.y - active_edit.y;
                        }
                    } else if (active_edit.type == 'rotate') {
                        let rotate = Math.atan2(-cursor.y, cursor.x) - Math.atan2(-active_edit.y, active_edit.x);
                        let expand = 1.0;

                        if (pressed_keys.ctrl)
                            expand = computeDistance(ORIGIN, cursor) / computeDistance(ORIGIN, active_edit);

                        for (let p of select_persons) {
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

                    for (let p of select_persons)
                        snapPerson(p);

                    mod.showGuide('editPerson', USER_GUIDES.editPerson);
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

                if (idx > 0) {
                    let auto = false;

                    for (let p of select_persons) {
                        if (p == target)
                            continue;

                        let link = {
                            id: Util.makeULID(),

                            a: p.id,
                            b: target.id,
                            quality: active_edit.quality
                        };

                        if (link.a > link.b)
                            [link.a, link.b] = [link.b, link.a];

                        let prev_link = links.find(it => it.a == link.a && it.b == link.b);

                        if (prev_link != null) {
                            mod.registerChange(prev_link, auto);
                            prev_link.quality = link.quality;
                        } else {
                            links.push(link);
                            mod.registerPush(links, link, auto);

                            (async () => {
                                await mod.showGuide('editLink', USER_GUIDES.editLink)
                                await mod.showGuide('peopleMode', USER_GUIDES.peopleMode, { highlight: '#net_people' });
                            })();
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
        if (active_edit == null && (active_tool == 'move' || active_tool == 'rotate') && !select_many) {
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

                    mouse_state.left = 0;
                }
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
        if (active_edit == null && active_tool == 'link' && !select_many) {
            let [target, idx] = findPerson(cursor);

            if (idx > 0) {
                if (mouse_state.left > 0) {
                    if (!select_persons.includes(target)) {
                        select_persons.length = 0;
                        select_persons.push(target);
                    }

                    active_edit = {
                        type: 'link',
                        x: cursor.x,
                        y: cursor.y,
                        quality: last_link.quality
                    };

                    mouse_state.left = 0;
                }
            }
        }

        // Click on stuff to edit or select
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

                if (!select_many) {
                    if (mouse_state.left == -1) {
                        changeLink(target);
                    } else if (mouse_state.right == -1) {
                        deleteLink(target);
                    }
                }
            } else if ([target, idx] = findPerson(cursor), idx >= 0) {
                let tooltip = namePerson(target, true);

                if (tooltip)
                    mod.tooltip(tooltip);

                if (select_many) {
                    if (mouse_state.left == -1) {
                        if (idx > 0 && !select_persons.includes(target))
                            select_persons.push(target);

                        mouse_state.left = 0;
                    }
                } else {
                    if (mouse_state.left == -1) {
                        if (idx > 0)
                            changePerson(target);
                    } else if (idx > 0 && mouse_state.right == -1) {
                        if (select_persons.includes(target)) {
                            deletePersons(select_persons);
                        } else {
                            deletePersons(target);
                        }
                    }
                }
            }

            if (target != null && target != persons[0])
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
    }

    function toggleLasso() {
        if (select_persons.length) {
            select_lasso = false;
        } else {
            select_lasso = !select_lasso;
        }

        select_points.length = 0;
        select_persons.length = 0;
    }

    function selectInPolygon(points) {
        select_persons.length = 0;

        for (let i = 1; i < persons.length; i++) {
            let p = persons[i];

            if (isInsidePolygon(p, points))
                select_persons.push(p);
        }
    }

    async function createPersons() {
        let text = '';

        for (;;) {
            let names = await UI.dialog({
                run: (render, close) => {
                    text = text.trim().replace(/ *-+ */g, '-');

                    return html`
                        <div class="tabbar">
                            <a class="active">Ajouter des relations</a>
                        </div>

                        <div class="tab">
                            <div class="help">
                                <img src=${ASSETS['pictures/help1']} alt="" />
                                <div>
                                    <p>Commencez par saisir les <b>noms ou libellés</b> de toutes les personnes auxquelles vous pensez. Séparez les par des espaces, des virgules ou des nouvelles lignes.
                                    <p><i>Exemple : « Maman Papa Mitchouk, Apolline »</i>
                                </div>
                            </div>

                            <div class="box">
                                <label>
                                    <span>
                                        Quels sont les noms/libellés des personnes que vous souhaitez ajouter ?
                                        ${UI.safe('Les noms des personnes sont privées.')}
                                    </span>

                                    <textarea rows="7" @input=${e => { text = e.target.value; render(); }}>${text}</textarea>
                                    <div class="tip">Vous pourrez préciser le type de chaque relation dans un second temps.</div>
                                </label>
                            </div>

                            <div class="actions">
                                <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                                <button type="submit" ?disabled=${!text}>Ajouter</button>
                            </div>
                        </div>
                    `;
                },

                submit: (elements) => {
                    let names = text.split(/[, \n]+/).filter(name => name.length);
                    return names;
                }
            });

            try {
                let items = names.map(name => ({
                    name: name,
                    kind: DEFAULT_KIND,
                    quality: DEFAULT_QUALITY
                }));

                await UI.dialog({
                    run: (render, close) => {
                        return html`
                            <div class="tabbar">
                                <a class="active">Ajouter des relations</a>
                            </div>

                            <div class="tab">
                                <div class="help right">
                                    <div>
                                        <p>Pour chacune de ces personnes, indiquez :
                                        <ul>
                                            <li>Le <b>type de relation</b>
                                            <li>La <b>qualité de la relation</b>
                                        </ul>
                                        <p>Validez vos choix pour ajouter ces personnes à votre sociogramme. Vous pourrez toujours <b>modifier vos choix</b> en cliquant sur chaque personne du sociogramme !
                                    </div>
                                    <img src=${ASSETS['pictures/help2']} alt="" />
                                </div>

                                <div class="box">
                                    ${items.map(it => html`
                                        <label style="align-items: stretch;">
                                            <span>${it.name}</span>
                                            <div class="row">
                                                <select @change=${e => { it.kind = e.target.value; render(); }}>
                                                    ${Object.keys(PERSON_KINDS).map(kind => {
                                                        let info = PERSON_KINDS[kind];
                                                        let active = (kind == it.kind);

                                                        return html`<option value=${kind} ?selected=${active}>${info.text}</option>`;
                                                    })}
                                                </select>
                                                <div class="slider net_quality" style="flex: 1;">
                                                    <span>Très mauvaise</span>
                                                    <input type="range" min="-5" max="5" value=${it.quality}
                                                           @change=${e => { it.quality = parseInt(e.target.value, 10); render(); }} />
                                                    <span>Très bonne</span>
                                                </div>
                                            </div>
                                        </label>
                                    `)}
                                </div>

                                <div class="actions">
                                    <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                                    <button type="submit">Ajouter</button>
                                </div>
                            </div>
                        `;
                    },

                    submit: (elements) => {
                        for (let i = 0; i < items.length; i++) {
                            let it = items[i];
                            let subject = createSubject(it.name, it.kind, it.quality);

                            world.subjects.push(subject);
                            mod.registerPush(world.subjects, subject, i > 0);

                            createPerson(subject, true);
                        }
                    }
                });

                break;
            } catch (err) {
                if (err != null)
                    throw err;
            }
        }

        if (world.subjects.length > 1) {
            let step1 = mod.isGuideNeeded('movePersons');

            if (step1) {
                await mod.showGuide('movePersons', USER_GUIDES.movePersons);
            } else {
                await mod.showGuide('addMore', USER_GUIDES.addMore);
                await mod.showGuide('createLinks', USER_GUIDES.createLinks, { highlight: '#net_links' });
            }
        }
    }

    function createSubject(name, kind, quality) {
        let subject = {
            id: Util.makeULID(),

            name: name,
            last_kind: kind,
            last_quality: quality
        };

        return subject;
    }

    function createPerson(subject, auto = false) {
        let p = {
            id: Util.makeULID(),

            subject: subject.id,
            kind: subject.last_kind,
            quality: subject.last_quality,

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
        new_persons.set(p, runner.drawCounter);

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
        let name = namePerson(p, false);

        await UI.dialog({
            run: (render, close) => html`
                <div class="tabbar">
                    <a class="active">Modifier la relation ${name}</a>
                </div>

                <div class="tab">
                    <div class="box">
                        <label>
                            <span>Nom ou libellé de la personne</span>
                            <input name="name" value=${subject.name} />
                        </label>
                        <label>
                            <span>Quel type de relation avez-vous avec cette personne ?</span>
                            <select name="kind">
                                ${Object.keys(PERSON_KINDS).map(kind => {
                                    let info = PERSON_KINDS[kind];
                                    let active = (kind == p.kind);

                                    return html`<option value=${kind} ?selected=${active}>${info.text}</option>`;
                                })}
                            </select>
                        </label>
                        <label>
                            <span>Comment évaluez-vous cette relation ?</span>
                            <div class="slider net_quality">
                                <span>Très mauvaise</span>
                                <input name="quality" type="range" min="-5" max="5" value=${p.quality} />
                                <span>Très bonne</span>
                            </div>
                        </label>
                    </div>
                </div>

                <div class="actions">
                    <button type="button" class="secondary"
                            @click=${UI.confirm(`Supprimer la relation ${name}`, e => { deletePersons(p); close(); })}>Supprimer</button>
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                    <button type="submit">Appliquer</button>
                </div>
            `,

            submit: (elements) => {
                let name = elements.name.value.trim();

                if (!name)
                    throw new Error('Le libellé ne peut pas être vide');

                mod.registerChange([p, subject]);

                p.kind = elements.kind.value;
                p.quality = parseInt(elements.quality.value, 10);

                subject.name = name;
                subject.last_kind = p.kind;
                subject.last_quality = p.quality;
            }
        });

        await mod.showGuide('addMore', USER_GUIDES.addMore);
    }

    function deletePersons(targets) {
        if (!Array.isArray(targets))
            targets = [targets];

        let auto = false;

        for (let p of targets) {
            let idx = persons.indexOf(p);

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
                <div class="tabbar">
                    <a class="active">Modifier le lien entre ${namePerson(a, false)} et ${namePerson(b, false)}</a>
                </div>

                <div class="tab">
                    <div class="box">
                        <label>
                            <span>Comment évaluez-vous cette relation ?</span>
                            <div class="slider net_quality">
                                <span>Très mauvaise</span>
                                <input name="quality" type="range" min="-5" max="5" value=${link.quality} />
                                <span>Très bonne</span>
                            </div>
                        </label>
                    </div>

                    <div class="actions">
                        <button type="button" class="secondary"
                                @click=${UI.insist(e => { deleteLink(link); close(); })}>Supprimer</button>
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                        <button type="submit">Appliquer</button>
                    </div>
                </div>
            `,

            submit: (elements) => {
                mod.registerChange(link);

                link.quality = parseInt(elements.quality.value, 10);

                Object.assign(last_link, link);
            }
        });
    }

    function deleteLink(link) {
        let idx = links.indexOf(link);

        links.splice(idx, 1);
        mod.registerRemove(links, idx, link);
    }

    function namePerson(p, details) {
        if (mod.anonymous || p == persons[0])
            return '';

        let subject = world.subjects.find(subject => subject.id == p.subject);
        let kind = PERSON_KINDS[p.kind];

        let text = subject.name + (details ? ` [${kind.text}]` : '');
        return text;
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
            let radius = computePersonRadius(p);

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

            let [from, to] = computeLinkCoordinates(a, b);
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
        for (let i = 1; i < PROXIMITY_LEVELS.length; i++) {
            let level = world.levels[i];

            let handle = { x: level.radius, y: 0 };
            let distance = computeDistance(handle, at);

            if (distance <= 0.02)
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

            ctx.fillStyle = '#0000000b';
            ctx.strokeStyle = '#44444433';
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

            ctx.fillStyle = '#aaaaaa';

            for (let i = 1; i < PROXIMITY_LEVELS.length; i++) {
                let level = PROXIMITY_LEVELS[i];
                let radius = world.levels[i].radius;

                ctx.beginPath();
                ctx.arc(radius, 0, CIRCLE_HANDLE, 0, Math.PI * 2, false);
                ctx.fill();
            }

            ctx.font = `bold ${(20 + state.zoom) * window.devicePixelRatio}px Open Sans`;
            ctx.fillStyle = '#77777755';

            for (let i = 1; i < PROXIMITY_LEVELS.length; i++) {
                let level = PROXIMITY_LEVELS[i];
                let next = (i + 1) % PROXIMITY_LEVELS.length;

                let radius0 = world.levels[i].radius;
                let radius1 = world.levels[next].radius;
                let radius = (radius0 + 1.2 * radius1) / 2.2;

                runner.text(0, radius, level.text, { align: 2 });
            }

            ctx.restore();
        }

        // Draw self
        {
            ctx.save();

            ctx.fillStyle = '#ffffff';

            let radius = world.levels[0].radius;

            ctx.beginPath();
            ctx.arc(0, 0, radius, 0, Math.PI * 2, false);
            ctx.fill();

            if (textures.self != null) {
                ctx.clip();
                ctx.drawImage(textures.self, 0 - radius, 0 - radius, radius * 2, radius * 2);
            }

            ctx.restore();
        }

        // Draw web
        {
            ctx.save();

            ctx.strokeStyle = '#00000033';
            ctx.lineWidth = 0.002;
            ctx.setLineDash([0.01, 0.01]);

            for (let i = 1; i < persons.length; i++) {
                let p = persons[i];

                let [from, to] = computeLinkCoordinates(persons[0], persons[i]);

                ctx.beginPath();
                ctx.moveTo(from.x, from.y);
                ctx.lineTo(to.x, to.y);
                ctx.stroke();
            }

            ctx.restore();
        }

        // Draw links
        {
            ctx.save();

            for (let link of links) {
                let a = persons.find(p => p.id == link.a);
                let b = persons.find(p => p.id == link.b);

                let [color, width] = computeLinkStyle(link.quality);

                let [from, to] = computeLinkCoordinates(a, b, 0.02);
                let angle = Math.atan2(from.y - to.y, to.x - from.x);

                // Draw arrows
                {
                    ctx.lineWidth = width;
                    ctx.fillStyle = color;
                    ctx.strokeStyle = color + 'dd';

                    ctx.beginPath();
                    ctx.moveTo(from.x, from.y);
                    ctx.lineTo(to.x, to.y);
                    ctx.stroke();

                    drawTip(from, angle + Math.PI);
                    drawTip(to, angle);
                }

                // Draw center
                {
                    let [from, to] = computeLinkCoordinates(a, b);
                    let distance = computeDistance(from, to);
                    let angle = Math.atan2(from.y - to.y, to.x - from.x);

                    let p = { x: (from.x + to.x) / 2, y: (from.y + to.y) / 2 };
                    let radius = Math.min(distance / 10, LINK_RADIUS);

                    ctx.fillStyle = color;

                    ctx.beginPath();
                    ctx.arc(p.x, p.y, radius, 0, Math.PI * 2, false);
                    ctx.fill();
                }
            }

            ctx.restore();
        }

        // Draw persons
        for (let i = persons.length - 1; i >= 1; i--) {
            let p = persons[i];
            let info = PERSON_KINDS[p.kind];

            let radius = computePersonRadius(p);
            let [color, _] = computeLinkStyle(p.quality);

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

            let start = new_persons.get(p);
            let pos = { x: p.x, y: p.y };

            if (start != null) {
                let delay = (runner.drawCounter - start) / 10;

                if (delay < 1) {
                    let animate = Math.sin((delay * Math.PI) / 2);

                    pos.x *= animate;
                    pos.y *= animate;
                } else {
                    new_persons.delete(p);
                }
            }

            ctx.beginPath();
            ctx.arc(pos.x, pos.y, radius + 0.005, 0, Math.PI * 2, false);
            ctx.stroke();
            ctx.fill();

            let icon = textures[info.icon];
            let size = radius + 0.03;

            ctx.drawImage(icon, pos.x - size / 2, pos.y - size / 2, size, size);
        }

        // Name persons
        if (!mod.anonymous) {
            ctx.font = (12 * window.devicePixelRatio) + 'px Open Sans';
            ctx.fillStyle = '#000000';

            for (let i = persons.length - 1; i >= 0; i--) {
                let p = persons[i];
                let name = namePerson(p, false);

                if (!name)
                    continue;
                if (new_persons.has(p))
                    continue;

                if (i) {
                    let radius = computePersonRadius(p);
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

            let [color, width] = computeLinkStyle(active_edit.quality);

            ctx.strokeStyle = color + '88';
            ctx.lineWidth = width;
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

    function drawTip(tip, angle) {
        let p1 = {
            x: tip.x + 0.01 * Math.cos(angle),
            y: tip.y - 0.01 * Math.sin(angle)
        };
        let p2 = {
            x: tip.x + 0.03 * Math.cos(angle + 3.3 * Math.PI / 4),
            y: tip.y - 0.03 * Math.sin(angle + 3.3 * Math.PI / 4)
        };
        let p3 = {
            x: tip.x + 0.03 * Math.cos(angle - 3.3 * Math.PI / 4),
            y: tip.y - 0.03 * Math.sin(angle - 3.3 * Math.PI / 4)
        };

        ctx.beginPath();
        ctx.moveTo(p1.x, p1.y);
        ctx.lineTo(p2.x, p2.y);
        ctx.lineTo(p3.x, p3.y);
        ctx.closePath();

        ctx.fill();
    }

    function computePersonRadius(p) {
        if (p == persons[0])
            return world.levels[0].radius;

        let level = PROXIMITY_LEVELS[p.proximity];
        return PERSON_RADIUS * level.size;
    }

    function computeLinkCoordinates(a, b, delta = 0) {
        let angle = Math.atan2(a.y - b.y, b.x - a.x);

        let radius1 = computePersonRadius(a) + delta;
        let radius2 = computePersonRadius(b) + delta;

        let from = {
            x: a.x + radius1 * Math.cos(angle),
            y: a.y - radius1 * Math.sin(angle)
        };
        let to = {
            x: b.x - radius2 * Math.cos(angle),
            y: b.y + radius2 * Math.sin(angle),
        };

        return [from, to];
    }

    function computeLinkStyle(quality) {
        if (quality > 0) {
            let factor = quality / 5;
            let color = mixColors(QUALITY_COLORS.end, QUALITY_COLORS.middle, factor);
            let width = MIN_LINK_WIDTH + factor * (MAX_LINK_WIDTH - MIN_LINK_WIDTH);

            color = '#' + color.toString(16).padStart(6, '0');

            return [color, width];
        } else if (quality < 0) {
            let factor = -quality / 5;
            let color = mixColors(QUALITY_COLORS.start, QUALITY_COLORS.middle, factor);
            let width = MIN_LINK_WIDTH + factor * (MAX_LINK_WIDTH - MIN_LINK_WIDTH);

            color = '#' + color.toString(16).padStart(6, '0');

            return [color, width];
        } else {
            let color = '#' + QUALITY_COLORS.middle.toString(16).padStart(6, '0');
            return [color, MIN_LINK_WIDTH];
        }
    }

    function mixColors(color1, color2, factor) {
        let r1 = (color1 >> 16) & 0xFF;
        let g1 = (color1 >> 8) & 0xFF;
        let b1 = (color1 >> 0) & 0xFF;
        let r2 = (color2 >> 16) & 0xFF;
        let g2 = (color2 >> 8) & 0xFF;
        let b2 = (color2 >> 0) & 0xFF;

        let r = r1 * factor + r2 * (1 - factor);
        let g = g1 * factor + g2 * (1 - factor);
        let b = b1 * factor + b2 * (1 - factor);
        let color = (r << 16) | (g << 8) | b;

        return color;
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

    function computeDistance(p1, p2) {
        let dx = p1.x - p2.x;
        let dy = p1.y - p2.y;

        return Math.sqrt(dx * dx + dy * dy);
    }
}

export { NetworkWidget }
