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

window.addEventListener('load', e => {
    initDeploy();
    initMenu();
    initSide();
    initScroll();
});

function initDeploy() {
    let el = document.querySelector('#deploy');

    if (el != null)
        el.addEventListener('click', deploy);
}

function deploy() {
    let top = document.querySelector('nav#top');
    top.classList.toggle('active');
}

function initMenu() {
    let items = document.querySelectorAll('nav#top li');

    document.body.addEventListener('click', e => {
        if (e.target.tagName == 'DIV' && findParent(e.target, el => el.id == 'top'))
            return;

        // Expand top menu categories
        {
            let target = findParent(e.target, el => el.tagName == 'LI');

            if (target?.querySelector('div') != null) {
                for (let item of items) {
                    if (item == target) {
                        item.classList.toggle('active');
                    } else {
                        item.classList.remove('active');
                    }
                }
            }
        }

        if (e.target.tagName == 'A' && e.target.getAttribute('href') == '#')
            e.preventDefault();
    });
}

function initSide() {
    // Find all side menu items, and the pointed to elements
    let links = [].slice.call(document.querySelectorAll('nav#side a'));
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

        let style = window.getComputedStyle(document.documentElement);
        let treshold = parseFloat(style.getPropertyValue('scroll-padding-top')) + 10;

        let idx;
        for (idx = 0; idx < items.length; idx++) {
            let rect = items[idx][1].getBoundingClientRect();
            if (rect.top >= treshold)
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

function initScroll() {
    window.addEventListener('scroll', adjust_top);
    adjust_top();

    function adjust_top() {
        let top = document.querySelector('nav#top');

        if (top != null)
            top.classList.toggle('border', window.pageYOffset >= 20);
    }
}

function findParent(el, func) {
    while (el && !func(el))
        el = el.parentElement;
    return el;
}


document.documentElement.classList.remove('nojs');
document.documentElement.classList.add('js');

export { deploy }
