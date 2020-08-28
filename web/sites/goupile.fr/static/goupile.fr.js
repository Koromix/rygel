// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

'use strict';

let veil;
let veil_img;
let timer_id;

document.addEventListener('readystatechange', function(e) {
    if (document.readyState === 'complete') {
        initMenu();
        initScreenshots();
    }
});

function initMenu() {
    let headings = document.querySelectorAll('h1, h2, h3, h4, h5, h6');

    let uls = [];
    for (let li, i = 0; i < headings.length; i++) {
        let h = headings[i];

        if (h.id) {
            let depth = parseInt(h.tagName.substr(1), 10);

            while (depth > uls.length) {
                let ul = document.createElement('ul');

                if (li)
                    li.appendChild(ul);
                uls.push(ul);
            }
            while (depth < uls.length)
                uls.pop();

            li = document.createElement('li');

            let a = document.createElement('a');
            a.setAttribute('href', '#' + h.id);
            a.textContent = h.textContent;

            li.appendChild(a);
            uls[uls.length - 1].appendChild(li);
        }
    }

    if (uls.length) {
        let menu = document.createElement('nav');
        menu.id = 'menu';
        menu.appendChild(uls[0]);

        document.body.appendChild(menu);
    }
}

function initScreenshots() {
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
