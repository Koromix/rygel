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

import { loadTexture } from '../../../web/libjs/runner.js';

import ui_left from '../../assets/ui/left.png';
import ui_right from '../../assets/ui/right.png';
import ui_busy from '../../assets/ui/busy.png';
import ui_move from '../../assets/ui/move.png';
import ui_delete from '../../assets/ui/delete.png';
import ui_mouse from '../../assets/ui/mouse.png';
import ui_open from '../../assets/ui/open.png';
import ui_redo from '../../assets/ui/redo.png';
import ui_save from '../../assets/ui/save.png';
import ui_undo from '../../assets/ui/undo.png';

import main_configure from '../../assets/main/configure.png';
import main_copy from '../../assets/main/copy.png';
import main_dashboard from '../../assets/main/dashboard.png';
import main_new from '../../assets/main/new.png';
import main_network from '../../assets/main/network.png';

import network_anonymous from '../../assets/network/anonymous.png';
import network_child from '../../assets/network/child.png';
import network_couple from '../../assets/network/couple.png';
import network_create from '../../assets/network/create.png';
import network_family from '../../assets/network/family.png';
import network_friend from '../../assets/network/friend.png';
import network_healthcare from '../../assets/network/healthcare.png';
import network_insert from '../../assets/network/insert.png';
import network_link from '../../assets/network/link.png';
import network_move from '../../assets/network/move.png';
import network_rotate from '../../assets/network/rotate.png';
import network_select from '../../assets/network/select.png';
import network_unselect from '../../assets/network/unselect.png';
import network_work from '../../assets/network/work.png';

let assets = {};

async function loadAssets() {
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

    assets.main = {
        configure: main_configure,
        copy: main_copy,
        dashboard: main_dashboard,
        new: main_new,
        network: main_network
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

    let objects = Object.values(assets);
    let resources = objects.flatMap(obj => Object.keys(obj).map(key => ({ obj, key })));

    await Promise.all(resources.map(async res => {
        let url = 'build/' + res.obj[res.key];

        if (url.endsWith('.png') || url.endsWith('.webp')) {
            res.obj[res.key] = await loadTexture(url);
        } else {
            throw new Error(`Unknown resource type for '${url}'`);
        }
    }));
}

export {
    loadAssets,
    assets
}
