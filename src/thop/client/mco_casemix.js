// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_casemix = (function() {
    let self = this;

    let route = {};
    this.route = route;

    this.run = async function() {
        if (!settings.permissions.mco_casemix)
            throw new Error('Vous n\'avez pas accès à cette page');

        switch (route.mode) {
            case 'ghm': {} break;
            case 'units': {} break;
            case 'valorisation': { await runValorisation(); } break;
            case 'rss': {} break;

            default: {
                throw new Error(`Mode inconnu '${route.mode}'`);
            } break;
        }
    };

    this.parseURL = function(path, params = {}) {
        let parts = path.split('/');

        // For everyone
        let args = {
            mode: parts[0] || 'ghm',

            main: {},
            diff: {},
            valorisation: {}
        };
        if (!settings.permissions.mco_casemix)
            return args;

        // Authorized users
        args.main = {
            ...parseDateRange(parts[1], settings.mco.casemix.max_date),
            algorithm: params.algorithm || settings.mco.casemix.default_algorithm,
        };
        args.coeff = !!parseInt(params.coeff, 10) || false;

        // Mode-specific part
        switch (args.mode) {
            case 'valorisation': {
                args.valorisation.ghm_root = parts[2] || null;
            } break;
        }

        return args;
    };

    function parseDateRange(str, default_end) {
        str = str || '';

        do {
            let parts = str.split('..');
            if (parts.length !== 2)
                break;

            let range = {
                start_date: dates.fromString(parts[0] || null),
                end_date: dates.fromString(parts[1] || null)
            };
            if (!range.start_date || !range.end_date)
                break;

            if (range.end_date <= range.start_date) {
                log.error('Range is invalid: end date is earlier than start date');
                break;
            }

            return range;
        } while (false);

        let default_range = {
            start_date: dates.create(default_end.minus(1).year, 1, 1),
            end_date: default_end
        };

        return default_range;
    }

    this.makeURL = function(args = {}) {
        args = util.assignDeep({}, route, args);

        let url = `${env.base_url}mco_casemix/${args.mode}`;
        if (args.main.start_date && args.main.end_date)
            url += `/${args.main.start_date}..${args.main.end_date}`;

        switch (args.mode) {
            case 'valorisation': {
                if (args.valorisation.ghm_root)
                    url += `/${args.valorisation.ghm_root}`;
            } break;
        }

        url += '?' + new URLSearchParams({
            algorithm: args.main.algorithm,
            coeff: 0 + args.coeff
        });

        return url;
    };

    // ------------------------------------------------------------------------
    // Valorisation
    // ------------------------------------------------------------------------

    async function runValorisation() {
        let ghm_roots = await concepts.load('mco').then(mco => mco.ghm_roots);

        if (!route.valorisation.ghm_root)
            route.valorisation.ghm_root = ghm_roots[0].code;

        // Options
        render(html`
            ${renderPeriodPicker(route.main.start_date, route.main.end_date)}
            <label>Algorithme <select @change=${e => thop.go(self, {main: {algorithm: e.target.value}})}>
                ${settings.mco.casemix.algorithms.map(algo => {
                    let selected = (route.main.algorithm === algo);
                    let label = `Algorithme ${algo}${algo === settings.mco.casemix.default_algorithm ? '*' : ''}`;

                    return html`<option value=${algo} .selected=${selected}>${label}</option>`;
                })}
            </select></label>
            <label>Coefficient <input type="checkbox" .checked=${route.coeff}
                                      @change=${e => thop.go(self, {coeff: e.target.checked})}/></label>
            ${renderGhmRootSelector(ghm_roots, route.valorisation.ghm_root)}
        `, document.querySelector('#th_options'));

        // View
        render(html``, document.querySelector('#th_view'));
    }

    // ------------------------------------------------------------------------
    // Options
    // ------------------------------------------------------------------------

    function renderPeriodPicker(start_date, end_date) {
        let ppik = new PeriodPicker;

        ppik.changeHandler = (start_date, end_date) =>
            thop.go(self, {main: {start_date: start_date, end_date: end_date}});

        ppik.setRange(settings.mco.casemix.min_date, settings.mco.casemix.max_date);
        ppik.setDates(start_date, end_date);

        return ppik.render();
    }

    function renderGhmRootSelector(ghm_roots, current_ghm_root) {
        return html`
            <select @change=${e => thop.go(self, {valorisation: {ghm_root: e.target.value}})}>
                ${ghm_roots.map(ghm_root => {
                    let disabled = false;
                    let label = `${ghm_root.code} – ${ghm_root.desc}${disabled ? ' *' : ''}`;

                    return html`<option value=${ghm_root.code} ?disabled=${disabled}
                                        .selected=${ghm_root.code === current_ghm_root}>${label}</option>`
                })}
            </select>
        `;
    }

    return this;
}).call({});
