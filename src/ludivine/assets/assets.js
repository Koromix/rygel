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

import asset0 from './main/footer.webp';
import asset1 from './main/ldv.webp';
import asset2 from './main/logo.webp';
import asset3 from './network/animal.png';
import asset4 from './network/community.png';
import asset5 from './network/family.png';
import asset6 from './network/friend.png';
import asset7 from './network/help.png';
import asset8 from './network/love.png';
import asset9 from './network/online.png';
import asset10 from './network/other.png';
import asset11 from './network/work.png';
import asset12 from './pictures/cn2r.webp';
import asset13 from './pictures/dispositif.webp';
import asset14 from './pictures/ecriture.webp';
import asset15 from './pictures/etirements.webp';
import asset16 from './pictures/help1.webp';
import asset17 from './pictures/help2.webp';
import asset18 from './pictures/kezako.webp';
import asset19 from './pictures/participer.webp';
import asset20 from './pictures/philosophie.webp';
import asset21 from './pictures/respiration.webp';
import asset22 from './pictures/sociotrauma.webp';
import asset23 from './pictures/staks.webp';
import asset24 from './resources/film_bescond.jpg';
import asset25 from './resources/film_ozon.jpg';
import asset26 from './resources/film_scherfig.jpg';
import asset27 from './resources/film_suco.jpg';
import asset28 from './resources/livre_alloinguillon.jpg';
import asset29 from './resources/livre_cabrel.jpg';
import asset30 from './resources/livre_delacourt.jpg';
import asset31 from './resources/livre_jamoulle.jpg';
import asset32 from './resources/livre_lambda.jpg';
import asset33 from './resources/livre_vantran.jpg';
import asset34 from './resources/musique_blueoctober.jpg';
import asset35 from './resources/musique_gherbo.jpg';
import asset36 from './resources/musique_jamesarthur.jpg';
import asset37 from './resources/musique_ladygaga.jpg';
import asset38 from './resources/musique_thecranberries.jpg';
import asset39 from './resources/pro_brillon.jpg';
import asset40 from './resources/pro_hingrayelhage.jpg';
import asset41 from './resources/pro_kolk.jpg';
import asset42 from './resources/video_cn2r.jpg';
import asset43 from './resources/video_fenvac.jpg';
import asset44 from './resources/video_legriguer.jpg';
import asset45 from './team/female.webp';
import asset46 from './team/male.webp';
import asset47 from './team/niels_martignene.jpg';
import asset48 from './ui/3114.webp';
import asset49 from './ui/anonymous.png';
import asset50 from './ui/book.png';
import asset51 from './ui/calendar.png';
import asset52 from './ui/child.png';
import asset53 from './ui/configure.png';
import asset54 from './ui/copy.png';
import asset55 from './ui/create.png';
import asset56 from './ui/dashboard.png';
import asset57 from './ui/delete.png';
import asset58 from './ui/dice.webp';
import asset59 from './ui/fullscreen.png';
import asset60 from './ui/insert.png';
import asset61 from './ui/left.webp';
import asset62 from './ui/link.png';
import asset63 from './ui/move.png';
import asset64 from './ui/movie.png';
import asset65 from './ui/music.png';
import asset66 from './ui/network.png';
import asset67 from './ui/new.png';
import asset68 from './ui/paper.png';
import asset69 from './ui/people.png';
import asset70 from './ui/redo.png';
import asset71 from './ui/right.webp';
import asset72 from './ui/rotate.png';
import asset73 from './ui/select.png';
import asset74 from './ui/track.png';
import asset75 from './ui/undo.png';
import asset76 from './ui/unselect.png';
import asset77 from './ui/user.png';
import asset78 from './ui/video.png';

const ASSETS = {
    'main/footer': asset0,
    'main/ldv': asset1,
    'main/logo': asset2,
    'network/animal': asset3,
    'network/community': asset4,
    'network/family': asset5,
    'network/friend': asset6,
    'network/help': asset7,
    'network/love': asset8,
    'network/online': asset9,
    'network/other': asset10,
    'network/work': asset11,
    'pictures/cn2r': asset12,
    'pictures/dispositif': asset13,
    'pictures/ecriture': asset14,
    'pictures/etirements': asset15,
    'pictures/help1': asset16,
    'pictures/help2': asset17,
    'pictures/kezako': asset18,
    'pictures/participer': asset19,
    'pictures/philosophie': asset20,
    'pictures/respiration': asset21,
    'pictures/sociotrauma': asset22,
    'pictures/staks': asset23,
    'resources/film_bescond': asset24,
    'resources/film_ozon': asset25,
    'resources/film_scherfig': asset26,
    'resources/film_suco': asset27,
    'resources/livre_alloinguillon': asset28,
    'resources/livre_cabrel': asset29,
    'resources/livre_delacourt': asset30,
    'resources/livre_jamoulle': asset31,
    'resources/livre_lambda': asset32,
    'resources/livre_vantran': asset33,
    'resources/musique_blueoctober': asset34,
    'resources/musique_gherbo': asset35,
    'resources/musique_jamesarthur': asset36,
    'resources/musique_ladygaga': asset37,
    'resources/musique_thecranberries': asset38,
    'resources/pro_brillon': asset39,
    'resources/pro_hingrayelhage': asset40,
    'resources/pro_kolk': asset41,
    'resources/video_cn2r': asset42,
    'resources/video_fenvac': asset43,
    'resources/video_legriguer': asset44,
    'team/female': asset45,
    'team/male': asset46,
    'team/niels_martignene': asset47,
    'ui/3114': asset48,
    'ui/anonymous': asset49,
    'ui/book': asset50,
    'ui/calendar': asset51,
    'ui/child': asset52,
    'ui/configure': asset53,
    'ui/copy': asset54,
    'ui/create': asset55,
    'ui/dashboard': asset56,
    'ui/delete': asset57,
    'ui/dice': asset58,
    'ui/fullscreen': asset59,
    'ui/insert': asset60,
    'ui/left': asset61,
    'ui/link': asset62,
    'ui/move': asset63,
    'ui/movie': asset64,
    'ui/music': asset65,
    'ui/network': asset66,
    'ui/new': asset67,
    'ui/paper': asset68,
    'ui/people': asset69,
    'ui/redo': asset70,
    'ui/right': asset71,
    'ui/rotate': asset72,
    'ui/select': asset73,
    'ui/track': asset74,
    'ui/undo': asset75,
    'ui/unselect': asset76,
    'ui/user': asset77,
    'ui/video': asset78,
};

export { ASSETS }
