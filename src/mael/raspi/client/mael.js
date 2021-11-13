// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

let assets = {};

let ws;
let connected = false;
let recv_time;
let recv_first;
let recv_last;

let robot;

async function init() {
    let asset_paths = [
        'playground.webp',
        'ui/busy.png',
        'ui/info.png',
        'ui/error.png',
        'ui/left.png',
        'ui/right.png'
    ];

    let images = await Promise.all(asset_paths.map(path => {
        let basename = path.replace(/^.*\//, '');
        let url = 'static/' + basename;

        return loadTexture(url);
    }));

    for (let i = 0; i < asset_paths.length; i++) {
        let url = asset_paths[i];
        let ptr = assets;

        let parts = url.split('/');
        let basename = parts.pop().replace(/\..*$/, '');

        for (let part of parts) {
            if (ptr[part] == null)
                ptr[part] = {};
            ptr = ptr[part];
        }
        ptr[basename] = images[i];
    }

    recv_time = -100000;
}

function update() {
    let delay = performance.now() - recv_time;

    // Check and update connection status
    if (delay > 8000) {
        if (ws != null && ws.readyState === 1) {
            let err = new Error('Data connection timed out');
            log.error(err);

            connected = false;
        } else if (!connected) {
            let url = new URL(window.location.href);
            ws = new WebSocket(`ws://${url.host}/api/ws`);

            ws.onerror = e => {
                if (connected) {
                    let err = new Error('Lost connection to WebSocket endpoint');
                    log.error(err);
                } else {
                    let err = new Error('Failed to connect to WebSocket endpoint');
                    log.error(err);
                }

                connected = false;
                ws.close();
                ws = null;
            };

            ws.onclose = e => {
                if (connected) {
                    let err = new Error('WebSocket connection has been closed');
                    log.error(err);
                }

                connected = false;
                ws.close();
                ws = null;
            };

            ws.onmessage = async e => {
                connected = true;
                recv_time = performance.now();

                let buf = await e.data.arrayBuffer();
                let pkt = {
                    data: buf,
                    next: null
                };

                if (recv_last) {
                    recv_last.next = pkt;
                } else {
                    recv_first = pkt;
                    recv_last = pkt;
                }
            };
        }

        recv_time = performance.now();
    }

    // Process incoming packets
    for (let pkt = recv_first; pkt; pkt = pkt.next) {
        let view = new DataView(pkt.data);

        if (view.byteLength < 8) {
            console.log('Truncated packet');
            continue;
        }

        let crc32 = view.getInt32(0, true);
        let type = view.getUint16(4, true);
        let payload = view.getUint16(6, true);

        if (payload !== view.byteLength - 8) {
            console.log('Invalid payload length');
            continue;
        }
        if (type > messages.length) {
            console.log('Invalid packet type');
            continue;
        }
        if (crc32 !== CRC32.buf(new Uint8Array(pkt.data, 4))) {
            console.log('Packet failed CRC32 check');
            continue;
        }

        let obj = {};
        try {
            let info = messages[type];
            let offset = 8;

            for (let key in info.members) {
                let type = info.members[key];

                switch (type) {
                    case 'double': {
                        obj[key] = view.getFloat64(offset, true);
                        offset += 8;
                    } break;
                    case 'Vec2': {
                        obj[key] = {
                            x: view.getFloat64(offset, true),
                            y: view.getFloat64(offset + 8, true)
                        };
                        offset += 16;
                    } break;
                    case 'Vec3': {
                        obj[key] = {
                            x: view.getFloat64(offset, true),
                            y: view.getFloat64(offset + 8, true),
                            z: view.getFloat64(offset + 16, true)
                        };
                        offset += 24;
                    } break;
                }
            }

            if (offset !== view.byteLength)
                throw new RangeError();

            switch (info.name) {
                case 'Imu': { robot = obj; } break;
            }
        } catch (err) {
            console.log('Mis-sized packet payload');
            continue;
        }
    }
    recv_first = null;
    recv_last = null;

    // Actions
    if (connected) {
        button(canvas.width - 12, canvas.height - 12, "Reset position", { align: 3 }, btn => {
            sendPacket('SetPosition', {
                position: { x: 0.0, y: 0.0, z: 0.0 }
            });
        });
    }
}

function sendPacket(type, obj) {
    if (typeof type === 'string')
        type = messages.findIndex(info => info.name === type);
    let info = messages[type];

    // Compute payload size
    let payload = 0;
    for (let key in info.members) {
        let type = info.members[key];

        switch (type) {
            case 'double': { payload += 8; } break;
            case 'Vec2': { payload += 16; } break;
            case 'Vec3': { payload += 24; } break;
        }
    }

    let data = new ArrayBuffer(8 + payload);
    let view = new DataView(data);

    // Encode payload
    {
        let offset = 8;

        for (let key in info.members) {
            let type = info.members[key];

            switch (type) {
                case 'double': {
                    view.setFloat64(offset, obj[key], true);
                    offset += 8;
                } break;
                case 'Vec2': {
                    view.setFloat64(offset, obj[key].x, true);
                    view.setFloat64(offset + 8, obj[key].y, true);
                    offset += 16;
                } break;
                case 'Vec3': {
                    view.setFloat64(offset, obj[key].x, true);
                    view.setFloat64(offset + 8, obj[key].y, true);
                    view.setFloat64(offset + 16, obj[key].z, true);
                    offset += 24;
                } break;
            }
        }
    }

    // Encode header
    view.setUint16(4, type, true);
    view.setUint16(6, payload, true);
    view.setInt32(0, CRC32.buf(new Uint8Array(data, 4)), true);

    ws.send(data);
}

function draw() {
    ctx.fillStyle = 'white';
    ctx.strokeStyle = 'white';
    ctx.font = '20px Open Sans';
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    // Paint stable background
    {
        ctx.save();

        if (!connected)
            ctx.filter = 'grayscale(96%)';

        let img = assets.playground;
        let cx = canvas.width / 2;
        let cy = canvas.height / 2;
        let factor = Math.min(canvas.width / img.width, canvas.height / img.height);

        ctx.drawImage(img, cx - img.width * factor / 2, cy - img.height * factor / 2,
                           img.width * factor, img.height * factor);

        ctx.restore();
    }

    // Paint robot
    if (connected && robot != null) {
        ctx.save();
        ctx.fillStyle = '#ff0000';
        ctx.translate(robot.position.x + canvas.width / 2,
                      robot.position.y + canvas.height / 2);
        ctx.rotate(robot.orientation.x);
        ctx.fillRect(-20, -20, 40, 40);
        ctx.restore();

        let text1 = 'Position : ' + robot.position.x.toFixed(1) +
                            ' x ' + robot.position.y.toFixed(1) +
                            ' x ' + robot.position.z.toFixed(1);
        label(12, canvas.height - 138, text1, { align: 1 });

        let text2 = 'Speed : ' + robot.speed.x.toFixed(1) +
                         ' x ' + robot.speed.y.toFixed(1) +
                         ' x ' + robot.speed.z.toFixed(1);
        label(12, canvas.height - 96, text2, { align: 1 });

        let text3 = 'Acceleration : ' + robot.acceleration.x.toFixed(1) +
                                ' x ' + robot.acceleration.y.toFixed(1) +
                                ' x ' + robot.acceleration.z.toFixed(1);
        label(12, canvas.height - 54, text3, { align: 1 });

        let text4 = 'Rotation : ' + (robot.orientation.x * (180 / Math.PI)).toFixed(1) + '°' +
                            ' x ' + (robot.orientation.y * (180 / Math.PI)).toFixed(1) + '°' +
                            ' x ' + (robot.orientation.z * (180 / Math.PI)).toFixed(1) + '°';
        label(12, canvas.height - 12, text4, { align: 1 });
    }

    // Status and delay warning
    {
        let text = connected ? 'Status: Online' : 'Status: Offline';
        label(12, 12, text, { align: 7, color: connected ? 'black' : '#ff0000' });

        if (connected) {
            let delay = performance.now() - recv_time;

            if (delay > 1000) {
                let text = `Last update: ${(delay / 1000).toFixed(1)} sec`;
                label(12, 54, text, { align: 7, icon: assets.ui.error, color: '#ff0000' });
            }
        }
    }

    // FPS
    {
        let text = `FPS : ${(1000 / frame_time).toFixed(0)} (${frame_time.toFixed(1)} ms)`;
        label(canvas.width - 12, 12, text, { align: 9 });
    }
}
