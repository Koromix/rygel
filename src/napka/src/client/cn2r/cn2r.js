// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, unsafeHTML } from '../../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net } from '../../../../core/web/base/base.js';
import * as UI from '../../../../core/web/base/ui.js';
import * as parse from '../../lib/parse.js';
import { start, zoom, makeField, makeEdit, renderMarkdown,
         login, logout, isConnected, loadTexture } from '../map.js';

const ICONS = {
    crp: 'static/icons/crp.png',
    misc: 'static/icons/misc.png'
};

function Cn2rProvider() {
    let entries;
    let fields;

    let icons = {};

    this.clusterTreshold = 3;

    this.loadMap = async function() {
        let [data, images] = await Promise.all([
            Net.get('api/entries/etablissements'),
            Promise.all(Object.values(ICONS).map(loadTexture))
        ]);

        entries = data.rows;
        fields = data.fields;

        entries.sort((entry1, entry2) => !!entry1.etab_crp - !!entry2.etab_crp);

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

            <div id="admin">
                ${!isConnected() ? html`<button @click=${UI.wrap(login)}>Se connecter</button>` : ''}
                ${isConnected() ? html`<button @click=${UI.insist(logout)}>Se déconnecter</button>` : ''}
            </div>
        `;
    }

    this.fillMap = function(filters) {
        let markers = [];
        let total = 0;

        for (let entry of entries) {
            if (entry.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(entry))) {
                    let marker = null;

                    if (entry.etab_crp) {
                        marker = {
                            latitude: entry.address.latitude,
                            longitude: entry.address.longitude,
                            cluster: '#000052',
                            tooltip: entry.name,
                            priority: 2,
                            icon: icons.crp,
                            size: 48,
                            clickable: true
                        };
                    } else {
                        marker = {
                            latitude: entry.address.latitude,
                            longitude: entry.address.longitude,
                            cluster: '#bf4866',
                            tooltip: entry.name,
                            priority: 1,
                            icon: icons.misc,
                            size: 40,
                            clickable: true
                        };
                    }

                    marker.entry = entry;

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

    this.renderEntry = function(entry, edit_key) {
        let columns = 0;
        let show_we = 0;
        let days_map = {};
        if (entry.rdv_horaires != null && entry.rdv_horaires.schedule != null) {
            let schedule = entry.rdv_horaires.schedule;

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
                    ${!isConnected() && entry.etab_crp ?
                        html`<span class="tag" style="background: #000552;"
                                   title="Centre Régional de Psychotraumatisme">CRP</span>&nbsp;` : ''}
                    <i>${field(entry, 'etab_statut')}</i>
                    ${isConnected() ?
                        html`<br/>Statut <span class="tag" style="background: #000552;"
                                               title="Centre Régional de Psychotraumatisme">CRP</span> : ${field(entry, 'etab_crp')}` : ''}
                    <br/><br/>

                    Adresse : <b>${field(entry, 'address')}</b><br/><br/>

                    ${!checkSchedule(entry.rdv_horaires) ? html`<span class="tag" style="background: #db0a0a;">⚠\uFE0E Horaires non communiqués</span>${makeEdit(entry, 'rdv_horaires')}<br/>` : ''}
                    ${checkSchedule(entry.rdv_horaires) ? html`
                        <div class="planning_tip">Horaires du secrétariat${makeEdit(entry, 'rdv_horaires')}</div>
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
                    ${edit_key == 'rdv_horaires' ? html`${field(entry, 'rdv_horaires')}<br/>` : ''}<br/>

                    Accès personnes à mobilité réduite : ${field(entry, 'etab_acces_pmr')}<br/>
                </div>

                <div>
                    Accueil : ${field(entry, 'rdv_publics')}<br/>
                    ${isConnected() ? html`
                        Consultations : ${field(entry, 'rdv_consultations', (entry.rdv_consultations || []).map(cs =>
                            html`<span class="tag" style="background: #8fc7bf;">${cs}</span> `))}<br/><br/>
                    ` : ''}
                    Modalités : ${field(entry, 'rdv_modalites')}<br/><br/>

                    <u>Contacter la structure</u> :<br/><br/>
                    ${isConnected() || entry.rdv_fixe ? html`Téléphone : <b>${field(entry, 'rdv_fixe', parse.cleanPhoneNumber(entry.rdv_fixe))}</b>
                                       ${isConnected() || entry.rdv_portable ? html` ou ${field(entry, 'rdv_portable', parse.cleanPhoneNumber(entry.rdv_portable))}` : ''}<br/>` : ''}
                    ${isConnected() || entry.rdv_mail ? html`Courriel : ${field(entry, 'rdv_mail', entry.rdv_mail ? html`<a href=${'mailto:' + entry.rdv_mail} style="white-space: nowrap;">${entry.rdv_mail}</a>` : html`<span class="sub">(inconnu)</span>`)}</a><br/>` : ''}
                    ${!entry.rdv_fixe && !entry.rdv_mail && !entry.rdv_web ? html`Présentez-vous directement au centre<br/>` : ''}
                    ${entry.rdv_courrier_mt && !isConnected() ? html`<br/>⚠\uFE0E Vous devez disposer d'un courrier de votre médecin traitant<br/>` : ''}
                    ${isConnected() ? html`<br/>⚠\uFE0E Nécessité d'un médecin traitant : ${field(entry, 'rdv_courrier_mt')}<br/>` : ''}
                    ${!isConnected() && entry.rdv_complement ? html`${unsafeHTML(renderMarkdown(entry.rdv_complement.trim()))}` : ''}
                    ${isConnected() ? html`<br/>${field(entry, 'rdv_complement')}` : ''}
                </div>
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
