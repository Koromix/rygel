<script type="module">
    document.body.classList.add('loading');

    import { load, start } from './{{ ASSET ../../staks/src/game.js }}';

    run();

    async function run() {
        try {
            let main = document.querySelector('main');

            await load('static/staks', (value, total) => {
                let progress = Math.floor(value / total * 100);
                main.innerHTML = progress + '%';
            });
            main.innerHTML = '';

            await start(main);
        } catch (err) {
            console.error(err);

            document.body.innerHTML =
                '<div id="fatal"><div>' +
                    '<span style="color: red;">⚠\uFE0E <b>Une erreur est survenue pendant le chargement</b></span><br/>' +
                    err.message + '<br/><br/>' +
                    window.SUPPORTED_BROWSERS +
                '</div></div>';
        } finally {
            document.body.classList.remove('loading');
        }
    }
</script>
