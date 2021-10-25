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

function init() {
    // Nothing to do yet
}

function update() {
    // Nothing to do yet
}

function draw() {
    ctx.fillStyle = 'white';
    ctx.strokeStyle = 'white';
    ctx.font = '18px Open Sans';
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Debug / FPS
    {
        let text = `FPS : ${(1000 / frame_time).toFixed(0)} (${frame_time.toFixed(1)} ms)` +
                   ` | Update : ${update_time.toFixed(1)} ms | Draw : ${draw_time.toFixed(1)} ms`;
        ctx.textAlign = 'right';
        ctx.fillText(text, canvas.width - 8, 24);
    }
}
