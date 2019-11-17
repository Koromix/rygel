// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FileManager(db) {
    let self = this;

    this.create = async function(path, data) {
        if (!(data instanceof Blob))
            data = new Blob([data]);

        let file = {
            path: path,
            sha256: await computeSha256(data),
            data: data
        };

        return Object.freeze(file);
    };

    // Javascript Blob/File API sucks, plain and simple
    async function computeSha256(blob) {
        let sha256 = new Sha256;

        for (let offset = 0; offset < blob.size; offset += 65536) {
            let piece = blob.slice(offset, offset + 65536);
            let buf = await new Promise((resolve, reject) => {
                let reader = new FileReader;

                reader.onload = e => resolve(e.target.result);
                reader.onerror = e => {
                    reader.abort();
                    reject(e.target.error);
                };

                reader.readAsArrayBuffer(piece);
            });

            sha256.update(buf);
        }

        sha256.finalize();
        return sha256.toString();
    }

    this.transaction = function(func) {
        return db.transaction(db => func(self));
    };

    this.save = async function(file) {
        await db.transaction(db => {
            let file2 = {
                path: file.path,
                sha256: file.sha256
            };

            db.save('files', file2);
            db.saveWithKey('files_data', file.path, file.data);
        });
    };

    this.delete = async function(path) {
        await db.transaction(db => {
            db.delete('files', path);
            db.delete('files_data', path);
        });
    };

    this.clear = async function() {
        await db.transaction(db => {
            db.clear('files');
            db.clear('files_data');
        });
    };

    this.load = async function(path) {
        let [file, data] = await Promise.all([
            db.load('files', path),
            db.load('files_data', path)
        ]);

        if (file) {
            file.data = data;
            return file;
        } else {
            return null;
        }
    };

    this.list = async function() {
        let files = await db.loadAll('files');
        return files.map(file => file.path);
    };
}
