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

import { ApplicationInfo, ApplicationBuilder } from '../client/instance_app.js';

function VmApi(native, main) {
    let self = this;

    // Build main script
    main = buildScript(main, ['app', 'profile']);

    this.buildApp = async function(profile) {
        let app = new ApplicationInfo(profile);
        let builder = new ApplicationBuilder(app);

        try {
            await main({
                app: builder,
                profile: JSON.parse(profile)
            });

            if (!app.pages.length)
                throw new Error('Main script does not define any page');
        } catch (err) {
            throw err;
        }

        return app;
    };

    this.runData = function(app, profile, payload) {
        profile = JSON.parse(profile);
        payload = JSON.parse(payload);

        let transformed = payload;

        let json = JSON.stringify(transformed, null, 4);
        return json;
    };

    function buildScript(code, variables) {
        // JS is always classy
        let AsyncFunction = Object.getPrototypeOf(async function(){}).constructor;

        try {
            let func = new AsyncFunction(variables, code);

            return async api => {
                try {
                    let values = variables.map(key => api[key]);
                    await func(...values);
                } catch (err) {
                    throwScriptError(err);
                }
            };
        } catch (err) {
            throwScriptError(err);
        }
    }

    function throwScriptError(err) {
        let line = Util.parseEvalErrorLine(err);
        let msg = `Erreur de script\n${line != null ? `Ligne ${line} : ` : ''}${err.message}`;

        throw new Error(msg);
    }
}

export { VmApi }
