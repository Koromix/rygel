<?php

define("PASSWORD_HASH", '$2y$10$wTBwj.0pChAHLS7xlCCja.AwEHg873eOgxnk7kBt2Tbokjn9ueZla');

require_once(__DIR__ . "/util.php");
require_once(__DIR__ . "/database.php");

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
                                      ON CONFLICT(id) DO UPDATE SET png = IIF(excluded.png = 1, png, excluded.png),
                                                                    title = excluded.title,
                                                                    content = excluded.content,
                                                                    changeset = excluded.changeset");
                if (isset($item["id"]))
                    $stmt->bindValue(":id", $item["id"]);
                if (is_bool($item["png"]) || $item["png"] === null) {
                    $stmt->bindValue(":png", $item["png"] ? 1 : null);
                } else {
                    $stmt->bindValue(":png", base64_decode($item["png"]));
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

        $res = $db->query("SELECT id, IIF(png IS NULL, 0, 1) AS png, title, content FROM news ORDER BY id");
        $news = fetch_all($res);

        foreach ($news as &$it)
            $it["png"] = boolval($it["png"]);

        echo json_encode($news, JSON_UNESCAPED_UNICODE);
    } break;

    case "png": {
        $db = open_database();

        if (empty($_GET["id"]))
            fatal(422, "Missing image index");

        try {
            $stream = $db->openBlob("news", "png", intval($_GET["id"]));
        } catch (ErrorException $e) {
            fatal(404, "PNG does not exist");
        }
        $png = stream_get_contents($stream);

        header('Content-Type: image/png');
        echo $png;
    } break;

    default: { fatal(404, "Unknown API"); } break;
}

?>
