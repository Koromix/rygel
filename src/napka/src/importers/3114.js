// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import fs from 'fs';
import fetch from 'node-fetch';
import sqlite3 from 'better-sqlite3';
import * as database from '../lib/database.js';
import { Util } from 'lib/web/base/base.js';
import * as parse from '../lib/parse.js';
import * as imp from '../lib/import.js';

const F2RSM_EXPORT_KEY = process.env.F2RSM_EXPORT_KEY || '';
const GOOGLE_API_KEY = process.env.GOOGLE_API_KEY || '';

const EXPORT_URL = 'https://f2rsm.goupile.org/2nps/api/records/export';

const MAPPINGS = {
    categorie: {
        1: `Sanitaire hors psychiatrie`,
        2: `Psychiatrie du secteur public`,
        3: `Psychiatrie du secteur privé (cliniques privées)`,
        8: `Psychiatrie du secteur privé (à but non lucratif)`,
        4: `Psychiatrie libérale`,
        5: `Psychologue libéral`,
        6: `Médecin généraliste`,
        7: `Médico-social / social`,
        9: `Associations (hors ligne d'écoute)`,
        10: `Ligne d'écoute`,
        11: `Organisme de formation`
    },
    categorie2_1: {
        1: `Urgences`,
        2: `Gériatrie`,
        3: `Pédiatrie`,
        4: `Addictologie`,
        99: `Autre`
    },
    categorie2_2: {
        1: `Hospitalisation de jour (HDJ)`,
        2: `Hospitalisation complète`,
        3: `Centre médico-psychologique (CMP)`,
        4: `Centre d'accueil Thérapeutique à temps partiel (CATTP)`,
        5: `Consultations d'urgence`,
        99: `Autre`
    },
    categorie2_3: {
        1: `Hospitalisation de jour (HDJ)`,
        2: `Hospitalisation complète`,
        99: `Autre`
    },
    categorie2_7: {
        1: `Centre Commun d'Action Sociale (CCAS)`,
        2: `Centre Médico-Psycho-Pédagogique (CMPP)`,
        3: `Foyer d'hébergement`,
        4: `Maison d'Accueil Spécialisée (MAS)`,
        5: `Foyer d'Accueil Médicalisé (FAM)`,
        6: `Institut Médico-Éducatif`,
        7: `Etablissement social`,
        99: `Autre`
    },
    categorie2_8: {
        1: `Hospitalisation de jour (HDJ)`,
        2: `Hospitalisation complète`,
        99: `Autre`
    },
    modalites: {
        1: `Sur place`,
        2: `Visio`,
        3: `Téléphone`,
        4: `Tchat`,
        99: `Autre`
    },
    public: {
        1: `Enfants`,
        2: `Adultes`
    },
    publics: {
        1: `Professions particulières`,
        2: `Populations à risque de discrimination`,
        3: `Harcèlement / violences psychiques`,
        4: `Violences physiques ou sexuelles`,
        5: `Isolement / solitude`,
        6: `Soutien aux pairs aidants`,
        7: `Difficultés financières`,
        8: `Difficultés judiciaires / administratives`,
        9: `Évènements de vie`,
        10: `Problématiques médicales`,
        11: `Problématiques spécifiques de tranches d'âges`,
        99: `Autre`
    },
    cout: {
        1: `Gratuit dans tous les cas`,
        2: `Gratuit sous conditions`,
        3: `Non gratuit`
    },
    delai_pec: {
        1: `Urgence (immédiat)`,
        2: `Très rapide (24 à 72h)`,
        3: `Rapide (dans la semaine)`,
        4: `Moyen (dans le mois)`,
        5: `Long (plus d'1 mois)`
    },
    rayon: {
        1: `Local`,
        2: `Régional`,
        3: `France métropolitaine`,
        4: `France métropolitaine + DOM-TOM`
    }
};

main();

async function main() {
    try {
        await run();
        console.log('Success!');
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function run() {
    // Check prerequisites
    {
        let errors = [];

        if (!F2RSM_EXPORT_KEY)
            errors.push('Missing F2RSM_EXPORT_KEY (export key for Goupile 3114 project)');
        if (!GOOGLE_API_KEY)
            errors.push('Missing GOOGLE_API_KEY');

        if (errors.length)
            throw new Error(errors.join('\n'));
    }

    let db = database.open();

    // Create map and layers if needed
    db.transaction(() => {
        let map_id = db.prepare(`INSERT INTO maps (name, title, mail)
                                     VALUES ('3114', '3114', 'annuaire@3114.fr')
                                     ON CONFLICT DO UPDATE SET title = excluded.title,
                                                               mail = excluded.mail
                                     RETURNING id`).pluck().get();

        let stmt = db.prepare(`INSERT INTO layers (map_id, name, title, fields) VALUES (?, ?, ?, ?)
                                   ON CONFLICT DO UPDATE SET title = excluded.title,
                                                             fields = excluded.fields`);

        stmt.run(map_id, 'commun', 'Commun', '{}');
    })();

    // Load online database
    let rows;
    {
        let response = await fetch(EXPORT_URL, {
            headers: {
                'X-API-Key': F2RSM_EXPORT_KEY
            }
        });

        if (!response.ok) {
            let text = (await response.text()).trim();
            throw new Error(text);
        }

        let blob = await response.blob();
        let buffer = Buffer.from(await blob.arrayBuffer());
        let filename = './tmp/3114.db';

        fs.writeFileSync(filename, buffer);

        let extract = sqlite3(filename, { fileMustExist: true, readonly: true });

        rows = extract.prepare(`SELECT e.*, a.*, c.*, i.*
                                FROM etablissement e
                                LEFT JOIN accueil a ON (a.__ROOT = e.__ROOT)
                                LEFT JOIN cible c ON (c.__ROOT = e.__ROOT)
                                LEFT JOIN informations i ON (i.__ROOT = e.__ROOT)`).all();

        extract.close();
    }

    // Clean up rows
    let entries = [];
    for (let row of rows) {
        if (row.__ULID == null)
            continue;

        let entry = transformRow(row);
        entries.push(entry);
    }

    // Update entries and fields
    db.transaction(() => {
        imp.updateEntries(db, '3114', 'commun', entries);
    })();
    await imp.geomapMissing(db, '3114', GOOGLE_API_KEY);

    db.close();
}

function transformRow(row) {
    let copy = Util.assignDeep({
        import: row.__ROOT,
        version: row.__MTIME,

        name: row.nom_complet,
        address: row.adresse
    }, row);

    delete copy.__ROOT;
    delete copy.__ULID;
    delete copy.__HID;
    delete copy.__CTIME;
    delete copy.__MTIME;
    delete copy.nom_complet;
    delete copy.adresse;

    // Update horaires
    {
        let horaires = [];

        for (let type of ['rdv', 'libre']) {
            for (let day = 1; day <= 7; day++) {
                let start = findNext(copy, day, 0, type, true);

                while (start < 96) {
                    let end = findNext(copy, day, start + 1, type, false);

                    let horaire = {
                        type: type,
                        jour: day,
                        debut: indexToHHMM(start),
                        fin: indexToHHMM(end)
                    };
                    horaires.push(horaire);

                    start = findNext(copy, day, end, type, true);
                }
            }
        }

        copy.horaires = horaires.length ? horaires : null;

        for (let type of ['rdv', 'libre']) {
            for (let day = 1; day <= 7; day++) {
                for (let h = 0; h <= 24; h++) {
                    delete copy[`horaires.${day}_${h}_0_${type}`];
                    delete copy[`horaires.${day}_${h}_15_${type}`];
                    delete copy[`horaires.${day}_${h}_30_${type}`];
                    delete copy[`horaires.${day}_${h}_45_${type}`];
                    delete copy[`horaires.${day}_${h}_60_${type}`];
                }
            }
        }
    }

    // Clean up multi-valued values
    for (let key in copy) {
        let offset = key.indexOf('.');

        if (offset >= 0) {
            if (copy[key]) {
                let prefix = key.substr(0, offset);
                let suffix = key.substr(offset + 1);

                let values = copy[prefix];
                if (!Array.isArray(values)) {
                    values = [];
                    copy[prefix] = values;
                }

                let value = transformValue(prefix, suffix);
                values.push(value);
            }

            delete copy[key];
        } else {
            copy[key] = transformValue(key, copy[key]);
        }
    }

    // Minor fixups
    if (row.categorie2 != null) {
        let test = 'categorie2_' + row.categorie;

        if (MAPPINGS[test] != null) {
            copy.categorie2 = transformValue(test, copy.categorie2);
        } else {
            delete copy.categorie2;
        }
    }
    if (row.tel != null)
        copy.tel = row.tel.replace(/[^0-9]/g, '');
    if (row.www != null && !row.www.match(/^https?:/))
        row.www = 'https://' + row.www;

    return copy;
}

function transformValue(key, value) {
    let dict = MAPPINGS[key];

    if (dict != null && dict[value] != null) {
        return dict[value];
    } else if (Util.isNumeric(value)) {
        return parseFloat(value);
    } else {
        return value;
    }
}

function findNext(row, day, start, type, expected) {
    for (let i = start; i <= 96; i++) {
        let key = indexToKey(day, i, type);

        if (!!row[key] == expected)
            return i;
    }

    return 96;
}

function indexToKey(day, idx, type) {
    let h = Math.floor(idx / 4);
    let m = idx % 4 * 15;

    return `horaires.${day}_${h}_${m}_${type}`;
}

function indexToHHMM(idx) {
    let h = Math.floor(idx / 4);
    let m = idx % 4 * 15;

    return h * 100 + m;
}
