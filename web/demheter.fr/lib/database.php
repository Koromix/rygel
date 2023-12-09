<?php

require_once(__DIR__ . "/util.php");

function open_database() {
    $db = new SQLite3(__DIR__ . "/../data/news.db", SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE);

    $schema = 3;
    $version = $db->querySingle("PRAGMA user_version");

    try {
        $db->exec("BEGIN TRANSACTION");

        switch ($version) {
            case 0: {
                $db->exec("
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
                $db->exec("
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

            case 2: {
                $res = $db->query("SELECT id, png FROM news");
                $news = fetch_all($res);

                foreach ($news as $item) {
                    if ($item["png"] == null)
                        continue;

                    $stream = $db->openBlob("news", "png", $item["id"]);
                    $png = stream_get_contents($stream);
                    fclose($stream);

                    $sha256 = hash("sha256", $png);
                    $filename = __DIR__ . '/../data/' . $sha256 . '.png';

                    file_put_contents($filename, $png);

                    $stmt = $db->prepare("UPDATE news SET png = :png WHERE id = :id");
                    $stmt->bindValue(":id", $item["id"]);
                    $stmt->bindValue(":png", $sha256);
                    $stmt->execute();
                }

                $db->query("
                    ALTER TABLE news RENAME TO news_BAK;

                    CREATE TABLE news (
                        id INTEGER,
                        png TEXT,
                        title TEXT NOT NULL,
                        content TEXT NOT NULL,
                        date INTEGER,
                        duration INTEGER,

                        changeset TEXT,

                        PRIMARY KEY('id')
                    );

                    INSERT INTO news (id, png, title, content, date, duration)
                        SELECT id, png, title, content, date, duration FROM news_BAK;

                    DROP TABLE news_BAK;
                ");
            } // fallthrough
        }

        $db->exec("PRAGMA user_version = " . $schema);
        $db->exec("COMMIT");
    } catch (Throwable $e) {
        $db->exec("ROLLBACK");
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