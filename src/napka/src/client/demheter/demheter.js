// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net } from 'lib/web/base/base.js';
import { start, zoom, makeField, renderMarkdown, loadTexture } from '../map.js';

const ICONS = {
    dual: 'static/icons/dual.png',
    demheter: 'static/icons/demheter.png',
    ect: 'static/icons/ect.png',
    psychologist: 'static/icons/psychologist.png'
};

function DemheterProvider() {
    let entries;

    let icons = {};

    this.loadMap = async function() {
        let [data, images] = await Promise.all([
            Net.get('api/entries'),
            Promise.all(Object.values(ICONS).map(loadTexture))
        ]);

        entries = [
            ...data.psychologues.rows.map(psy => ({ type: 'Psychologue', ...psy })),
            ...data.centres.rows.map(centre => ({ type: 'Centre', ...centre }))
        ];

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

                <div class="group">
                    <label>
                        <input type="checkbox" data-filter="this.type === 'Centre' && this.demheter"
                               checked/> Centres DEMHETER
                        <img src="static/icons/demheter.png" width="32" height="32" alt="" />
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.type === 'Centre' && this.ect"
                               checked/> ECT
                        <img src="static/icons/ect.png" width="32" height="32" alt="" />
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.type === 'Psychologue'"
                               checked/> Psychologues
                        <img src="static/icons/psychologist.png" width="32" height="32" alt="" />
                    </label>
                </div>
            </div>
        `;
    };

    this.fillMap = function(filters) {
        let markers = [];
        let total = 0;

        for (let entry of entries) {
            if (entry.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(entry))) {
                    let marker = {
                        latitude: entry.address.latitude,
                        longitude: entry.address.longitude,
                        cluster: (entry.type == 'Psychologue') ? '#d352a3' : null,
                        priority: 1 + (entry.type == 'Centre') + !!entry.demheter,
                        icon: getEntryIcon(entry),
                        size: (entry.type == 'Centre') ? 64 : 40,
                        clickable: true
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

    function getEntryIcon(entry) {
        if (entry.type == 'Centre') {
            if (entry.demheter && entry.ect) {
                return icons.dual;
            } else if (entry.demheter) {
                return icons.demheter;
            } else if (entry.ect) {
                return icons.ect;
            }
        } else if (entry.type === 'Psychologue') {
            return icons.psychologist;
        }

        return null;
    }

    this.renderEntry = function(entry, edit_key) {
        let content = html`
            <div>
                <i>${entry.type}</i>
                ${entry.type == 'Centre' && entry.demheter ? html`<span class="tag" style="background: #ff6600;">DEMHETER</span>` : ''}
                ${entry.type == 'Centre' && entry.ect ? html`<span class="tag" style="background: #18a059;">ECT</span>` : ''}
                <br/><br/>

                Adresse : <b>${field(entry, 'address')}</b><br/><br/>

                <u>Pour prendre un rendez-vous :</u><br/>
                ${entry.type == 'Psychologue' ? unsafeHTML(renderMarkdown(entry.orientation.trim())) : ''}
                ${entry.type == 'Centre' && entry.mail ?
                    html`Adressez un courriel électronique à l'adresse <a href=${'mailto:' + entry.mail}>${entry.mail}</a><br/>` : ''}
                ${entry.type == 'Centre' && !entry.mail ? html`Prenez rendez-vous par téléphone<br/>` : ''}
                <br/>
                ${entry.mail ? html`Courriel : <a href=${'mailto:' + entry.mail}>${entry.mail}</a><br/>` : ''}
                ${entry.telephone ? html`Téléphone : <a href=${'tel:+33' + entry.telephone.substr(1)}>${entry.telephone}</a><br/>` : ''}
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
    let provider = new DemheterProvider;

    let latitude = 50.44;
    let longitude = 2.5;
    let zoom = (window.innerWidth <= 580) ? 7 : 8;

    await start(provider, {
        latitude: latitude,
        longitude: longitude,
        zoom: zoom
    });
}

export { zoom }
