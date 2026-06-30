// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from 'vendor/lit-html/lit-html.bundle.js';
import * as UI from 'lib/web/ui/ui.js';
import * as App from './m_app.js';
import { route } from './m_app.js';
import * as PlanMod from './b_plan.js';
import * as RepositoryMod from './b_repository.js';

const MODES = {
    repositories: { run: RepositoryMod.runRepositories },
    repository: { run: RepositoryMod.runRepository, path: [{ key: 'repository', type: 'integer' }]},

    plans: { run: PlanMod.runPlans },
    plan: { run: PlanMod.runPlan, path: [{ key: 'plan', type: 'integer' }]}
};
const DEFAULT_MODE = 'repositories';

async function start() {
    await App.init(MODES, DEFAULT_MODE, renderMenu);
    await App.go(window.location.href, false);

    UI.main();
    document.body.classList.remove('loading');
}

function renderMenu() {
    if (!App.isLogged())
        return '';

    let in_repositories = ['repositories', 'repository'].includes(route.mode);
    let in_plans = ['plans', 'plan'].includes(route.mode);

    return html`
        <li><a href=${!in_repositories && route.repository != null ? makeURL({ mode: 'repository' }) : '/repositories'}
               class=${in_repositories ? 'active' : ''}>${T.repositories}</a></li>
        <li><a href=${!in_plans && route.plan != null ? makeURL({ mode: 'plan' }) : '/plans'}
               class=${in_plans ? 'active' : ''}>${T.machines}</a></li>
    `;
}

export { start }
