/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

CREATE TABLE gp_values (
    id INTEGER PRIMARY KEY,
    table_name TEXT NOT NULL,
    entity_id INTEGER NOT NULL,
    key TEXT NOT NULL,
    value BLOB,
    creation_date INTEGER NOT NULL,
    change_date INTEGER NOT NULL
);
