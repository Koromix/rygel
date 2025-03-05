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
import asset21 from './partners/interactions1.webp';
import asset22 from './partners/interactions2.webp';
import asset23 from './partners/universite_lille.webp';
import asset24 from './pictures/aurora.webp';
import asset25 from './pictures/dispositif.webp';
import asset26 from './pictures/ecriture.webp';
import asset27 from './pictures/etirements.webp';
import asset28 from './pictures/etudes.webp';
import asset29 from './pictures/help1.webp';
import asset30 from './pictures/help2.webp';
import asset31 from './pictures/kezako.webp';
import asset32 from './pictures/participer.webp';
import asset33 from './pictures/philosophie.webp';
import asset34 from './pictures/respiration.webp';
import asset35 from './pictures/sociotrauma.webp';
import asset36 from './pictures/staks.webp';
import asset37 from './resources/film_bescond.jpg';
import asset38 from './resources/film_ozon.jpg';
import asset39 from './resources/film_scherfig.jpg';
import asset40 from './resources/film_suco.jpg';
import asset41 from './resources/livre_alloinguillon.jpg';
import asset42 from './resources/livre_cabrel.jpg';
import asset43 from './resources/livre_delacourt.jpg';
import asset44 from './resources/livre_jamoulle.jpg';
import asset45 from './resources/livre_lambda.jpg';
import asset46 from './resources/livre_vantran.jpg';
import asset47 from './resources/musique_blueoctober.jpg';
import asset48 from './resources/musique_gherbo.jpg';
import asset49 from './resources/musique_jamesarthur.jpg';
import asset50 from './resources/musique_ladygaga.jpg';
import asset51 from './resources/musique_thecranberries.jpg';
import asset52 from './resources/pro_brillon.jpg';
import asset53 from './resources/pro_hingrayelhage.jpg';
import asset54 from './resources/pro_kolk.jpg';
import asset55 from './resources/video_cn2r.jpg';
import asset56 from './resources/video_fenvac.jpg';
import asset57 from './resources/video_legriguer.jpg';
import asset58 from './ui/116006.webp';
import asset59 from './ui/15.webp';
import asset60 from './ui/3114.webp';
import asset61 from './ui/anonymous.png';
import asset62 from './ui/book.png';
import asset63 from './ui/calendar.png';
import asset64 from './ui/child.png';
import asset65 from './ui/configure.png';
import asset66 from './ui/confirm.png';
import asset67 from './ui/copy.png';
import asset68 from './ui/create.png';
import asset69 from './ui/dashboard.png';
import asset70 from './ui/delete.png';
import asset71 from './ui/diary.png';
import asset72 from './ui/dice.webp';
import asset73 from './ui/fullscreen.png';
import asset74 from './ui/insert.png';
import asset75 from './ui/left.webp';
import asset76 from './ui/link.png';
import asset77 from './ui/move.png';
import asset78 from './ui/movie.png';
import asset79 from './ui/music.png';
import asset80 from './ui/network.png';
import asset81 from './ui/new.png';
import asset82 from './ui/paper.png';
import asset83 from './ui/people.png';
import asset84 from './ui/private.png';
import asset85 from './ui/redo.png';
import asset86 from './ui/right.webp';
import asset87 from './ui/rotate.png';
import asset88 from './ui/select.png';
import asset89 from './ui/track.png';
import asset90 from './ui/undo.png';
import asset91 from './ui/unselect.png';
import asset92 from './ui/user.png';
import asset93 from './ui/video.png';

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
    'partners/interactions1': asset21,
    'partners/interactions2': asset22,
    'partners/universite_lille': asset23,
    'pictures/aurora': asset24,
    'pictures/dispositif': asset25,
    'pictures/ecriture': asset26,
    'pictures/etirements': asset27,
    'pictures/etudes': asset28,
    'pictures/help1': asset29,
    'pictures/help2': asset30,
    'pictures/kezako': asset31,
    'pictures/participer': asset32,
    'pictures/philosophie': asset33,
    'pictures/respiration': asset34,
    'pictures/sociotrauma': asset35,
    'pictures/staks': asset36,
    'resources/film_bescond': asset37,
    'resources/film_ozon': asset38,
    'resources/film_scherfig': asset39,
    'resources/film_suco': asset40,
    'resources/livre_alloinguillon': asset41,
    'resources/livre_cabrel': asset42,
    'resources/livre_delacourt': asset43,
    'resources/livre_jamoulle': asset44,
    'resources/livre_lambda': asset45,
    'resources/livre_vantran': asset46,
    'resources/musique_blueoctober': asset47,
    'resources/musique_gherbo': asset48,
    'resources/musique_jamesarthur': asset49,
    'resources/musique_ladygaga': asset50,
    'resources/musique_thecranberries': asset51,
    'resources/pro_brillon': asset52,
    'resources/pro_hingrayelhage': asset53,
    'resources/pro_kolk': asset54,
    'resources/video_cn2r': asset55,
    'resources/video_fenvac': asset56,
    'resources/video_legriguer': asset57,
    'ui/116006': asset58,
    'ui/15': asset59,
    'ui/3114': asset60,
    'ui/anonymous': asset61,
    'ui/book': asset62,
    'ui/calendar': asset63,
    'ui/child': asset64,
    'ui/configure': asset65,
    'ui/confirm': asset66,
    'ui/copy': asset67,
    'ui/create': asset68,
    'ui/dashboard': asset69,
    'ui/delete': asset70,
    'ui/diary': asset71,
    'ui/dice': asset72,
    'ui/fullscreen': asset73,
    'ui/insert': asset74,
    'ui/left': asset75,
    'ui/link': asset76,
    'ui/move': asset77,
    'ui/movie': asset78,
    'ui/music': asset79,
    'ui/network': asset80,
    'ui/new': asset81,
    'ui/paper': asset82,
    'ui/people': asset83,
    'ui/private': asset84,
    'ui/redo': asset85,
    'ui/right': asset86,
    'ui/rotate': asset87,
    'ui/select': asset88,
    'ui/track': asset89,
    'ui/undo': asset90,
    'ui/unselect': asset91,
    'ui/user': asset92,
    'ui/video': asset93,
};

export { ASSETS }
