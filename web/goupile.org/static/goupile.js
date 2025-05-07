// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

import '../../../src/web/flat/static.js';

let timer_id = null;
let veil = null;

window.addEventListener('load', e => {
    initScreenshots();
});

function initScreenshots() {
    let containers = document.querySelectorAll('.mini, .screenshot, .slideshow');

    for (let i = 0; i < containers.length; i++) {
        let images = containers[i].querySelectorAll('img');
        let anchors = containers[i].querySelectorAll('.legend > a');

        for (let j = 0; j < images.length; j++)
            images[j].addEventListener('click', function(e) { zoomImage(e.target.src); });
        for (let j = 0; j < anchors.length; j++)
            anchors[j].addEventListener('click', function(e) { toggleSlideshow(containers[i], j, 20000); });

        if (anchors.length >= 2)
            toggleSlideshow(containers[i], 0);
    }
}

function toggleSlideshow(parent, idx, delay) {
    if (delay == null)
        delay = 10000;

    let images = parent.querySelectorAll('img');
    let texts = parent.querySelectorAll('.legend > p');
    let anchors = parent.querySelectorAll('.legend > a');

    for (let i = 0; i < images.length; i++) {
        images[i].classList.toggle('active', i <= idx);
        texts[i].classList.toggle('active', i === idx);
        anchors[i].classList.toggle('active', i === idx);
        anchors[i].textContent = (i === idx) ? '●' : '○';
    }

    let next_idx = idx + 1;
    if (next_idx >= images.length)
        next_idx = 0;

    clearTimeout(timer_id);
    timer_id = setTimeout(function() { toggleSlideshow(parent, next_idx) }, delay);
}

function zoomImage(src) {
    if (!veil) {
        veil = document.createElement('div');
        veil.id = 'veil';
        veil.addEventListener('click', function() { veil.classList.remove('active'); });

        veil_img = document.createElement('img');

        veil.appendChild(veil_img);
        document.body.appendChild(veil);
    }

    veil_img.src = src;
    veil.classList.add('active');
}
