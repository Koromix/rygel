<?php

define("PASSWORD_HASH", '$2y$10$wTBwj.0pChAHLS7xlCCja.AwEHg873eOgxnk7kBt2Tbokjn9ueZla');

require_once("util.php");

$method = $_GET["method"] ?? null;

switch ($method) {
    case "login": {
        $data = read_json_body();

        if (!password_verify($data["password"], PASSWORD_HASH))
            fatal(403, "Wrong password");

        session_start();
        $_SESSION["admin"] = true;

        echo json_encode([], JSON_UNESCAPED_UNICODE);
    } break;

    case "logout": {
        session_start();
        session_destroy();

        echo json_encode([], JSON_UNESCAPED_UNICODE);
    } break;

    case "news": {
        assert_admin();

        $db = new SQLite3("../data/news.db", SQLITE3_OPEN_READWRITE);

        if ($_SERVER["REQUEST_METHOD"] == "POST") {
            $data = read_json_body();

            $db->query("BEGIN TRANSACTION");
            $db->query("DELETE FROM news");
            foreach ($data["news"] as $news) {
                $stmt = $db->prepare("INSERT INTO news (image, title, content) VALUES (:image, :title, :content)");

                $stmt->bindValue(":image", $news["image"]);
                $stmt->bindValue(":title", $news["title"]);
                $stmt->bindValue(":content", $news["content"]);

                $stmt->execute();
            }
            $db->query("COMMIT");
        }

        $res = $db->query("SELECT image, title, content FROM news ORDER BY id");
        $news = fetch_all($res);

        echo json_encode($news, JSON_UNESCAPED_UNICODE);
    } break;

    default: { fatal(404, "Unknown API"); } break;
}

?>
