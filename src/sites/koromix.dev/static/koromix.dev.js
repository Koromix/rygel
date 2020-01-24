// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function initMenuHighlight() {
    // Find all side menu items, and the pointed to elements
    let links = [].slice.call(document.querySelectorAll('nav#side_menu a'));
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
            if (rect.top >= 40)
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

function toggleMenu(selector) {
    let menu = document.querySelector(selector);
    menu.classList.toggle('active');
}

document.addEventListener('readystatechange', () => {
    if (document.readyState === 'complete')
        initMenuHighlight();
});

document.body.classList.remove('nojs');
document.body.classList.add('js');
