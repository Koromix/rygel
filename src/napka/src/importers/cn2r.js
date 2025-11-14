// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import fs from 'fs';
import fetch from 'node-fetch';
import sqlite3 from 'better-sqlite3';
import * as database from '../lib/database.js';
import { Util } from 'src/core/web/base/base.js';
import * as parse from '../lib/parse.js';
import * as imp from '../lib/import.js';

const CN2R_DS_TOKEN = process.env.CN2R_DS_TOKEN || '';
const GOOGLE_API_KEY = process.env.GOOGLE_API_KEY || '';

const URL = 'https://www.demarches-simplifiees.fr/api/v2/graphql';
const DEMARCHE_STRUCTURES = 34851;

const MAPPINGS = {
    etab_champs: [
        'Consultation au sein du centre régional du psychotraumatisme',
        'Consultation "psychotraumatisme"',
        'Consultation "psychotraumatisme" réservée aux mineurs',
        'Consultation "psychotraumatisme" réservée aux adultes'
    ],

    etab_crp: 'boolean',
    etab_acces_pmr: 'boolean',

    etab_statut: [
        'Etablissement public',
        'Etablissement privé à but non lucratif',
        'Etablissement privé à but lucratif'
    ],

    rdv_horaires: 'schedule',

    rdv_fixe: 'telephone',
    rdv_portable: 'telephone',

    rdv_publics: [
        'Adulte',
        'Adolescent',
        'Enfant'
    ],

    rdv_publics_specifiques: [
        'Migrants',
        'Victimes de violences',
        'Autres'
    ],

    rdv_psychotraumas: [
        'Soins dans le cas de psychotraumatisme simple',
        'Soins dans le cas de psychotraumatisme complexe si le.la patient.e est stabilisé.e'
    ],

    rdv_consultations: [
        'Individuel',
        'Groupe',
        'Famille'
    ],

    rdv_modalites: [
        'Présentiel',
        'Téléconsultation',
        'Domicile'
    ],

    rdv_courrier_mt: 'boolean',
    rdv_complement: 'markdown'
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

        if (!CN2R_DS_TOKEN)
            errors.push('Missing CN2R_DS_TOKEN (for demarches-simplifiees.fr)');
        if (!GOOGLE_API_KEY)
            errors.push('Missing GOOGLE_API_KEY');

        if (errors.length)
            throw new Error(errors.join('\n'));
    }

    let db = database.open();

    // Create map and layers if needed
    db.transaction(() => {
        let map_id = db.prepare(`INSERT INTO maps (name, title, mail)
                                     VALUES ('cn2r', 'CN2R', 'contact@cn2r.fr')
                                     ON CONFLICT DO UPDATE SET title = excluded.title,
                                                               mail = excluded.mail
                                     RETURNING id`).pluck().get();

        let stmt = db.prepare(`INSERT INTO layers (map_id, name, title, fields) VALUES (?, ?, ?, ?)
                                   ON CONFLICT DO UPDATE SET title = excluded.title,
                                                             fields = excluded.fields`);

        stmt.run(map_id, 'etablissements', 'Établissements', JSON.stringify(MAPPINGS));
    })();

    let etablissements = await fetchDossiers(DEMARCHE_STRUCTURES);
    etablissements = etablissements.map(transformEtablissement);

    db.transaction(() => {
        imp.updateEntries(db, 'cn2r', 'etablissements', etablissements, { etab_personnel: 'individus' });
    })();
    await imp.geomapMissing(db, 'cn2r', GOOGLE_API_KEY);

    // Update missing statuses
    {
        let finesses = sqlite3('src/importers/finess.db', { readonly: true, fileMustExist: true });
        let rows = db.prepare(`SELECT e.id, json_extract(e.main, '$.finess') AS finess,
                                            json_extract(e.main, '$.etab_statut') AS statut
                                   FROM entries e
                                   INNER JOIN layers l ON (l.id = e.layer_id)
                                   INNER JOIN maps m ON (m.id = l.map_id)
                                   WHERE m.name = 'cn2r' AND l.name = 'etablissements'`).all();

        for (let row of rows) {
            let match = finesses.prepare('SELECT codesph FROM finess WHERE nofinesset = ? OR nofinessej = ?').get(row.finess, row.finess);
            let statut = match?.codesph;

            switch (statut) {
                case '1': { statut = 'Etablissement public'; } break;
                case '6': { statut = 'Etablissement privé à but non lucratif'; } break;
                default: { statut = 'Etablissement privé à but lucratif'; } break;
            }

            if (statut != row.statut)
                db.prepare(`UPDATE entries SET main = json_insert(main, '$.etab_statut', ?) WHERE id = ?`).run(statut, row.id);
        }
    }

    db.close();
}

async function fetchDossiers(demarche) {
    let response = await fetch('https://www.demarches-simplifiees.fr/api/v2/graphql', {
        method: 'POST',
        headers: {
            'Authorization': `Bearer token=${CN2R_DS_TOKEN}`,
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            query: `
                {
                    demarche(number: ${demarche}) {
                        dossiers {
                            nodes {
                                state,
                                archived,
                                dateDerniereModification,
                                demandeur {
                                    ... on PersonneMorale { siret }
                                }
                                champs {
                                    __typename,
                                    id,
                                    ... on TextChamp { value }
                                    ... on CiviliteChamp { value }
                                    ... on DateChamp { value }
                                    ... on CheckboxChamp { value }
                                    ... on IntegerNumberChamp { value }
                                    ... on DecimalNumberChamp { value }
                                    ... on MultipleDropDownListChamp { values }
                                    ... on PieceJustificativeChamp {
                                        file {
                                            filename,
                                            url
                                        }
                                    }
                                    ... on RepetitionChamp {
                                        champs {
                                            __typename,
                                            id,
                                            ... on TextChamp { value }
                                            ... on CiviliteChamp { value }
                                            ... on DateChamp { value }
                                            ... on CheckboxChamp { value }
                                            ... on IntegerNumberChamp { value }
                                            ... on DecimalNumberChamp { value }
                                            ... on MultipleDropDownListChamp { values }
                                            ... on PieceJustificativeChamp {
                                                file {
                                                    filename,
                                                    url
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            `
        })
    });
    let json = await response.json();

    if (json.errors != null)
        throw new Error(json.errors[0].message);

    let rows = [];
    for (let node of json.data.demarche.dossiers.nodes) {
        if (node.state == 'accepte') {
            let row = {
                demandeur: node.demandeur,
                mtime: node.dateDerniereModification,
                champs: null
            };
            row.champs = await mapFields(node.champs);

            rows.push(row);
        }
    }

    return rows;
}

async function mapFields(fields) {
    let obj = {};

    for (let field of fields) {
        let buf = Buffer.from(field.id, 'base64');
        let key = buf.toString('ascii');

        if (field.__typename === 'RepetitionChamp') {
            let rows = [];

            let keys = new Set;
            let ptr = [];
            rows.push(ptr);

            for (let it of field.champs) {
                if (keys.has(it.id)) {
                    keys.clear();
                    ptr = [];
                    rows.push(ptr);
                }

                keys.add(it.id);
                ptr.push(it);
            }

            for (let i = 0; i < rows.length; i++)
                rows[i] = await mapFields(rows[i]);

            obj[key] = rows;
        } else if (field.__typename === 'MultipleDropDownListChamp') {
            obj[key] = field.values;
        } else if (field.__typename === 'IntegerNumberChamp') {
            obj[key] = parseInt(field.value, 10);
        } else if (field.__typename === 'TextChamp') {
            obj[key] = field.value?.trim() || null;
        } else if (field.__typename === 'PieceJustificativeChamp') {
            if (field.file != null) {
                let response = await fetch(field.file.url);
                let buffer = await response.buffer();

                obj[key] = new FileInfo(field.file.filename, buffer);
            } else {
                obj[key] = new FileInfo();
            }
        } else if (field.value !== undefined) {
            obj[key] = field.value;
        }
    }

    return obj;
}

function transformEtablissement(row) {
    let structure = {
        import: null, // Computed below
        version: row.mtime,

        name: row.champs['Champ-1355232'],
        address: row.champs['Champ-1355231'],

        finess: ('' + row.champs['Champ-1355233']).padStart(9, '0'),
        etab_champs: row.champs['Champ-1420670'],
        etab_acces_pmr: row.champs['Champ-1355230'],
        etab_complement: row.champs['Champ-1355208'],
        etab_web: parse.cleanURL(row.champs['Champ-1421546']),
        etab_personnel: row.champs['Champ-1297932'].map(it => ({
            nom: it['Champ-1297934']?.toUpperCase(),
            prenom: it['Champ-1298132'],
            profession: it['Champ-1298137']?.replace(/^- /, ''),
            profession_autre: it['Champ-1298137'] === '- Autre' ? it['Champ-1298140'] : null,
            experience_espt: it['Champ-1358238'],
        })),

        rdv_fixe: parse.cleanPhoneNumber(row.champs['Champ-1355227']),
        rdv_portable: parse.cleanPhoneNumber(row.champs['Champ-1355226']),
        rdv_mail: parse.cleanMail(row.champs['Champ-1355225']),
        rdv_web: parse.cleanURL(row.champs['Champ-1355224']),
        rdv_courrier_mt: row.champs['Champ-1355228'],
        rdv_horaires_str: row.champs['Champ-1355212'],
        rdv_horaires: parse.parseSchedule(row.champs['Champ-1355212']),
        rdv_duree: row.champs['Champ-1355210'],
        rdv_publics: row.champs['Champ-1355221'],
        rdv_publics_specifiques: mapEnum(row.champs['Champ-1355220'], {
            "Femmes victimes de violences": "Victimes de violences"
        }),
        rdv_publics_autres: row.champs['Champ-1355219'],
        rdv_langues_etrangeres: row.champs['Champ-1355218'],
        rdv_psychotraumas: row.champs['Champ-1355217'],
        rdv_psychotraumas_remarques: row.champs['Champ-1355216'],
        rdv_consultations: mapEnum(row.champs['Champ-1355215'], {
            "Consultation individuelle": "Individuel",
            "En groupe" : "Groupe",
            "Avec des membres de la famille": "Famille"
        }),
        rdv_modalites: mapEnum(row.champs['Champ-1355214'], {
            "En présentiel": "Présentiel",
            "En téléconsultation": "Téléconsultation",
            "Au domicile de la ou des personnes souffrant de syndrome de stress post-traumatique": "Domicile"
        }),

        educ_formations: row.champs['Champ-1298210'],
        educ_accord: row.champs['Champ-1358688'],

        engag_adhesion: row.champs['Champ-1121351'],
        engag_informes: row.champs['Champ-1354349'],
        engag_has: row.champs['Champ-1121341'],
        engag_identite: row.champs['Champ-1298338'],
        engag_honneur: row.champs['Champ-1121882'],
        engag_accord: row.champs['Champ-1121889'],
        engag_mail: parse.cleanMail(row.champs['Champ-1358242'])
    };

    structure.import = `${structure.finess}@${structure.rdv_fixe || 'NULL'}`;
    structure.etab_personnel = structure.etab_personnel.filter(p => p.nom != null);

    return structure;
}

function mapEnum(value, mapping) {
    if (Array.isArray(value)) {
        return value.map(it => mapEnum(it, mapping));
    } else {
        let ret = mapping[value];
        return (ret != null) ? ret : value;
    }
}
