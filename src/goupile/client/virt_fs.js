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

        await db.transaction('rw', ['fs_entries', 'fs_data'], () => {
            db.save('fs_entries', file);
            db.saveWithKey('fs_data', path, data);
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
                    reject(new Error(e.target.error));
                };

                reader.readAsArrayBuffer(piece);
            });

            sha256.update(buf);
        }

        return sha256.finalize();
    }

    this.reset = async function(path) {
        if (env.use_offline) {
            let url = util.pasteURL(`${env.base_url}${path.substr(1)}`, {direct: 1});
            let response = await net.fetch(url);

            if (response.ok) {
                let blob = await response.blob();
                let file = await self.save(path, blob);

                delete file.data;
                await db.save('fs_sync', file);
            } else if (response.status === 404) {
                await db.transaction('rw', ['fs_entries', 'fs_data'], () => {
                    db.delete('fs_entries', path);
                    db.delete('fs_data', path);
                });
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        } else {
            await db.transaction('rw', ['fs_entries', 'fs_data'], () => {
                db.delete('fs_entries', path);
                db.delete('fs_data', path);
            });
        }
    };

    this.delete = async function(path) {
        await db.transaction('rw', ['fs_entries', 'fs_data'], () => {
            db.save('fs_entries', {path: path});
            db.delete('fs_data', path);
        });
    };

    this.clear = async function() {
        await db.transaction('rw', ['fs_entries', 'fs_data', 'fs_sync'], () => {
            db.clear('fs_entries');
            db.clear('fs_data');
            db.clear('fs_sync');
        });
    };

    this.load = async function(path) {
        let [file, data] = await Promise.all([
            db.load('fs_entries', path),
            db.load('fs_data', path)
        ]);

        if (file) {
            if (file.sha256) {
                file.data = data;
                return file;
            } else {
                return null;
            }
        } else if (net.isOnline() || !env.use_offline) {
            let response = await net.fetch(`${env.base_url}${path.substr(1)}`);

            if (response.ok) {
                let blob = await response.blob();
                let file = {
                    path: path,
                    size: blob.size,
                    sha256: response.headers.get('ETag')
                };

                await db.save('fs_sync', file);

                file.data = blob;
                return file;
            } else {
                return null;
            }
        } else {
            return null;
        }
    };

    this.listAll = async function(remote = true) {
        remote &= net.isOnline() || !env.use_offline;

        let files = await self.status(remote);

        files = files.filter(file => file.sha256 || file.remote_sha256);
        files = files.map(file => ({
            path: file.path,
            size: file.sha256 ? file.size : file.remote_size,
            sha256: file.sha256 || file.remote_sha256
        }));

        return files;
    };

    this.status = async function(remote = true) {
        let [local_files, sync_files, remote_files] = await Promise.all([
            db.loadAll('fs_entries'),
            db.loadAll('fs_sync'),
            remote ? net.fetch(`${env.base_url}api/files.json`).then(response => response.json()) : db.loadAll('fs_sync')
        ]);

        let local_map = util.mapArray(local_files, file => file.path);
        let sync_map = util.mapArray(sync_files, file => file.path);
        let remote_map = util.mapArray(remote_files, file => file.path);

        let files = [];

        // Handle local files
        for (let local_file of local_files) {
            let remote_file = remote_map[local_file.path];
            let sync_file = sync_map[local_file.path];

            if (remote_file) {
                if (local_file.sha256) {
                    if (local_file.sha256 === remote_file.sha256) {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'noop'));
                    } else if (sync_file && sync_file.sha256 === local_file.sha256) {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'pull'));
                    } else if (sync_file && sync_file.sha256 === remote_file.sha256) {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'push'));
                    } else {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'conflict'));
                    }
                } else {
                    if (sync_file && sync_file.sha256 === remote_file.sha256) {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'push'));
                    } else {
                        files.push(makeSyncEntry(local_file.path, local_file, remote_file, 'conflict'));
                    }
                }
            } else {
                if (!local_file.sha256) {
                    // Special case, the file does not exist anywhere anymore but the client
                    // still has some metadata. This pull action will clean up the pieces.
                    files.push(makeSyncEntry(local_file.path, local_file, null, 'pull'));
                } else if (sync_file && sync_file.sha256 === local_file.sha256) {
                    files.push(makeSyncEntry(local_file.path, local_file, null, 'pull'));
                } else {
                    files.push(makeSyncEntry(local_file.path, local_file, null, 'conflict'));
                }
            }
        }

        // Pull remote-only files
        {
            let new_sync_files = [];

            for (let remote_file of remote_files) {
                if (!local_map[remote_file.path]) {
                    let action = env.use_offline ? 'pull' : 'noop';
                    files.push(makeSyncEntry(remote_file.path, null, remote_file, action));

                    new_sync_files.push(remote_file);
                }
            }

            await db.saveAll('fs_sync', new_sync_files);
        }

        files.sort((file1, file2) => util.compareValues(file1.path, file2.path));

        return files;
    };

    function makeSyncEntry(path, local, remote, action) {
        let file = {
            path: path,
            action: action
        };

        if (local) {
            if (local.sha256) {
                file.size = local.size;
                file.sha256 = local.sha256;
            } else {
                file.deleted = true;
            }
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
        let url = util.pasteURL(`${env.base_url}${file.path.substr(1)}`, {
            direct: 1,
            sha256: file.remote_sha256 || ''
        });

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
                    if (env.use_offline) {
                        await db.save('fs_sync', file2);
                    } else {
                        await db.transaction('rw', ['fs_entries', 'fs_data', 'fs_sync'], () => {
                            db.delete('fs_entries', file.path);
                            db.delete('fs_data', file.path);
                            db.save('fs_sync', file2);
                        });
                    }
                } else {
                    let response = await net.fetch(url, {method: 'DELETE'});
                    if (!response.ok && response.status !== 404) {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }

                    await db.transaction('rw', ['fs_entries', 'fs_sync'], () => {
                        db.delete('fs_entries', file.path);
                        db.delete('fs_sync', file.path);
                    });
                }
            } break;

            case 'pull': {
                if (file.remote_sha256) {
                    let blob = await net.fetch(url).then(response => response.blob());
                    let file2 = await self.save(file.path, blob);

                    delete file2.data;
                    await db.save('fs_sync', file2);
                } else {
                    await db.transaction('rw', ['fs_entries', 'fs_data', 'fs_sync'], () => {
                        db.delete('fs_entries', file.path);
                        db.delete('fs_data', file.path);
                        db.delete('fs_sync', file.path);
                    });
                }
            } break;
        }
    }
}
