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

import { Util } from '../../web/core/base.js';

// Brief recap of various commands used to produce sound files:
//
// * WAV volume adjustment: sox -v FACTOR input.wav output.wav
// * Music opus encoding: opusenc --music --discard-comments --discard-pictures input.wav output.opus
// * Sound opus encoding: opusenc --downmix-mono --discard-comments --discard-pictures input.wav output.opus
// * Convert Ogg (Opus) container to WebM: ffmpeg -i input.opus -acodec copy output.webm
//
// * Convert audio to MP3 for older browsers: ffmpeg -i input.webm -codec:a libmp3lame -qscale:a 4 output.mp3

// Aurora Night by psiipilehto: https://www.deviantart.com/psiipilehto/art/Aurora-Night-608184628
import backgrounds_aurora from '../assets/backgrounds/aurora.webp';
// Winter scenery by psiipilehto: https://www.deviantart.com/psiipilehto/art/winter-scenery-346519787
import backgrounds_winter from '../assets/backgrounds/winter.webp';
// Somewhere by psiipilehto: https://www.deviantart.com/psiipilehto/art/Somewhere-269008123
import backgrounds_somewhere from '../assets/backgrounds/somewhere.webp';

// Abstract Piano Ambient by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/tomorrow/abstract-piano-ambient/
import musics_abstract_piano_ambient from '../assets/musics/abstract_piano_ambient.webm';
import musics_abstract_piano_ambient_mp3 from '../assets/musics/abstract_piano_ambient.mp3';
// Ambient Future Tech by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/sci-fi-times/ambient-future-tech/
import musics_ambient_future_tech from '../assets/musics/ambient_future_tech.webm';
import musics_ambient_future_tech_mp3 from '../assets/musics/ambient_future_tech.mp3';
// Architecture by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/motivational-style/architecture-1/
import musics_architecture from '../assets/musics/architecture.webm';
import musics_architecture_mp3 from '../assets/musics/architecture.mp3';
// Medical by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/sci-fi-times/medical-2/
import musics_medical from '../assets/musics/medical.webm';
import musics_medical_mp3 from '../assets/musics/medical.mp3';
// Motivational Day by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/motivational-style/motivational-day-1/
import musics_motivational_day from '../assets/musics/motivational_day.webm';
import musics_motivational_day_mp3 from '../assets/musics/motivational_day.mp3'
// Motivational Stylish Background by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/pop-inspiration/motivational-stylish-background/
import musics_motivational_stylish_background from '../assets/musics/motivational_stylish_background.webm';
import musics_motivational_stylish_background_mp3 from '../assets/musics/motivational_stylish_background.mp3';
// Serious Corporate by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/corporate/serious-corporate/
import musics_serious_corporate from '../assets/musics/serious_corporate.webm';
import musics_serious_corporate_mp3 from '../assets/musics/serious_corporate.mp3';
// Summer Beauty by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/life/summer-beauty/
import musics_summer_beauty from '../assets/musics/summer_beauty.webm';
import musics_summer_beauty_mp3 from '../assets/musics/summer_beauty.mp3';
// Summer Every Day by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/pop-inspiration/summer-every-day/
import musics_summer_every_day from '../assets/musics/summer_every_day.webm';
import musics_summer_every_day_mp3 from '../assets/musics/summer_every_day.mp3';
// Tech Corporation by AudioCoffee: https://freemusicarchive.org/music/becouse-heart/technology-1/tech-corporation/
import musics_tech_corporation from '../assets/musics/tech_corporation.webm';
import musics_tech_corporation_mp3 from '../assets/musics/tech_corporation.mp3';

// powerup by BananaMilkshake: https://freesound.org/people/BananaMilkshake/sounds/632702/
import sound_clear from '../assets/sounds/clear.webm';
import sound_clear_mp3 from '../assets/sounds/clear.mp3';
// game over by Leszek_Szary: https://freesound.org/people/Leszek_Szary/sounds/133283/
import sound_gameover from '../assets/sounds/gameover.webm';
import sound_gameover_mp3 from '../assets/sounds/gameover.mp3';
// Notification Sound 1 by deadrobotmusic: https://freesound.org/people/deadrobotmusic/sounds/750607/
import sound_hold from '../assets/sounds/hold.webm';
import sound_hold_mp3 from '../assets/sounds/hold.mp3';
// You find a treasure by xkeril: https://freesound.org/people/xkeril/sounds/632661/
import sound_levelup from '../assets/sounds/levelup.webm';
import sound_levelup_mp3 from '../assets/sounds/levelup.mp3';
// Blip 1 by HenKonen: https://freesound.org/people/HenKonen/sounds/757175/
import sound_lock from '../assets/sounds/lock.webm';
import sound_lock_mp3 from '../assets/sounds/lock.mp3';
// Simple GUI click by qubodup: https://freesound.org/people/qubodup/sounds/159697/
import sound_move from '../assets/sounds/move.webm';
import sound_move_mp3 from '../assets/sounds/move.mp3';

import ui_background from '../assets/ui/background.png';
import ui_clockwise from '../assets/ui/clockwise.png';
import ui_counterclock from '../assets/ui/counterclock.png';
import ui_drop from '../assets/ui/drop.png';
import ui_fullscreen from '../assets/ui/fullscreen.png';
import ui_ghost from '../assets/ui/ghost.png';
import ui_help from '../assets/ui/help.png';
import ui_left from '../assets/ui/left.png';
import ui_music0 from '../assets/ui/music0.png';
import ui_music1 from '../assets/ui/music1.png';
import ui_pause0 from '../assets/ui/pause0.png';
import ui_pause1 from '../assets/ui/pause1.png';
import ui_right from '../assets/ui/right.png';
import ui_sound0 from '../assets/ui/sound0.png';
import ui_sound1 from '../assets/ui/sound1.png';
import ui_start from '../assets/ui/start.png';
import ui_turbo from '../assets/ui/turbo.png';

const LAZY_CATEGORIES = ['musics'];

async function loadAssets(prefix, progress) {
    if (window.createImageBitmap == null)
        throw new Error('ImageBitmap support is not available (old browser?)');

    prefix = prefix.replace(/\/+$/g, '') + '/';

    if (progress == null)
        progress = (value, total) => {};

    // Detect support for Opus in WebM container
    let mp3 = (document.createElement('audio').canPlayType?.('audio/webm;codecs=opus') != 'probably');

    let assets = {
        backgrounds: {
            aurora: backgrounds_aurora,
            winter: backgrounds_winter,
            somewhere: backgrounds_somewhere
        },
        musics: {
            abstract_piano_ambient: mp3 ? musics_abstract_piano_ambient_mp3 : musics_abstract_piano_ambient,
            ambient_future_tech: mp3 ? musics_ambient_future_tech_mp3 : musics_ambient_future_tech,
            architecture: mp3 ? musics_architecture_mp3 : musics_architecture,
            medical: mp3 ? musics_medical_mp3 : musics_medical,
            motivational_day: mp3 ? musics_motivational_day_mp3 : musics_motivational_day,
            motivational_stylish_background: mp3 ? musics_motivational_stylish_background_mp3 : musics_motivational_stylish_background,
            serious_corporate: mp3 ? musics_serious_corporate_mp3 : musics_serious_corporate,
            summer_beauty: mp3 ? musics_summer_beauty_mp3 : musics_summer_beauty,
            summer_every_day: mp3 ? musics_summer_every_day_mp3 : musics_summer_every_day,
            tech_corporation: mp3 ? musics_tech_corporation_mp3 : musics_tech_corporation
        },
        sounds: {
            clear: mp3 ? sound_clear_mp3 : sound_clear,
            gameover: mp3 ? sound_gameover_mp3 : sound_gameover,
            hold: mp3 ? sound_hold_mp3 : sound_hold,
            levelup: mp3 ? sound_levelup_mp3 : sound_levelup,
            lock: mp3 ? sound_lock_mp3 : sound_lock,
            move: mp3 ? sound_move_mp3 : sound_move
        },
        ui: {
            background: ui_background,
            clockwise: ui_clockwise,
            counterclock: ui_counterclock,
            drop: ui_drop,
            fullscreen: ui_fullscreen,
            ghost: ui_ghost,
            help: ui_help,
            left: ui_left,
            music0: ui_music0,
            music1: ui_music1,
            pause0: ui_pause0,
            pause1: ui_pause1,
            right: ui_right,
            sound0: ui_sound0,
            sound1: ui_sound1,
            start: ui_start,
            turbo: ui_turbo
        }
    };

    // Shuffle musics
    assets.musics = Util.shuffle(Object.keys(assets.musics)).reduce((obj, key) => { obj[key] = assets.musics[key]; return obj; }, {});

    let categories = Object.keys(assets);
    let resources = categories.flatMap(category => {
        let obj = assets[category];
        return Object.keys(obj).map(key => ({ category, obj, key }));
    });
    let downloaded = 0;

    resources = Util.shuffle(resources);

    await Promise.all(Util.mapRange(0, 3, async start => {
        for (let i = start; i < resources.length; i += 3) {
            let res = resources[i];
            let url = prefix + res.obj[res.key];

            if (LAZY_CATEGORIES.includes(res.category)) {
                res.obj[res.key] = url;
            } else if (url.endsWith('.png') || url.endsWith('.webp')) {
                res.obj[res.key] = await loadTexture(url);
            } else if (url.endsWith('.webm') || url.endsWith('.mp3')) {
                res.obj[res.key] = await loadSound(url);
            } else {
                throw new Error(`Unknown resource type for '${url}'`);
            }

            downloaded++;
            progress(downloaded, resources.length);
        }
    }));

    return assets;
}

async function loadTexture(url) {
    let response = await self.fetch(url);

    let blob = await response.blob();
    let texture = await createImageBitmap(blob);

    return texture;
}

async function loadSound(url) {
    let response = await fetch(url);
    let buf = await response.arrayBuffer();

    return buf;
}

export { loadAssets }
