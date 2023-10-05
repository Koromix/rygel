<?php

require_once("util.php");
require_once("../vendor/parsedown/Parsedown.php");

session_start();
assert_admin();

$db = new SQLite3("../data/news.db", SQLITE3_OPEN_READONLY);
$db->enableExceptions(true);

$res = $db->query("SELECT image, title, content FROM news ORDER BY id");
$news = fetch_all($res);

?>
<html>
    <head>
        <title>News DEMHETER</title>

        <meta charset="utf-8">
        <link rel="stylesheet" href="assets/admin.css">

        <script>
            function update(e) {
                e.preventDefault();

                let target = e.target;
                let all = [];

                for (let tr of target.querySelectorAll('tbody > tr')) {
                    let news = {
                        image: tr.querySelector('.image').value,
                        title: tr.querySelector('.title').value,
                        content: tr.querySelector('.content').value
                    };

                    all.push(news);
                }

                fetch('api.php?method=news', {
                    method: 'POST',
                    body: JSON.stringify({
                        news: all
                    })
                });
            }

            function addRow(e) {
                let tbody = document.querySelector('tbody');

                let tr = document.createElement('tr');

                tr.innerHTML = `
                     <td><input class="image" type="text" value=""></td>
                    <td><input class="title" type="text" value=""></td>
                    <td><textarea class="content" style="width: 100%";" rows="5"></textarea></td>
                    <td class="right">
                        <button type="button" class="small"
                                onclick="deleteRow(event)"><img src="assets/delete.webp" alt="Supprimer" /></button>
                    </td>
                ;`

                tbody.appendChild(tr);
            }

            function deleteRow(e) {
                let tbody = findParent(e.target, el => el.tagName == 'TBODY');
                let tr = findParent(e.target, el => el.tagName == 'TR');

                tbody.removeChild(tr);
            }

            function findParent(el, func) {
                if (typeof func != 'function') {
                    let tag = func;
                    func = el => el.tagName == tag;
                }

                while (el && !func(el))
                    el = el.parentElement;
                return el;
            }
        </script>
    </head>
    <body>
        <div class="page">
            <div class="dialog screen">
                <form onsubmit="update(event)">
                    <div class="title">
                        News DEMHETER
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary"
                                onclick="addRow(event)">Ajouter</button>
                    </div>

                    <div class="main">
                        <table style="table-layout: fixed;">
                            <colgroup>
                                <col>
                                <col>
                                <col>
                                <col class="check">
                            </colgroup>

                            <thead>
                                <th>Image (URL)</th>
                                <th>Titre</th>
                                <th>Contenu</th>
                            </thead>

                            <tbody>
<?php

foreach ($news as $i => $item) {
    $image = htmlspecialchars($item["image"]);
    $title = htmlspecialchars($item["title"]);
    $content = htmlspecialchars($item["content"]);

    echo <<<ROW
                            <tr>
                                <td><input class="image" type="text" value=$image></td>
                                <td><input class="title" type="text" value=$title></td>
                                <td><textarea class="content" style="width: 100%";" rows="5">$content</textarea></td>
                                <td class="right">
                                    <button type="button" class="small"
                                            onclick="deleteRow(event)"><img src="assets/delete.webp" alt="Supprimer" /></button>
                                </td>
                            </tr>
    ROW;
}

?>
                            </tbody>
                        </table>
                    </div>
                    <div class="footer">
                        <button class="danger" onclick="window.location.href = window.location.href">Annuler</button>
                        <button type="submit">Valider</button>
                    </div>
                </form>
            </div>
        </div>
    </body>
</html>
