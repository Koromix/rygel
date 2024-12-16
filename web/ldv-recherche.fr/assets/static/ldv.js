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

import '../../../../src/web/flaat/flaat.js';

window.addEventListener('load', e => {
    initCards();
});

function initCards() {
    let cardsets = document.querySelectorAll('.cardset');

    for (let cardset of cardsets) {
        let cards = Array.from(cardset.querySelectorAll('.card'));

        function toggle(active) {
            let left = -Math.floor((cards.length - 1) / 2);
            let right = Math.floor(cards.length / 2);

            for (let i = -1; i >= left; i--) {
                let idx = active + i;
                if (idx < 0)
                    idx = cards.length + idx;
                cards[idx].style.setProperty('--position', i);
                cards[idx].classList.toggle('active', i == 0);
            }
            for (let i = 0; i <= right; i++) {
                let idx = (active + i) % cards.length;
                cards[idx].style.setProperty('--position', i);
                cards[idx].classList.toggle('active', i == 0);
            }
        }

        for (let i = 0; i < cards.length; i++)
            cards[i].addEventListener('click', () => toggle(i));

        let middle = Math.floor((cards.length - 1) / 2);
        toggle(middle);
    }
}

