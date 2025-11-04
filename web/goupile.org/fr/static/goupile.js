// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import '../../../../src/core/web/flat/static.js';

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
