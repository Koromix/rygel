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

import { loadTexture, loadSound } from '../../web/core/runner.js';

// Aurora Night by psiipilehto: https://www.deviantart.com/psiipilehto/art/Aurora-Night-608184628
import backgrounds_aurora from '../assets/backgrounds/aurora.webp';
// Illustration of Fomalhaut b by ESA, NASA, and L. Calcada: https://fr.m.wikipedia.org/wiki/Fichier:Fomalhaut_planet.jpg
import backgrounds_planet from '../assets/backgrounds/planet.webp';
// Somewhere by psiipilehto: https://www.deviantart.com/psiipilehto/art/Somewhere-269008123
import backgrounds_lonely from '../assets/backgrounds/somewhere.webp';

// Tetris theme A by tuxmain: https://play.dogmazic.net/song.php?action=show_song&song_id=60597
import soundtracks_themeA from '../assets/soundtracks/themeA.mp3';

import ui_background from '../assets/ui/background.png';
import ui_clockwise from '../assets/ui/clockwise.png';
import ui_counterclock from '../assets/ui/counterclock.png';
import ui_fullscreen from '../assets/ui/fullscreen.png';
import ui_left from '../assets/ui/left.png';
import ui_play from '../assets/ui/play.png';
import ui_pause from '../assets/ui/pause.png';
import ui_right from '../assets/ui/right.png';
import ui_sound from '../assets/ui/sound.png';
import ui_silence from '../assets/ui/silence.png';
import ui_turbo from '../assets/ui/turbo.png';

let assets = {};

async function loadAssets() {
    assets.backgrounds = {
        aurora: backgrounds_aurora,
        planet: backgrounds_planet,
        somewhere: backgrounds_lonely
    };

    assets.soundtracks = {
        themeA: soundtracks_themeA
    };

    assets.ui = {
        background: ui_background,
        clockwise: ui_clockwise,
        counterclock: ui_counterclock,
        fullscreen: ui_fullscreen,
        left: ui_left,
        pause: ui_pause,
        play: ui_play,
        right: ui_right,
        sound: ui_sound,
        silence: ui_silence,
        turbo: ui_turbo
    };

    let objects = Object.values(assets);
    let resources = objects.flatMap(obj => Object.keys(obj).map(key => ({ obj, key })));

    await Promise.all(resources.map(async res => {
        let url = 'static/' + res.obj[res.key];

        if (url.endsWith('.png') || url.endsWith('.webp')) {
            res.obj[res.key] = await loadTexture(url);
        } else if (url.endsWith('.mp3')) {
            res.obj[res.key] = await loadSound(url);
        } else {
            throw new Error(`Unknown resource type for '${url}'`);
        }
    }));
}

export {
    loadAssets,
    assets
}
