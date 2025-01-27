// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util } from '../../../web/core/base.js';
import '../../../web/flat/static.js';
import { ASSETS } from '../assets.js';

window.addEventListener('load', e => {
    initCards();
});

function initCards() {
    let cardsets = document.querySelectorAll('.cardset');

    for (let cardset of cardsets) {
        let cards = Array.from(cardset.querySelectorAll('.card'));

        let arrows = document.createElement('div');
        render(html`
            <img style="left: 16px;" src=${ASSETS['web/misc/left']} alt="" @click=${e => toggleCard(cards, -1)} />
            <div style="flex: 1;"></div>
            <img style="right: 16px;" src=${ASSETS['web/misc/right']} alt="" @click=${e => toggleCard(cards, 1)} />
        `, arrows);
        arrows.classList.add('arrows');
        cardset.appendChild(arrows);

        for (let i = 0; i < cards.length; i++) {
            let card = cards[i];

            card.addEventListener('click', () => {
                if (isTouchDevice())
                    return;
                activateCard(cards, i);
            });
        }

        shuffleCards(cards);
        activateCard(cards, 0);

        if (cardset == cardsets[0]) {
            let key_timer = null;
            let key_left = false;
            let key_right = false;

            window.addEventListener('keydown', e => {
                if (e.keyCode == 37 && !key_left) {
                    key_left = true;
                    toggle(false);
                } else if (e.keyCode == 39 && !key_right) {
                    key_right = true;
                    toggle(false);
                }
            });
            window.addEventListener('keyup', e => {
                if (e.keyCode == 37) {
                    key_left = false;
                } else if (e.keyCode == 39) {
                    key_right = false;
                }

                if (key_timer != null && !key_left && !key_right)
                    clearTimeout(key_timer);
                key_timer = null;
            });

            function toggle(repeat) {
                let delta = key_right - key_left;

                if (delta)
                    toggleCard(cards, delta);

                if (key_timer == null) {
                    let delay = repeat ? 300 : 600;

                    key_timer = setTimeout(() => {
                        key_timer = null;
                        toggle(true);
                    }, delay);
                }
            }
        }

        detectSwipe(cardset, delta => toggleCard(cards, delta));
    }
}

function shuffleCards(cards) {
    let seq = Util.sequence(0, cards.length);
    let shuffle = Util.shuffle(seq);

    for (let i = 0; i < cards.length; i++) {
        let card = cards[i];
        card.dataset.rnd = shuffle[i];
    }
}

function toggleCard(cards, delta) {
    let active = cards.findIndex(card => card.classList.contains('active'));
    let idx = offset(active, cards.length, delta);

    activateCard(cards, idx);
}

function activateCard(cards, active) {
    let left = -Math.floor((cards.length - 1) / 2);
    let right = Math.floor(cards.length / 2);

    for (let i = -1; i >= left; i--) {
        let idx = offset(active, cards.length, i);
        let card = cards[idx];

        card.style.setProperty('--position', i);
        card.classList.toggle('active', i == 0);
    }

    for (let i = 0; i <= right; i++) {
        let idx = offset(active, cards.length, i);
        let card = cards[idx];

        card.style.setProperty('--position', i);
        card.classList.toggle('active', i == 0);
    }
}

function randomCard(cards) {
    if (!Array.isArray(cards)) {
        if (typeof cards == 'string')
            cards = document.querySelector(cards);
        if (!(cards instanceof NodeList))
            cards = cards.querySelectorAll('.card');
        cards = Array.from(cards);
    }

    let active = cards.findIndex(card => card.classList.contains('active'));
    let rnd = (parseInt(cards[active].dataset.rnd, 10) + 1) % cards.length;
    let next = cards.findIndex(card => card.dataset.rnd == rnd);

    if (!rnd) {
        do {
            shuffleCards(cards);
            next = cards.findIndex(card => card.dataset.rnd == rnd);
        } while (next == active);
    }

    activateCard(cards, next);
}

function sos(e) {
    dialog(e, 'help', close => html`
        <img src=${ASSETS['web/misc/3114']} width="183" height="86" alt="">
        <div>
            <p>Le <b>3114</b> est le numéro national de prévention de suicide. Consultez le <a href="https://3114.fr/" target="_blank">site du 3114</a> pour plus d'informations sur la prévention du suicide.
            <p>Si vous êtes en <b>détresse et/ou avez des pensées suicidaires</b>, ou si vous voulez aider une personne en souffrance, vous pouvez contacter le numéro national de prévention du suicide, le <a href="tel:3114">3114</a>.
        </div>
        <button type="button" class="secondary" @click=${close}>Fermer</button>
    `);

    if (e.target == e.currentTarget)
        e.preventDefault();
}

function dialog(e, id, func) {
    let dlg = document.getElementById(id);

    if (dlg == null) {
        dlg = document.createElement('dialog');
        dlg.id = id ?? '';
    }

    let content = func(close);
    render(html`<div @click=${stop}>${content}</div>`, dlg);

    if (!dlg.open) {
        document.body.appendChild(dlg);
        dlg.show();
    } else {
        close();
    }

    function close() {
        dlg.close();
        dlg.parentNode.removeChild(dlg);
    }

    function stop(e) {
        if (e.target != e.currentTarget && e.target.tagName == 'A')
            e.stopPropagation();
    }
}

function detectSwipe(el, func) {
    let identifier = null;
    let start = null;
    let scroll = null;
    let swiped = null;

    el.addEventListener('touchstart', e => {
        if (e.changedTouches.length != 1)
            return;

        let touch = e.changedTouches[0];

        identifier = touch.identifier;
        start = touch.screenX;
        scroll = window.scrollY;
        swiped = null;
    }, { passive: false });

    el.addEventListener('touchmove', e => {
        let now = performance.now();

        if (Math.abs(window.scrollY - scroll) >= 1)
            start = null;
        if (start == null)
            return;

        if (swiped != null) {
            e.preventDefault();

            if (now - swiped < 200)
                return;
        }

        for (let touch of e.changedTouches) {
            if (touch.identifier !== identifier)
                continue;

            let dx = touch.screenX - start;

            if (Math.abs(dx) >= 20) {
                let direction = (dx < 0) ? 1 : -1;
                func(direction);

                start = touch.screenX;
                swiped = now;
            }
        }
    }, { passive: false });
}

function isTouchDevice() {
    if (!('ontouchstart' in window))
        return false;
    if (!navigator.maxTouchPoints)
        return false;
    if (navigator.msMaxTouchPoints)
        return false;

    return true;
}

function offset(start, length, delta) {
    let idx = (start + length + delta) % length;
    return idx;
}

export {
    render,
    html,

    randomCard,

    dialog,
    sos
}
