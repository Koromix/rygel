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
import asset3 from './app/main/configure.png';
import asset4 from './app/main/copy.png';
import asset5 from './app/main/dashboard.png';
import asset6 from './app/main/network.png';
import asset7 from './app/main/new.png';
import asset8 from './app/main/track.png';
import asset9 from './app/main/user.png';
import asset10 from './app/network/anonymous.png';
import asset11 from './app/network/child.png';
import asset12 from './app/network/couple.png';
import asset13 from './app/network/create.png';
import asset14 from './app/network/family.png';
import asset15 from './app/network/friend.png';
import asset16 from './app/network/healthcare.png';
import asset17 from './app/network/insert.png';
import asset18 from './app/network/link.png';
import asset19 from './app/network/move.png';
import asset20 from './app/network/rotate.png';
import asset21 from './app/network/select.png';
import asset22 from './app/network/unselect.png';
import asset23 from './app/network/work.png';
import asset24 from './app/ui/busy.png';
import asset25 from './app/ui/crosshair.png';
import asset26 from './app/ui/delete.png';
import asset27 from './app/ui/edit.png';
import asset28 from './app/ui/left.png';
import asset29 from './app/ui/locked.png';
import asset30 from './app/ui/mouse.png';
import asset31 from './app/ui/move.png';
import asset32 from './app/ui/open.png';
import asset33 from './app/ui/redo.png';
import asset34 from './app/ui/right.png';
import asset35 from './app/ui/save.png';
import asset36 from './app/ui/undo.png';
import asset37 from './web/equipe/female.webp';
import asset38 from './web/equipe/male.webp';
import asset39 from './web/equipe/niels_martignene.jpg';
import asset40 from './web/illustrations/calypso.webp';
import asset41 from './web/illustrations/cn2r.webp';
import asset42 from './web/illustrations/dispositif.webp';
import asset43 from './web/illustrations/donnees.webp';
import asset44 from './web/illustrations/ecriture.webp';
import asset45 from './web/illustrations/etirements.webp';
import asset46 from './web/illustrations/participer.webp';
import asset47 from './web/illustrations/respiration.webp';
import asset48 from './web/illustrations/sociotrauma.webp';
import asset49 from './web/illustrations/staks.webp';
import asset50 from './web/livres/apprendre_a_soigner_les_depressions.jpg';
import asset51 from './web/livres/humeur_normale_a_la_depression.jpg';
import asset52 from './web/livres/les_troubles_bipolaires.jpg';
import asset53 from './web/livres/psychoeducation_dans_depression.jpg';
import asset54 from './web/livres/traiter_la_depression.jpg';
import asset55 from './web/misc/3114.webp';
import asset56 from './web/misc/dice.webp';
import asset57 from './web/misc/left.webp';
import asset58 from './web/misc/right.webp';
import asset59 from './web/videos/blessures_invisibles.webp';
import asset60 from './web/videos/bloque_bloquee.webp';
import asset61 from './web/videos/trauma_vicariant.webp';
import asset62 from './web/videos/trop_trop_peur.webp';
import asset63 from './web/videos/violences_sexuelles_enfance.webp';

const ASSETS = {
    'footer': asset0,
    'ldv': asset1,
    'logo': asset2,
    'app/main/configure': asset3,
    'app/main/copy': asset4,
    'app/main/dashboard': asset5,
    'app/main/network': asset6,
    'app/main/new': asset7,
    'app/main/track': asset8,
    'app/main/user': asset9,
    'app/network/anonymous': asset10,
    'app/network/child': asset11,
    'app/network/couple': asset12,
    'app/network/create': asset13,
    'app/network/family': asset14,
    'app/network/friend': asset15,
    'app/network/healthcare': asset16,
    'app/network/insert': asset17,
    'app/network/link': asset18,
    'app/network/move': asset19,
    'app/network/rotate': asset20,
    'app/network/select': asset21,
    'app/network/unselect': asset22,
    'app/network/work': asset23,
    'app/ui/busy': asset24,
    'app/ui/crosshair': asset25,
    'app/ui/delete': asset26,
    'app/ui/edit': asset27,
    'app/ui/left': asset28,
    'app/ui/locked': asset29,
    'app/ui/mouse': asset30,
    'app/ui/move': asset31,
    'app/ui/open': asset32,
    'app/ui/redo': asset33,
    'app/ui/right': asset34,
    'app/ui/save': asset35,
    'app/ui/undo': asset36,
    'web/equipe/female': asset37,
    'web/equipe/male': asset38,
    'web/equipe/niels_martignene': asset39,
    'web/illustrations/calypso': asset40,
    'web/illustrations/cn2r': asset41,
    'web/illustrations/dispositif': asset42,
    'web/illustrations/donnees': asset43,
    'web/illustrations/ecriture': asset44,
    'web/illustrations/etirements': asset45,
    'web/illustrations/participer': asset46,
    'web/illustrations/respiration': asset47,
    'web/illustrations/sociotrauma': asset48,
    'web/illustrations/staks': asset49,
    'web/livres/apprendre_a_soigner_les_depressions': asset50,
    'web/livres/humeur_normale_a_la_depression': asset51,
    'web/livres/les_troubles_bipolaires': asset52,
    'web/livres/psychoeducation_dans_depression': asset53,
    'web/livres/traiter_la_depression': asset54,
    'web/misc/3114': asset55,
    'web/misc/dice': asset56,
    'web/misc/left': asset57,
    'web/misc/right': asset58,
    'web/videos/blessures_invisibles': asset59,
    'web/videos/bloque_bloquee': asset60,
    'web/videos/trauma_vicariant': asset61,
    'web/videos/trop_trop_peur': asset62,
    'web/videos/violences_sexuelles_enfance': asset63,
};

export { ASSETS }
