// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function ApplicationNavigator() {
    let self = this;

    // Used to prevent go() from working when a script is first opened or changed
    // in the editor, because then we wouldn't be able to come back to the script to
    // fix the code.

    // Avoid async here, because it may fail (see block_go) and the caller
    // may need to catch that synchronously.
    this.go = function(url = undefined, push_history = true) {
        if (goupile.isRunning()) {
            throw new Error(`A navigation function (e.g. go()) has been interrupted.
Navigation functions should only be called in reaction to user events, such as button clicks.`);
        }

        if (url != null && !url.match(/^((http|ftp|https):\/\/|\/)/g))
            url = `${env.base_url}app/${url}`;

        goupile.go(url, push_history);
    };

    this.isConnected = function() { return  goupile.isConnected(); };
    this.isTablet = function() { return goupile.isTablet(); };
    this.isStandalone = function() { return goupile.isStandalone(); };
    this.isLocked = function() { return goupile.isLocked(); };
}
