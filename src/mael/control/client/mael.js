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

async function init() {
    let filenames = {
        table: 'table.png'
    };

    let images = await Promise.all(Object.keys(filenames).map(key => {
        let url = 'static/' + filenames[key];
        return loadTexture(url);
    }));

    for (let i = 0; i < Object.keys(filenames).length; i++) {
        let key = Object.keys(filenames)[i];
        assets[key] = images[i];
    }
}

function update() {
    if (connected && performance.now() - recv_time > 5000) {
        let err = new Error('WebSocket connection timed out');
        console.log(err);

        connected = false;
        ws.close();
        ws = null;
    }

    if (!connected && !ws) {
        let url = new URL(window.location.href);
        ws = new WebSocket(`ws://${url.host}/api/ws`);

        ws.onopen = () => {
            connected = true;
            recv_time = performance.now();
        };
        ws.onerror = e => {
            if (connected) {
                let err = new Error('Lost connection to WebSocket API');
                console.log(err);
            } else {
                let err = new Error('Failed to connect to WebSocket API');
                console.log(err);
            }

            connected = false;
            ws.close();
            ws = null;
        };

        ws.onmessage = e => receiveMessage(e.data);
    }
}

function receiveMessage(msg) {
    recv_time = performance.now();
    console.log(msg);
}

function draw() {
    ctx.fillStyle = 'white';
    ctx.strokeStyle = 'white';
    ctx.font = '18px Open Sans';
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    // Paint stable background
    {
        ctx.save();

        if (!connected)
            ctx.filter = 'grayscale(100%)';

        let img = assets.table;
        let cx = canvas.width / 2;
        let cy = canvas.height / 2;
        let factor = Math.min(canvas.width / img.width, canvas.height / img.height) * 0.9;

        ctx.drawImage(img, cx - img.width * factor / 2, cy - img.height * factor / 2,
                           img.width * factor, img.height * factor);

        ctx.restore();
    }

    // Debug / FPS
    {
        let text = `FPS : ${(1000 / frame_time).toFixed(0)} (${frame_time.toFixed(1)} ms)` +
                   ` | Status : ${connected ? 'Online' : 'Offline'}`;
        ctx.textAlign = 'right';
        ctx.fillText(text, canvas.width - 8, 24);
    }
}
