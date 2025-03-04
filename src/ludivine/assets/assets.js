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

import asset0 from './calendars/apple.png';
import asset1 from './calendars/google.png';
import asset2 from './calendars/ical.png';
import asset3 from './calendars/office365.png';
import asset4 from './calendars/outlook.png';
import asset5 from './calendars/yahoo.png';
import asset6 from './main/footer.webp';
import asset7 from './main/ldv.webp';
import asset8 from './main/logo.webp';
import asset9 from './network/animal.png';
import asset10 from './network/community.png';
import asset11 from './network/family.png';
import asset12 from './network/friend.png';
import asset13 from './network/help.png';
import asset14 from './network/love.png';
import asset15 from './network/online.png';
import asset16 from './network/other.png';
import asset17 from './network/work.png';
import asset18 from './partners/cn2r.webp';
import asset19 from './partners/f2rsmpsy.webp';
import asset20 from './partners/france_victimes.webp';
import asset21 from './partners/universite_lille.webp';
import asset22 from './pictures/aurora.webp';
import asset23 from './pictures/dispositif.webp';
import asset24 from './pictures/ecriture.webp';
import asset25 from './pictures/etirements.webp';
import asset26 from './pictures/etudes.webp';
import asset27 from './pictures/help1.webp';
import asset28 from './pictures/help2.webp';
import asset29 from './pictures/kezako.webp';
import asset30 from './pictures/participer.webp';
import asset31 from './pictures/philosophie.webp';
import asset32 from './pictures/respiration.webp';
import asset33 from './pictures/sociotrauma.webp';
import asset34 from './pictures/staks.webp';
import asset35 from './resources/film_bescond.jpg';
import asset36 from './resources/film_ozon.jpg';
import asset37 from './resources/film_scherfig.jpg';
import asset38 from './resources/film_suco.jpg';
import asset39 from './resources/livre_alloinguillon.jpg';
import asset40 from './resources/livre_cabrel.jpg';
import asset41 from './resources/livre_delacourt.jpg';
import asset42 from './resources/livre_jamoulle.jpg';
import asset43 from './resources/livre_lambda.jpg';
import asset44 from './resources/livre_vantran.jpg';
import asset45 from './resources/musique_blueoctober.jpg';
import asset46 from './resources/musique_gherbo.jpg';
import asset47 from './resources/musique_jamesarthur.jpg';
import asset48 from './resources/musique_ladygaga.jpg';
import asset49 from './resources/musique_thecranberries.jpg';
import asset50 from './resources/pro_brillon.jpg';
import asset51 from './resources/pro_hingrayelhage.jpg';
import asset52 from './resources/pro_kolk.jpg';
import asset53 from './resources/video_cn2r.jpg';
import asset54 from './resources/video_fenvac.jpg';
import asset55 from './resources/video_legriguer.jpg';
import asset56 from './ui/3114.webp';
import asset57 from './ui/anonymous.png';
import asset58 from './ui/book.png';
import asset59 from './ui/calendar.png';
import asset60 from './ui/child.png';
import asset61 from './ui/configure.png';
import asset62 from './ui/confirm.png';
import asset63 from './ui/copy.png';
import asset64 from './ui/create.png';
import asset65 from './ui/dashboard.png';
import asset66 from './ui/delete.png';
import asset67 from './ui/dice.webp';
import asset68 from './ui/fullscreen.png';
import asset69 from './ui/insert.png';
import asset70 from './ui/left.webp';
import asset71 from './ui/link.png';
import asset72 from './ui/move.png';
import asset73 from './ui/movie.png';
import asset74 from './ui/music.png';
import asset75 from './ui/network.png';
import asset76 from './ui/new.png';
import asset77 from './ui/paper.png';
import asset78 from './ui/people.png';
import asset79 from './ui/redo.png';
import asset80 from './ui/right.webp';
import asset81 from './ui/rotate.png';
import asset82 from './ui/select.png';
import asset83 from './ui/track.png';
import asset84 from './ui/undo.png';
import asset85 from './ui/unselect.png';
import asset86 from './ui/user.png';
import asset87 from './ui/video.png';

const ASSETS = {
    'calendars/apple': asset0,
    'calendars/google': asset1,
    'calendars/ical': asset2,
    'calendars/office365': asset3,
    'calendars/outlook': asset4,
    'calendars/yahoo': asset5,
    'main/footer': asset6,
    'main/ldv': asset7,
    'main/logo': asset8,
    'network/animal': asset9,
    'network/community': asset10,
    'network/family': asset11,
    'network/friend': asset12,
    'network/help': asset13,
    'network/love': asset14,
    'network/online': asset15,
    'network/other': asset16,
    'network/work': asset17,
    'partners/cn2r': asset18,
    'partners/f2rsmpsy': asset19,
    'partners/france_victimes': asset20,
    'partners/universite_lille': asset21,
    'pictures/aurora': asset22,
    'pictures/dispositif': asset23,
    'pictures/ecriture': asset24,
    'pictures/etirements': asset25,
    'pictures/etudes': asset26,
    'pictures/help1': asset27,
    'pictures/help2': asset28,
    'pictures/kezako': asset29,
    'pictures/participer': asset30,
    'pictures/philosophie': asset31,
    'pictures/respiration': asset32,
    'pictures/sociotrauma': asset33,
    'pictures/staks': asset34,
    'resources/film_bescond': asset35,
    'resources/film_ozon': asset36,
    'resources/film_scherfig': asset37,
    'resources/film_suco': asset38,
    'resources/livre_alloinguillon': asset39,
    'resources/livre_cabrel': asset40,
    'resources/livre_delacourt': asset41,
    'resources/livre_jamoulle': asset42,
    'resources/livre_lambda': asset43,
    'resources/livre_vantran': asset44,
    'resources/musique_blueoctober': asset45,
    'resources/musique_gherbo': asset46,
    'resources/musique_jamesarthur': asset47,
    'resources/musique_ladygaga': asset48,
    'resources/musique_thecranberries': asset49,
    'resources/pro_brillon': asset50,
    'resources/pro_hingrayelhage': asset51,
    'resources/pro_kolk': asset52,
    'resources/video_cn2r': asset53,
    'resources/video_fenvac': asset54,
    'resources/video_legriguer': asset55,
    'ui/3114': asset56,
    'ui/anonymous': asset57,
    'ui/book': asset58,
    'ui/calendar': asset59,
    'ui/child': asset60,
    'ui/configure': asset61,
    'ui/confirm': asset62,
    'ui/copy': asset63,
    'ui/create': asset64,
    'ui/dashboard': asset65,
    'ui/delete': asset66,
    'ui/dice': asset67,
    'ui/fullscreen': asset68,
    'ui/insert': asset69,
    'ui/left': asset70,
    'ui/link': asset71,
    'ui/move': asset72,
    'ui/movie': asset73,
    'ui/music': asset74,
    'ui/network': asset75,
    'ui/new': asset76,
    'ui/paper': asset77,
    'ui/people': asset78,
    'ui/redo': asset79,
    'ui/right': asset80,
    'ui/rotate': asset81,
    'ui/select': asset82,
    'ui/track': asset83,
    'ui/undo': asset84,
    'ui/unselect': asset85,
    'ui/user': asset86,
    'ui/video': asset87,
};

export { ASSETS }
