// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function VirtualFS(db) {
    let self = this;

    this.save = async function(path, data) {
        if (!(data instanceof Blob))
            data = new Blob([data]);

        let file = {
            path: path,
            size: data.size,
            // XXX: Fix race condition between hash computation and file storage
            sha256: await computeSha256(data)
        };

        await db.transaction('rw', ['files', 'files_data'], () => {
            db.save('files', file);
            db.saveWithKey('files_data', path, data);
        });

        return file;
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
        await db.transaction('rw', ['files', 'files_data'], () => {
            db.save('files', {path: path});
            db.delete('files_data', path);
        });
    };

    this.clear = async function() {
        await db.transaction('rw', ['files', 'files_data'], () => {
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
            if (file.sha256) {
                file.data = data;
                return file;
            } else {
                return null;
            }
        } else {
            let response = await net.fetch(`${env.base_url}${path.substr(1)}`);

            if (response.ok) {
                let blob = await response.blob();
                let file = {
                    path: path,
                    size: blob.size,
                    sha256: response.headers.get('ETag')
                };

                await db.save('files_cache', file);

                file.data = blob;
                return file;
            } else {
                return null;
            }
        }
    };

    this.listAll = async function(remote = true) {
        let files = await self.status(remote);

        files = files.filter(file => file.sha256 || file.remote_sha256);
        files = files.map(file => ({
            path: file.path,
            size: file.sha256 ? file.size : file.remote_size,
            sha256: file.sha256 || file.remote_sha256
        }));

        return files;
    };

    this.listLocal = async function() {
        let files = await db.loadAll('files');
        return files.filter(file => file.sha256);
    };

    this.status = async function(remote = true) {
        let [local_files, cache_files, remote_files] = await Promise.all([
            db.loadAll('files'),
            db.loadAll('files_cache'),
            remote ? net.fetch(`${env.base_url}api/files.json`).then(response => response.json()) : db.loadAll('files_cache')
        ]);

        let local_map = util.mapArray(local_files, file => file.path);
        let cache_map = util.mapArray(cache_files, file => file.path);
        let remote_map = util.mapArray(remote_files, file => file.path);

        let files = [];

        // Handle local files
        for (let local_file of local_files) {
            let remote_file = remote_map[local_file.path];
            let cache_file = cache_map[local_file.path];

            if (remote_file && remote_file.sha256 === local_file.sha256) {
                if (local_file.sha256) {
                    files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'noop'));
                } else {
                    files.push(makeSyncEntry(local_file.path, null, remote_file, 'push'));
                }
            } else if (remote_file) {
                if (local_file.sha256) {
                    if (cache_file && cache_file.sha256 === local_file.sha256) {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'pull'));
                    } else if (cache_file && cache_file.sha256 === remote_file.sha256) {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'push'));
                    } else {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'conflict'));
                    }
                } else {
                    if (cache_file && cache_file.sha256 === remote_file.sha256) {
                        files.push(makeSyncEntry(local_file.path, null, remote_file, 'push'));
                    } else {
                        files.push(makeSyncEntry(local_file.path, null, remote_file, 'conflict'));
                    }
                }
            } else {
                if (!local_file.sha256) {
                    files.push(makeSyncEntry(local_file.path, null, null, 'noop'));
                } else if (cache_file && cache_file.sha256 === local_file.sha256) {
                    files.push(makeSyncEntry(local_file.path, local_file, null, 'pull'));
                } else {
                    files.push(makeSyncEntry(local_file.path, local_file, null, 'push'));
                }
            }
        }

        // Pull remote-only files
        for (let remote_file of remote_files) {
            if (!local_map[remote_file.path])
                files.push(makeSyncEntry(remote_file.path, null, remote_file, 'pull'));
        }

        return files;
    };

    function makeSyncEntry(path, local, remote, action) {
        let file = {
            path: path,
            action: action
        };

        if (local) {
            file.size = local.size;
            file.sha256 = local.sha256;
        }
        if (remote) {
            file.remote_size = remote.size;
            file.remote_sha256 = remote.sha256;
        }

        return file;
    }

    this.sync = async function(files) {
        if (files.some(file => file.action == 'conflict'))
            throw new Error('Conflits non résolus');

        for (let i = 0; i < files.length; i += 10) {
            let p = files.slice(i, i + 10).map(executeSyncAction);
            await Promise.all(p);
        }
    };

    async function executeSyncAction(file) {
        let url = util.pasteURL(`${env.base_url}${file.path.substr(1)}`,
                                {sha256: file.remote_sha256 || ''});

        switch (file.action) {
            case 'push': {
                if (file.sha256) {
                    let file2 = await self.load(file.path);

                    let response = await net.fetch(url, {method: 'PUT', body: file2.data});
                    if (!response.ok) {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }

                    delete file2.data;
                    await db.save('files_cache', file2);
                } else {
                    let response = await net.fetch(url, {method: 'DELETE'});
                    if (!response.ok) {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }

                    await db.transaction('rw', ['files', 'files_cache'], () => {
                        db.delete('files', file.path);
                        db.delete('files_cache', file.path);
                    });
                }
            } break;

            case 'pull': {
                if (file.remote_sha256) {
                    let blob = await net.fetch(url).then(response => response.blob());
                    let file2 = await self.save(file.path, blob);

                    delete file2.data;
                    await db.save('files_cache', file2);
                } else {
                    await db.transaction('rw', ['files', 'files_cache', 'files_data'], () => {
                        db.delete('files', file.path);
                        db.delete('files_data', file.path);
                        db.delete('files_cache', file.path);
                    });
                }
            } break;
        }
    }
}
