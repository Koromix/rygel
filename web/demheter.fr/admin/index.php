<!DOCTYPE html>
<html>
    <head>
        <title>News DEMHETER</title>

        <meta charset="utf-8">

        <link rel="stylesheet" href="static/admin.min.css">
        <script src="static/admin.min.js"></script>

        <style>
            .loading {
                display: flex;
                height: 100%;
                align-items: center;
                justify-content: center;
                visibility: hidden;
                background: white;
                animation: loading_wait 0.4s 1 linear;
                animation-fill-mode: forwards;
            }
            .loading::after {
                content: ' ';
                display: block;
                width: 48px;
                height: 48px;
                margin: 1px;
                border-radius: 50%;
                border: 12px solid #76b35a;
                border-color: #76b35a transparent #76b35a transparent;
                animation: loading_spinner 0.8s ease-in-out infinite;
            }
            .loading > * { display: none !important; }
            @keyframes loading_wait {
                99% { visibility: hidden; }
                100% { visibility: visible; }
            }
            @keyframes loading_spinner {
                0% { transform: rotate(0deg); }
                100% { transform: rotate(360deg); }
            }

            .fatal {
                display: flex;
                height: 100%;
                align-items: center;
                justify-content: center;
            }
            .fatal > div {
                padding: 5px;
                font-size: 1.4em;
                text-align: center;
            }

            .version {
                font-size: 0.8em;
                color: #444;
            }
        </style>
    </head>

    <body>
        <div id="log"></div>
        <div id="page">
            <noscript>
                <div class="fatal"><div>
                    <span style="color: red;">⚠&#xFE0E; <b>Cette application nécessite un navigateur récent et Javascript</b></span><br/><br/>
                    Nous vous recommandons <u>l'un de ces navigateurs</u> :<br/><br/>
                    <b>Mozilla Firefox</b> <span class="version">(version ≥ 60)</span><br/>
                    <b>Google Chrome</b> <span class="version">(version ≥ 55)</span><br/>
                    <b>Microsoft Edge</b> <span class="version">(version ≥ 17)</span><br/>
                    <b>Apple Safari</b> <span class="version">(version ≥ 11)</span>
                </div></div>
            </noscript>
        </div>

        <script>
            try {
                // Quick way to check for recent browser
                eval('let test = async () => {}');
                eval('let test = function () { if (!new.target) {} }');
                eval('let test = new URLSearchParams');

                // See here: https://github.com/dfahlander/Dexie.js/issues/317
                var m = navigator.userAgent.toLowerCase().match('firefox/([0-9]+)');
                if (m != null) {
                    var version = parseInt(m[1], 10);
                    if (version < 60)
                        throw new Error('Cannot work with broken async support in IndexedDB');
                }

                document.body.classList.add('loading');
                window.addEventListener('load', function(e) {
                    if (typeof admin !== 'undefined') {
                        var p = admin.start();

                        p.catch(function(err) {
                            document.querySelector('#page').innerHTML =
                                '<div class="fatal"><div>' +
                                    '<span style="color: red;">⚠\uFE0E <b>Une erreur est survenue pendant le chargement</b></span><br/>' +
                                    err.message +
                                '</div></div>';
                            document.body.classList.remove('loading');
                        });
                    } else {
                        proposeBrowsers();
                    }
                });
            } catch (err) {
                proposeBrowsers();
            }

            function proposeBrowsers() {
                document.querySelector('#page').innerHTML =
                    '<div class="fatal"><div>' +
                        '<span style="color: red;">⚠\uFE0E <b>Ce navigateur n\'est pas supporté, ou il s\'agit d\'une version trop ancienne</b></span><br/><br/>' +
                        'Nous vous recommandons <u>l\'un de ces navigateurs</u> :<br/><br/>' +
                        '<b>Mozilla Firefox</b> <span class="browser">(version ≥ 60)</span><br/>' +
                        '<b>Google Chrome</b> <span class="browser">(version ≥ 55)</span><br/>' +
                        '<b>Microsoft Edge</b> <span class="browser">(version ≥ 17)</span><br/>' +
                        '<b>Apple Safari</b> <span class="browser">(version ≥ 11)</span>' +
                    '</div></div>';
                document.body.classList.remove('loading');
            }
        </script>
    </body>
</html>
