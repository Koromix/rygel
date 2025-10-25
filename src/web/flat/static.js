// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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
