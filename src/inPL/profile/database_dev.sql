/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

-------------------- Structure --------------------

CREATE TABLE sc_resources (
    schedule TEXT NOT NULL CHECK(schedule IN ('pl', 'entreprise')),
    date TEXT NOT NULL,
    time INTEGER NOT NULL,

    slots INTEGER NOT NULL,
    overbook INTEGER NOT NULL
);
CREATE UNIQUE INDEX sc_resources_sdt ON sc_resources (schedule, date, time);

CREATE TABLE sc_meetings (
    schedule TEXT NOT NULL CHECK(schedule IN ('pl', 'entreprise')),
    date TEXT NOT NULL,
    time INTEGER NOT NULL,

    name TEXT NOT NULL
);
CREATE INDEX sc_meetings_sd ON sc_meetings (schedule, date);

-------------------- Test data --------------------

INSERT INTO sc_resources VALUES ('pl', '2019-04-01', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-01', 1130, 2, 0);
INSERT INTO sc_resources VALUES ('pl', '2019-04-02', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-02', 1130, 2, 0);
INSERT INTO sc_resources VALUES ('pl', '2019-04-03', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-03', 1130, 2, 0);
INSERT INTO sc_resources VALUES ('pl', '2019-04-04', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-04', 1130, 2, 0);
INSERT INTO sc_resources VALUES ('pl', '2019-04-05', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-05', 1130, 2, 0);

INSERT INTO sc_meetings VALUES ('pl', '2019-04-01', 730, 'Peter PARKER');
INSERT INTO sc_meetings VALUES ('pl', '2019-04-01', 730, 'Mary JANE');
INSERT INTO sc_meetings VALUES ('pl', '2019-04-01', 730, 'Gwen STACY');
INSERT INTO sc_meetings VALUES ('pl', '2019-04-02', 730, 'Clark KENT');
INSERT INTO sc_meetings VALUES ('pl', '2019-04-02', 1130, 'Lex LUTHOR');
