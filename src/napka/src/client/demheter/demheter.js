import { render, html } from '../../../node_modules/lit/html.js';
import { unsafeHTML } from '../../../node_modules/lit/directives/unsafe-html.js';
import { util, log, net } from '../../../../web/libjs/util.js';
import { ui } from '../../lib/ui.js';
import { start, makeField, makeEdit, updateEntry, deleteEntry, renderMarkdown, isConnected } from '../map.js';

function DemheterProvider() {
    let etablissements;

    this.loadMap = async function() {
        let json = await net.get('api/entries');

        etablissements = [
            ...json.psychologues.rows.map(psy => ({ type: 'Psychologue', ...psy })),
            ...json.centres.rows.map(centre => ({ type: 'Centre', ...centre }))
        ];
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

        for (let etab of etablissements) {
            if (etab.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(etab))) {
                    let marker = {
                        latitude: etab.address.latitude,
                        longitude: etab.address.longitude,
                        icon: getEtabIcon(etab),
                        size: (etab.type == 'Centre' ? 48 : 40)
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

    function getEtabIcon(etab) {
        if (etab.type == 'Centre') {
            if (etab.demheter && etab.ect) {
                return 'static/icons/dual.png';
            } else if (etab.demheter) {
                return 'static/icons/demheter.png';
            } else if (etab.ect) {
                return 'static/icons/ect.png';
            }
        } else if (etab.type === 'Psychologue') {
            return 'static/icons/psychologist.png';
        }

        return null;
    }

    this.renderPopup = function(etab, edit_key) {
        let content = html`
            <div>
                <i>${etab.type}</i>
                ${etab.type == 'Centre' && etab.demheter ? html`<span class="tag" style="background: #ff6600;">DEMHETER</span>` : ''}
                ${etab.type == 'Centre' && etab.ect ? html`<span class="tag" style="background: #18a059;">ECT</span>` : ''}
                <br/><br/>

                Adresse : <b>${field(etab, 'address')}</b><br/><br/>

                <u>Pour prendre un rendez-vous :</u><br/>
                ${etab.type == 'Psychologue' ? html`${etab.orientation}` : ''}
                ${etab.type == 'Centre' && etab.mail ?
                    html`Adressez un courriel électronique à l'adresse <a href=${'mailto:' + etab.mail}>${etab.mail}</a>` : ''}
                ${etab.type == 'Centre' && !etab.mail ? 'Par téléphone' : ''}
                <br/><br/>
                ${etab.mail ? html`Courriel : <a href=${'mailto:' + etab.mail}>${etab.mail}</a><br/>` : ''}
                ${etab.telephone ? html`Téléphone : <a href=${'tel:+33' + etab.telephone.substr(1)}>${etab.telephone.match(/(.{1,2})/g).join('.')}</a><br/>` : ''}
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
