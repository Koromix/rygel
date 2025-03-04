<div id="exercise"></div>

<div id="attributions">
    Fond par <a href="https://www.deviantart.com/psiipilehto/" target="_blank">psiipilehto</a>
</div>

<script>
    const { render, html } = app;

    let root = document.querySelector('#exercise');

    function menu() {
        render(html`
            <div class="dialog">
                <p>Relaxez-vous en alternant des <b>inspirations et des expirations profondes</b> toutes les 5 secondes.</p>
                <div class="actions" style="flex-direction: column;">
                    <a @click=${e => run(60)}>1 minute</a>
                    <a @click=${e => run(180)}>3 minutes</a>
                    <a @click=${e => run(300)}>5 minutes</a>
                </div>
                <p>Commencez <b>quand vous le souhaitez</b>, et répétez l'exercice si besoin.</p>
            </div>
        `, root);
    }

    async function run(duration) {
        render(html`
            <div id="progress"></div>
            <div id="bubble"><span></span></div>
        `, root);

        let progress = root.querySelector('#progress');
        let bubble = root.querySelector('#bubble');
        let span = bubble.querySelector('span');

        for (let i = 3; i > 0; i--) {
            render(i, span);
            await wait(1000);
        }

        progress.addEventListener('animationend', end);
        progress.style.setProperty('--duration', duration + 's');

        bubble.addEventListener('animationiteration', update);
        update();

        bubble.classList.add('animate');
        progress.classList.add('animate');

        function update(e) {
            let elasped = e?.elapsedTime ?? 0;

            if (elasped >= duration)
                return;

            let cycle = Math.floor(elasped / 5);
            let inspiring = (cycle % 2 == 0);

            let span = bubble.querySelector('span');
            span.innerHTML = inspiring ? 'Inspirez' : 'Expirez';
        }
    }

    function end() {
        render(html`
            <div class="dialog">
                <p>C'est terminé, mais n'hésitez pas à répéter cet exercice <b>autant de fois que nécessaire</b> !</p>
                <div class="actions" style="flex-direction: column;">
                    <a @click=${menu}>Recommencer</a>
                </div>
            </div>
        `, root);
    }

    async function wait(ms) {
        let p = new Promise((resolve, reject) => setTimeout(resolve, ms));
        await p;
    }

    menu();
</script>

<style>
    main {
        display: flex;
        margin: 0;
        max-width: none;
        background: url('{{ ASSET ../assets/pictures/aurora.webp }}');
        background-size: cover;
        position: relative;
    }

    #exercise {
        display: flex;
        width: 600px;
        max-width: 100%;
        flex: 1;
        margin: 0 1em;
        padding: 1em;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        position: relative;
    }
    #exercise > p {
        margin-left: 4em;
        margin-right: 4em;
        text-align: center;
    }

    #attributions {
        position: absolute;
        left: 6px;
        bottom: 4px;
        font-size: 12px;
        color: #fff;
    }
    #attributions > a {
        text-decoration: underline;
        color: inherit;
    }

    .dialog {
        display: flex;
        border-radius: 16px;
        flex-direction: column;
        align-items: center;
        padding: 2em;
        background: #ffffffbb;
    }

    @property --progress {
        initial-value: 0%;
        inherits: false;
        syntax: '<percentage>';
    }

    #progress {
        --color: #e9a07f;

        position: absolute;
        left: 50%;
        top: 2em;
        width: calc(100% - 8px);
        height: 20px;
        transform: translateX(-50%);
        background: linear-gradient(to right, var(--color) 0 var(--progress), #f8f8f8 var(--progress));
        border-radius: 10px;
    }
    #progress.animate { animation: progress var(--duration) linear; }

    #bubble {
        --base: #e9a07f;
        --expand: #ffaf8b;

        display: flex;
        width: 280px;
        height: 280px;
        margin-top: 28px;
        background: var(--base);
        color: white;
        align-items: center;
        justify-content: center;
        border-radius: 50%;
        transform: scale(0.5);
        user-select: none;
    }
    #bubble > span {
        margin-top: -6px;
        font-size: 4em;
    }
    #bubble.animate { animation: bubble 5s ease-in-out infinite alternate; }
    #bubble.animate > span { animation: text 5s ease-in-out infinite alternate; }
    #bubble.done {
        animation: none;
        transform: scale(1);
        background: #e477e1;
    }

    @keyframes bubble {
        0% {
            transform: scale(0.5);
            background: var(--base);
        }
        100% {
            transform: scale(1);
            background: var(--expand);
        }
    }
    @keyframes text {
        0% { transform: scale(0.6); }
        100% { transform: scale(1); }
    }
    @keyframes progress {
        0% { --progress: 0%; }
        100% { --progress: 100%; }
    }

    @media screen and (max-width: 960px) {
        #exercise > p {
            margin-left: 0;
            margin-right: 0;
        }
    }
</style>
