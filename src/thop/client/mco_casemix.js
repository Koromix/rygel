// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_casemix = new function() {
    let self = this;

    let route = {};
    this.route = route;

    this.run = async function() {
        if (!settings.permissions.mco_casemix) {
            render('', document.querySelector('#th_options'));
            throw new Error('Vous n\'avez pas accès à cette page');
        }

        // Fetch resources
        let [mco, structures] = await Promise.all([
            data.fetchDictionary('mco'),
            data.fetchJSON(`${env.base_url}api/structures.json`)
        ]);

        // Check options
        if (!route.structure) {
            route.structure = structures[0].name;
        } else if (!structures.find(structure => structure.name === route.structure)) {
            log.error(`La structure '${route.structure}' n'existe pas`);
            route.structure = structures[0].name;
        }
        if (!route.main.units)
            route.main.units = new Set(structures[0].entities.map(ent => ent.unit));
        if (!route.main.ghm_roots)
            route.main.ghm_roots = new Set(mco.ghm_roots.definitions.map(ghm_root => ghm_root.code));

        // Run appropriate mode handler
        switch (route.mode) {
            case 'ghm': { await runSummary('ghm_root'); } break;
            case 'units': { await runSummary('unit'); } break;
            case 'valorisation': { await runValorisation(); } break;
            case 'rss': { await runRSS(); } break;

            default: {
                throw new Error(`Mode inconnu '${route.mode}'`);
            } break;
        }
    };

    this.parseURL = function(path, params = {}) {
        let args = {
            mode: path[0] || 'ghm',

            main: {},
            diff: {},
            valorisation: {}
        };
        if (!settings.permissions.mco_casemix)
            return args;

        // Authorized users
        args.main = {
            ...parseDateRange(path[1], settings.mco.casemix.max_date),
            algorithm: params.algorithm || settings.mco.casemix.default_algorithm,
            units: params.units != null ? decodeUnits(params.units) : null,
            ghm_roots: params.ghm_roots != null ? decodeGhmRoots(params.ghm_roots) : null
        };
        args.coeff = !!parseInt(params.coeff, 10) || false;
        args.structure = params.structure || null;

        // Mode-specific part
        switch (args.mode) {
            case 'valorisation': {
                args.valorisation.ghm_root = path[2] || null;
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

    function decodeUnits(str) {
        let units = new Set;

        try {
            let encoded = atob(str);

            let unit = 0;
            for (let i = 0; i < encoded.length; i) {
                let x = encoded.charCodeAt(i++);

                if (x & 0x40) {
                    x &= ~0x40;
                    unit += (x & ~0x40);
                } else {
                    let y = encoded.charCodeAt(i++);
                    let z = encoded.charCodeAt(i++);
                    unit = (x << 12) + (y << 6) + z;
                }

                units.add(unit);
            }
        } catch (err) {
            log.error('La chaîne d\'unités n\'est pas valide');
        }

        return units;
    }

    function decodeGhmRoots(str) {
        let ghm_roots = new Set;

        try {
            let encoded = atob(str);

            let x;
            let y;
            for (let i = 0; i < encoded.length;) {
                let z = encoded.charCodeAt(i++);
                if (z & 0x40) {
                    z &= ~0x40;
                } else {
                    x = z || 90;
                    y = encoded[i++];
                    z = encoded.charCodeAt(i++);
                }

                let ghm_root = ('' + x).padStart(2, '0') + y + ('' + z).padStart(2, '0');
                ghm_roots.add(ghm_root);
            }
        } catch (err) {
            log.error('La chaîne de racines de GHM n\'est pas valide');
        }

        return ghm_roots;
    }

    this.makeURL = function(args = {}) {
        args = util.assignDeep({}, route, args);

        let path = ['mco_casemix'];
        let params = {};

        // Common part
        path.push(args.mode);
        if (args.main.start_date && args.main.end_date)
            path.push(`${args.main.start_date}..${args.main.end_date}`);

        params.coeff = (args.coeff != null) ? (0 + args.coeff) : null;
        params.structure = args.structure;
        params.algorithm = args.main.algorithm;
        params.ghm_roots = args.main.ghm_roots ? encodeGhmRoots(args.main.ghm_roots) : null;
        params.units = args.main.units ? encodeUnits(args.main.units) : null;

        // Mode-specific part
        switch (args.mode) {
            case 'valorisation': {
                if (args.valorisation.ghm_root)
                    path.push(args.valorisation.ghm_root);
            } break;
        }

        return util.pasteURL(`${env.base_url}${path.join('/')}`, params);
    };

    function encodeUnits(units) {
        units = Array.from(units);

        let encoded = '';
        for (let i = 0; i < units.length; i++) {
            let delta = units[i] - (i ? units[i - 1] : Infinity);
            if (delta > 0 && delta < 64) {
                let delta = units[i] - units[i - 1];
                encoded += String.fromCharCode(delta | 0x40);
            } else {
                encoded += String.fromCharCode((units[i] >> 12) & 0x3F) +
                           String.fromCharCode((units[i] >> 6) & 0x3F) +
                           String.fromCharCode(units[i] & 0x3F);
            }
        }

        return btoa(encoded);
    }

    function encodeGhmRoots(ghm_roots) {
        ghm_roots = Array.from(ghm_roots);

        let encoded = '';

        for (let i = 0; i < ghm_roots.length; i++) {
            if (i && ghm_roots[i].substr(0, 3) == ghm_roots[i - 1].substr(0, 3)) {
                let z = parseInt(ghm_roots[i].substr(3), 10);
                encoded += String.fromCharCode((z & 0x3F) | 0x40);
            } else {
                let x = parseInt(ghm_roots[i].substr(0, 2), 10);
                if (x === 90)
                    x = 0;
                let y = ghm_roots[i][2];
                let z = parseInt(ghm_roots[i].substr(3), 10);
                encoded += String.fromCharCode(x & 0x3F) + y +
                                 String.fromCharCode(z & 0x3F);
            }
        }

        return btoa(encoded);
    }

    // ------------------------------------------------------------------------
    // Summaries
    // ------------------------------------------------------------------------

    async function runSummary(by) {
        let [mco, structures] = await Promise.all([
            data.fetchDictionary('mco'),
            data.fetchJSON(`${env.base_url}api/structures.json`)
        ]);

        // Options
        render(html`
            ${renderPeriodPicker(settings.mco.casemix.min_date, settings.mco.casemix.max_date,
                                 route.main.start_date, route.main.end_date)}
            ${renderAlgorithmSelector(settings.mco.casemix.algorithms, route.main.algorithm)}
            ${renderCoefficientCheckbox(route.coeff)}
            ${renderUnitTree(structures, route.structure, route.main.units)}
            ${renderGhmRootTree(mco, route.main.ghm_roots)}
        `, document.querySelector('#th_options'));

        // View
        render('', document.querySelector('#th_view'));
    }

    // ------------------------------------------------------------------------
    // Valorisation
    // ------------------------------------------------------------------------

    async function runValorisation() {
        let [mco, structures] = await Promise.all([
            data.fetchDictionary('mco'),
            data.fetchJSON(`${env.base_url}api/structures.json`)
        ]);

        if (!route.valorisation.ghm_root)
            route.valorisation.ghm_root = mco.ghm_roots.definitions[0].code;

        // Options
        render(html`
            ${renderPeriodPicker(settings.mco.casemix.min_date, settings.mco.casemix.max_date,
                                 route.main.start_date, route.main.end_date)}
            ${renderAlgorithmSelector(settings.mco.casemix.algorithms, route.main.algorithm)}
            ${renderCoefficientCheckbox(route.coeff)}
            ${renderUnitTree(structures, route.structure, route.main.units)}
            ${renderGhmRootSelector(mco, route.valorisation.ghm_root)}
        `, document.querySelector('#th_options'));

        // View
        render('', document.querySelector('#th_view'));
    }

    function renderGhmRootSelector(mco, current_ghm_root) {
        return html`
            <select @change=${e => thop.go(self, {valorisation: {ghm_root: e.target.value}})}>
                ${mco.ghm_roots.definitions.map(defn => {
                    let disabled = false;
                    let label = `${defn.describe()}${disabled ? ' *' : ''}`;

                    return html`<option value=${defn.code} ?disabled=${disabled}
                                        .selected=${defn.code === current_ghm_root}>${label}</option>`
                })}
            </select>
        `;
    }

    // ------------------------------------------------------------------------
    // RSS
    // ------------------------------------------------------------------------

    async function runRSS() {
        let [mco, structures] = await Promise.all([
            data.fetchDictionary('mco'),
            data.fetchJSON(`${env.base_url}api/structures.json`)
        ]);

        // Options
        render(html`
            ${renderPeriodPicker(settings.mco.casemix.min_date, settings.mco.casemix.max_date,
                                 route.main.start_date, route.main.end_date)}
            ${renderAlgorithmSelector(settings.mco.casemix.algorithms, route.main.algorithm)}
            ${renderCoefficientCheckbox(route.coeff)}
            ${renderUnitTree(structures, route.structure, route.main.units)}
            ${renderGhmRootTree(mco, route.main.ghm_roots)}
        `, document.querySelector('#th_options'));

        // View
        render('', document.querySelector('#th_view'));
    }

    // ------------------------------------------------------------------------
    // Common options
    // ------------------------------------------------------------------------

    function renderPeriodPicker(min_date, max_date, start_date, end_date) {
        let ppik = new PeriodPicker;

        ppik.clickHandler = (e, start_date, end_date) =>
            thop.go(self, {main: {start_date: start_date, end_date: end_date}});

        ppik.setRange(min_date, max_date);
        ppik.setDates(start_date, end_date);

        return ppik.render();
    }

    function renderAlgorithmSelector(algorithms, current_algorithm) {
        return html`
            <label>Algorithme <select @change=${e => thop.go(self, {main: {algorithm: e.target.value}})}>
                ${settings.mco.casemix.algorithms.map(algo => {
                    let selected = (route.main.algorithm === algo);
                    let label = `Algorithme ${algo}`;

                    return html`<option value=${algo} .selected=${selected}>${label}</option>`;
                })}
            </select></label>
        `;
    }

    function renderCoefficientCheckbox(current_coeff) {
        return html`<label>Coefficient <input type="checkbox" .checked=${current_coeff}
                                              @change=${e => thop.go(self, {coeff: e.target.checked})}/></label>`;
    }

    function renderUnitTree(structures, current_tab, current_units) {
        let tsel = new TreeSelector;

        tsel.clickHandler = (e, values, tab) =>
            thop.go(self, {main: {units: values}, structure: tab});

        tsel.setPrefix('Unités : ');
        for (let structure of structures) {
            tsel.addTab(structure.name);
            for (ent of structure.entities)
                tsel.addOption(ent.path, ent.unit, {selected: current_units.has(ent.unit)});
        }
        tsel.setCurrentTab(current_tab);

        return tsel.render();
    }

    function renderGhmRootTree(mco, current_ghm_roots) {
        let tsel = new TreeSelector;

        tsel.clickHandler = (e, values, tab) =>
            thop.go(self, {main: {ghm_roots: values}});

        tsel.setPrefix('Racines de GHM : ');
        tsel.addTab('roots', 'Racines');
        for (let defn of mco.ghm_roots.definitions)
            tsel.addOption(defn.describe(), defn.code, {selected: current_ghm_roots.has(defn.code)});
        for (let group of mco.ghm_roots.parents) {
            tsel.addTab(group, mco[group].title);
            for (let defn of mco.ghm_roots.definitions) {
                let title = [defn.describeParent(group), defn.describe()];
                tsel.addOption(title, defn.code, {selected: current_ghm_roots.has(defn.code)});
            }
        }
        tsel.setCurrentTab('roots');

        return tsel.render();
    }
};
