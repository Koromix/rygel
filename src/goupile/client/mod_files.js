// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FileManager(db) {
    let self = this;

    this.create = function(path, data) {
        if (!(data instanceof Blob))
            data = new Blob([data]);

        let file = {
            path: path,
            data: data
        };

        return Object.freeze(file);
    };

    this.transaction = function(func) {
        return db.transaction(db => func(self));
    };

    this.save = async function(file) {
        // TODO: Fix race condition between hash computation and file storage
        let hash = await computeSha256(file.data);

        await db.transaction(db => {
            let file2 = {
                path: file.path,
                sha256: hash
            };

            db.save('files', file2);
            db.saveWithKey('files_data', file.path, file.data);
        });
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

        return sha256.finalize();
    }

    this.delete = async function(path) {
        await db.delete('files_data', path);
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

        if (file && data) {
            file.data = data;
            return Object.freeze(file);
        } else {
            return null;
        }
    };

    this.list = async function() {
        let [files, set] = await Promise.all([
            db.loadAll('files'),
            db.list('files_data').then(list => new Set(list))
        ]);

        return files.filter(file => set.has(file.path));
    };
}
