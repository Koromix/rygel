import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Net, Log } from '../../../../lib/web/base/base.js';
import { Base64 } from '../../../../lib/web/base/mixer.js';
import { UI } from './ui.js';

import './admin.css';

const IMAGE_HEIGHT = 640;

let admin = false;
let news = null;

async function start() {
    Log.pushHandler(UI.notifyHandler);

    try {
        news = await Net.get('api.php?method=news');
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
            news = await Net.get('api.php?method=news');
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

    await Net.post('api.php?method=login', { password: password });
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
                    <table style="table-layout: fixed; width: 800px;">
                        <colgroup>
                            <col style="width: 140px;">
                            <col style="width: 200px;">
                            <col>
                            <col style="width: 80px;">
                        </colgroup>

                        <thead>
                            <th>Image</th>
                            <th>Titre</th>
                            <th>Contenu</th>
                            <th></th>
                        </thead>

                        <tbody>
                            ${news.map(item => {
                                let image = null;

                                if (typeof item.webp == 'string') {
                                    if (item.webp.match(/^[a-z0-9]{64}$/)) {
                                        image = `/data/${item.webp}.webp`;
                                    } else {
                                        image = 'data:image/webp;base64,' + item.webp;
                                    }
                                }

                                return html`
                                    <tr>
                                        <td style="text-align: center;">
                                            ${image != null ? html`<img src=${image} height="32" alt=""/><br/>` : ''}
                                            <div>
                                                <button type="button" class="small" @click=${e => updateImage(item)}>Modifier</button>
                                                ${item.webp != null ? html`<button type="button" class="small"  @click=${e => { item.webp = null; run(); }}><img src="static/delete.webp" alt="Supprimer" /></button>` : ''}
                                            </div>
                                        </td>
                                        <td><input class="title" type="text" value=${item.title}
                                                   @input=${e => { item.title = e.target.value; }}></td>
                                        <td><textarea class="content" style="width: 100%;" rows="7"
                                                      @input=${e => { item.content = e.target.value; }}>${item.content}</textarea></td>
                                        <td class="center">
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
        webp: null,
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
    let img = null;

    // Resize if needed
    {
        let file = await Util.loadFile();
        let src = await loadImage(file);

        if (src.height != IMAGE_HEIGHT) {
            let height = IMAGE_HEIGHT;
            let width = Math.round(src.width / src.height * height);

            img = await resizeImage(src, width, height);
        } else {
            img = file;
        }
    }

    let buf = await img.arrayBuffer();
    let base64 = Base64.toBase64(buf);

    item.webp = base64;

    run();
}

async function loadImage(file) {
    let url = URL.createObjectURL(file);

    let img = new Promise((resolve, reject) => {
        let img = new Image;

        img.addEventListener('load', () => resolve(img));
        img.addEventListener('error', e => reject(new Error('Failed to load image')));

        img.src = url;
    });

    return img;
}

async function resizeImage(img, width, height) {
    let canvas = new OffscreenCanvas(width, height);
    let ctx = canvas.getContext('2d');

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    ctx.drawImage(img, 0, 0, width, height);

    let blob = await canvas.convertToBlob({ type: 'image/webp', quality: 0.9 });
    return blob;
}

async function resetNews() {
    news = await Net.get('api.php?method=news');
    run();
}

async function submitNews(e) {
    let target = e.target;
    let payload = [];

    news = await Net.post('api.php?method=news', { news: news });

    run();
}

export { start }
