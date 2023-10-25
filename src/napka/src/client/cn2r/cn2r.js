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
import { Util, Log, Net } from '../../../../web/libjs/common.js';
import * as parse from '../../lib/parse.js';
import { start, zoom, makeField, makeEdit, updateEntry, deleteEntry, renderMarkdown, isConnected } from '../map.js';

const ICONS = {
    crp: 'static/icons/crp.png',
    misc: 'static/icons/misc.png'
};

function Cn2rProvider() {
    let etablissements;
    let fields;

    let icons = {};

    this.loadMap = async function() {
        let [data, images] = await Promise.all([
            Net.get('api/entries/etablissements'),
            Promise.all(Object.values(ICONS).map(url => Net.loadImage(url, true)))
        ]);

        etablissements = data.rows;
        fields = data.fields;

        etablissements.sort((etab1, etab2) => !!etab1.etab_crp - !!etab2.etab_crp);

        // We've preloaded images
        {
            let keys = Object.keys(ICONS);

            for (let i = 0; i < keys.length; i++)
                icons[keys[i]] = images[i];
        }
    }

    this.renderFilters = function() {
        return html`
            <div id="filters">
                <div id="count"></div>

                <fieldset class="group">
                    <legend>Établissements</legend>

                    <div>
                        <label><input type="checkbox" data-filter="this.etab_statut === 'Etablissement public'"
                                      checked/>Publics</label>
                        <label><input type="checkbox" data-filter="this.etab_statut === 'Etablissement privé à but non lucratif'"
                                      checked/>Privés à but non lucratif</label>
                        <label><input type="checkbox" data-filter="this.etab_statut === 'Etablissement privé à but lucratif'"
                                      checked/>Privés à but lucratif</label>
                    </div>
                </fieldset>
                <fieldset class="group">
                    <legend>Publics</legend>

                    <div>
                        <label><input type="checkbox" data-filter="this.rdv_publics.indexOf('Enfant') != -1"
                                      checked/>Enfants</label>
                        <label><input type="checkbox" data-filter="this.rdv_publics.indexOf('Adolescent') != -1"
                                      checked/>Adolescents</label>
                        <label><input type="checkbox" data-filter="this.rdv_publics.indexOf('Adulte') != -1"
                                      checked/>Adultes</label>
                    </div>
                </fieldset>
                <fieldset class="group">
                    <legend>Thérapies</legend>

                    <div>
                        <label><input type="checkbox" data-filter="this.rdv_consultations.indexOf('Individuel') != -1"
                                      checked/>Individuel</label>
                        <label><input type="checkbox" data-filter="this.rdv_consultations.indexOf('Groupe') != -1"
                                      checked/>Collectif</label>
                        <label><input type="checkbox" data-filter="this.rdv_consultations.indexOf('Famille') != -1"
                                      checked/>Thérapie familiale</label>
                    </div>
                </fieldset>
                <fieldset class="group">
                    <legend>Modalités</legend>

                    <div>
                        <label><input type="checkbox" data-filter="this.rdv_modalites.indexOf('Présentiel') != -1"
                                      checked/>Sur place</label>
                        <label><input type="checkbox" data-filter="this.rdv_modalites.indexOf('Téléconsultation') != -1"
                                      checked/>Téléconsultation</label>
                        <label><input type="checkbox" data-filter="this.rdv_modalites.indexOf('Domicile') != -1"
                                      checked/>A domicile</label>
                    </div>
                </fieldset>
            </div>
        `;
    }

    this.fillMap = function(filters) {
        let markers = [];
        let total = 0;

        for (let etab of etablissements) {
            if (etab.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(etab))) {
                    let marker = null;

                    if (etab.etab_crp) {
                        marker = {
                            latitude: etab.address.latitude,
                            longitude: etab.address.longitude,
                            icon: icons.crp,
                            size: 48,
                            clickable: true
                        };
                    } else {
                        marker = {
                            latitude: etab.address.latitude,
                            longitude: etab.address.longitude,
                            icon: icons.misc,
                            size: 40,
                            clickable: true
                        };
                    }

                    marker.etab = etab;

                    markers.push(marker);
                }
            }
        }

        // Show number of matches
        let text = markers.length ? html`${markers.length} résultat${markers.length > 1 ? 's' : ''}` : 'Aucun résultat';
        render(text, document.querySelector('#count'));

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
        if (etab.rdv_horaires != null && etab.rdv_horaires.schedule != null) {
            let schedule = etab.rdv_horaires.schedule;

            for (let i = 0; i < schedule.length; i++) {
                let horaire = schedule[i];

                let ptr = days_map[horaire.jour];
                if (ptr == null) {
                    ptr = [];
                    days_map[horaire.jour] = ptr;
                }

                ptr.push(horaire);
                columns = Math.max(columns, ptr.length);
                show_we |= (horaire.jour >= 6);
            }
        }

        let content = html`
            <div class="info">
                <div>
                    ${!isConnected() && etab.etab_crp ?
                        html`<span class="tag" style="background: #24579d;"
                                   title="Centre Régional de Psychotraumatisme">CRP</span>&nbsp;` : ''}
                    <i>${field(etab, 'etab_statut')}</i>
                    ${isConnected() ?
                        html`<br/>Statut <span class="tag" style="background: #24579d;"
                                               title="Centre Régional de Psychotraumatisme">CRP</span> : ${field(etab, 'etab_crp')}` : ''}
                    <br/><br/>

                    Adresse : <b>${field(etab, 'address')}</b><br/><br/>

                    ${!checkSchedule(etab.rdv_horaires) ? html`<span class="tag" style="background: #db0a0a;">⚠\uFE0E Horaires non communiqués</span>${makeEdit(etab, 'rdv_horaires')}<br/>` : ''}
                    ${checkSchedule(etab.rdv_horaires) ? html`
                        <div class="planning_tip">Horaires donnés à titre indicatif${makeEdit(etab, 'rdv_horaires')}</div>
                        ${edit_key != 'rdv_horaires' ? html`
                            <table class="planning">
                                ${Util.mapRange(1, show_we ? 8 : 6, day => {
                                    let horaires = days_map[day] || [];

                                    return html`
                                        <tr class=${!horaires.length ? 'closed' : ''}>
                                            <td>${Util.formatDay(day)}</td>
                                            ${horaires.map(horaire => html`
                                                <td>${Util.formatTime(horaire.horaires[0])} à
                                                    ${Util.formatTime(horaire.horaires[1])}</td>
                                            `)}
                                            ${columns > horaires.length ?
                                                html`<td colspan=${columns - horaires.length}>${!horaires.length ? 'Fermé' : ''}</td>` : ''}
                                        </tr>
                                    `;
                                })}
                            </table>
                        ` : ''}
                    ` : ''}
                    ${edit_key == 'rdv_horaires' ? html`${field(etab, 'rdv_horaires')}<br/>` : ''}<br/>

                    Accès personnes à mobilité réduite : ${field(etab, 'etab_acces_pmr')}<br/>
                </div>

                <div>
                    Accueil : ${field(etab, 'rdv_publics')}<br/>
                    ${isConnected() ? html`
                        Consultations : ${field(etab, 'rdv_consultations', (etab.rdv_consultations || []).map(cs =>
                            html`<span class="tag" style="background: #444;">${cs}</span> `))}<br/><br/>
                    ` : ''}
                    Modalités : ${field(etab, 'rdv_modalites')}<br/><br/>

                    <u>Pour contacter le CRP :</u><br/><br/>
                    ${isConnected() || etab.rdv_fixe ? html`Téléphone : <b>${field(etab, 'rdv_fixe', parse.cleanPhoneNumber(etab.rdv_fixe))}</b>
                                       ${isConnected() || etab.rdv_portable ? html` ou ${field(etab, 'rdv_portable', parse.cleanPhoneNumber(etab.rdv_portable))}` : ''}<br/>` : ''}
                    ${isConnected() || etab.rdv_mail ? html`Courriel : ${field(etab, 'rdv_mail', etab.rdv_mail ? html`<a href=${'mailto:' + etab.rdv_mail} style="white-space: nowrap;">${etab.rdv_mail}</a>` : html`<span class="sub">(inconnu)</span>`)}</a><br/>` : ''}
                    ${!etab.rdv_fixe && !etab.rdv_mail && !etab.rdv_web ? html`Présentez-vous directement au centre<br/>` : ''}
                    ${etab.rdv_courrier_mt && !isConnected() ? html`<br/>⚠\uFE0E Vous devez disposer d'un courrier de votre médecin traitant<br/>` : ''}
                    ${isConnected() ? html`<br/>⚠\uFE0E Nécessité d'un médecin traitant : ${field(etab, 'rdv_courrier_mt')}<br/>` : ''}
                    ${!isConnected() && etab.rdv_complement ? html`${unsafeHTML(renderMarkdown(etab.rdv_complement.trim()))}` : ''}
                    ${isConnected() ? html`<br/>${field(etab, 'rdv_complement')}` : ''}
                </div>
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

    function checkSchedule(schedule) {
        return schedule != null && schedule.schedule != null;
    }
}

export async function main() {
    let provider = new Cn2rProvider;

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
