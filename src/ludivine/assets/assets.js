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

import asset0 from './architecture/data.svg';
import asset1 from './architecture/login.svg';
import asset2 from './architecture/signup.svg';
import asset3 from './calendars/apple.png';
import asset4 from './calendars/google.png';
import asset5 from './calendars/ical.png';
import asset6 from './calendars/office365.png';
import asset7 from './calendars/outlook.png';
import asset8 from './calendars/yahoo.png';
import asset9 from './main/footer.webp';
import asset10 from './main/ldv.webp';
import asset11 from './main/logo.webp';
import asset12 from './misc/verres_std.png';
import asset13 from './network/animal.png';
import asset14 from './network/community.png';
import asset15 from './network/family.png';
import asset16 from './network/friend.png';
import asset17 from './network/help.png';
import asset18 from './network/love.png';
import asset19 from './network/online.png';
import asset20 from './network/org.png';
import asset21 from './network/other.png';
import asset22 from './network/work.png';
import asset23 from './partners/cn2r.webp';
import asset24 from './partners/f2rsmpsy.webp';
import asset25 from './partners/france_victimes.webp';
import asset26 from './partners/interactions1.webp';
import asset27 from './partners/interactions2.webp';
import asset28 from './partners/universite_lille.webp';
import asset29 from './pictures/aurora.webp';
import asset30 from './pictures/dispositif.webp';
import asset31 from './pictures/ecriture.webp';
import asset32 from './pictures/etirements.webp';
import asset33 from './pictures/etudes.webp';
import asset34 from './pictures/help1.webp';
import asset35 from './pictures/help2.webp';
import asset36 from './pictures/kezako.webp';
import asset37 from './pictures/participer.webp';
import asset38 from './pictures/philosophie.webp';
import asset39 from './pictures/respiration.webp';
import asset40 from './pictures/sociotrauma.webp';
import asset41 from './pictures/staks.webp';
import asset42 from './resources/film_bescond.jpg';
import asset43 from './resources/film_ozon.jpg';
import asset44 from './resources/film_scherfig.jpg';
import asset45 from './resources/film_suco.jpg';
import asset46 from './resources/livre_alloinguillon.jpg';
import asset47 from './resources/livre_cabrel.jpg';
import asset48 from './resources/livre_delacourt.jpg';
import asset49 from './resources/livre_jamoulle.jpg';
import asset50 from './resources/livre_lambda.jpg';
import asset51 from './resources/livre_vantran.jpg';
import asset52 from './resources/musique_blueoctober.jpg';
import asset53 from './resources/musique_gherbo.jpg';
import asset54 from './resources/musique_jamesarthur.jpg';
import asset55 from './resources/musique_ladygaga.jpg';
import asset56 from './resources/musique_thecranberries.jpg';
import asset57 from './resources/pro_brillon.jpg';
import asset58 from './resources/pro_hingrayelhage.jpg';
import asset59 from './resources/pro_kolk.jpg';
import asset60 from './resources/video_cn2r.jpg';
import asset61 from './resources/video_fenvac.jpg';
import asset62 from './resources/video_legriguer.jpg';
import asset63 from './ui/116006.webp';
import asset64 from './ui/15.webp';
import asset65 from './ui/3114.webp';
import asset66 from './ui/anonymous.png';
import asset67 from './ui/book.png';
import asset68 from './ui/calendar.png';
import asset69 from './ui/child.png';
import asset70 from './ui/configure.png';
import asset71 from './ui/confirm.png';
import asset72 from './ui/copy.png';
import asset73 from './ui/create.png';
import asset74 from './ui/dashboard.png';
import asset75 from './ui/delete.png';
import asset76 from './ui/diary.png';
import asset77 from './ui/dice.webp';
import asset78 from './ui/fullscreen.png';
import asset79 from './ui/insert.png';
import asset80 from './ui/left.webp';
import asset81 from './ui/link.png';
import asset82 from './ui/move.png';
import asset83 from './ui/movie.png';
import asset84 from './ui/music.png';
import asset85 from './ui/network.png';
import asset86 from './ui/new.png';
import asset87 from './ui/paper.png';
import asset88 from './ui/people.png';
import asset89 from './ui/private.png';
import asset90 from './ui/redo.png';
import asset91 from './ui/right.webp';
import asset92 from './ui/rotate.png';
import asset93 from './ui/select.png';
import asset94 from './ui/track.png';
import asset95 from './ui/undo.png';
import asset96 from './ui/unselect.png';
import asset97 from './ui/user.png';
import asset98 from './ui/validate.png';
import asset99 from './ui/video.png';

const ASSETS = {
    'architecture/data': asset0,
    'architecture/login': asset1,
    'architecture/signup': asset2,
    'calendars/apple': asset3,
    'calendars/google': asset4,
    'calendars/ical': asset5,
    'calendars/office365': asset6,
    'calendars/outlook': asset7,
    'calendars/yahoo': asset8,
    'main/footer': asset9,
    'main/ldv': asset10,
    'main/logo': asset11,
    'misc/verres_std': asset12,
    'network/animal': asset13,
    'network/community': asset14,
    'network/family': asset15,
    'network/friend': asset16,
    'network/help': asset17,
    'network/love': asset18,
    'network/online': asset19,
    'network/org': asset20,
    'network/other': asset21,
    'network/work': asset22,
    'partners/cn2r': asset23,
    'partners/f2rsmpsy': asset24,
    'partners/france_victimes': asset25,
    'partners/interactions1': asset26,
    'partners/interactions2': asset27,
    'partners/universite_lille': asset28,
    'pictures/aurora': asset29,
    'pictures/dispositif': asset30,
    'pictures/ecriture': asset31,
    'pictures/etirements': asset32,
    'pictures/etudes': asset33,
    'pictures/help1': asset34,
    'pictures/help2': asset35,
    'pictures/kezako': asset36,
    'pictures/participer': asset37,
    'pictures/philosophie': asset38,
    'pictures/respiration': asset39,
    'pictures/sociotrauma': asset40,
    'pictures/staks': asset41,
    'resources/film_bescond': asset42,
    'resources/film_ozon': asset43,
    'resources/film_scherfig': asset44,
    'resources/film_suco': asset45,
    'resources/livre_alloinguillon': asset46,
    'resources/livre_cabrel': asset47,
    'resources/livre_delacourt': asset48,
    'resources/livre_jamoulle': asset49,
    'resources/livre_lambda': asset50,
    'resources/livre_vantran': asset51,
    'resources/musique_blueoctober': asset52,
    'resources/musique_gherbo': asset53,
    'resources/musique_jamesarthur': asset54,
    'resources/musique_ladygaga': asset55,
    'resources/musique_thecranberries': asset56,
    'resources/pro_brillon': asset57,
    'resources/pro_hingrayelhage': asset58,
    'resources/pro_kolk': asset59,
    'resources/video_cn2r': asset60,
    'resources/video_fenvac': asset61,
    'resources/video_legriguer': asset62,
    'ui/116006': asset63,
    'ui/15': asset64,
    'ui/3114': asset65,
    'ui/anonymous': asset66,
    'ui/book': asset67,
    'ui/calendar': asset68,
    'ui/child': asset69,
    'ui/configure': asset70,
    'ui/confirm': asset71,
    'ui/copy': asset72,
    'ui/create': asset73,
    'ui/dashboard': asset74,
    'ui/delete': asset75,
    'ui/diary': asset76,
    'ui/dice': asset77,
    'ui/fullscreen': asset78,
    'ui/insert': asset79,
    'ui/left': asset80,
    'ui/link': asset81,
    'ui/move': asset82,
    'ui/movie': asset83,
    'ui/music': asset84,
    'ui/network': asset85,
    'ui/new': asset86,
    'ui/paper': asset87,
    'ui/people': asset88,
    'ui/private': asset89,
    'ui/redo': asset90,
    'ui/right': asset91,
    'ui/rotate': asset92,
    'ui/select': asset93,
    'ui/track': asset94,
    'ui/undo': asset95,
    'ui/unselect': asset96,
    'ui/user': asset97,
    'ui/validate': asset98,
    'ui/video': asset99,
};

export { ASSETS }
