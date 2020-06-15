// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function ApplicationNavigator() {
    let self = this;

    let block = 0;

    this.block = function() { block++; };
    this.unblock = function() { block--; };

    // Avoid async here, because it may fail (see block_go) and the caller
    // may need to catch that synchronously.
    this.go = function(url = null, push_history = true) {
        if (block) {
            throw new Error(`A navigation function (e.g. go()) has been interrupted.
Navigation functions should only be called in reaction to user events, such as button clicks.`);
        }

        if (!url.match(/^((http|ftp|https):\/\/|\/)/g))
            url = `${env.base_url}app/${url}`;

        goupile.go(url, push_history);
    };
}
