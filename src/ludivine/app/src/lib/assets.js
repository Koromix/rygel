// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { Util } from '../../../../web/core/base.js';
import { loadTexture } from './util.js';

import ui_left from '../../../assets/app/ui/left.png';
import ui_right from '../../../assets/app/ui/right.png';
import ui_busy from '../../../assets/app/ui/busy.png';
import ui_move from '../../../assets/app/ui/move.png';
import ui_delete from '../../../assets/app/ui/delete.png';
import ui_mouse from '../../../assets/app/ui/mouse.png';
import ui_open from '../../../assets/app/ui/open.png';
import ui_redo from '../../../assets/app/ui/redo.png';
import ui_save from '../../../assets/app/ui/save.png';
import ui_undo from '../../../assets/app/ui/undo.png';

import app_logo from '../../../assets/logo.webp';
import app_footer from '../../../assets/footer.webp';

import main_configure from '../../../assets/app/main/configure.png';
import main_copy from '../../../assets/app/main/copy.png';
import main_dashboard from '../../../assets/app/main/dashboard.png';
import main_new from '../../../assets/app/main/new.png';
import main_network from '../../../assets/app/main/network.png';
import main_track from '../../../assets/app/main/track.png';
import main_user from '../../../assets/app/main/user.png';

import network_anonymous from '../../../assets/app/network/anonymous.png';
import network_child from '../../../assets/app/network/child.png';
import network_couple from '../../../assets/app/network/couple.png';
import network_create from '../../../assets/app/network/create.png';
import network_family from '../../../assets/app/network/family.png';
import network_friend from '../../../assets/app/network/friend.png';
import network_healthcare from '../../../assets/app/network/healthcare.png';
import network_insert from '../../../assets/app/network/insert.png';
import network_link from '../../../assets/app/network/link.png';
import network_move from '../../../assets/app/network/move.png';
import network_rotate from '../../../assets/app/network/rotate.png';
import network_select from '../../../assets/app/network/select.png';
import network_unselect from '../../../assets/app/network/unselect.png';
import network_work from '../../../assets/app/network/work.png';

let assets = {};
let textures = {};

async function loadAssets(prefix) {
    assets.app = {
        logo: app_logo,
        footer: app_footer
    };

    assets.main = {
        configure: main_configure,
        copy: main_copy,
        dashboard: main_dashboard,
        new: main_new,
        network: main_network,
        track: main_track,
        user: main_user
    };

    assets.network = {
        anonymous: network_anonymous,
        child: network_child,
        couple: network_couple,
        create: network_create,
        family: network_family,
        friend: network_friend,
        healthcare: network_healthcare,
        insert: network_insert,
        link: network_link,
        move: network_move,
        rotate: network_rotate,
        select: network_select,
        unselect: network_unselect,
        work: network_work
    };

    assets.ui = {
        left: ui_left,
        right: ui_right,
        busy: ui_busy,
        move: ui_move,
        delete: ui_delete,
        mouse: ui_mouse,
        open: ui_open,
        redo: ui_redo,
        save: ui_save,
        undo: ui_undo
    };

    let categories = Object.keys(assets);

    for (let category of categories) {
        let keys = Object.keys(assets[category]);

        textures[category] = {};

        await Promise.all(keys.map(async key => {
            let url = assets[category][key];

            textures[category][key] = await loadTexture(url);
            assets[category][key] = url;
        }));
    }
}

export {
    assets,
    textures,

    loadAssets
}
