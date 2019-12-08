// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_files = new function() {
    let self = this;

    let actions;
    let user_actions = {};

    this.runFiles = async function() {
        // Compute actions, and overwrite with user choices (if any)
        actions = await file_manager.status();
        for (let action of actions)
            action.type = user_actions[action.path] || action.type;

        renderActions();
    };

    function renderActions() {
        let enable_sync = actions.some(action => action.type !== 'noop') &&
                          !actions.some(action => action.type === 'conflict');

        render(html`
            <table class="dev_sync">
                <thead><tr>
                    <th>Local</th>
                    <th class="dev_action"></th>
                    <th class="dev_action"></th>
                    <th class="dev_action"></th>
                    <th>Distant</th>
                </tr></thead>

                <tbody>${actions.map(action => {
                    return html`<tr>
                        <td class=${action.type == 'pull' ? 'dev_overwrite' : ''}>${action.local ? action.path : ''}</td>
                        <td class=${action.type === 'pull' ? 'dev_action current' : 'dev_action'}
                            @click=${e => toggleAction(action, 'pull')}>←</td>
                        <td class=${action.type === 'noop' ? 'dev_action current' : 'dev_action'}
                            @click=${e => toggleAction(action, 'noop')}>${action.type === 'conflict' ? '?' : '='}</td>
                        <td class=${action.type === 'push' ? 'dev_action current' : 'dev_action'}
                            @click=${e => toggleAction(action, 'push')}>→</td>
                        <td class=${action.type == 'push' ? 'dev_overwrite' : ''}>${action.remote ? action.path : ''}</td>
                    `;
                })}</tbody>
            </table>

            <div class="af_buttons">
                <button class="af_button" ?disabled=${!enable_sync}
                        @click=${syncFiles}>Synchroniser</button>
            </div>
        `, document.querySelector('#dev_files'));
    }

    function toggleAction(action, type) {
        action.type = type;
        user_actions[action.path] = action.type;

        renderActions();
    }

    async function syncFiles() {
        let entry = new log.Entry;
        entry.progress('Synchronisation en cours');

        try {
            await file_manager.sync(actions);
            entry.success('Synchronisation terminée !');
        } catch (err) {
            entry.error(err);
        }

        user_actions = {};
        await self.runFiles();
    }
};
