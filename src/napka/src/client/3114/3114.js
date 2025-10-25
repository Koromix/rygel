// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, unsafeHTML } from '../../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net } from '../../../../web/core/base.js';
import * as parse from '../../lib/parse.js';
import { start, zoom, refreshMap, makeField, makeEdit, updateEntry, deleteEntry,
         renderMarkdown, isConnected, loadTexture } from '../map.js';

const ICONS = {
    'Psychiatrie du secteur public': 'static/icons/hospital.png',
    'Médecin généraliste': 'static/icons/doctor.png',
    'Sanitaire hors psychiatrie': 'static/icons/emergency.png',
    'Psychiatrie du secteur privé (cliniques privées)': 'static/icons/clinic.png',
    'Psychiatrie du secteur privé (à but non lucratif)': 'static/icons/clinic.png',
    'Psychiatrie libérale': 'static/icons/psychiatrist.png',
    'Psychologue libéral': 'static/icons/psychologist.png',
    'Médico-social / social': 'static/icons/social.png',
    'Associations (hors ligne d\'écoute)': 'static/icons/volunteer.png',
    'Ligne d\'écoute': 'static/icons/telephone.png',
    'Organisme de formation': 'static/icons/graduate.png',
    'Non connu': 'static/icons/unknown.png'
};

function PpnpsProvider() {
    let entries;

    let ignore_refresh = false;

    let icons = {};

    this.loadMap = async function() {
        let [data, images] = await Promise.all([
            Net.get('api/entries/commun'),
            Promise.all(Object.values(ICONS).map(loadTexture))
        ]);

        entries = data.rows;
        entries.sort((entry1, entry2) => {
            let wide1 = entry1.rayon?.startsWith?.('France');
            let wide2 = entry2.rayon?.startsWith?.('France');

            return wide1 - wide2;
        });

        // We've preloaded images
        {
            let keys = Object.keys(ICONS);

            for (let i = 0; i < keys.length; i++)
                icons[keys[i]] = images[i];
        }
    };

    this.renderFilters = function() {
        return html`
            <div id="filters">
                <div id="count"></div>

                <fieldset class="group">
                    <legend>Catégories d'établissements</legend>

                    <div>
                        <div class="toggle"><a @click=${e => toggleGroup(e, true)}>cocher</a> | <a @click=${e => toggleGroup(e, false)}>décocher</a></div>

                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Sanitaire hors psychiatrie'"
                                   checked />Sanitaire hors psychiatrie
                            <img src="static/icons/emergency.png" width="16" height="16" alt="" />
                            <div class="prec">Ex : urgences, addictologie, gériatrie...</div>
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Psychiatrie du secteur public'"
                                   checked />Psychiatrie du secteur public
                            <img src="static/icons/hospital.png" width="16" height="16" alt="" />
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie?.startsWith('Psychiatrie du secteur privé ')"
                                   checked />Psychiatrie du secteur privé
                            <img src="static/icons/clinic.png" width="16" height="16" alt="" />
                            <div class="prec">Cliniques privées et établissements non lucratifs</div>
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Psychiatrie libérale'"
                                   checked />Psychiatrie libérale
                            <img src="static/icons/psychiatrist.png" width="16" height="16" alt="" />
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Psychologue libéral'"
                                   checked />Psychologue libéral
                            <img src="static/icons/psychologist.png" width="16" height="16" alt="" />
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Médecin généraliste'"
                                   checked />Médecin généraliste
                            <img src="static/icons/doctor.png" width="16" height="16" alt="" />
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Médico-social / social'"
                                   checked />Médico-social / social
                            <img src="static/icons/social.png" width="16" height="16" alt="" />
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Associations (hors ligne d\\'écoute)'"
                                   checked />Associations (hors ligne d'écoute)
                            <img src="static/icons/volunteer.png" width="16" height="16" alt="" />
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Ligne d\\'écoute'"
                                   checked />Ligne d'écoute
                            <img src="static/icons/telephone.png" width="16" height="16" alt="" />
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == 'Organisme de formation'"
                                   checked />Organisme de formation
                            <img src="static/icons/graduate.png" width="16" height="16" alt="" />
                        </label>
                        <label>
                            <input type="checkbox"
                                   data-filter="this.categorie == null"
                                   checked /><i>Non connu</i>
                            <img src="static/icons/unknown.png" width="16" height="16" alt="" />
                        </label>
                    </div>
                </fieldset>

                <fieldset class="group">
                    <legend>Publics pris en charge</legend>

                    <div>
                        <div class="toggle"><a @click=${e => toggleGroup(e, true)}>cocher</a> | <a @click=${e => toggleGroup(e, false)}>décocher</a></div>

                        <label><input type="checkbox"
                                      data-filter="this.public?.includes?.('Adultes')"
                                      checked />Adultes</label>
                        <label><input type="checkbox"
                                      data-filter="this.public?.includes?.('Enfants')"
                                      checked />Enfants</label>
                        <label><input type="checkbox"
                                      data-filter="this.public == null"
                                      checked /><i>Non connu</i></label>
                    </div>
                </fieldset>

                <fieldset class="group">
                    <legend>Coût</legend>

                    <div>
                        <div class="toggle"><a @click=${e => toggleGroup(e, true)}>cocher</a> | <a @click=${e => toggleGroup(e, false)}>décocher</a></div>

                        <label><input type="checkbox"
                                      data-filter="this.cout == 'Gratuit dans tous les cas'"
                                      checked />Gratuit dans tous les cas</label>
                        <label><input type="checkbox"
                                      data-filter="this.cout == 'Gratuit sous conditions'"
                                      checked />Gratuit sous conditions</label>
                        <label><input type="checkbox"
                                      data-filter="this.cout == 'Non gratuit'"
                                      checked />Non gratuit</label>
                        <label><input type="checkbox"
                                      data-filter="this.cout == null"
                                      checked /><i>Non connu</i></label>
                    </div>
                </fieldset>

                <fieldset class="group">
                    <legend>Délai de PEC</legend>

                    <div>
                        <div class="toggle"><a @click=${e => toggleGroup(e, true)}>cocher</a> | <a @click=${e => toggleGroup(e, false)}>décocher</a></div>

                        <label><input type="checkbox"
                                      data-filter="this.delai_pec == 'Urgence (immédiat)'"
                                      checked />Urgence (immédiat)</label>
                        <label><input type="checkbox"
                                      data-filter="this.delai_pec == 'Très rapide (24 à 72h)'"
                                      checked />Très rapide (24 à 72h)</label>
                        <label><input type="checkbox"
                                      data-filter="this.delai_pec == 'Rapide (dans la semaine)'"
                                      checked />Rapide (dans la semaine)</label>
                        <label><input type="checkbox"
                                      data-filter="this.delai_pec == 'Moyen (dans le mois)'"
                                      checked />Moyen (dans le mois)</label>
                        <label><input type="checkbox"
                                      data-filter="this.delai_pec == 'Long (plus d\\'1 mois)'"
                                      checked />Long (plus d'1 mois)</label>
                        <label><input type="checkbox"
                                      data-filter="this.delai_pec == null"
                                      checked /><i>Non connu</i></label>
                    </div>
                </fieldset>
            </div>
        `;
    };

    function toggleGroup(e, enable) {
        let group = findParent(e.target, el => el.classList.contains('group'));
        let checkboxes = group.querySelectorAll('input[type="checkbox"]');

        let changed = false;
        ignore_refresh = true;
        for (let checkbox of checkboxes) {
            changed |= (checkbox.checked !== enable);
            checkbox.checked = enable;
        }
        ignore_refresh = false;

        if (changed)
            refreshMap();
    }

    function findParent(el, func) {
        while (el && !func(el))
            el = el.parentElement;
        return el;
    }

    this.fillMap = function(filters) {
        if (ignore_refresh)
            return;

        let markers = [];
        let total = 0;

        for (let entry of entries) {
            if (entry.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(entry))) {
                    let wide = entry.rayon?.startsWith?.('France');
                    let icon = icons[entry.categorie];

                    if (icon == null) {
                        if (entry.categorie != null)
                            console.error('Missing icon', entry.categorie);
                        icon = icons['Non connu'];
                    }

                    let marker = {
                        latitude: entry.address.latitude,
                        longitude: entry.address.longitude,
                        icon: icon,
                        size: wide ? 48 : 40,
                        priority: 0 + wide,
                        clickable: true,
                        filter: wide ? 'hue-rotate(180deg)' : null
                    };

                    marker.entry = entry;

                    markers.push(marker);
                }
            }
        }

        if (markers.length < total) {
            let text = `${markers.length} / ${total}`;
            render(text, document.querySelector('#count'));
        } else if (markers.length) {
            let text = `${markers.length} entrée${markers.length > 1 ? 's' : ''}`;
            render(text, document.querySelector('#count'));
        } else {
            let text = 'Aucun résultat';
            render(text, document.querySelector('#count'));
        }

        return markers;
    };

    this.renderEntry = function(entry, edit_key) {
        let columns = 0;
        let show_we = 0;
        let days_map = {};
        if (entry.horaires != null) {
            for (let i = 0; i < entry.horaires.length; i++) {
                let horaire = entry.horaires[i];

                let ptr = days_map[horaire.type + '_' + horaire.jour];
                if (ptr == null) {
                    ptr = [];
                    days_map[horaire.type + '_' + horaire.jour] = ptr;
                }

                ptr.push(horaire);
                columns = Math.max(columns, ptr.length);
                show_we |= (horaire.jour >= 6);
            }
        }

        let content = html`
            <div class="info">
                <div>
                    ${entry.categorie ? html`
                        <i>
                            ${entry.categorie}
                            ${entry.categorie2 && (entry.categorie2 !== 'Autre' || entry.categorie2_prec == null) ? html`<br/>${entry.categorie2}` : ''}
                            ${entry.categorie2 === 'Autre' && entry.categorie2_prec != null ? html`<br/>Autre : ${entry.categorie2_prec}` : ''}
                        </i><br/><br/>
                    ` : ''}

                    Adresse : <b>${field(entry, 'address')}</b>
                    ${entry.rayon ? html`<br/>Rayon d'action : <b>${entry.rayon}</b>` : ''}<br/><br/>

                    ${entry.horaires == null ? html`<span class="tag" style="background: #db0a0a;">⚠\uFE0E Horaires non communiqués</span>` : ''}
                    ${entry.horaires != null ? html`
                        <div class="planning_tip">Horaires donnés à titre indicatif</div>
                        <table class="planning">
                            <colgroup>
                                <col style="width: 90px;"/>
                                <col style="width: 60px;"/>
                                ${Util.mapRange(0, columns, idx => html`<col/>`)}
                            </colgroup>

                            ${Util.mapRange(0, show_we ? 14 : 10, idx => {
                                let type = idx % 2 ? 'libre' : 'rdv';
                                let day = 1 + Math.floor(idx / 2);

                                let horaires = days_map[type + '_' + day] || [];

                                return html`
                                    <tr class=${!horaires.length ? 'closed' : ''}>
                                        ${type === 'rdv' ? html`<td rowspan="2">${Util.formatDay(day)}</td>` : ''}
                                        <td style="font-style: italic;">${type.toUpperCase()}</td>

                                        ${horaires.map(horaire =>
                                            html`<td>${Util.formatTime(horaire.debut)} à ${Util.formatTime(horaire.fin)}</td>`)}
                                        ${columns > horaires.length ?
                                            html`<td colspan=${columns - horaires.length}>${!horaires.length ? 'Fermé' : ''}</td>` : ''}
                                    </tr>
                                `;
                            })}
                        </table>
                    ` : ''}
                </div>

                <div>
                    ${entry.public != null ? html`
                        Publics : ${entry.public.map(pub => html`<span class="tag" style="background: #ff6600;">${pub}</span> `)}<br/><br/>
                    ` : ''}

                    ${entry.publics != null ? html`
                        Problématiques : ${entry.publics.map(pub => pub !== 'Autre' ? html`<br/><i>- ${pub}</i>` : '')}
                        ${entry.publics99 ? html`<br/><i>- Autres : ${entry.publics99.trim()}</i>` : ''}<br/><br/>
                    ` : ''}

                    ${entry.modalites != null ? html`
                        Modalités : ${entry.modalites.map(mod => mod !== 'Autre' ? html`<span class="tag" style="background: #444;">${mod}</span> ` : '')}
                        ${entry.modalites_prec ? html`<br/><i>Autre modalité : ${entry.modalites_prec.trim()}</i>` : ''}<br/><br/>
                    ` : ''}

                    ${entry.cout != null ? html`
                        Coût : <span class="tag" style="background: #24579d;">${entry.cout}</span>
                        ${entry.cout_prec ? html`<br/><i>Autre option : ${entry.cout_prec.trim()}</i>` : ''}<br/><br/>
                    ` : ''}

                    ${entry.delai_pec != null ? html`
                        Délai de PEC : <span class="tag" style="background: #2d8261;">${entry.delai_pec}</span><br/><br/>
                    ` : ''}

                    <u>Pour prendre un rendez-vous :</u><br/><br/>
                    ${entry.tel || entry.mail || entry.www ? html`
                        ${entry.tel ? html`Par téléphone : <a href=${'tel:' + parse.cleanPhoneNumber(entry.tel)}><b>${parse.cleanPhoneNumber(entry.tel)}</b></a><br/>` : ''}
                        ${entry.mail ? html`Par courriel : <a href=${'mailto:' + parse.cleanMail(entry.mail)}>${parse.cleanMail(entry.mail)}</a><br/>` : ''}
                        ${entry.www ? html`Par internet : <a href=${parse.cleanURL(entry.www)} target="_blank">Accéder au site</a><br/>` : ''}
                    ` : html`Présentez-vous directement au centre<br/>`}
                </div>
            </div>

            <div class="info">
                ${entry.orgaforma ? html`<br/><i>${Util.capitalize(entry.orgaforma.trim())}</i>` : ''}
            </div>
        `;

        return content;
    };

    function field(entry, key, view = null) {
        if (key == 'address' && entry[key] != null) {
            return makeField(entry, key, 'address');
        } else {
            return makeField(entry, key, fields[key], view);
        }
    }
}

export async function main() {
    let provider = new PpnpsProvider;

    let latitude = (window.innerWidth > 1100) ? 47 :
                   (window.innerWidth > 580) ? 47.65 : 48.4;
    let longitude = 1.9;
    let zoom = (window.innerWidth <= 580) ? 4 : 5;

    await start(provider, {
        latitude: latitude,
        longitude: longitude,
        zoom: zoom
    });
}

export { zoom }
