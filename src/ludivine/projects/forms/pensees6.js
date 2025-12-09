// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form) {
    form.intro = html`
        <p>Voici une liste de problèmes parfois vécus à la suite d’une expérience très stressante. Pour ce questionnaire, gardez en tête <b>l’évènement qui vous a amené ici</b>, c’est-à-dire l’évènement décrit dans le bilan initial.
        <p>Pour chaque question, indiquez à quel point vous avez été dérangé au <b>cours du dernier mois</b>.
        <p><i>Ce questionnaire est le plus long de l’étude SocioTrauma, nous vous sommes reconnaissants du temps que vous passez pour notre étude !</i>
    `

    let lik5 = [
        [0, "Pas du tout"],
        [1, "Un petit peu"],
        [2, "Modérément"],
        [3, "Beaucoup"],
        [4, "Extrêmement"]
    ]
    let lik7 = [
        [1, "Totalement en désaccord"],
        [2, "Fortement en désaccord"],
        [3, "Légèrement en désaccord"],
        [4, "Neutre"],
        [5, "Légèrement en accord"],
        [6, "Fortement en accord"],
        [7, "Totalement en accord"]
    ]

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous été dérangé...</i>`)
        form.enumButtons("q1", "Par des souvenirs répétitifs, perturbants et non désirés de l'expérience stressante ?", lik5)
        form.enumButtons("q2", "Par des rêves répétitifs et perturbants de l'expérience stressante ?", lik5)
        form.enumButtons("q3", "Par l'impression soudaine de vous sentir ou d'agir comme si l'expérience stressante se produisait à nouveau ?", lik5, {
            help: "Comme si vous étiez là en train de le revivre"
        })
    })

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous été dérangé...</i>`)
        form.enumButtons("q4", "Par le fait d'être bouleversé lorsque quelque chose vous a rappelé l'expérience stressante ?", lik5)
        form.enumButtons("q5", "Par de fortes réactions physiques quand quelque chose vous a rappelé l'expérience stressante ?", lik5)
        form.enumButtons("q6", "Par l'évitement des souvenirs, pensées ou émotions associées à l'expérience stressante ?", lik5)
    })

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous été dérangé...</i>`)
        form.enumButtons("q7", "Par l'évitement des rappels externes de l'expérience stressante ?", lik5, {
            help: "Par exemple des personnes, des endroits, des conversations, des activités, des objets ou des situations"
        })
        form.enumButtons("q8", "Par le fait d'avoir de la difficulté à vous souvenir de certaines parties importantes de l'expérience stressante ?", lik5)
        form.enumButtons("q9", "Par le fait d'avoir de fortes croyances négatives de vous-mêmes, d'autrui ou du monde ?", lik5, {
            help: "Par exemple, avoir des pensées telles que « Je suis mauvais il y a quelque chose qui ne va vraiment pas chez moi, on ne peut faire confiance à personne, le monde est tout à fait dangereux »"
        })
    })

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous été dérangé...</i>`)
        form.enumButtons("q10", "Par le fait de vous blâmer ou de blâmer quelqu'un d'autre pour l'expérience stressante et/ou pour ce qui s'est produit par la suite ?", lik5)
        form.enumButtons("q11", "Par la présence de fortes émotions négatives telles que la peur, l'horreur, la colère, la culpabilité ou la honte ?", lik5)
        form.enumButtons("q12", "Par la perte d'intérêt pour les activités que vous aimez auparavant ?", lik5)
    })

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous été dérangé...</i>`)
        form.enumButtons("q13", "Par un sentiment d'éloignement ou d'isolement vis-à-vis des autres ?", lik5)
        form.enumButtons("q14", "Par le fait d'avoir de la difficulté à ressentir des émotions positives ?", lik5, {
            help: "Par exemple, être incapable de ressentir de la joie ou de ressentir de l'amour pour vos proches"
        })
        form.enumButtons("q15", "Par le fait de vous sentir irritiable ou en colère ou le fait d'agir de façon aggressive ?", lik5)
    })

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous été dérangé...</i>`)
        form.enumButtons("q16", "Par le fait de prendre trop de risques ou de faire des choses qui pourraient vos blesser ?", lik5)
        form.enumButtons("q17", "Par le fait de vous sentir en état d'alerte, vigilant ou sur vos gardes ?", lik5)
        form.enumButtons("q18", "Par le fait de vous sentir agité ou de sursauter facilement ?", lik5)
    })

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous été dérangé...</i>`)
        form.enumButtons("q19", "Par des difficultés de concentration ?", lik5)
        form.enumButtons("q20", "Par des difficultés à vous endormir ou à rester endormi ?", lik5)
    })

    form.intro = html`
        <p>Vous remarquerez peut-être que certaines questions <b>semblent se répéter</b>. C'est normal et voulu ! Chaque question a son importance pour nous permettre d'analyser correctement les résultats.
        <p>Nous vous sommes très reconnaissants pour votre temps et votre patience. Vos réponses sont précieuses pour notre recherche !
        <p>Indiquez à quel point vous avez été perturbé par les <b>problèmes indiqués le mois dernier</b>.
    `

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous...</i>`)
        form.enumButtons("p1", "Eu des rêves perturbants où se rejoue une partie de l’expérience ou qui sont clairement en relation avec l’expérience ?", lik5)
        form.enumButtons("p2", "Été perturbé par des images ou des souvenirs forts (qui viennent à l’esprit) comme si l’expérience se rejoue ici et maintenant ?", lik5)
        form.enumButtons("p3", "Évité les ressentis qui rappellent l’expérience (par exemple, pensées, sentiments ou sensations physiques) ?", lik5)
    })

    form.section(() => {
        form.output(html`<i>Au <b>cours du dernier mois</b>, à quel point avez-vous...</i>`)
        form.enumButtons("p4", "Évité des éléments extérieurs qui rappellent l’expérience ?", lik5, {
            help: "Par exemple des personnes, lieux, conversations, objets, activités, ou situations"
        })
        form.enumButtons("p5", "Été en état de super-alerte, vigilance ou sur ses gardes ?", lik5)
        form.enumButtons("p6", "Eu des réactions exagérées de surprise ou sursaut ?", lik5)
    })

    form.section(() => {
        form.enumButtons("p7", "Est-ce que cela a affecté vos relations et votre vie sociale ?", lik5)
        form.enumButtons("p8", "Est-ce que cela a affecté votre travail ou votre capacité à travailler ?", lik5)
        form.enumButtons("p9", "Est-ce que cela a affecté d’autres parties importantes de votre vie telles que la capacité à s’occuper de vos enfants, vos études, ou toutes autres activités importantes ?", lik5)
    })

    form.intro = html`
        <p>Les questions suivantes se rapportent à la manière dont vous vous sentez essentiellement, que vous pensez de vous-même essentiellement ou les manières dont vous êtes essentiellement en relation avec les autres.
        <p>Indiquez à quel point chaque proposition <b>est vraie pour vous</b>.
    `

    form.section(() => {
        form.output(html`<i>À quel point <b>est-ce vrai pour vous</b> ?</i>`)
        form.enumButtons("c1", "Quand je suis contrarié(e), il me faut beaucoup de temps pour me calmer :", lik5)
        form.enumButtons("c2", "Je me sens insensible ou émotionnellement éteint(e) :", lik5)
        form.enumButtons("c3", "Je me sens nul(le)", lik5)
    })

    form.section(() => {
        form.output(html`<i>À quel point <b>est-ce vrai pour vous</b> ?</i>`)
        form.enumButtons("c4", "Je me sens sans valeur :", lik5)
        form.enumButtons("c5", "Je me sens distant(e) ou coupé(e) des autres :", lik5)
        form.enumButtons("c6", "je trouve difficile de rester émotionnellement proche des autres :", lik5)
    })

    form.intro = html`
        <p>Au cours du dernier mois, les problèmes mentionnés ci-dessus concernant <b>vos émotions, vos croyances sur vous-même et vos relations</b> ont-ils :
    `

    form.section(() => {
        form.enumButtons("c7", "Créé de l'inquiétude ou de la détresse concernant vos relations ou votre vie sociale :", lik5)
        form.enumButtons("c8", "Affecté votre travail ou capacité à travailler :", lik5)
        form.enumButtons("c9", "Affecté d'autres parties importantes de votre vie telle que la capacité à s'occuper de vos enfants, vos études, ou toutes autres activités importantes :", lik5)
    })

    form.intro = html`
        <p>Nous souhaiterions maintenant en savoir plus sur les <b>pensées négatives que vous pourriez avoir</b> suite à l’évènement qui vous a amené ici.
        <p>Vous trouverez ci-dessous une <b>liste d’énoncés</b> qui peuvent être représentatifs ou non de ces pensées. Chaque personne réagit de façon différente à un événement très stressant. Il n’y a pas de bonnes ou de mauvaises réponses à ces énoncés.
    `

    form.section(() => {
        form.enumButtons("n1", "L’événement est arrivé à cause de la façon dont j’ai agi :", lik7)
        form.enumButtons("n2", "Je n’ai pas confiance que je ferai ce qui est juste et bon :", lik7)
        form.enumButtons("n3", "Je suis une personne faible :", lik7)
    })

    form.section(() => {
        form.enumButtons("n4", "Je ne serai pas capable de contrôler ma colère et je ferai quelque chose de terrible :", lik7)
        form.enumButtons("n5", "Je ne suis pas capable de gérer la moindre frustration :", lik7)
        form.enumButtons("n6", "Auparavant j’étais une personne heureuse, mais maintenant je suis toujours malheureux :", lik7)
    })

    form.section(() => {
        form.enumButtons("n7", "On ne peut pas faire confiance aux gens :", lik7)
        form.enumButtons("n8", "Je dois toujours être sur mes gardes :", lik7)
        form.enumButtons("n9", "Je me sens mort à l’intérieur :", lik7)
    })

    form.section(() => {
        form.enumButtons("n10", "On ne peut jamais savoir qui nous fera du mal :", lik7)
        form.enumButtons("n11", "Je dois être particulièrement vigilant parce qu’on ne sait jamais ce qui nous attend :", lik7)
        form.enumButtons("n12", "Je suis inadéquat :", lik7)
    })

    form.section(() => {
        form.enumButtons("n13", "Je ne serai pas capable de contrôler mes émotions et quelque chose de terrible va arriver :", lik7)
        form.enumButtons("n14", "Si je pense à l’événement, je ne serai pas capable de le gérer :", lik7)
        form.enumButtons("n15", "L’événement m’est arrivé à cause du type de personne que je suis :", lik7)
    })

    form.section(() => {
        form.enumButtons("n16", "Mes réactions depuis l’événement indiquent que je suis en train de devenir fou :", lik7)
        form.enumButtons("n17", "Je ne pourrai jamais ressentir des émotions normales de nouveau :", lik7)
        form.enumButtons("n18", "Le monde est un endroit dangereux :", lik7)
    })

    form.section(() => {
        form.enumButtons("n19", "Quelqu’un d’autre aurait été capable d’empêcher l’événement d’arriver :", lik7)
        form.enumButtons("n20", "J’ai changé pour le pire et je ne redeviendrai jamais normal :", lik7)
        form.enumButtons("n21", "Je me sens comme un objet plutôt que comme une personne :", lik7)
    })

    form.section(() => {
        form.enumButtons("n22", "Quelqu’un d’autre ne se serait pas mis dans cette situation :", lik7)
        form.enumButtons("n23", "Je ne peux pas compter sur les autres :", lik7)
        form.enumButtons("n24", "Je me sens isolé et mis à part des autres :", lik7)
    })

    form.section(() => {
        form.enumButtons("n25", "Je n’ai pas d’avenir :", lik7)
        form.enumButtons("n26", "Je ne peux pas empêcher que des mauvaises choses m’arrivent :", lik7)
        form.enumButtons("n27", "Les gens ne sont pas ce qu’ils semblent être :", lik7)
    })

    form.section(() => {
        form.enumButtons("n28", "Ma vie a été gâchée par l’événement :", lik7)
        form.enumButtons("n29", "Il y a quelque chose qui ne va pas en moi :", lik7)
        form.enumButtons("n30", "Mes réactions depuis l’événement prouvent que je n’ai pas la capacité d’y faire face :", lik7)
    })

    form.section(() => {
        form.enumButtons("n31", "Il y a quelque chose en moi qui a provoqué l’événement :", lik7)
        form.enumButtons("n32", "Je ne serai pas capable de tolérer mes pensées à propos de l’événement et je vais m’effondrer :", lik7)
        form.enumButtons("n33", "J’ai l’impression de ne plus me connaître :", lik7)
    })

    form.section(() => {
        form.enumButtons("n34", "On ne sait jamais quand quelque chose de terrible va arriver :", lik7)
        form.enumButtons("n35", "Je ne peux pas me faire confiance :", lik7)
        form.enumButtons("n36", "Plus rien de positif ne peut m’arriver :", lik7)
    })
}

export default build
