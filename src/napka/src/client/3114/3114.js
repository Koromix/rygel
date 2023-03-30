// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

import { render, html } from '../../../node_modules/lit/html.js';
import { unsafeHTML } from '../../../node_modules/lit/directives/unsafe-html.js';
import { util, log, net } from '../../../../web/libjs/util.js';
import { ui } from '../../lib/ui.js';
import parse from '../../lib/parse.js';
import { start, refreshMap, makeField, makeEdit, updateEntry, deleteEntry, renderMarkdown, isConnected } from '../map.js';

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
    let etablissements;

    let ignore_refresh = false;

    this.loadMap = async function() {
        etablissements = await net.get('api/entries/commun');
        etablissements = etablissements.rows;

        etablissements.sort((etab1, etab2) => {
            let wide1 = (etab1.rayon != null && etab1.rayon.startsWith('France'));
            let wide2 = (etab2.rayon != null && etab2.rayon.startsWith('France'));

            return wide1 - wide2;
        });
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
                                   data-filter="this.categorie != null && this.categorie.startsWith('Psychiatrie du secteur privé ')"
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
                                      data-filter="this.public != null && this.public.indexOf('Adultes') != -1"
                                      checked />Adultes</label>
                        <label><input type="checkbox"
                                      data-filter="this.public != null && this.public.indexOf('Enfants') != -1"
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

        for (let etab of etablissements) {
            if (etab.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(etab))) {
                    let wide = etab.rayon != null && etab.rayon.startsWith('France');
                    let icon = ICONS[etab.categorie];

                    if (icon == null) {
                        if (etab.categorie != null)
                            console.error('Missing icon', etab.categorie);
                        icon = ICONS['Non connu'];
                    }

                    let marker = {
                        latitude: etab.address.latitude,
                        longitude: etab.address.longitude,
                        icon: icon,
                        size: wide ? 48 : 40,
                        filter: wide ? 'hue-rotate(180deg)' : null
                    };

                    marker.etab = etab;

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

    this.renderPopup = function(etab, edit_key) {
        let columns = 0;
        let show_we = 0;
        let days_map = {};
        if (etab.horaires != null) {
            for (let i = 0; i < etab.horaires.length; i++) {
                let horaire = etab.horaires[i];

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
                    ${etab.categorie ? html`
                        <i>
                            ${etab.categorie}
                            ${etab.categorie2 && (etab.categorie2 !== 'Autre' || etab.categorie2_prec == null) ? html`<br/>${etab.categorie2}` : ''}
                            ${etab.categorie2 === 'Autre' && etab.categorie2_prec != null ? html`<br/>Autre : ${etab.categorie2_prec}` : ''}
                        </i><br/><br/>
                    ` : ''}

                    Adresse : <b>${field(etab, 'address')}</b>
                    ${etab.rayon ? html`<br/>Rayon d'action : <b>${etab.rayon}</b>` : ''}<br/><br/>

                    ${etab.horaires == null ? html`<span class="tag" style="background: #db0a0a;">⚠\uFE0E Horaires non communiqués</span>` : ''}
                    ${etab.horaires != null ? html`
                        <div class="planning_tip">Horaires donnés à titre indicatif</div>
                        <table class="planning">
                            <colgroup>
                                <col style="width: 90px;"/>
                                <col style="width: 60px;"/>
                                ${util.mapRange(0, columns, idx => html`<col/>`)}
                            </colgroup>

                            ${util.mapRange(0, show_we ? 14 : 10, idx => {
                                let type = idx % 2 ? 'libre' : 'rdv';
                                let day = 1 + Math.floor(idx / 2);

                                let horaires = days_map[type + '_' + day] || [];

                                return html`
                                    <tr class=${!horaires.length ? 'closed' : ''}>
                                        ${type === 'rdv' ? html`<td rowspan="2">${util.formatDay(day)}</td>` : ''}
                                        <td style="font-style: italic;">${type.toUpperCase()}</td>

                                        ${horaires.map(horaire =>
                                            html`<td>${util.formatTime(horaire.debut)} à ${util.formatTime(horaire.fin)}</td>`)}
                                        ${columns > horaires.length ?
                                            html`<td colspan=${columns - horaires.length}>${!horaires.length ? 'Fermé' : ''}</td>` : ''}
                                    </tr>
                                `;
                            })}
                        </table>
                    ` : ''}
                </div>

                <div>
                    ${etab.public != null ? html`
                        Publics : ${etab.public.map(pub => html`<span class="tag" style="background: #ff6600;">${pub}</span> `)}<br/><br/>
                    ` : ''}

                    ${etab.publics != null ? html`
                        Problématiques : ${etab.publics.map(pub => pub !== 'Autre' ? html`<br/><i>- ${pub}</i>` : '')}
                        ${etab.publics99 ? html`<br/><i>- Autres : ${etab.publics99.trim()}</i>` : ''}<br/><br/>
                    ` : ''}

                    ${etab.modalites != null ? html`
                        Modalités : ${etab.modalites.map(mod => mod !== 'Autre' ? html`<span class="tag" style="background: #444;">${mod}</span> ` : '')}
                        ${etab.modalites_prec ? html`<br/><i>Autre modalité : ${etab.modalites_prec.trim()}</i>` : ''}<br/><br/>
                    ` : ''}

                    ${etab.cout != null ? html`
                        Coût : <span class="tag" style="background: #24579d;">${etab.cout}</span>
                        ${etab.cout_prec ? html`<br/><i>Autre option : ${etab.cout_prec.trim()}</i>` : ''}<br/><br/>
                    ` : ''}

                    ${etab.delai_pec != null ? html`
                        Délai de PEC : <span class="tag" style="background: #2d8261;">${etab.delai_pec}</span><br/><br/>
                    ` : ''}

                    <u>Pour prendre un rendez-vous :</u><br/><br/>
                    ${etab.tel || etab.mail || etab.www ? html`
                        ${etab.tel ? html`Par téléphone : <a href=${'tel:' + parse.cleanPhoneNumber(etab.tel)}><b>${parse.cleanPhoneNumber(etab.tel)}</b></a><br/>` : ''}
                        ${etab.mail ? html`Par courriel : <a href=${'mailto:' + parse.cleanMail(etab.mail)}>${parse.cleanMail(etab.mail)}</a><br/>` : ''}
                        ${etab.www ? html`Par internet : <a href=${parse.cleanURL(etab.www)} target="_blank">Accéder au site</a><br/>` : ''}
                    ` : html`Présentez-vous directement au centre<br/>`}
                </div>
            </div>

            <div class="info">
                ${etab.orgaforma ? html`<br/><i>${util.capitalize(etab.orgaforma.trim())}</i>` : ''}
            </div>
        `;

        return content;
    };

    function field(etab, key, view = null) {
        if (key == 'address' && etab[key] != null) {
            return makeField(etab, key, 'address');
        } else {
            return makeField(etab, key, fields[key], view);
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
