/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

-------------------- Structure --------------------

CREATE TABLE sc_activities (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL,
    slots_per_doctor INTEGER NOT NULL
);

CREATE TABLE sc_slots (
    id INTEGER PRIMARY KEY,
    activity_id INTEGER NOT NULL,
    date TEXT NOT NULL,
    time INTEGER NOT NULL,
    overbook INTEGER NOT NULL,

    FOREIGN KEY (activity_id) REFERENCES sc_activities(id)
);

CREATE TABLE sc_resources (
    slot_id INTEGER NOT NULL,
    type TEXT CHECK(type IN ('doctor')),
    name TEXT NOT NULL,

    FOREIGN KEY (slot_id) REFERENCES sc_slots(id)
);

-------------------- Test data --------------------

INSERT INTO sc_activities VALUES (1, 'pl', 1);
INSERT INTO sc_slots VALUES (1, 1, '2019-04-01', 730, 0);
INSERT INTO sc_slots VALUES (2, 1, '2019-04-01', 1130, 3);
INSERT INTO sc_slots VALUES (3, 1, '2019-04-02', 730, 0);
INSERT INTO sc_slots VALUES (4, 1, '2019-04-02', 1130, 1);
INSERT INTO sc_slots VALUES (5, 1, '2019-04-03', 730, 0);
INSERT INTO sc_slots VALUES (6, 1, '2019-04-03', 1130, 1);
INSERT INTO sc_resources VALUES (1, 'doctor', 'X');
INSERT INTO sc_resources VALUES (1, 'doctor', 'Y');
INSERT INTO sc_resources VALUES (2, 'doctor', 'X');
INSERT INTO sc_resources VALUES (3, 'doctor', 'X');
INSERT INTO sc_resources VALUES (4, 'doctor', 'X');
INSERT INTO sc_resources VALUES (5, 'doctor', 'X');
INSERT INTO sc_resources VALUES (6, 'doctor', 'X');
