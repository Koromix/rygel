<?php

require_once(__DIR__ . "/util.php");

function open_database() {
    $db = new SQLite3(__DIR__ . "/../data/news.db", SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE);

    $schema = 2;
    $version = $db->querySingle("PRAGMA user_version");

    try {
        $db->query("BEGIN TRANSACTION");

        switch ($version) {
            case 0: {
                $db->query("
                    CREATE TABLE IF NOT EXISTS news (
                        id INTEGER,
                        image TEXT NOT NULL,
                        title TEXT NOT NULL,
                        content TEXT NOT NULL,
                        date INTEGER,
                        duration INTEGER,

                        PRIMARY KEY('id')
                    );
                ");
            } // fallthrough

            case 1: {
                $db->query("
                    ALTER TABLE news RENAME TO news_BAK;

                    CREATE TABLE news (
                        id INTEGER,
                        png BLOB,
                        title TEXT NOT NULL,
                        content TEXT NOT NULL,
                        date INTEGER,
                        duration INTEGER,

                        changeset TEXT,

                        PRIMARY KEY('id')
                    );

                    INSERT INTO news (id, title, content, date, duration)
                        SELECT id, title, content, date, duration FROM news_BAK;

                    DROP TABLE news_BAK;
                ");
            } // fallthrough
        }

        $db->query("PRAGMA user_version = " . $schema);
        $db->query("COMMIT");
    } catch (Throwable $e) {
        $db->query("ROLLBACK");
        throw $e;
    }

    return $db;
}

function fetch_all($res) {
    $rows = [];
    while ($row = $res->fetchArray(SQLITE3_ASSOC))
        $rows[] = $row;
    return $rows;
}

?>