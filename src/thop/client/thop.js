// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let thop = (function() {
    let self = this;

    function initLog() {
        // STUB
    }

    function initNavigation() {
        window.addEventListener('popstate', e => {
            self.go(window.location.href, false);
        });

        document.body.addEventListener('click', e => {
            if (e.target && e.target.tagName == 'A' &&
                    !e.ctrlKey && !e.target.getAttribute('download')) {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    self.go(href);
                    e.preventDefault();
                }
            }
        });
    }

    this.go = function(href, history = true) {
        // STUB
    };

    function initThop() {
        initLog();
        initNavigation();

        self.go(window.location.href, false);
    }

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete')
            initThop();
    });

    return this;
}).call({});
