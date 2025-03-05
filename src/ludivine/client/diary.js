// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, LocalDate } from '../../web/core/base.js';
import * as app from './app.js';
import * as UI from './ui.js';
import { ASSETS } from '../assets/assets.js';
import { ClassicEditor, AutoLink, Bold, Essentials,
         Italic, Link, Paragraph, Translations } from '../../../vendor/ckeditor5/ckeditor5.bundle.js';

import '../../../vendor/ckeditor5/ckeditor5.bundle.css';

function DiaryModule(db, id) {
    let self = this;

    let data = null;

    let div = null;
    let editor = null;

    let save_timer = null;

    this.start = async function() {
        div = document.createElement('div');

        // Load existing data
        data = await db.fetch1('SELECT date, title, content FROM diary WHERE id = ?', id);

        if (data != null)
            data.date = LocalDate.parse(data.date);

        // We need the DOM node to create instantiate CKEditor
        self.render();

        editor = await ClassicEditor.create(div.querySelector('#editor'), {
            toolbar: {
                items: ['bold', 'italic', '|', 'link'],
                shouldNotGroupWhenFull: false
            },
            plugins: [AutoLink, Bold, Essentials, Italic, Link, Paragraph],
            licenseKey: 'GPL',
            initialData: data?.content ?? '',
            translations: [Translations],
            language: 'fr',
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
            placeholder: 'Dites ce que vous voulez ici',
        });

        editor.model.document.on('change:data', autoSave);
    };

    this.stop = function() {
        if (editor != null)
            editor.destroy();
    };

    this.render = function(section) {
        render(html`
            <div class="box">
                <div class="header">
                    ${data != null ? `Journal - Entrée du ${data.date.toLocaleString()}` : ''}
                    ${data == null ? `Journal - Nouvelle entrée` : ''}
                </div>

                <form @submit=${e => e.preventDefault()}>
                    <label>
                        <span>Titre de l'entrée</span>
                        <input type="text" name="title" value=${data?.title ?? ''} @input=${autoSave} />
                    </label>
                    <div class="widget">
                        <label for="editor">Contenu de l'entrée</label>
                        <div id="editor"></div>
                    </div>
                </form>

                ${id != null ? html`
                    <div class="actions">
                        <button type="button" class="secondary"
                                @click=${UI.confirm(`Supprimer l'entrée de journal du ${data.date.toLocaleString()}`, deleteEntry)}>Supprimer cette entrée</button>
                    </div>
                ` : ''}
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

        id = await db.pluck(`INSERT INTO diary (id, date, title, content)
                             VALUES (?, ?, ?, ?)
                             ON CONFLICT DO UPDATE SET title = excluded.title,
                                                       content = excluded.content
                             RETURNING id`,
                            id, data.date.toString(), data.title, data.content);

        self.render();
    }

    async function deleteEntry() {
        await db.exec('DELETE FROM diary WHERE id = ?', id);
        await app.go('/profile');
    }
}

export { DiaryModule }
