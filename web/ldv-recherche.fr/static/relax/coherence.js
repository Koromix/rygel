// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import './coherence.css';

const { render, html } = App;

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
