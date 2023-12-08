import { Util, Net, Log, Base64, UI } from './base.js';
import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';

let admin = false;
let news = null;

async function start() {
    Log.pushHandler(UI.notifyHandler);

    try {
        news = await Net.get('/api/api.php?method=news');
        admin = true;
    } catch (err) {
        if (err.status != 401)
            throw err;
    }

    await run();
}

async function run() {
    if (admin) {
        if (news == null)
            news = await Net.get('/api/api.php?method=news');
        renderNews();
    } else {
        renderLogin();
    }

    document.body.classList.remove('loading');
}

function renderLogin() {
    let page_el = document.querySelector('#page');

    render(html`
        <div class="dialog screen">
            <form @submit=${UI.wrap(login)}>
                <div class="title">Back-office DEMHETER</div>
                <div class="main">
                    <label>
                        <span>Mot de passe</span>
                        <input name="password" type="password" />
                    </label>
                </div>

                <div class="footer">
                    <button>Valider</button>
                </div>
            </form>
        </div>
    `, page_el);
}

async function login(e) {
    let target = e.target;
    let password = target.elements.password.value;

    await Net.post('/api/api.php?method=login', { password: password });
    admin = true;

    run();
}

function renderNews() {
    let page_el = document.querySelector('#page');

    render(html`
        <div class="dialog screen">
            <form @submit=${UI.wrap(submitNews)}>
                <div class="title">
                    News DEMHETER
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(addNews)}>Ajouter</button>
                </div>

                <div class="main">
                    <table style="table-layout: fixed;">
                        <colgroup>
                            <col>
                            <col>
                            <col>
                            <col class="check">
                        </colgroup>

                        <thead>
                            <th>Image</th>
                            <th>Titre</th>
                            <th>Contenu</th>
                            <th></th>
                        </thead>

                        <tbody>
                            ${news.map(item => {
                                return html`
                                    <tr>
                                        <td style="text-align: center;">
                                            ${item.png === true ? html`<img src=${'/api/api.php?method=png&id=' + item.id} height="32" alt=""/><br/>` : ''}
                                            ${typeof item.png == 'string' ? html`<img src=${'data:image/png;base64,' + item.png} height="32" alt=""/><br/>` : ''}
                                            <div style="float: right">
                                                <button type="button" class="small" @click=${e => updateImage(item)}>Modifier</button>
                                                ${item.png ? html`<button type="button" class="small"  @click=${e => { item.png = null; run(); }}><img src="static/delete.webp" alt="Supprimer" /></button>` : ''}
                                            </div>
                                        </td>
                                        <td><input class="title" type="text" value=${item.title}
                                                   @input=${e => { item.title = e.target.value; }}></td>
                                        <td><textarea class="content" style="width: 100%";" rows="5"
                                                      @input=${e => { item.content = e.target.value; }}>${item.content}</textarea></td>
                                        <td class="right">
                                            <button type="button" class="small"
                                                    @click=${UI.wrap(e => deleteNews(item))}><img src="static/delete.webp" alt="Supprimer" /></button>
                                        </td>
                                    </tr>
                                `;
                            })}
                            ${!news.length ? html`<tr><td colspan="4" style="text-align: center;">Aucun contenu à afficher</td></tr>` : ''}
                        </tbody>
                    </table>
                </div>

                <div class="footer">
                    <button type="button" class="danger" @click=${UI.wrap(resetNews)}>Annuler</button>
                    <button type="submit">Valider</button>
                </div>
            </form>
        </div>
    `, page_el);
}

function addNews(e) {
    let item = {
        png: null,
        title: '',
        content: ''
    };
    news.push(item);

    run();
}

function deleteNews(item) {
    news = news.filter(it => it !== item);
    run();
}

async function updateImage(item) {
    let file = await Util.loadFile();

    let buf = await file.arrayBuffer();
    let base64 = Base64.toBase64(buf);

    item.png = base64;

    run();
}

async function resetNews() {
    news = await Net.get('/api/api.php?method=news');
    run();
}

async function submitNews(e) {
    let target = e.target;
    let payload = [];

    news = await Net.post('/api/api.php?method=news', { news: news });

    run();
}

export { start }
