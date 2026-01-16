<!DOCTYPE html>
<html lang="fr" class="nojs">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">

        <title>{{ TITLE }}</title>
        <meta name="description" content="{{ DESCRIPTION }}">
        <link rel="icon" href="{{ ASSET favicon.png }}">

        <meta name="theme-color" content="#3c7778">
        <link rel="stylesheet" href="{{ ASSET static/site.min.css }}">

        <script type="text/javascript" src="{{ ASSET static/site.min.js }}"></script>

        <style>
            #news {
                display: grid;
                background: #112a2c;
                height: 256px;
                margin: 64px 0 1em 0;
                position: relative;
            }
            #news > img {
                position: absolute;
                width: 32px;
                height: 32px;
                top: calc(50% - 16px);
                cursor: pointer;
                z-index: 2;
            }
            #news > div {
                grid-column: 1;
                grid-row: 1;
                display: flex;
                padding-left: calc((100% - 1200px) / 2 + 16px);
                padding-right: calc((100% - 1200px) / 2 + 16px);
                opacity: 0;
                transition: opacity 0.26s ease;
                user-select: none;
                pointer-events: none;
            }
            #news > div.active {
                opacity: 1;
                user-select: auto;
                pointer-events: auto;
            }
            #news > div > img {
                margin-top: -64px;
                height: 320px;
            }
            #news > div > div {
                flex: 1;
                padding: 2em;
                color: white;
                --color: white;
            }
            #news > div p.title {
                font-size: 1.1em;
                font-weight: bold;
                margin-bottom: 1.4em;
            }
            #news > div a {
                color: white;
                text-decoration: underline;
                font-weight: bold;
            }

            @media screen and (max-width: 960px) {
                #news {
                    height: auto;
                    margin: 1em 0;
                    padding: 0;
                    background: none;
                    position: relative;
                    overflow: hidden;
                }
                #news > img { top: calc(100% - 48px); }
                #news > div > img {
                    position: absolute;
                    width: 100%;
                    height: 100%;
                    margin-top: 0;
                    left: 0;
                    top: 0;
                    object-fit: cover;
                    z-index: -1;
                }
                #news > div > div {
                    padding: 2em 1em calc(3em + 32px) 1em;
                    background: #112a2ccc;
                }
                #news > div p.title { margin-bottom: 0; }
            }

            @media print {
                #news { display: none; }
            }
        </style>
    </head>

    <body>
        <nav id="top">
            <div class="deploy"></div>
            <menu>
                <a id="logo" href="/"><img src="{{ ASSET static/logo.webp }}" alt="Logo DEMHETER" /></a>

{{ LINKS }}
            </menu>
        </nav>

<?php

require_once(__DIR__ . "/lib/parsedown/Parsedown.php");

if (file_exists(__DIR__ . "/data/news.db")) {
    require_once(__DIR__ . "/lib/database.php");

    $db = open_database();

    $res = $db->query("SELECT png, title, content FROM news ORDER BY id");
    $news = fetch_all($res);
} else {
    $news = [];
}

if (count($news)) {
    echo '<div id="news">';

    if (count($news) > 1) {
        echo '<img style="left: 16px;" src="/static/misc/left.png" alt="" onclick="toggleNews(-1, true)" />
              <img style="right: 16px;" src="/static/misc/right.png" alt="" onclick="toggleNews(1, true)" />';
    }

    foreach ($news as $i => $item) {
        $cls = $i ? "" : "active";

        $image = $item["png"] ? "/data/{$item["png"]}.png" : "";
        $title = htmlspecialchars($item["title"]);
        $content = parse_markdown($item["content"]);

        echo <<<INFO
            <div class="{$cls}">
                <img src="{$image}" alt="" />
                <div>
                    <p class="title">$title</p>
                    $content
                </div>
            </div>
        INFO;
    }

    echo '</div>';
}

function parse_markdown($text) {
    $parser = new Parsedown();
    return $parser->text($text);
}

?>

        <main>
            {{ CONTENT }}
        </main>

        <footer>
            <div>DEMHETER © 2025</div>
            <img src="{{ ASSET favicon.png }}" alt="" width="48" height="48">
            <div style="font-size: 0.8em;">
                CHU de Lille, 59037 Lille CEDEX<br>
                <a href="mailto:demheter@chu-lille.fr" style="font-weight: bold; color: inherit;">demheter@chu-lille.fr</a>
            </div>
        </footer>

        <script>
            let news_timer = null;

            resetTimer(false);

            function toggleNews(delta, explicit) {
                let news = document.querySelector('#news');

                if (news == null)
                    return;

                if (explicit || !news.classList.contains('expand')) {
                    let divs = Array.from(document.querySelectorAll('#news > div'));
                    let idx = divs.findIndex(div => div.classList.contains('active'));

                    if (delta < 0) {
                        if (--idx < 0)
                            idx = divs.length - 1;
                    } else if (delta > 0) {
                        if (++idx >= divs.length)
                            idx = 0;
                    }

                    for (let i = 0; i < divs.length; i++) {
                        let div = divs[i];
                        div.classList.toggle('active', i == idx);
                    }

                    news.classList.remove('expand');
                }

                resetTimer(explicit);
            }

            function expandNews(e, enable = undefined) {
                let news = document.querySelector('#news');
                news.classList.toggle('expand', enable);

                resetTimer(false);
            }

            function resetTimer(explicit) {
                if (news_timer != null)
                    clearTimeout(news_timer);
                news_timer = setTimeout(() => toggleNews(1, false), explicit ? 14000 : 6000);
            }
        </script>
    </body>
</html>
