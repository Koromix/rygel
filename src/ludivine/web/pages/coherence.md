<div id="exercise"></div>

<script>
    let root = document.querySelector('#exercise');

    function menu() {
        render(html`
            <p>Relaxez-vous en alternant des <b>inspirations et des expirations profondes</b> toutes les 5 secondes.</p>
            <div class="actions" style="flex-direction: column;">
                <a @click=${e => run(60)}>1 minute</a>
                <a @click=${e => run(180)}>3 minutes</a>
                <a @click=${e => run(300)}>5 minutes</a>
            </div>
            <p>Commencez <b>quand vous le souhaitez</b>, et répétez l'exercice si besoin.</p>
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
            <p>C'est terminé, mais n'hésitez pas à répéter cet exercice <b>autant de fois que nécessaire</b> !</p>
            <div class="actions" style="flex-direction: column;">
                <a @click=${menu}>Recommencer</a>
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
    main { display: flex; }

    #exercise {
        display: flex;
        width: 600px;
        max-width: 100%;
        flex: 1;
        margin: 0 1em;
        padding: 1em;
        align-items: center;
        justify-content: center;
        flex-direction: column;
        position: relative;
    }
    #exercise > p {
        margin-left: 4em;
        margin-right: 4em;
        text-align: center;
    }

    @property --progress {
        initial-value: 0%;
        inherits: false;
        syntax: '<percentage>';
    }

    #progress {
        position: absolute;
        left: 4px;
        top: 4px;
        width: calc(100% - 8px);
        height: 20px;
        background: linear-gradient(to right, #4e92ff 0 var(--progress), #f8f8f8 var(--progress));
        border-radius: 10px;
    }
    #progress.animate { animation: progress var(--duration) linear; }

    #bubble {
        display: flex;
        width: 280px;
        height: 280px;
        margin-top: 28px;
        background: #417ad5;
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
            background: #417ad5;
        }
        100% {
            transform: scale(1);
            background: #4e92ff;
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
