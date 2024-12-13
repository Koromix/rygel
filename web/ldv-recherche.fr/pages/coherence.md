<div id="exercise"></div>

<script src="{{ ASSET static/interactive.min.js }}"></script>
<script>
    let root = document.querySelector('#exercise');

    function menu() {
        render(html`
            <p>Cet exercice a pour but de vous aider à vous relaxer en alternant des inspirations profondes de 5 secondes, et des expirations profondes de 5 secondes.</p>
            <div class="buttons" style="flex-direction: column;">
                <a @click=${e => run(60)}>1 minute</a>
                <a @click=${e => run(180)}>3 minutes</a>
                <a @click=${e => run(300)}>5 minutes</a>
            </div>
            <p>Commencez <b>quand vous le souhaitez</b>, et répétez l'exercice si besoin.</p>
        `, root);
    }

    function run(duration) {
        render(html`
            <div id="progress"></div>
            <div id="bubble"><span></span></div>
        `, root);

        let progress = root.querySelector('#progress');
        let bubble = root.querySelector('#bubble');

        progress.addEventListener('animationend', end);
        progress.style.setProperty('--duration', duration + ('s'));

        bubble.addEventListener('animationiteration', update);
        update();

        setTimeout(() => {
            bubble.classList.add('animate');
            progress.classList.add('animate');
        }, 1000);

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
            <p>Bravo, répétez cet exercice autant de fois que nécessaire</p>
            <div class="buttons" style="flex-direction: column;">
                <a @click=${menu}>Recommencer</a>
            </div>
        `, root);
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
        background: #fafafa;
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
        left: 10px;
        top: 10px;
        width: calc(100% - 20px);
        height: 20px;
        background: linear-gradient(to right, #4e92ff 0 var(--progress), white var(--progress));
        border-radius: 10px;
    }
    #progress.animate { animation: progress var(--duration) linear; }

    #bubble {
        display: flex;
        border-radius: 50%;
        width: 150px;
        height: 150px;
        background: #417ad5;
        color: white;
        align-items: center;
        justify-content: center;
        transform: scale(1);
    }
    #bubble.animate { animation: breathe 5s ease-in-out infinite alternate; }
    #bubble.animate > span { animation: breathe 5s ease-in-out infinite alternate; }
    #bubble.done {
        animation: none;
        transform: scale(1);
        background: #e477e1;
    }

    @keyframes breathe {
        0% {
            transform: scale(1);
            background: #417ad5;
        }
        100% {
            transform: scale(2);
            background: #4e92ff;
        }
    }
    @keyframes progress {
        0% { --progress: 0%; }
        100% { --progress: 100%; }
    }
</style>
