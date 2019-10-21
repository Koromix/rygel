// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FileManager(db) {
    let self = this;

    this.create = function(path, data) {
        let file = {
            path: path,
            data: data
        };

        return file;
    };

    this.transaction = function(func) {
        return db.transaction(db => func(self));
    };

    this.save = async function(file) {
        await db.saveWithKey('files', file.path, file.data);
    };

    this.delete = async function(path) {
        await db.delete('files', path);
    };

    this.clear = async function() {
        await db.clear('files');
    };

    this.load = async function(path) {
        let data = await db.load('files', path);

        if (data) {
            let file = {
                path: path,
                data: data
            };

            return file;
        } else {
            return null;
        }
    };

    this.list = async function() {
        return await db.list('files');
    };
}
