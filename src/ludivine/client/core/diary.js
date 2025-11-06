// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, LocalDate } from '../../../core/web/base/base.js';
import { ASSETS } from '../../assets/assets.js';
import { ClassicEditor, AutoLink, Bold, Code, Essentials, FontBackgroundColor,
         FontColor, FontFamily, FontSize, Highlight, Italic, Link, Paragraph,
         RemoveFormat, Strikethrough, Subscript, Superscript, Underline, Translations } from '../../../../vendor/ckeditor5/ckeditor5.bundle.js';

import '../../../../vendor/ckeditor5/ckeditor5.bundle.css';

function DiaryModule(app) {
    let self = this;

    const UI = app.UI;
    const route = app.route;
    const db = app.db;

    let data = null;

    let div = document.createElement('div');
    let editor = null;

    let save_timer = null;

    this.run = async function() {
        if (data == null || data.id != route.entry) {
            data = await db.fetch1('SELECT id, date, title, content FROM diary WHERE id = ?', route.entry);

            if (data != null)
                data.date = LocalDate.parse(data.date);

            if (editor != null)
                editor.destroy();
            editor = null;
        }

        if (editor == null) {
            // We need the DOM node to create instantiate CKEditor
            self.render();

            editor = await ClassicEditor.create(div.querySelector('#editor'), {
                toolbar: {
                    items: [
                        'fontSize',
                        'fontFamily',
                        'fontColor',
                        'fontBackgroundColor',
                        '|',
                        'bold',
                        'italic',
                        'underline',
                        'strikethrough',
                        'subscript',
                        'superscript',
                        'code',
                        'removeFormat',
                        '|',
                        'link',
                        'highlight'
                    ],
                    shouldNotGroupWhenFull: false
                },
                plugins: [AutoLink, Bold, Code, Essentials, FontBackgroundColor, FontColor, FontFamily,
                          FontSize, Highlight, Italic, Link, Paragraph, RemoveFormat,
                          Strikethrough, Subscript, Superscript, Underline],
                licenseKey: 'GPL',
                initialData: data?.content ?? '',
                translations: [Translations],
                language: 'fr',
                fontFamily: {
                    supportAllValues: true
                },
                fontSize: {
                    options: [10, 12, 14, 'default', 18, 20, 22],
                    supportAllValues: true
                },
                link: {
                    addTargetToExternalLinks: true,
                    defaultProtocol: 'https://',
                    decorators: {
                        toggleDownloadable: {
                            mode: 'manual',
                            label: 'Downloadable',
                            attributes: {
                                download: 'file'
                            }
                        }
                    }
                },
                menuBar: {
                    isVisible: true
                },
                placeholder: `Écrivez ce que vous voulez, c'est pour vous et nous n'y aurons pas accès !`,
            });

            editor.model.document.on('change:data', autoSave);
        }
    };

    this.stop = function() {
        if (editor != null)
            editor.destroy();
    };

    this.render = function(section) {
        let entry = route.entry;

        render(html`
            <div class="box">
                <div class="header">
                    ${data != null ? html`
                        Journal - Entrée du ${data.date.toLocaleString()}
                        <button type="button" class="small secondary" style="margin-left: 30px;"
                                @click=${UI.wrap(e => deleteEntry(data))}>Supprimer cette entrée</button>
                    ` : ''}
                    ${data == null ? `Journal - Nouvelle entrée` : ''}
                </div>

                <form @submit=${e => e.preventDefault()}>
                    <label>
                        <span>Titre de l'entrée</span>
                        <input type="text" name="title" value=${data?.title ?? ''}
                               placeholder="Titre libre" @input=${autoSave} />
                    </label>
                    <div class="widget">
                        <label for="editor">Contenu de l'entrée</label>
                        <div id="editor"></div>
                    </div>
                </form>

                <div class="actions">
                    <button type="button" @click=${UI.wrap(exit)}>
                        ${data != null ? 'Enregistrer et retourner au profil' : ''}
                        ${data == null ? 'Retourner au profil' : ''}
                    </button>
                </div>
            </div>
        `, div);

        return div;
    };

    this.hasUnsavedData = function() {
        let unsafe = (save_timer != null);
        return unsafe;
    };

    function autoSave() {
        if (save_timer != null)
            clearTimeout(save_timer);

        save_timer = setTimeout(() => {
            save_timer = null;
            save();
        }, 1000);
    }

    async function save() {
        let form = div.querySelector('form');
        let elements = form.elements;

        if (data == null) {
            data = {
                date: LocalDate.today(),
                title: null,
                content: null
            };
        }

        data.title = elements.title.value;
        data.content = editor.getData();

        route.entry = await db.pluck(`INSERT INTO diary (id, date, title, content)
                                      VALUES (?, ?, ?, ?)
                                      ON CONFLICT DO UPDATE SET title = excluded.title,
                                                                content = excluded.content
                                      RETURNING id`,
                                     route.entry, data.date.toString(), data.title, data.content);

        await app.run();
    }

    async function deleteEntry(data) {
        await UI.dialog((render, close) => html`
            <div class="tabbar">
                <a class="active">Supprimer l'entrée de journal du ${data.date.toLocaleString()}</a>
            </div>

            <div class="tab">
                <div class="box">Attention, cette action n'est pas réversible !</div>

                <div class="actions">
                    <button type="button" class="secondary" @click=${close}>Annuler</button>
                    <button type="submit">Confirmer</button>
                </div>
            </div>
        `);

        await db.exec('DELETE FROM diary WHERE id = ?', data.id);

        if (data.id == route.entry)
            await app.go('/profil');
    }

    async function exit() {
        if (save_timer != null) {
            clearTimeout(save_timer);
            await save();
        }

        await app.go('/profil');
    }
}

export { DiaryModule }
