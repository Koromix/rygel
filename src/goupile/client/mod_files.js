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
        let [files, exist_set] = await Promise.all([
            db.loadAll('files'),
            db.list('files_data').then(list => new Set(list))
        ]);

        return files.filter(file => exist_set.has(file.path));
    };

    this.status = async function() {
        let [files, exist_set, cache_files, remote_files] = await Promise.all([
            db.loadAll('files'),
            db.list('files_data').then(list => new Set(list)),
            db.loadAll('files_remote'),
            fetch(`${env.base_url}api/files.json`).then(response => response.json())
        ]);

        let files_map = util.mapArray(files, file => file.path);
        let cache_map = util.mapArray(cache_files, file => file.path);
        let remote_map = util.mapArray(remote_files, file => file.path);

        let actions = [];

        // Handle local files
        for (let file of files) {
            let remote_file = remote_map[file.path];
            let cache_file = cache_map[file.path];

            if (remote_file && remote_file.sha256 === file.sha256) {
                if (exist_set.has(file.path)) {
                    actions.push(makeAction(file.path, file.sha256, remote_file.sha256, 'noop'));
                } else {
                    actions.push(makeAction(file.path, null, remote_file.sha256, 'push'));
                }
            } else if (remote_file) {
                if (exist_set.has(file.path)) {
                    if (cache_file && cache_file.sha256 === file.sha256) {
                        actions.push(makeAction(file.path, file.sha256, remote_file.sha256, 'pull'));
                    } else if (cache_file && cache_file.sha256 === remote_file.sha256) {
                        actions.push(makeAction(file.path, file.sha256, remote_file.sha256, 'push'));
                    } else {
                        actions.push(makeAction(file.path, file.sha256, remote_file.sha256, 'conflict'));
                    }
                } else {
                    if (cache_file && cache_file.sha256 === remote_file.sha256) {
                        actions.push(makeAction(file.path, null, remote_file.sha256, 'push'));
                    } else {
                        actions.push(makeAction(file.path, null, remote_file.sha256, 'conflict'));
                    }
                }
            } else {
                if (!exist_set.has(file.path)) {
                    actions.push(makeAction(file.path, null, null, 'noop'));
                } else if (cache_file && cache_file.sha256 === file.sha256) {
                    actions.push(makeAction(file.path, file.sha256, null, 'pull'));
                } else {
                    actions.push(makeAction(file.path, file.sha256, null, 'push'));
                }
            }
        }

        // Pull remote-only files
        for (let remote_file of remote_files) {
            if (!files_map[remote_file.path])
                actions.push(makeAction(remote_file.path, null, remote_file.sha256, 'pull'));
        }

        actions.sort(action => action.path);
        return actions;
    };

    function makeAction(path, local, remote, type) {
        let action = {
            path: path,
            local: local,
            remote: remote,
            type: type
        };

        return action;
    }

    this.sync = async function(actions) {
        let entry = new log.Entry;

        entry.progress('Synchronisation en cours');
        try {
            if (actions.some(action => action.type == 'conflict'))
                throw new Error('Conflits non résolus');

            // Perform actions
            await Promise.all(actions.map(action => {
                switch (action.type) {
                    case 'push': {
                        if (action.local) {
                            return self.load(action.path).then(file =>
                                fetch(`${env.base_url}${action.path.substr(1)}`, {method: 'PUT', body: file.data}));
                        } else {
                            return fetch(`${env.base_url}${action.path.substr(1)}`, {method: 'DELETE'});
                        }
                    } break;

                    case 'pull': {
                        if (action.remote) {
                            return fetch(`${env.base_url}${action.path.substr(1)}`).then(response =>
                                response.blob()).then(blob => self.save(self.create(action.path, blob)));
                        } else {
                            return db.transaction(db => {
                                db.delete('files', action.path);
                                db.delete('files_data', action.path);
                            });
                        }
                    } break;
                }
            }));

            // Update information about remote files
            let remote_files = await fetch(`${env.base_url}api/files.json`).then(response => response.json());
            await db.transaction(db => {
                db.clear('files_remote');
                db.saveAll('files_remote', remote_files);
            });

            entry.success('Synchronisation terminée !');
        } catch (err) {
            entry.error(err);
            throw err;
        }
    };
}
