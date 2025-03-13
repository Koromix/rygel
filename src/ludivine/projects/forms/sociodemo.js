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

import { LocalDate } from '../../../web/core/base.js'
import { html } from '../../../../vendor/lit-html/lit-html.bundle.js'

let LANGUAGES = [
    ["aa", "Afar"],
    ["ab", "Abkhaze"],
    ["ae", "Avestique"],
    ["af", "Afrikaans"],
    ["ak", "Akan"],
    ["am", "Amharique"],
    ["an", "Aragonais"],
    ["ar", "Arabe"],
    ["as", "Assamais"],
    ["av", "Avar"],
    ["ay", "Aymara"],
    ["az", "Azéri"],
    ["ba", "Bachkir"],
    ["be", "Biélorusse"],
    ["bg", "Bulgare"],
    ["bh", "Bihari"],
    ["bi", "Bichelamar"],
    ["bm", "Bambara"],
    ["bn", "Bengali"],
    ["bo", "bod"],
    ["br", "Breton"],
    ["bs", "Bosnien"],
    ["ca", "Catalan"],
    ["ce", "Tchétchène"],
    ["ch", "Chamorro"],
    ["co", "Corse"],
    ["cr", "Cri"],
    ["cs", "Tchèque"],
    ["cu", "Vieux-slave"],
    ["cv", "Tchouvache"],
    ["cy", "Gallois"],
    ["da", "Danois"],
    ["de", "Allemand"],
    ["dv", "Maldivien"],
    ["dz", "Dzongkha"],
    ["ee", "Ewe"],
    ["el", "Grec moderne"],
    ["en", "Anglais"],
    ["eo", "Espéranto"],
    ["es", "Espagnol"],
    ["et", "Estonien"],
    ["eu", "Basque"],
    ["fa", "Persan"],
    ["ff", "Peul"],
    ["fi", "Finnois"],
    ["fj", "Fidjien"],
    ["fo", "Féroïen"],
    ["fr", "Français"],
    ["fy", "Frison occidental"],
    ["ga", "Irlandais"],
    ["gd", "Écossais"],
    ["gl", "Galicien"],
    ["gn", "Guarani"],
    ["gu", "Gujarati"],
    ["gv", "Mannois"],
    ["ha", "Haoussa"],
    ["he", "Hébreu"],
    ["hi", "Hindi"],
    ["ho", "Hiri motu"],
    ["hr", "Croate"],
    ["ht", "Créole"],
    ["hu", "Hongrois"],
    ["hy", "Arménien"],
    ["hz", "Héréro"],
    ["ia", "Interlingua"],
    ["id", "Indonésien"],
    ["ie", "Occidental"],
    ["ig", "Igbo"],
    ["ii", "Yi"],
    ["ik", "Inupiak"],
    ["io", "Ido"],
    ["is", "Islandais"],
    ["it", "Italien"],
    ["iu", "Inuktitut"],
    ["ja", "Japonais"],
    ["jv", "Javanais"],
    ["ka", "Géorgien"],
    ["kg", "Kikongo"],
    ["ki", "Kikuyu"],
    ["kj", "Kuanyama"],
    ["kk", "Kazakh"],
    ["kl", "Groenlandais"],
    ["km", "Khmer"],
    ["kn", "Kannada"],
    ["ko", "Coréen"],
    ["kr", "Kanouri"],
    ["ks", "Cachemiri"],
    ["ku", "Kurde"],
    ["kv", "Komi"],
    ["kw", "Cornique"],
    ["ky", "Kirghiz"],
    ["la", "Latin"],
    ["lb", "Luxembourgeois"],
    ["lg", "Ganda"],
    ["li", "Limbourgeois"],
    ["ln", "Lingala"],
    ["lo", "Lao"],
    ["lt", "Lituanien"],
    ["lu", "Luba"],
    ["lv", "Letton"],
    ["mg", "Malgache"],
    ["mh", "Marshallais"],
    ["mi", "Maori de Nouvelle-Zélande"],
    ["mk", "Macédonien"],
    ["ml", "Malayalam"],
    ["mn", "Mongol"],
    ["mo", "Moldave"],
    ["mr", "Marathi"],
    ["ms", "Malais"],
    ["mt", "Maltais"],
    ["my", "Birman"],
    ["na", "Nauruan"],
    ["nb", "Norvégien"],
    ["nd", "Sindebele"],
    ["ne", "Népalais"],
    ["ng", "Ndonga"],
    ["nl", "Néerlandais"],
    ["nn", "Norvégien"],
    ["no", "Norvégien"],
    ["nr", "Nrebele"],
    ["nv", "Navajo"],
    ["ny", "Chichewa"],
    ["oc", "Occitan"],
    ["oj", "Ojibwé"],
    ["om", "Oromo"],
    ["or", "Oriya"],
    ["os", "Ossète"],
    ["pa", "Pendjabi"],
    ["pi", "Pali"],
    ["pl", "Polonais"],
    ["ps", "Pachto"],
    ["pt", "Portugais"],
    ["qu", "Quechua"],
    ["rm", "Romanche"],
    ["rn", "Kirundi"],
    ["ro", "Roumain"],
    ["ru", "Russe"],
    ["rw", "Kinyarwanda"],
    ["sa", "Sanskrit"],
    ["sc", "Sarde"],
    ["sd", "Sindhi"],
    ["se", "Same du Nord"],
    ["sg", "Sango"],
    ["sh", "Serbo-croate"],
    ["si", "Cingalais"],
    ["sk", "Slovaque"],
    ["sl", "Slovène"],
    ["sm", "Samoan"],
    ["sn", "Shona"],
    ["so", "Somali"],
    ["sq", "Albanais"],
    ["sr", "Serbe"],
    ["ss", "Swati"],
    ["st", "Sotho"],
    ["su", "Soundanais"],
    ["sv", "Suédois"],
    ["sw", "Swahili"],
    ["ta", "Tamoul"],
    ["te", "Télougou"],
    ["tg", "Tadjik"],
    ["th", "Thaï"],
    ["ti", "Tigrigna"],
    ["tk", "Turkmène"],
    ["tl", "Tagalog"],
    ["tn", "Tswana"],
    ["to", "Tongien"],
    ["tr", "Turc"],
    ["ts", "Tsonga"],
    ["tt", "Tatar"],
    ["tw", "Twi"],
    ["ty", "Tahitien"],
    ["ug", "Ouïghour"],
    ["uk", "Ukrainien"],
    ["ur", "Ourdou"],
    ["uz", "Ouzbek"],
    ["ve", "Venda"],
    ["vi", "Vietnamien"],
    ["vo", "Volapük"],
    ["wa", "Wallon"],
    ["wo", "Wolof"],
    ["xh", "Xhosa"],
    ["yi", "Yiddish"],
    ["yo", "Yoruba"],
    ["za", "Zhuang"],
    ["zh", "Chinois"],
    ["zu", "Zoulou"]
]
LANGUAGES.sort((a, b) => {
    if (a[1] < b[1]) {
        return -1
    } else if (a[1] > b[1]) {
        return 1
    } else {
        return 0
    }
})
LANGUAGES = [
    ["fr", "Français"],
    ["en", "Anglais"],
    ["de", "Allemand"],
    ["es", "Espagnol"],

    ...LANGUAGES
]

let PAYS = [
    ["FR", "France"],

    ["DE", "Allemagne"],
    ["BE", "Belgique"],
    ["ES", "Espagne"],
    ["GB", "Royaume-Uni"],
    ["CH", "Suisse"],

    ["AF", "Afghanistan"],
    ["AL", "Albanie"],
    ["AQ", "Antarctique"],
    ["DZ", "Algérie"],
    ["AS", "Samoa Américaines"],
    ["AD", "Andorre"],
    ["AO", "Angola"],
    ["AG", "Antigua-et-Barbuda"],
    ["AZ", "Azerbaïdjan"],
    ["AR", "Argentine"],
    ["AU", "Australie"],
    ["AT", "Autriche"],
    ["BS", "Bahamas"],
    ["BH", "Bahreïn"],
    ["BD", "Bangladesh"],
    ["AM", "Arménie"],
    ["BB", "Barbade"],
    ["BE", "Belgique"],
    ["BM", "Bermudes"],
    ["BT", "Bhoutan"],
    ["BO", "Bolivie"],
    ["BA", "Bosnie-Herzégovine"],
    ["BW", "Botswana"],
    ["BV", "Île Bouvet"],
    ["BR", "Brésil"],
    ["BZ", "Belize"],
    ["IO", "Territoire Britannique de l'Océan Indien"],
    ["SB", "Îles Salomon"],
    ["VG", "Îles Vierges Britanniques"],
    ["BN", "Brunéi Darussalam"],
    ["BG", "Bulgarie"],
    ["MM", "Myanmar"],
    ["BI", "Burundi"],
    ["BY", "Bélarus"],
    ["KH", "Cambodge"],
    ["CM", "Cameroun"],
    ["CA", "Canada"],
    ["CV", "Cap-vert"],
    ["KY", "Îles Caïmanes"],
    ["CF", "République Centrafricaine"],
    ["LK", "Sri Lanka"],
    ["TD", "Tchad"],
    ["CL", "Chili"],
    ["CN", "Chine"],
    ["TW", "Taïwan"],
    ["CX", "Île Christmas"],
    ["CC", "Îles Cocos (Keeling)"],
    ["CO", "Colombie"],
    ["KM", "Comores"],
    ["YT", "Mayotte"],
    ["CG", "République du Congo"],
    ["CD", "République Démocratique du Congo"],
    ["CK", "Îles Cook"],
    ["CR", "Costa Rica"],
    ["HR", "Croatie"],
    ["CU", "Cuba"],
    ["CY", "Chypre"],
    ["CZ", "République Tchèque"],
    ["BJ", "Bénin"],
    ["DK", "Danemark"],
    ["DM", "Dominique"],
    ["DO", "République Dominicaine"],
    ["EC", "Équateur"],
    ["SV", "El Salvador"],
    ["GQ", "Guinée Équatoriale"],
    ["ET", "Éthiopie"],
    ["ER", "Érythrée"],
    ["EE", "Estonie"],
    ["FO", "Îles Féroé"],
    ["FK", "Îles (malvinas) Falkland"],
    ["GS", "Géorgie du Sud et les Îles Sandwich du Sud"],
    ["FJ", "Fidji"],
    ["FI", "Finlande"],
    ["AX", "Îles Åland"],
    ["FR", "France"],
    ["GF", "Guyane Française"],
    ["PF", "Polynésie Française"],
    ["TF", "Terres Australes Françaises"],
    ["DJ", "Djibouti"],
    ["GA", "Gabon"],
    ["GE", "Géorgie"],
    ["GM", "Gambie"],
    ["PS", "Territoire Palestinien Occupé"],
    ["DE", "Allemagne"],
    ["GH", "Ghana"],
    ["GI", "Gibraltar"],
    ["KI", "Kiribati"],
    ["GR", "Grèce"],
    ["GL", "Groenland"],
    ["GD", "Grenade"],
    ["GP", "Guadeloupe"],
    ["GU", "Guam"],
    ["GT", "Guatemala"],
    ["GN", "Guinée"],
    ["GY", "Guyana"],
    ["HT", "Haïti"],
    ["HM", "Îles Heard et Mcdonald"],
    ["VA", "Saint-Siège (état de la Cité du Vatican)"],
    ["HN", "Honduras"],
    ["HK", "Hong-Kong"],
    ["HU", "Hongrie"],
    ["IS", "Islande"],
    ["IN", "Inde"],
    ["ID", "Indonésie"],
    ["IR", "République Islamique d'Iran"],
    ["IQ", "Iraq"],
    ["IE", "Irlande"],
    ["IL", "Israël"],
    ["IT", "Italie"],
    ["CI", "Côte d'Ivoire"],
    ["JM", "Jamaïque"],
    ["JP", "Japon"],
    ["KZ", "Kazakhstan"],
    ["JO", "Jordanie"],
    ["KE", "Kenya"],
    ["KP", "République Populaire Démocratique de Corée"],
    ["KR", "République de Corée"],
    ["KW", "Koweït"],
    ["KG", "Kirghizistan"],
    ["LA", "République Démocratique Populaire Lao"],
    ["LB", "Liban"],
    ["LS", "Lesotho"],
    ["LV", "Lettonie"],
    ["LR", "Libéria"],
    ["LY", "Jamahiriya Arabe Libyenne"],
    ["LI", "Liechtenstein"],
    ["LT", "Lituanie"],
    ["LU", "Luxembourg"],
    ["MO", "Macao"],
    ["MG", "Madagascar"],
    ["MW", "Malawi"],
    ["MY", "Malaisie"],
    ["MV", "Maldives"],
    ["ML", "Mali"],
    ["MT", "Malte"],
    ["MQ", "Martinique"],
    ["MR", "Mauritanie"],
    ["MU", "Maurice"],
    ["MX", "Mexique"],
    ["MC", "Monaco"],
    ["MN", "Mongolie"],
    ["MD", "République de Moldova"],
    ["MS", "Montserrat"],
    ["MA", "Maroc"],
    ["MZ", "Mozambique"],
    ["OM", "Oman"],
    ["NA", "Namibie"],
    ["NR", "Nauru"],
    ["NP", "Népal"],
    ["NL", "Pays-Bas"],
    ["AN", "Antilles Néerlandaises"],
    ["AW", "Aruba"],
    ["NC", "Nouvelle-Calédonie"],
    ["VU", "Vanuatu"],
    ["NZ", "Nouvelle-Zélande"],
    ["NI", "Nicaragua"],
    ["NE", "Niger"],
    ["NG", "Nigéria"],
    ["NU", "Niué"],
    ["NF", "Île Norfolk"],
    ["NO", "Norvège"],
    ["MP", "Îles Mariannes du Nord"],
    ["UM", "Îles Mineures Éloignées des États-Unis"],
    ["FM", "États Fédérés de Micronésie"],
    ["MH", "Îles Marshall"],
    ["PW", "Palaos"],
    ["PK", "Pakistan"],
    ["PA", "Panama"],
    ["PG", "Papouasie-Nouvelle-Guinée"],
    ["PY", "Paraguay"],
    ["PE", "Pérou"],
    ["PH", "Philippines"],
    ["PN", "Pitcairn"],
    ["PL", "Pologne"],
    ["PT", "Portugal"],
    ["GW", "Guinée-Bissau"],
    ["TL", "Timor-Leste"],
    ["PR", "Porto Rico"],
    ["QA", "Qatar"],
    ["RE", "Réunion"],
    ["RO", "Roumanie"],
    ["RU", "Fédération de Russie"],
    ["RW", "Rwanda"],
    ["SH", "Sainte-Hélène"],
    ["KN", "Saint-Kitts-et-Nevis"],
    ["AI", "Anguilla"],
    ["LC", "Sainte-Lucie"],
    ["PM", "Saint-Pierre-et-Miquelon"],
    ["VC", "Saint-Vincent-et-les Grenadines"],
    ["SM", "Saint-Marin"],
    ["ST", "Sao Tomé-et-Principe"],
    ["SA", "Arabie Saoudite"],
    ["SN", "Sénégal"],
    ["SC", "Seychelles"],
    ["SL", "Sierra Leone"],
    ["SG", "Singapour"],
    ["SK", "Slovaquie"],
    ["VN", "Viet Nam"],
    ["SI", "Slovénie"],
    ["SO", "Somalie"],
    ["ZA", "Afrique du Sud"],
    ["ZW", "Zimbabwe"],
    ["ES", "Espagne"],
    ["EH", "Sahara Occidental"],
    ["SD", "Soudan"],
    ["SR", "Suriname"],
    ["SJ", "Svalbard etÎle Jan Mayen"],
    ["SZ", "Swaziland"],
    ["SE", "Suède"],
    ["CH", "Suisse"],
    ["SY", "République Arabe Syrienne"],
    ["TJ", "Tadjikistan"],
    ["TH", "Thaïlande"],
    ["TG", "Togo"],
    ["TK", "Tokelau"],
    ["TO", "Tonga"],
    ["TT", "Trinité-et-Tobago"],
    ["AE", "Émirats Arabes Unis"],
    ["TN", "Tunisie"],
    ["TR", "Turquie"],
    ["TM", "Turkménistan"],
    ["TC", "Îles Turks et Caïques"],
    ["TV", "Tuvalu"],
    ["UG", "Ouganda"],
    ["UA", "Ukraine"],
    ["MK", "L'ex-République Yougoslave de Macédoine"],
    ["EG", "Égypte"],
    ["GB", "Royaume-Uni"],
    ["IM", "Île de Man"],
    ["TZ", "République-Unie de Tanzanie"],
    ["US", "États-Unis"],
    ["VI", "Îles Vierges des États-Unis"],
    ["BF", "Burkina Faso"],
    ["UY", "Uruguay"],
    ["UZ", "Ouzbékistan"],
    ["VE", "Venezuela"],
    ["WF", "Wallis et Futuna"],
    ["WS", "Samoa"],
    ["YE", "Yémen"],
    ["CS", "Serbie-et-Monténégro"],
    ["ZM", "Zambie"]
]

let DEPARTEMENTS = [
    ["01", "Ain"],
    ["02", "Aisne"],
    ["03", "Allier"],
    ["04", "Alpes-de-Haute-Provence"],
    ["05", "Hautes-Alpes"],
    ["06", "Alpes-Maritimes"],
    ["07", "Ardèche"],
    ["08", "Ardennes"],
    ["09", "Ariège"],
    ["10", "Aube"],
    ["11", "Aude"],
    ["12", "Aveyron"],
    ["13", "Bouches-du-Rhône"],
    ["14", "Calvados"],
    ["15", "Cantal"],
    ["16", "Charente"],
    ["17", "Charente-Maritime"],
    ["18", "Cher"],
    ["19", "Corrèze"],
    ["21", "Côte-d'Or"],
    ["22", "Côtes-d'Armor"],
    ["23", "Creuse"],
    ["24", "Dordogne"],
    ["25", "Doubs"],
    ["26", "Drôme"],
    ["27", "Eure"],
    ["28", "Eure-et-Loir"],
    ["29", "Finistère"],
    ["2A", "Corse-du-Sud"],
    ["2B", "Haute-Corse"],
    ["30", "Gard"],
    ["31", "Haute-Garonne"],
    ["32", "Gers"],
    ["33", "Gironde"],
    ["34", "Hérault"],
    ["35", "Ille-et-Vilaine"],
    ["36", "Indre"],
    ["37", "Indre-et-Loire"],
    ["38", "Isère"],
    ["39", "Jura"],
    ["40", "Landes"],
    ["41", "Loir-et-Cher"],
    ["42", "Loire"],
    ["43", "Haute-Loire"],
    ["44", "Loire-Atlantique"],
    ["45", "Loiret"],
    ["46", "Lot"],
    ["47", "Lot-et-Garonne"],
    ["48", "Lozère"],
    ["49", "Maine-et-Loire"],
    ["50", "Manche"],
    ["51", "Marne"],
    ["52", "Haute-Marne"],
    ["53", "Mayenne"],
    ["54", "Meurthe-et-Moselle"],
    ["55", "Meuse"],
    ["56", "Morbihan"],
    ["57", "Moselle"],
    ["58", "Nièvre"],
    ["59", "Nord"],
    ["60", "Oise"],
    ["61", "Orne"],
    ["62", "Pas-de-Calais"],
    ["63", "Puy-de-Dôme"],
    ["64", "Pyrénées-Atlantiques"],
    ["65", "Hautes-Pyrénées"],
    ["66", "Pyrénées-Orientales"],
    ["67", "Bas-Rhin"],
    ["68", "Haut-Rhin"],
    ["69", "Rhône"],
    ["70", "Haute-Saône"],
    ["71", "Saône-et-Loire"],
    ["72", "Sarthe"],
    ["73", "Savoie"],
    ["74", "Haute-Savoie"],
    ["75", "Paris"],
    ["76", "Seine-Maritime"],
    ["77", "Seine-et-Marne"],
    ["78", "Yvelines"],
    ["79", "Deux-Sèvres"],
    ["80", "Somme"],
    ["81", "Tarn"],
    ["82", "Tarn-et-Garonne"],
    ["83", "Var"],
    ["84", "Vaucluse"],
    ["85", "Vendée"],
    ["86", "Vienne"],
    ["87", "Haute-Vienne"],
    ["88", "Vosges"],
    ["89", "Yonne"],
    ["90", "Territoire de Belfort"],
    ["91", "Essonne"],
    ["92", "Hauts-de-Seine"],
    ["93", "Seine-Saint-Denis"],
    ["94", "Val-de-Marne"],
    ["95", "Val-d'Oise"],
    ["971", "Guadeloupe"],
    ["972", "Martinique"],
    ["973", "Guyane"],
    ["974", "La Réunion"],
    ["976", "Mayotte"]
]

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Donnez-nous quelques informations pour nous aider à mieux vous comprendre.
    `

    form.part(() => {
        form.enumButtons("genre", "Quel est votre genre ?", [
            ["F", "Femme"],
            ["H", "Homme"],
            ["A", "Non-binaire"]
        ])
        form.enumButtons("sexe", "Quel est votre sexe biologique ?", [
            ["F", "Féminin"],
            ["H", "Masculin"],
            ["I", "Intersexe"]
        ])
    })

    form.part(() => {
        form.number("age", "Quel âge avez-vous ?", {
            min: 18, max: 120,
            suffix: value => value > 1 ? "ans" : "an",
            help: "Indiquez votre âge au moment de votre inscription initiale dans l'application"
        })
        form.enumDrop("pays_naissance", "Dans quel pays êtes-vous " + adapt("né", "née") + " ?", PAYS)
    })

    form.part(() => {
        form.enumDrop("pays", "Dans quel pays habitez-vous actuellement ?", PAYS, { value: values.pays_naissance })

        if (values.pays == "FR")
            form.enumDrop("departement", "Dans quel département ?", DEPARTEMENTS)

        form.enumRadio("situation", "Quelle est votre situation familiale ?", [
            ["C", "Célibataire"],
            ["M", adapt("Marié", "Mariée")],
            ["L", "En union libre"],
            ["P", adapt("Pacsé", "Pacsée")],
            ["D", adapt("Divorcé", "Divorcée")],
            ["S", adapt("Séparé", "Séparée")],
            ["V", adapt("Veuf", "Veuve")]
        ])
    })

    form.part(() => {
        if (values.sexe == "F")
            form.number("grossesses", "Combien de grossesses avez-vous eu ?")
        form.number("enfants", "Combien d'enfants avez-vous eu ?")
    })

    form.part(() => {
        form.binary("pec1", "Avez-vous, au cours du dernier mois consulté un médecin, pris un traitement médicamenteux ou suivi une psychothérapie en lien avec l’évènement que vous venez de vivre et qui vous a amené à participer à cette étude ?")

        form.multiCheck("?atcd", "Avant l’évènement, aviez-vous été diagnostiqué par un professionnel de la santé comme souffrant :", [
            [1, "D’un trouble de l’humeur ?"],
            [2, "D’un trouble anxieux ?"],
            [3, "D’un trouble alimentaire ?"],
            [4, "D’un trouble d'addiction ?"]
        ])

        if (values.atcd != null)
            form.binary("pec2", "Avez-vous consulté un médecin, pris un traitement médicamenteux ou suivi une psychothérapie pour un ou plusieurs des troubles cités ci-dessus ?")
    })

    form.part(() => {
        form.binary("diplome", "Avez-vous un diplôme scolaire ?", {
            help: "Si ce n'est pas le cas, ce n'est pas un problème ! Cette question nous aide simplement à cerner qui vous êtes."
        })

        if (values.diplome == 1) {
            form.enumRadio("diplome_max", "Quel est le plus haut diplôme que vous avez obtenu ?", [
                [1, "Brevet des collèges"],
                [2, "Baccalauréat"],
                [3, "Licence"],
                [4, "Master (maîtrise)"],
                [5, "Doctorat"],
                [99, "Autre"]
            ])
            if (values.diplome_max == 99)
                form.text("?diplome_prec", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })
        }
    })

    form.part(() => {
        form.enumDrop("langue1", "Quelle est votre langue maternelle ?", LANGUAGES, {
            help: "Vous pouvez en indiquer plusieurs, choisissez-en une et un champ supplémentaire s'affichera"
        })

        let idx = 1

        while (values["langue" + idx] != null) {
            let first = (idx++ == 1)

            form.enumDrop("?langue" + idx, first ? "Parlez vous d'autres langues ?" : null, LANGUAGES, {
                disabled: values.langue1 == null,
                help: "Non obligatoire"
            })
        }
    })

    form.part(() => {
        form.enumDrop("parents1", "Quelle est la principale langue parlée par vos parents ?", LANGUAGES, {
            help: "Vous pouvez en indiquer plusieurs, choisissez-en une et un champ supplémentaire s'affichera"
        })

        let idx = 1

        while (values["parents" + idx] != null) {
            let first = (idx++ == 1)

            form.enumDrop("?parents" + idx, first ? "Vos parents parlent-ils une ou plusieurs autres langues ?" : null, LANGUAGES, {
                disabled: values.parents1 == null,
                help: "Non obligatoire"
            })
        }
    })

    form.part(() => {
        form.enumRadio("connaissance", "Comment avez-vous pris connaissance de l'étude ?", [
            ["reseaux", "Réseaux sociaux"],
            ["parle", "Bouche-à-oreille"],
            ["fv", "Via France Victimes"],
            ["crp", "Via un centre régional de psychotraumatisme"],
            ["psy", "Par un professionel de santé (médecin, psychologue, etc.)"],
            ["cn2r", "Par le site web du CN2R"],
            ["publicite", "Publicité (affiche, etc.)"],
            ["recherche", "Moteur de recherche"],
            ["autre", "Autre"]
        ])
        if (values.connaissance == "autre")
            form.text("?connaissance_prec", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })
    })

    function adapt(h, f) {
        if (values.genre == 'H') {
            return h
        } else if (values.genre == 'F') {
            return f
        } else {
            return h + '/' + f
        }
    }
}

export default build
