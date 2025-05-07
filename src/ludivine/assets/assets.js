// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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
import asset16 from './network/org.png';
import asset17 from './network/other.png';
import asset18 from './network/work.png';
import asset19 from './partners/cn2r.webp';
import asset20 from './partners/f2rsmpsy.webp';
import asset21 from './partners/france_victimes.webp';
import asset22 from './partners/interactions1.webp';
import asset23 from './partners/interactions2.webp';
import asset24 from './partners/universite_lille.webp';
import asset25 from './pictures/aurora.webp';
import asset26 from './pictures/dispositif.webp';
import asset27 from './pictures/ecriture.webp';
import asset28 from './pictures/etirements.webp';
import asset29 from './pictures/etudes.webp';
import asset30 from './pictures/help1.webp';
import asset31 from './pictures/help2.webp';
import asset32 from './pictures/kezako.webp';
import asset33 from './pictures/participer.webp';
import asset34 from './pictures/philosophie.webp';
import asset35 from './pictures/respiration.webp';
import asset36 from './pictures/sociotrauma.webp';
import asset37 from './pictures/staks.webp';
import asset38 from './resources/film_bescond.jpg';
import asset39 from './resources/film_ozon.jpg';
import asset40 from './resources/film_scherfig.jpg';
import asset41 from './resources/film_suco.jpg';
import asset42 from './resources/livre_alloinguillon.jpg';
import asset43 from './resources/livre_cabrel.jpg';
import asset44 from './resources/livre_delacourt.jpg';
import asset45 from './resources/livre_jamoulle.jpg';
import asset46 from './resources/livre_lambda.jpg';
import asset47 from './resources/livre_vantran.jpg';
import asset48 from './resources/musique_blueoctober.jpg';
import asset49 from './resources/musique_gherbo.jpg';
import asset50 from './resources/musique_jamesarthur.jpg';
import asset51 from './resources/musique_ladygaga.jpg';
import asset52 from './resources/musique_thecranberries.jpg';
import asset53 from './resources/pro_brillon.jpg';
import asset54 from './resources/pro_hingrayelhage.jpg';
import asset55 from './resources/pro_kolk.jpg';
import asset56 from './resources/video_cn2r.jpg';
import asset57 from './resources/video_fenvac.jpg';
import asset58 from './resources/video_legriguer.jpg';
import asset59 from './ui/116006.webp';
import asset60 from './ui/15.webp';
import asset61 from './ui/3114.webp';
import asset62 from './ui/anonymous.png';
import asset63 from './ui/book.png';
import asset64 from './ui/calendar.png';
import asset65 from './ui/child.png';
import asset66 from './ui/configure.png';
import asset67 from './ui/confirm.png';
import asset68 from './ui/copy.png';
import asset69 from './ui/create.png';
import asset70 from './ui/dashboard.png';
import asset71 from './ui/delete.png';
import asset72 from './ui/diary.png';
import asset73 from './ui/dice.webp';
import asset74 from './ui/fullscreen.png';
import asset75 from './ui/insert.png';
import asset76 from './ui/left.webp';
import asset77 from './ui/link.png';
import asset78 from './ui/move.png';
import asset79 from './ui/movie.png';
import asset80 from './ui/music.png';
import asset81 from './ui/network.png';
import asset82 from './ui/new.png';
import asset83 from './ui/paper.png';
import asset84 from './ui/people.png';
import asset85 from './ui/private.png';
import asset86 from './ui/redo.png';
import asset87 from './ui/right.webp';
import asset88 from './ui/rotate.png';
import asset89 from './ui/select.png';
import asset90 from './ui/track.png';
import asset91 from './ui/undo.png';
import asset92 from './ui/unselect.png';
import asset93 from './ui/user.png';
import asset94 from './ui/validate.png';
import asset95 from './ui/video.png';

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
    'network/org': asset16,
    'network/other': asset17,
    'network/work': asset18,
    'partners/cn2r': asset19,
    'partners/f2rsmpsy': asset20,
    'partners/france_victimes': asset21,
    'partners/interactions1': asset22,
    'partners/interactions2': asset23,
    'partners/universite_lille': asset24,
    'pictures/aurora': asset25,
    'pictures/dispositif': asset26,
    'pictures/ecriture': asset27,
    'pictures/etirements': asset28,
    'pictures/etudes': asset29,
    'pictures/help1': asset30,
    'pictures/help2': asset31,
    'pictures/kezako': asset32,
    'pictures/participer': asset33,
    'pictures/philosophie': asset34,
    'pictures/respiration': asset35,
    'pictures/sociotrauma': asset36,
    'pictures/staks': asset37,
    'resources/film_bescond': asset38,
    'resources/film_ozon': asset39,
    'resources/film_scherfig': asset40,
    'resources/film_suco': asset41,
    'resources/livre_alloinguillon': asset42,
    'resources/livre_cabrel': asset43,
    'resources/livre_delacourt': asset44,
    'resources/livre_jamoulle': asset45,
    'resources/livre_lambda': asset46,
    'resources/livre_vantran': asset47,
    'resources/musique_blueoctober': asset48,
    'resources/musique_gherbo': asset49,
    'resources/musique_jamesarthur': asset50,
    'resources/musique_ladygaga': asset51,
    'resources/musique_thecranberries': asset52,
    'resources/pro_brillon': asset53,
    'resources/pro_hingrayelhage': asset54,
    'resources/pro_kolk': asset55,
    'resources/video_cn2r': asset56,
    'resources/video_fenvac': asset57,
    'resources/video_legriguer': asset58,
    'ui/116006': asset59,
    'ui/15': asset60,
    'ui/3114': asset61,
    'ui/anonymous': asset62,
    'ui/book': asset63,
    'ui/calendar': asset64,
    'ui/child': asset65,
    'ui/configure': asset66,
    'ui/confirm': asset67,
    'ui/copy': asset68,
    'ui/create': asset69,
    'ui/dashboard': asset70,
    'ui/delete': asset71,
    'ui/diary': asset72,
    'ui/dice': asset73,
    'ui/fullscreen': asset74,
    'ui/insert': asset75,
    'ui/left': asset76,
    'ui/link': asset77,
    'ui/move': asset78,
    'ui/movie': asset79,
    'ui/music': asset80,
    'ui/network': asset81,
    'ui/new': asset82,
    'ui/paper': asset83,
    'ui/people': asset84,
    'ui/private': asset85,
    'ui/redo': asset86,
    'ui/right': asset87,
    'ui/rotate': asset88,
    'ui/select': asset89,
    'ui/track': asset90,
    'ui/undo': asset91,
    'ui/unselect': asset92,
    'ui/user': asset93,
    'ui/validate': asset94,
    'ui/video': asset95,
};

export { ASSETS }
