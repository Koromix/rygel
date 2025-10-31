// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import path from 'path';
import sqlite3 from 'better-sqlite3';

const DATABASE = path.resolve('.', process.env.DATABASE || 'data/napka.db');
const SCHEMA = 17;

function open(options = {}) {
    let db = sqlite3(DATABASE, options);

    db.pragma('journal_mode = WAL');

    let version = db.pragma('user_version', { simple: true });

    if (version != SCHEMA) {
        if (version > SCHEMA)
            throw new Error('Database schema is too recent');

        migrate(db, version);
    }

    return db;
}

function migrate(db, version) {
    console.log(`Migrating database from ${version} to ${SCHEMA}`);

    db.transaction(() => {
        switch (version) {
            case 0: {
                db.exec(`
                    -- Create initial version

                    CREATE TABLE d_etablissements (
                        id TEXT NOT NULL,
                        finess TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        active INTEGER NOT NULL,

                        etab_nom TEXT,
                        etab_adresse TEXT,
                        etab_adresse_info TEXT,
                        etab_latitude REAL,
                        etab_longitude REAL,
                        etab_champs TEXT,
                        etab_acces_pmr INTEGER,
                        etab_complement TEXT,
                        etab_web TEXT,
                        etab_crp INTEGER,
                        etab_type TEXT,

                        rdv_fixe TEXT,
                        rdv_portable TEXT,
                        rdv_mail TEXT,
                        rdv_web TEXT,
                        rdv_courrier_mt INTEGER,
                        rdv_horaires_str TEXT,
                        rdv_horaires TEXT,
                        rdv_duree INTEGER,
                        rdv_publics TEXT,
                        rdv_publics_specifiques TEXT,
                        rdv_publics_autres TEXT,
                        rdv_langues_etrangeres TEXT,
                        rdv_psychotraumas TEXT,
                        rdv_psychotraumas_remarques TEXT,
                        rdv_consultations TEXT,
                        rdv_modalites TEXT,

                        educ_formations TEXT,
                        educ_accord INTEGER,

                        engag_adhesion INTEGER,
                        engag_informes INTEGER,
                        engag_has INTEGER,
                        engag_identite TEXT,
                        engag_honneur TEXT,
                        engag_accord TEXT,
                        engag_mail INTEGER,

                        UNIQUE(id, mtime)
                    );
                    CREATE INDEX d_etablissements_a ON d_etablissements (active);
                    CREATE TABLE d_etablissements_individus (
                        id TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        active INTEGER NOT NULL,

                        nom TEXT,
                        prenom TEXT,
                        profession TEXT,
                        profession_autre TEXT,
                        experience_espt TEXT
                    );
                    CREATE INDEX d_etablissements_individus_a ON d_etablissements_individus (active);

                    CREATE VIEW v_etablissements AS SELECT * FROM d_etablissements WHERE active = 1;
                    CREATE VIEW v_etablissements_individus AS SELECT * FROM d_etablissements_individus WHERE active = 1;

                    CREATE TABLE d_professionnels (
                        siret TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        active INTEGER NOT NULL,

                        sexe TEXT,
                        nom TEXT,
                        prenom TEXT,
                        profession TEXT,
                        telephone TEXT,
                        outils_psycho TEXT,
                        outils_psycho_autres TEXT,
                        contextes TEXT,
                        contextes_autres TEXT,

                        engag_charte INTEGER,
                        engag_has INTEGER,
                        engag_honneur INTEGER,
                        engag_accord INTEGER,

                        UNIQUE(siret, mtime)
                    );
                    CREATE INDEX d_professionnels_a ON d_professionnels (active);
                    CREATE TABLE d_professionnels_experiences (
                        siret TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        active INTEGER NOT NULL,

                        structure TEXT,
                        date_debut TEXT,
                        date_fin TEXT,
                        description TEXT,
                        attestation_pro_name TEXT,
                        attestation_pro_file BLOB,
                        attestation_honneur_name TEXT,
                        attestation_honneur_file BLOB
                    );
                    CREATE INDEX d_professionnels_experiences_a ON d_professionnels_experiences (active);
                    CREATE TABLE d_professionnels_attestations (
                        siret TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        active INTEGER NOT NULL,

                        type TEXT,
                        intitule TEXT,
                        annee INTEGER,
                        etablissement TEXT,
                        fichier_name TEXT,
                        fichier_file BLOB
                    );
                    CREATE INDEX d_professionnels_attestations_a ON d_professionnels_attestations (active);
                    CREATE TABLE d_professionnels_lieux (
                        siret TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        active INTEGER NOT NULL,

                        structure TEXT,
                        adresse TEXT,
                        adresse_info TEXT,
                        acces_pmr INTEGER,
                        fixe TEXT,
                        portable TEXT,
                        mail TEXT,
                        web TEXT,
                        publics TEXT,
                        publics_specifiques TEXT,
                        langues_etrangeres TEXT,
                        type_psychotrauma TEXT,
                        type_consult TEXT,
                        modalites TEXT,
                        horaires_str TEXT,
                        horaires TEXT,
                        duree INTEGER,
                        psycho_tarif TEXT,
                        psycho_tarif_particularites TEXT,
                        med_convention TEXT,
                        med_tarif TEXT,
                        med_tarif_particularites TEXT
                    );
                    CREATE INDEX d_professionnels_lieux_a ON d_professionnels_lieux (active);

                    CREATE VIEW v_professionnels AS SELECT * FROM d_professionnels WHERE active = 1;
                    CREATE VIEW v_professionnels_experiences AS SELECT * FROM d_professionnels_experiences WHERE active = 1;
                    CREATE VIEW v_professionnels_attestations AS SELECT * FROM d_professionnels_attestations WHERE active = 1;
                    CREATE VIEW v_professionnels_lieux AS SELECT * FROM d_professionnels_lieux WHERE active = 1;

                    -- Add field for more information

                    ALTER TABLE d_etablissements ADD COLUMN rdv_complement TEXT;

                    -- Detect status when importing

                    ALTER TABLE d_etablissements ADD COLUMN etab_statut TEXT;

                    DROP TABLE IF EXISTS n_etablissements;
                    DROP TABLE IF EXISTS n_entites_juridiques;

                    -- Enforce valid status

                    ALTER TABLE d_etablissements RENAME COLUMN etab_statut TO etab_statut_BAK;
                    ALTER TABLE d_etablissements ADD COLUMN etab_statut TEXT CHECK(etab_statut IN ('public', 'prive_non_lucratif', 'prive_lucratif'));
                    UPDATE d_etablissements SET etab_statut = etab_statut_BAK;
                    ALTER TABLE d_etablissements DROP COLUMN etab_statut_BAK;

                    -- Drop structured address column

                    ALTER TABLE d_etablissements DROP COLUMN etab_adresse_info;
                `);
            } // fallthrough

            case 1: {
                db.exec(`
                    DROP VIEW v_etablissements;
                    DROP VIEW v_etablissements_individus;
                    DROP VIEW v_professionnels;
                    DROP VIEW v_professionnels_attestations;
                    DROP VIEW v_professionnels_experiences;
                    DROP VIEW v_professionnels_lieux;

                    ALTER TABLE d_etablissements RENAME TO etablissements;
                    ALTER TABLE d_etablissements_individus RENAME TO etablissements_individus;
                    ALTER TABLE d_professionnels RENAME TO professionnels;
                    ALTER TABLE d_professionnels_attestations RENAME TO professionnels_attestations;
                    ALTER TABLE d_professionnels_experiences RENAME TO professionnels_experiences;
                    ALTER TABLE d_professionnels_lieux RENAME TO professionnels_lieux;

                    DROP INDEX d_etablissements_a;
                    ALTER TABLE etablissements DROP COLUMN active;
                    DROP INDEX d_etablissements_individus_a;
                    ALTER TABLE etablissements_individus DROP COLUMN active;
                    DROP INDEX d_professionnels_a;
                    ALTER TABLE professionnels DROP COLUMN active;
                    DROP INDEX d_professionnels_attestations_a;
                    ALTER TABLE professionnels_attestations DROP COLUMN active;
                    DROP INDEX d_professionnels_experiences_a;
                    ALTER TABLE professionnels_experiences DROP COLUMN active;
                    DROP INDEX d_professionnels_lieux_a;
                    ALTER TABLE professionnels_lieux DROP COLUMN active;
                `);
            } // fallthrough

            case 2: {
                let st =

                db.exec(`
                    CREATE TABLE maps (
                        id INTEGER PRIMARY KEY,
                        name TEXT NOT NULL,
                        title TEXT NOT NULL,
                        mail TEXT NOT NULL,
                        fields TEXT NOT NULL
                    );

                    CREATE TABLE entries (
                        id INTEGER PRIMARY KEY,
                        map_id INTEGER NOT NULL REFERENCES maps (id),
                        import TEXT,
                        mtime TEXT NOT NULL,
                        name TEXT NOT NULL,
                        address TEXT NOT NULL,
                        latitude REAL,
                        longitude REAL,
                        main TEXT NOT NULL,
                        extra TEXT NOT NULL
                    );

                    CREATE UNIQUE INDEX entries_mim ON entries (map_id, import, mtime);
                `);

                db.prepare('INSERT INTO maps (id, name, title, mail, fields) VALUES (?, ?, ?, ?, ?)')
                  .run(1, 'cn2r', 'CN2R', 'contact@cn2r.fr', JSON.stringify({
                    etab_champs: 'json',
                    rdv_horaires: 'json',
                    rdv_publics: 'json',
                    rdv_publics_specifiques: 'json',
                    rdv_psychotraumas: 'json',
                    rdv_consultations: 'json',
                    rdv_modalites: 'json',
                    educ_formations: 'json',
                    rdv_complement: 'markdown'
                }));

                let etablissements = db.prepare('SELECT * FROM etablissements').all();
                let individus = db.prepare('SELECT * FROM etablissements_individus').all();

                let entries = [];
                let map = {};

                for (let etab of etablissements) {
                    let entry = {
                        map_id: 1,
                        import: null,
                        mtime: null,
                        name: null,
                        address: null,
                        latitude: null,
                        longitude: null,
                        main: {},
                        extra: {}
                    };

                    for (let key in etab) {
                        if (Object.hasOwn(entry, key)) {
                            entry[key] = etab[key];
                        } else if (key == 'id') {
                            entry.import = etab[key];
                        } else if (key == 'etab_nom') {
                            entry.name = etab[key];
                        } else if (key == 'etab_adresse') {
                            entry.address = etab[key];
                        } else if (key == 'etab_statut') {
                            switch (etab.etab_statut) {
                                case 'public': { entry.main['etab_statut'] = 'Etablissement public'; } break;
                                case 'prive_non_lucratif': { entry.main['etab_statut'] = 'Etablissement privé à but non lucratif'; } break;
                                case 'prive_lucratif': { entry.main['etab_statut'] = 'Etablissement privé à but lucratif'; } break;
                            }
                        } else if (key.startsWith('etab_') &&
                                   Object.hasOwn(entry, key.substr(5))) {
                            entry[key.substr(5)] = etab[key];
                        } else {
                            entry.main[key] = etab[key];
                        }
                    }

                    map[entry.import] = entry;
                    entries.push(entry);
                }

                for (let indiv of individus) {
                    let entry = map[indiv.id];

                    if (entry == null)
                        continue;
                    if (entry.extra.individus == null)
                        entry.extra.individus = [];

                    entry.extra.individus.push({
                        nom: indiv.nom,
                        prenom: indiv.prenom,
                        profession: indiv.profession,
                        profession_autre: indiv.profession_autre
                    });
                }

                for (let entry of entries) {
                    entry.main = JSON.stringify(entry.main);
                    entry.extra = JSON.stringify(entry.extra);

                    db.prepare(`INSERT INTO entries (${Object.keys(entry).join(', ')})
                                VALUES (${Object.keys(entry).fill('?').join(', ')})`).run(...Object.values(entry));
                }

                db.exec(`
                    DROP TABLE etablissements;
                    DROP TABLE etablissements_individus;
                    DROP TABLE professionnels;
                    DROP TABLE professionnels_attestations;
                    DROP TABLE professionnels_experiences;
                    DROP TABLE professionnels_lieux;
                `);
            } // fallthrough

            case 3: {
                db.exec(`
                    ALTER TABLE entries RENAME TO entries_BAK;
                    DROP INDEX entries_mim;

                    CREATE TABLE layers (
                        id INTEGER PRIMARY KEY,
                        map_id INTEGER NOT NULL REFERENCES maps (id),
                        name TEXT NOT NULL,
                        title TEXT NOT NULL,
                        fields TEXT NOT NULL
                    );

                    INSERT INTO layers (id, map_id, name, title, fields)
                        SELECT 1, 1, 'etablissements', 'Établissements', fields FROM maps WHERE name = 'cn2r';

                    CREATE TABLE entries (
                        id INTEGER PRIMARY KEY,
                        layer_id INTEGER NOT NULL REFERENCES layers (id),
                        import TEXT,
                        mtime TEXT NOT NULL,
                        name TEXT NOT NULL,
                        address TEXT NOT NULL,
                        latitude REAL,
                        longitude REAL,
                        main TEXT NOT NULL,
                        extra TEXT NOT NULL
                    );

                    CREATE UNIQUE INDEX entries_lim ON entries (layer_id, import, mtime);

                    INSERT INTO entries (id, layer_id, import, mtime, name, address, latitude, longitude, main, extra)
                        SELECT id, map_id, import, mtime, name, address, latitude, longitude, main, extra FROM entries_BAK;

                    ALTER TABLE maps DROP COLUMN fields;
                    DROP TABLE entries_BAK;
                `);
            } // fallthrough

            case 4: {
                db.exec(`
                    CREATE UNIQUE INDEX maps_n ON maps (name);
                    CREATE UNIQUE INDEX layers_mn ON layers (map_id, name);
                `);
            } // fallthrough

            case 5: {
                db.exec(`
                    DROP INDEX entries_lim;
                    ALTER TABLE entries RENAME mtime TO version;
                    CREATE UNIQUE INDEX entries_liv ON entries (layer_id, import, version);
                `);
            } // fallthrough

            case 6: {
                db.exec(`
                    DROP INDEX entries_liv;
                    DELETE FROM entries
                        WHERE id NOT IN (SELECT max(id) FROM entries GROUP BY layer_id, import);
                    CREATE UNIQUE INDEX entries_li ON entries (layer_id, import);
                `);
            } // fallthrough

            case 7: {
                db.exec(`
                    CREATE TABLE old_entries (
                        id INTEGER PRIMARY KEY,
                        layer_id INTEGER NOT NULL REFERENCES layers (id),
                        import TEXT,
                        version TEXT NOT NULL,
                        name TEXT NOT NULL,
                        address TEXT NOT NULL,
                        latitude REAL,
                        longitude REAL,
                        main TEXT NOT NULL,
                        extra TEXT NOT NULL
                    );

                    CREATE TRIGGER entries_to_old BEFORE DELETE ON entries
                    BEGIN
                        INSERT INTO old_entries (layer_id, import, version, name, address, latitude, longitude, main, extra)
                            VALUES (old.layer_id, old.import, old.version, old.name, old.address, old.latitude, old.longitude, old.main, old.extra);
                    END;
                `);
            } // fallthrough

            case 8: {
                db.exec(`
                    ALTER TABLE entries RENAME TO entries_BAK;
                    DROP INDEX entries_li;

                    CREATE TABLE entries (
                        id INTEGER PRIMARY KEY,
                        layer_id INTEGER NOT NULL REFERENCES layers (id),
                        import TEXT,
                        version TEXT NOT NULL,
                        hide INTEGER NOT NULL,
                        name TEXT NOT NULL,
                        address TEXT NOT NULL,
                        latitude REAL,
                        longitude REAL,
                        main TEXT NOT NULL,
                        extra TEXT NOT NULL
                    );

                    CREATE UNIQUE INDEX entries_li ON entries (layer_id, import);

                    INSERT INTO entries (id, layer_id, import, version, hide, name, address, latitude, longitude, main, extra)
                        SELECT id, layer_id, import, version, 0, name, address, latitude, longitude, main, extra FROM entries_BAK;

                    DROP TABLE entries_BAK;
                `);
            } // fallthrough

            case 9: {
                let entries = db.prepare(`SELECT e.id, e.layer_id, e.main, l.fields FROM entries e
                                              INNER JOIN layers l ON (l.id = e.layer_id)`).all();
                let layers = db.prepare('SELECT id, fields FROM layers').all();

                for (let entry of entries) {
                    let main = JSON.parse(entry.main);

                    let fields = JSON.parse(entry.fields);

                    for (let key in fields) {
                        if (fields[key] == 'json') {
                            if (typeof main[key] == 'string') {
                                try {
                                    main[key] = JSON.parse(main[key]);
                                } catch (err) {
                                    // Just fixing the mess where some data is JSON encoded (inside JSON)
                                }
                            }
                        }
                    }

                    db.prepare('UPDATE entries SET main = ? WHERE id = ?').run(JSON.stringify(main), entry.id);
                }

                for (let layer of layers) {
                    let fields = JSON.parse(layer.fields);

                    for (let key in fields) {
                        if (fields[key] == 'json')
                            delete fields[key];
                    }

                    db.prepare('UPDATE layers SET fields = ? WHERE id = ?').run(JSON.stringify(fields), layer.id);
                }
            } // fallthrough

            case 10: {
                // Default user: admin/admin

                db.exec(`
                    CREATE TABLE users (
                        userid INTEGER PRIMARY KEY,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL
                    );

                    CREATE UNIQUE INDEX users_u ON users (username);

                    INSERT INTO users (userid, username, password_hash) VALUES (1, 'admin', 'sha256$100000$vuisDlBincyEdTVZSCXQm1QqGmg=$HHoDWKoALst/wtjpmd1WQCPrgJnSpq52ACJUNIKLEPU=');
                `)
            } // fallthrough

            case 11: {
                db.exec(`
                    CREATE TRIGGER entries_to_old BEFORE UPDATE ON entries
                    BEGIN
                        INSERT INTO old_entries (layer_id, import, version, name, address, latitude, longitude, main, extra)
                            VALUES (old.layer_id, old.import, old.version, old.name, old.address, old.latitude, old.longitude, old.main, old.extra);
                    END;
                `)
            } // fallthrough

            case 12: {
                db.exec(`
                    DROP TABLE users;

                    CREATE TABLE users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL
                    );

                    CREATE UNIQUE INDEX users_u ON users (username);
                `)
            } // fallthrough

            case 13: {
                db.exec(`
                    CREATE TABLE tiles (
                        url TEXT NOT NULL PRIMARY KEY,
                        data BLOB NOT NULL,
                        type TEXT NOT NULL
                    );
                `);
            } // fallthrough

            case 14: {
                db.exec(`
                    DROP TABLE tiles;
                `);
            } // fallthrough

            case 15: {
                db.exec(`
                    ALTER TABLE maps ADD COLUMN style_id TEXT;
                `);
            } // fallthrough

            case 16: {
                db.exec(`
                    DROP TRIGGER entries_to_old;
                    DROP INDEX entries_li;

                    ALTER TABLE old_entries RENAME TO old_entries_BAK;
                    ALTER TABLE entries RENAME TO entries_BAK;

                    CREATE TABLE entries (
                        id INTEGER PRIMARY KEY,
                        layer_id INTEGER NOT NULL REFERENCES layers (id),
                        import TEXT,
                        version TEXT NOT NULL,
                        hide INTEGER NOT NULL,
                        name TEXT NOT NULL,
                        address TEXT,
                        invalid_address INTEGER CHECK(invalid_address IN (0, 1)) NOT NULL,
                        latitude REAL,
                        longitude REAL,
                        main TEXT NOT NULL,
                        extra TEXT NOT NULL
                    );
                    CREATE TABLE old_entries (
                        id INTEGER PRIMARY KEY,
                        layer_id INTEGER NOT NULL REFERENCES layers (id),
                        import TEXT,
                        version TEXT NOT NULL,
                        hide INTEGER NOT NULL,
                        name TEXT NOT NULL,
                        address TEXT,
                        invalid_address INTEGER CHECK(invalid_address IN (0, 1)) NOT NULL,
                        latitude REAL,
                        longitude REAL,
                        main TEXT NOT NULL,
                        extra TEXT NOT NULL
                    );

                    INSERT INTO entries (id, layer_id, import, version, hide, name, address, invalid_address, latitude, longitude, main, extra)
                        SELECT id, layer_id, import, version, hide, name, address, 0, latitude, longitude, main, extra FROM entries_BAK;
                    INSERT INTO old_entries (id, layer_id, import, version, hide, name, address, invalid_address, latitude, longitude, main, extra)
                        SELECT id, layer_id, import, version, 0, name, address, 0, latitude, longitude, main, extra FROM old_entries_BAK;

                    CREATE UNIQUE INDEX entries_li ON entries (layer_id, import);

                    CREATE TRIGGER entries_to_old BEFORE UPDATE ON entries
                    BEGIN
                        INSERT INTO old_entries (layer_id, import, version, hide, name, address, invalid_address, latitude, longitude, main, extra)
                            VALUES (old.layer_id, old.import, old.version, old.hide, old.name, old.address, old.invalid_address, old.latitude, old.longitude, old.main, old.extra);
                    END;

                    DROP TABLE entries_BAK;
                    DROP TABLE old_entries_BAK;
                `);
            } // fallthrough
        }

        db.pragma('user_version = ' + SCHEMA);
    })();
}

export { open }
