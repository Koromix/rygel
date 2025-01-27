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

function SocioTrauma(study, start) {
    study.module('recueil', 'Recueil', mod => {
        mod.level = 'Temporalité';

        study.module('initial', 'Bilan initial', mod => {
            mod.level = 'Module';

            let options = { schedule: start };

            study.module('entourage', 'Entourage', () => {
                study.test('ssq6', 'Soutien social', options)
                study.test('sps10', 'Provisions sociales', options)
                study.test('sni', 'Réseau proche', options)
            });

            study.module('evenement', 'Évènement', () => {
                study.test('lec5', 'Situations stressantes', options)
                study.test('isrc', 'Pensées et ressenti', options)
                study.test('pcl5', 'Problèmes liés à l\'évènement', options)
                study.test('pdeq', 'Réactions pendant l\'évènement', options)
                study.test('adnm20', 'Évènements de vie', options)
                study.test('ctqsf', 'Enfance', options)
                study.test('ptci', 'Pensées suite à l\'évènement', options)
            });

            study.module('sante', 'Santé mentale', () => {
                study.test('phq9', 'Manifestations dépressives', options)
                study.test('gad7', 'Difficultés anxieuses', options)
                study.test('ssi', 'Idées suicidaires', options)
                study.test('substances', 'Consommations', options)
                study.test('isi', 'Qualité du sommeil', options)
            });

            study.module('qualite', 'Entourage', () => {
                study.test('mhqol', 'Qualité de vie', options)
            });

            study.module('divulgation', 'Entourage', () => {
                study.test('cses', 'Communication à l\'entourage', options)
                study.test('rds', 'Réactions des proches', options)
            });
        });
    })
}

export { SocioTrauma }
