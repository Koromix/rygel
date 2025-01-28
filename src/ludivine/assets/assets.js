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

import asset0 from './footer.webp';
import asset1 from './ldv.webp';
import asset2 from './logo.webp';
import asset3 from './app/main/calendar.png';
import asset4 from './app/main/configure.png';
import asset5 from './app/main/copy.png';
import asset6 from './app/main/dashboard.png';
import asset7 from './app/main/network.png';
import asset8 from './app/main/new.png';
import asset9 from './app/main/track.png';
import asset10 from './app/main/user.png';
import asset11 from './app/network/anonymous.png';
import asset12 from './app/network/child.png';
import asset13 from './app/network/couple.png';
import asset14 from './app/network/create.png';
import asset15 from './app/network/family.png';
import asset16 from './app/network/friend.png';
import asset17 from './app/network/healthcare.png';
import asset18 from './app/network/insert.png';
import asset19 from './app/network/link.png';
import asset20 from './app/network/move.png';
import asset21 from './app/network/rotate.png';
import asset22 from './app/network/select.png';
import asset23 from './app/network/unselect.png';
import asset24 from './app/network/work.png';
import asset25 from './app/ui/busy.png';
import asset26 from './app/ui/crosshair.png';
import asset27 from './app/ui/delete.png';
import asset28 from './app/ui/edit.png';
import asset29 from './app/ui/left.png';
import asset30 from './app/ui/locked.png';
import asset31 from './app/ui/mouse.png';
import asset32 from './app/ui/move.png';
import asset33 from './app/ui/open.png';
import asset34 from './app/ui/redo.png';
import asset35 from './app/ui/right.png';
import asset36 from './app/ui/save.png';
import asset37 from './app/ui/undo.png';
import asset38 from './web/equipe/female.webp';
import asset39 from './web/equipe/male.webp';
import asset40 from './web/equipe/niels_martignene.jpg';
import asset41 from './web/illustrations/calypso.webp';
import asset42 from './web/illustrations/cn2r.webp';
import asset43 from './web/illustrations/dispositif.webp';
import asset44 from './web/illustrations/donnees.webp';
import asset45 from './web/illustrations/ecriture.webp';
import asset46 from './web/illustrations/etirements.webp';
import asset47 from './web/illustrations/help.webp';
import asset48 from './web/illustrations/participer.webp';
import asset49 from './web/illustrations/respiration.webp';
import asset50 from './web/illustrations/sociotrauma.webp';
import asset51 from './web/illustrations/staks.webp';
import asset52 from './web/livres/apprendre_a_soigner_les_depressions.jpg';
import asset53 from './web/livres/humeur_normale_a_la_depression.jpg';
import asset54 from './web/livres/les_troubles_bipolaires.jpg';
import asset55 from './web/livres/psychoeducation_dans_depression.jpg';
import asset56 from './web/livres/traiter_la_depression.jpg';
import asset57 from './web/misc/3114.webp';
import asset58 from './web/misc/dice.webp';
import asset59 from './web/misc/left.webp';
import asset60 from './web/misc/right.webp';
import asset61 from './web/videos/blessures_invisibles.webp';
import asset62 from './web/videos/bloque_bloquee.webp';
import asset63 from './web/videos/trauma_vicariant.webp';
import asset64 from './web/videos/trop_trop_peur.webp';
import asset65 from './web/videos/violences_sexuelles_enfance.webp';

const ASSETS = {
    'footer': asset0,
    'ldv': asset1,
    'logo': asset2,
    'app/main/calendar': asset3,
    'app/main/configure': asset4,
    'app/main/copy': asset5,
    'app/main/dashboard': asset6,
    'app/main/network': asset7,
    'app/main/new': asset8,
    'app/main/track': asset9,
    'app/main/user': asset10,
    'app/network/anonymous': asset11,
    'app/network/child': asset12,
    'app/network/couple': asset13,
    'app/network/create': asset14,
    'app/network/family': asset15,
    'app/network/friend': asset16,
    'app/network/healthcare': asset17,
    'app/network/insert': asset18,
    'app/network/link': asset19,
    'app/network/move': asset20,
    'app/network/rotate': asset21,
    'app/network/select': asset22,
    'app/network/unselect': asset23,
    'app/network/work': asset24,
    'app/ui/busy': asset25,
    'app/ui/crosshair': asset26,
    'app/ui/delete': asset27,
    'app/ui/edit': asset28,
    'app/ui/left': asset29,
    'app/ui/locked': asset30,
    'app/ui/mouse': asset31,
    'app/ui/move': asset32,
    'app/ui/open': asset33,
    'app/ui/redo': asset34,
    'app/ui/right': asset35,
    'app/ui/save': asset36,
    'app/ui/undo': asset37,
    'web/equipe/female': asset38,
    'web/equipe/male': asset39,
    'web/equipe/niels_martignene': asset40,
    'web/illustrations/calypso': asset41,
    'web/illustrations/cn2r': asset42,
    'web/illustrations/dispositif': asset43,
    'web/illustrations/donnees': asset44,
    'web/illustrations/ecriture': asset45,
    'web/illustrations/etirements': asset46,
    'web/illustrations/help': asset47,
    'web/illustrations/participer': asset48,
    'web/illustrations/respiration': asset49,
    'web/illustrations/sociotrauma': asset50,
    'web/illustrations/staks': asset51,
    'web/livres/apprendre_a_soigner_les_depressions': asset52,
    'web/livres/humeur_normale_a_la_depression': asset53,
    'web/livres/les_troubles_bipolaires': asset54,
    'web/livres/psychoeducation_dans_depression': asset55,
    'web/livres/traiter_la_depression': asset56,
    'web/misc/3114': asset57,
    'web/misc/dice': asset58,
    'web/misc/left': asset59,
    'web/misc/right': asset60,
    'web/videos/blessures_invisibles': asset61,
    'web/videos/bloque_bloquee': asset62,
    'web/videos/trauma_vicariant': asset63,
    'web/videos/trop_trop_peur': asset64,
    'web/videos/violences_sexuelles_enfance': asset65,
};

export { ASSETS }
