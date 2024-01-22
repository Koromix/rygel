// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

'use strict';

let veil;
let veil_img;
let timer_id;

document.addEventListener('readystatechange', function(e) {
    if (document.readyState === 'complete') {
        initMenu();
        initHighlight();
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
            a.style.fontWeight = (h.tagName == 'H1') ? 'bold' : 'normal';
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

function initHighlight() {
    // Find all side menu items, and the pointed to elements
    let links = [].slice.call(document.querySelectorAll('#menu a'));
    let items = [].slice.call(links).map(link => {
        let anchor = document.querySelector(link.getAttribute('href'));
        return [link, anchor];
    });
    if (!items.length)
        return;

    let pending_request = false;
    let active_idx = null;
    function highlight(idx) {
        if (pending_request)
            return;

        window.requestAnimationFrame(() => {
            pending_request = false;

            if (idx !== active_idx) {
                if (active_idx !== null)
                    items[active_idx][0].classList.remove('active');
                items[idx][0].classList.add('active');
                active_idx = idx;
            }
        });
        pending_request = true;
    }

    // Event handlers, for clicks
    let ignore_scroll = false;
    function highlightOnClick(idx) {
        highlight(idx);
        setTimeout(() => { ignore_scroll = false; }, 50);
        ignore_scroll = true;
    }
    function highlightOnScroll() {
        if (ignore_scroll)
            return;

        let idx;
        for (idx = 0; idx < items.length; idx++) {
            let rect = items[idx][1].getBoundingClientRect();
            if (rect.top >= 50)
                break;
        }
        if (idx)
            idx--;

        highlight(idx);
    }

    // The scroll handler is not enough because it does not work for
    // small sections at the end of the page.
    for (let i = 0; i < items.length; i++) {
        items[i][0].addEventListener('click', highlightOnClick.bind(this, i));
    }

    // Enable only on big screens, when the side menu is fixed
    let mql = window.matchMedia('(min-width: 801px)');
    if (mql.matches) {
        window.addEventListener('scroll', highlightOnScroll);
        highlightOnScroll();
    }
    mql.addListener(mql => {
        if (mql.matches) {
            window.addEventListener('scroll', highlightOnScroll);
            highlightOnScroll();
        } else {
            window.removeEventListener('scroll', highlightOnScroll);
        }
    });
}

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
