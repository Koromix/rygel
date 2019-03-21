// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let misc = (function() {
    let self = this;

    this.computeEpices = function(data) {
        if (data.aq1_seco4 === null || data.aq1_seco7 === null || data.aq1_seco10 === null ||
                data.aq1_seco2 === null || data.aq1_seco3 === null || data.aq1_seco11 === null ||
                data.aq1_seco12 === null || data.aq1_seco13 === null || data.aq1_seco14 === null ||
                data.aq1_seco15 === null || data.aq1_seco16 === null)
            return null;

        let score = !!data.aq1_seco4 * 10.06 +
                    !!data.aq1_seco7 * -11.83 +
                    !!data.aq1_seco10 * -8.28 +
                    !!data.aq1_seco2 * -8.28 +
                    !!data.aq1_seco3 * 14.8 +
                    !!data.aq1_seco11 * -6.51 +
                    !!data.aq1_seco12 * -7.1 +
                    !!data.aq1_seco13 * -7.1 +
                    !!data.aq1_seco14 * -9.47 +
                    !!data.aq1_seco15 * -9.47 +
                    !!data.aq1_seco16 * -7.1 +
                    75.14;

        return score;
    };

    return this;
}).call({});
