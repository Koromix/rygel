<?php

define("PASSWORD_HASH", '$2y$10$wTBwj.0pChAHLS7xlCCja.AwEHg873eOgxnk7kBt2Tbokjn9ueZla');

require_once(__DIR__ . "/../lib/util.php");
require_once(__DIR__ . "/../lib/database.php");

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

        $db = open_database();

        if ($_SERVER["REQUEST_METHOD"] == "POST") {
            $data = read_json_body(8 * 1024 * 1024);

            $db->query("BEGIN TRANSACTION");
            $changeset = random_uuid4();

            foreach ($data["news"] as $item) {
                $stmt = $db->prepare("INSERT INTO news (id, png, title, content, changeset)
                                      VALUES (:id, :png, :title, :content, :changeset)
                                      ON CONFLICT(id) DO UPDATE SET png = excluded.png,
                                                                    title = excluded.title,
                                                                    content = excluded.content,
                                                                    changeset = excluded.changeset");
                if (isset($item["id"]))
                    $stmt->bindValue(":id", $item["id"]);
                if (is_string($item["png"]) && !preg_match("/^[a-z0-9]{64}$/", $item["png"])) {
                    $png = base64_decode($item["png"]);

                    $sha256 = hash("sha256", $item["pnfg"]);
                    $filename = __DIR__ . '/../data/' . $sha256 . '.png';

                    file_put_contents($filename, $png);

                    $stmt->bindValue(":png", $sha256);
                } else {
                    $stmt->bindValue(":png", $item["png"]);
                }
                $stmt->bindValue(":title", $item["title"]);
                $stmt->bindValue(":content", $item["content"]);
                $stmt->bindValue(":changeset", $changeset);
                $stmt->execute();
            }

            $stmt = $db->prepare("DELETE FROM news WHERE changeset IS NOT :changeset");
            $stmt->bindValue(":changeset", $changeset);
            $stmt->execute();

            $db->query("COMMIT");
        }

        $res = $db->query("SELECT id, png, title, content FROM news ORDER BY id");
        $news = fetch_all($res);

        echo json_encode($news, JSON_UNESCAPED_UNICODE);
    } break;

    default: { fatal(404, "Unknown API"); } break;
}

?>
