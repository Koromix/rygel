// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

'use strict';

let veil;
let veil_img;

let timer_id;

function toggleSlideshow(parent, idx, delay) {
    console.log(arguments);

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

document.addEventListener('readystatechange', function(e) {
    if (document.readyState === 'complete') {
        let containers = document.querySelectorAll('.screenshot, .slideshow');

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
});
