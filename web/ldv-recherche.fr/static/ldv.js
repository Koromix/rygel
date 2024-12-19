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

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util } from '../../../src/web/core/base.js';
import '../../../src/web/flaat/flaat.js';

import './ldv.css';

window.addEventListener('load', e => {
    initCards();
});

function initCards() {
    let cardsets = document.querySelectorAll('.cardset');

    for (let cardset of cardsets) {
        let cards = Array.from(cardset.querySelectorAll('.card'));
        let shuffle = Util.shuffle(Util.sequence(cards.length));

        let arrows = document.createElement('div');
        render(html`
            <img style="left: 16px;" src=${assets.left} alt="" @click=${e => toggleCard(cards, -1)} />
            <div style="flex: 1;"></div>
            <img style="right: 16px;" src=${assets.right} alt="" @click=${e => toggleCard(cards, 1)} />
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
            card.dataset.rnd = shuffle[i];
        }

        activateCard(cards, shuffle[0]);

        detectSwipe(cardset, delta => toggleCard(cards, delta));
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

    let active = cards.find(card => card.classList.contains('active'));
    let rnd = (parseInt(active.dataset.rnd, 10) + 1) % cards.length;
    let next = cards.findIndex(card => card.dataset.rnd == rnd);

    activateCard(cards, next);
}

function sos(e) {
    dialog(e, 'sos', html`
        <div>
            <img src=${assets['3114']} width="183" height="86" alt="">
            <div>
                <p>Le <b>3114</b> est le numéro national de prévention de suicide. Consultez le site pour plus d'informations sur la prévention du suicide.
                <p>Si vous êtes en <b>détresse et/ou avez des pensées suicidaires</b>, ou si vous voulez aider une personne en souffrance, vous pouvez contacter le numéro national de prévention du suicide, le 3114.
            </div>
        </div>
    `);

    if (e.target == e.currentTarget)
        e.preventDefault();
}

function dialog(e, id, content) {
    let parent = e.currentTarget;
    let dlg = document.getElementById(id);

    if (dlg == null) {
        dlg = document.createElement('dialog');
        dlg.id = id ?? '';
    }

    render(content, dlg);

    if (!dlg.open) {
        let parent = e.currentTarget;

        if (parent.tagName == 'A') {
            let rect = parent.getBoundingClientRect();
            let right = (rect.left + rect.width >= window.innerWidth / 2);

            dlg.classList.add(right ? 'right' : 'left');

            parent.appendChild(dlg);
        } else {
            document.body.appendChild(dlg);
        }

        dlg.show();
    } else {
        dlg.close();
        dlg.parentNode.removeChild(dlg);
    }

    function stop(e) {
        if (e.target != parent && e.target.tagName == 'A')
            e.stopPropagation();
    }
}

function detectSwipe(el, func) {
    let identifier = null;
    let start = null;
    let scroll = null;

    el.addEventListener('touchstart', e => {
        if (e.changedTouches.length != 1)
            return;

        let touch = e.changedTouches[0];

        identifier = touch.identifier;
        start = touch.screenX;
        scroll = touch.pageY;
    });

    el.addEventListener('touchend', e => {
        for (let touch of e.changedTouches) {
            if (touch.identifier !== identifier)
                continue;

            let dx = touch.screenX - start;
            let dy = Math.abs(touch.pageY - scroll);

            if (dy < 20) {
                if (dx <= -10) {
                    func(1);
                } else if (dx >= 10) {
                    func(-1);
                }
            }
        }
    });
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
