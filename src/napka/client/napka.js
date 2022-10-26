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
// along with this program. If not, see https://www.gnu.org/licenses/.

const napka = new function() {
    let self = this;

    let map_el;
    let map;

    this.start = async function() {
        map_el = document.createElement('div');
        map_el.id = 'map';

        await run();

        document.body.classList.remove('loading');
    };

    async function run() {
        let main_el = document.querySelector('#main');

        render(html`
            <div id="head">
                <div style="flex: 1;"></div>
                <img style="cursor: pointer;" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAYAAADgdz34AAACDElEQVRIia3V32vPYRQH8BcbMkxMKYSmYUwt86MUEqWWCyntSiMXpLgQCzeUXUgoWtnFcqFcuPA7RexCcSFGwtLKzzAWofwBLs6z9mlt3+/ni/fVOc9znvN+znnOOQ/5UYFaLMNslJVwtiAacQu/0I2HeIM+dKDmbx1PxA28QBPGDdqfgUOJ6CBGlOK8Es9wGuVFbKeJqM6UQnAZbYPWyrEE6zBniAu9RHMe52vRg1GZtR34gqe4gw/owvKMTT0+YXwxgkvYltFb8QQLBtltxleszqxdUSSKMlEtk5PegI+oSntbcQyrMiRvMTrpW3CxEMH05LAfbWhJcgc6cQDvsTGt38f6zIUeFyKYh1cZvRNrxHv8Fs0Gm3A9ySexN8k14v2GxRR8y+g3sUHUeJ+Bd2jBuSS3Y2eSV+BBIYKR+ClyTjRSe5KbEkkXXqM6RfYOdcmmGRcKERBl2JjkKvSKNPXrDRiTojqOa5mz7dhdjOAwTmX01aIcj2AuJolU3BblW5Wx7cHiYgR1opGys2UWzorUfMcj7MPYjE29GIK5ZlK3gVrPixOiR3Jhl5hHeTFBVF913gMVIu8Lc9rvV6SDh8Ie3M1hN1WUb22pBGWi5rcXsBmBqzhaqvN+zMdnrBxmvxX3RF/8NVaJB9wlOp34Ts/juYHJ+09YJG7aK3rgh+jayv/hPIuZWCoiyIU/yGpi++3oSLIAAAAASUVORK5CYII=" alt="Localisation GPS">
                <label><input type="search" style="width: 300px;" autocomplete="off" placeholder="Entrez votre adresse..."></label>
                <div style="flex: 1;"></div>
            </div>
            ${map_el}
        `, main_el);

        if (map == null) {
            setTimeout(() => {
                map = L.map('map', { zoomControl: false }).setView([46.6, 3.0], 6);

                L.control.zoom({ position: 'bottomleft' }).addTo(map);

                L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
                    maxZoom: 19,
                    attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
                }).addTo(map);
            }, 0);
        }
    }
};
