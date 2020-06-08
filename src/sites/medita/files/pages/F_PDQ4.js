if (typeof data !== 'undefined')
    data.makeHeader("Questionnaire de personnalité", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

form.output(html`
    <p>Le but de ce questionnaire est de vous aider à décrire le genre de personne que vous êtes. Pour répondre aux questions, pensez à la manière dont vous avez eu tendance à ressentir les choses, à penser et à agir durant ces dernières années. Afin de vous rappeler cette consigne, chaque page du questionnaire commence par la phrase : « Depuis plusieurs années.. ».</p>


    <p>V (vrai) signifie que cet énoncé est généralement vrai pour vous.
     <p>F (faux) signifie que cet énoncé est généralement faux pour vous.
 <p>Même si vous n'êtes pas tout à fait certain(e) de votre réponse, veuillez indiquer V ou F à chaque question
<p> Par exemple, à l'énoncé suivant : "J'ai tendance à être têtu(e)	V	F"


<p>Si, depuis plusieurs années, vous êtes effectivement têtu(e), vous répondrez «vrai» en entourant le
V. Si cet énoncé ne s'applique pas du tout à vous, vous répondrez «faux» en entourant le F.
<p>Il n'y a pas de réponses justes ou fausses.
Vous pouvez prendre tout le temps qu'il vous faut.
<p>Depuis plusieurs années..

`)

let dataQ = [[1, "VRAI"], [2, "FAUX"]]

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.enum("p1", "1. J'évite de travailler avec des gens qui pourraient me critiquer.", dataQ)
form.enum("p2", "2. Je ne peux prendre aucune décision sans le conseil ou le soutien des autres.", dataQ)
form.enum("p3", "3. Je me perds souvent dans les détails et n'ai plus de vision d'ensemble.", dataQ)
form.enum("p4", "4. J'ai besoin d'être au centre de l'attention générale.", dataQ)
form.enum("p5", "5. J'ai accompli beaucoup plus de choses que ce que les autres me reconnaissent.", dataQ)
form.enum("p6", "6. Je ferais n'importe quoi pour éviter que ceux qui me sont chers ne me quittent.", dataQ)
form.enum("p7", "7. Les autres se sont plaint que je ne sois pas à la hauteur professionnellement ou que je ne tienne pas mes engagements.", dataQ)
form.enum("p8", "8. J'ai eu des problèmes avec la loi à plusieurs reprises (ou j'en aurais eu si j'avais été pris(e).", dataQ)
form.enum("p9", "9. Passer du temps avec ma famille ou avec des amis ne m'intéresse pas vraiment.", dataQ)
form.enum("p10", "10. Je reçois des messages particuliers de ce qui se passe autour de moi.", dataQ)
form.enum("p11", "11. Je sais que, si je les laisse faire, les gens vont profiter de moi ou chercher à me tromper.", dataQ)
form.enum("p12", "12. Parfois, je me sens bouleversé(e).", dataQ)
form.enum("p13", "13. Je ne me lie avec les gens que lorsque je suis sur qu'ils m'aiment.", dataQ)
form.enum("p14", "14. Je suis habituellement déprimé(e.", dataQ)
form.enum("p15", "15. Je préfère que ce soit les autres qui soient responsables pour moi.", dataQ)
form.enum("p16", "16. Je perds du temps à m'efforcer de tout faire parfaitement.", dataQ)
form.enum("p17", "17. Je suis plus «sexy» que la plupart des gens.", dataQ)
form.enum("p18", "18. Je me surprends souvent à penser à la personne importante que je suis ou que je vais devenir un jour.", dataQ)
form.enum("p19", "19. J'aime ou je déteste quelqu'un, il n'y a pas de milieu pour moi.", dataQ)
form.enum("p20", "20. Je me bagarre beaucoup physiquement.", dataQ)
form.enum("p21", "21. Je sens très bien que les autre ne me comprennent pas ou ne m'apprécient pas", dataQ)
form.enum("p22", "22. J'aime mieux faire les choses tout(e) seul(e) qu'avec les autres", dataQ)
form.enum("p23", "23. Je suis capable de savoir que certaines choses vont se produire avant qu'elles n'arrivent", dataQ)
form.enum("p24", "24. Je me demande souvent si les gens que je connais sont dignes de confiance.", dataQ)
form.enum("p25", "25. Parfois, je parle des gens dans leur dos.", dataQ)
form.enum("p26", "26. Je suis inhibé(e) dans mes relations intimes parce que j'ai peur d'être ridiculisé.", dataQ)
form.enum("p27", "27. Je crais de perdre le soutien des autres si je ne suis pas d'accord avec eux.", dataQ)
form.enum("p28", "28. Je souffre d'un manque d'estime de moi.", dataQ)
form.enum("p29", "29. Je place mon travail avant la famille, les amis ou les loisirs.", dataQ)
form.enum("p30", "30. Je montre facilement mes émotions.", dataQ)
form.enum("p31", "31. Seules certaines personnes tout à fait spéciales sont capables de m'apprécier et de me comprendre.", dataQ)
form.enum("p32", "32. Je me demande souvent qui je suis réellement.", dataQ)
form.enum("p33", "33. J'ai de la peine à payer mes factures parce que je ne reste jamais bien longtemps dans le même emploi.", dataQ)
form.enum("p34", "34. Le sexe ne m'intéresse tout simplement pas.", dataQ)
form.enum("p35", "35. Les autres me trouvent « soupe au lait» (susceptible) et colérique.", dataQ)
form.enum("p36", "36. Il m'arrive souvent de percevoir ou de ressentir des choses alors que les autres ne perçoivent rien.", dataQ)
form.enum("p37", "37. Les autres vont utiliser ce que je dis contre moi.", dataQ)
form.enum("p38", "38. Il y a des gens que je n'aime pas.", dataQ)
form.enum("p39", "39. Je suis plus sensible à la critique et au rejet que la plupart des gens.", dataQ)
form.enum("p40", "40. J'ai de la peine à commencer quelque chose si je dois le faire tout(e) seul(e).", dataQ)
form.enum("p41", "41. J'ai un sens moral plus élevé que les autres gens.", dataQ)
form.enum("p42", "42. Je suis mon «propre» pire critique.", dataQ)
form.enum("p43", "43. Je me sers de mon apparence pour attirer l'attention dont j'ai besoin.", dataQ)
form.enum("p44", "44. J'ai un immense besoin que les autres gens me remarquent et me fassent des compliments.", dataQ)
form.enum("p45", "45. J'ai essayé de me blesser ou de me tuer.", dataQ)
form.enum("p46", "46. Je fais beaucoup de choses sans penser aux conséquences.", dataQ)
form.enum("p47", "47. Il n'y a pas beaucoup d'activités qui retiennent mon intérêt.", dataQ)
form.enum("p48", "48. Les gens ont souvent de la difficulté à comprendre ce que je dis.", dataQ)
form.enum("p49", "49. Je m'oppose verbalement à mes supérieurs lorsqu'ils me disent de quelle façon faire mon travail.", dataQ)
form.enum("p50", "50. Je suis très attentif(ve) à déterminer la signification réelle de ce que les gens disent.", dataQ)
form.enum("p51", "51. Je n'ai jamais dit un mensonge.", dataQ)
form.enum("p52", "52. J'ai peur de rencontrer de nouvelles personnes parce que je me sens inadéquat(e).", dataQ)
form.enum("p53", "53. J'ai tellement envie que les gens m'aiment que j'en viens à me porter volontaire pour des choses qu'en fait je préférerais ne pas faire.", dataQ)
form.enum("p54", "54. J'ai accumulé énormément de choses dont je n'ai pas besoin mais que je suis incapable de jeter.", dataQ)
form.enum("p55", "55. Bien que je parle beaucoup, les gens me disent que j'ai de la peine à faire passer mes idées.", dataQ)
form.enum("p56", "56. Je me fais beaucoup de soucis.", dataQ)
form.enum("p57", "57. J'attends des autres qu'ils m'accordent des faveurs, quand bien même il n'est pas dans mes habitudes de leur en consentir.", dataQ)
form.enum("p58", "58. Je suis très «soupe au lait» (susceptible).", dataQ)
form.enum("p59", "59. Mentir m'est facile et je le fais souvent.", dataQ)
form.enum("p60", "60. Je ne suis pas intéressé(e) à avoir des amis proches.", dataQ)
form.enum("p61", "61. Je suis souvent sur mes gardes, de peur que l'on ne profite de moi.", dataQ)
form.enum("p62", "62. Je n'oublie pas et je ne pardonne jamais à ceux qui m'ont fait du mal.", dataQ)
form.enum("p63", "63. J'en veux à ceux qui ont plus de chance que moi.", dataQ)
form.enum("p64", "64. Une guerre atomique ne serait peut-être pas une si mauvaise idée.", dataQ)
form.enum("p65", "65. Lorsque je suis seul(e), je me sens désemparé(e) et incapable de m'occuper de moi-même.", dataQ)
form.enum("p66", "66. Si les autres sont incapables de faire les choses correctement, je préfère les faire moi-même.", dataQ)
form.enum("p67", "67. J'ai un penchant pour le «dramatique».", dataQ)
form.enum("p68", "68. Il y a des gens qui pensent que je profite des autres.", dataQ)
form.enum("p69", "69. Il me semble que ma vie est sans intérêt et n'a aucun sens.", dataQ)
form.enum("p70", "70. Je suis critique à l'égard des autres.", dataQ)
form.enum("p71", "71. Je ne me soucie pas de ce que les autres peuvent avoir à dire à mon sujet.", dataQ)
form.enum("p72", "72. J'ai des difficultés à soutenir un face-à-face.", dataQ)
form.enum("p73", "73. Les autres se sont souvent plaint que je ne remarquais pas qu'ils étaient bouleversés.", dataQ)
form.enum("p74", "74. En me regardant, les autres pourraient penser que je suis plutôt original(e), excentrique et bizarre.", dataQ)
form.enum("p75", "75. J'aime faire des choses risquées.", dataQ)
form.enum("p76", "76. J'ai beaucoup menti dans ce questionnaire.", dataQ)
form.enum("p77", "77. Je me plains beaucoup de toutes les difficultés que j'ai.", dataQ)
form.enum("p78", "78. J'ai de la peine à contrôler ma colère ou mes sautes d'humeur.", dataQ)
form.enum("p79", "79. Certaines personnes sont jalouses de moi.", dataQ)
form.enum("p80", "80. Je suis facilement influencé(e) par les autres.", dataQ)
form.enum("p81", "81. J'estime être économe, mais les autres me trouvent pingre.", dataQ)
form.enum("p82", "82. Quand une relation proche prend fin, j'ai besoin de m'engager immédiatement dans une autre relation.", dataQ)
form.enum("p83", "83. Je souffre d'un manque d'estime de soi.", dataQ)
form.enum("p84", "84. Je suis un(e) pessimiste.", dataQ)
form.enum("p85", "85. Je ne perds pas mon temps à répliquer aux gens qui m'insultent.", dataQ)
form.enum("p86", "86. Être au milieu des gens me rend nerveux(se).", dataQ)
form.enum("p87", "87. Dans les situations nouvelles, je crains d'être mal à l'aise.", dataQ)
form.enum("p88", "88. Je suis terrifié(e) à l'idée de devoir m'assumer tout(e) seul(e).", dataQ)
form.enum("p89", "89. Les gens se plaignent que je sois aussi têtu(e) qu'une mule.", dataQ)
form.enum("p90", "90. Je prends les relations avec les autres beaucoup plus au sérieux qu'ils ne le font eux-mêmes.", dataQ)
form.enum("p91", "91. Je peux être méchant(e) avec quelqu'un à un moment et, dans la minute qui suit lui présenter mes excuses.", dataQ)
form.enum("p92", "92. Les autres pensent que je suis prétentieux(se).", dataQ)
form.enum("p93", "93. Quand je suis stressé(e), il m'arrive de devenir «parano» ou même de perdre conscience.", dataQ)
form.enum("p94", "94. Tant que j'obtiens ce que je veux, il m'est égal que les autres en souffrent.", dataQ)
form.enum("p95", "95. Je garde mes distances à l'égard des autres  .", dataQ)
form.enum("p96", "96. Je me demande souvent si ma femme (mari, ami(e)) m'a trompé(e).", dataQ)
form.enum("p97", "97. Je me sens souvent coupable.", dataQ)

form.multiCheck("p98", "98. J'ai fait, de manière impulsive, des choses (comme celles indiquées ci-dessous) qui pourraient me créer des problèmes.", [
        [0, "a. Dépenser plus d'argent que je n'en ai."],
        [1, "b. Avoir des rapports sexuels avec des gens que je connais à peine."],
        [2, "c. Boire trop."],
        [3, "d. Prendre des drogues"],
        [4, "e. Manger de façon boulimique."],
        [5, "f. Conduire imprudemment."],
                ])
form.multiCheck("p99", "99. Lorsquej'étais enfant (avantl'âgede 15ans), j'étais une sorte de délinquant(e) juvénile et je faisais certaines des choses ci-dessous. Veuillez indiquer celles qui s'appliquent à vous :", [
        [1, "1.	J'étais considéré(e) comme une brute."],
        [2, "2. J'ai souvent déclenché des bagarres avec les autres enfants."],
        [3, "3. J'ai utilisé une arme dans mes bagarres."],
        [4, "4. J'ai volé ou agressé des gens"],
        [5, "5. J'ai été physiquement cruel(le) avec d'autres gens."],
        [6, "6. J'ai été physiquement cruel(le) avec des animaux."],
        [7, "7. J'ai forcé quelqu'un à avoir des rapports sexuels avec moi."],
        [8, "8. J'ai beaucoup menti."],
        [9, "9. J'ai découché sans la permission de mes parents."],
        [10, "10. J'ai dérobé des choses aux autres."],
        [11, "11. J'ai allumé des incendies."],
        [12, "12. J'ai cassé des fenêtres ou détruit la propriété d'autrui."],
        [13, "13. Je me suis plus d'une fois enfui(e) de la maison en pleine nuit."],
        [14, "14. J'ai commencé à beaucoup manquer l'école avant l'âge de 13 ans."],
        [15, "15. Je me suis introduit(e) par effraction dans la maison, le bâtiment ou la voiture de quelqu'un."],
                ])

form.calc("persoparano", "Personnalité paranoïaque ", form.value("p11") + form.value("p24") + form.value("p37") + form.value("p50") + form.value("p62") + form.value("p85") + form.value("p96"));

form.calc("persohistrio", "Personnalité histrionique", form.value("p4") + form.value("p17") + form.value("p30") + form.value("p43") + form.value("p55") + form.value("p67") + form.value("p80") + form.value("p90"));

form.calc("persoantisociale", "Personnalité anti-sociale", form.value("p8") + form.value("p20") + form.value("p33") + form.value("p46") + form.value("p59") + form.value("p75") + form.value("p94") + form.value((form.value("p99") || []).length >= 2));

form.calc("poc", "Personnalité obsessionnelle compulsive", form.value("p3") + form.value("p16") + form.value("p29") + form.value("p41") + form.value("p54") + form.value("p66") + form.value("p81") + form.value("p89"));

form.calc("persoschizoide", "Personnalité schizoïde", form.value("p9") + form.value("p22") + form.value("p34") + form.value("p47") + form.value("p60") + form.value("p71") + form.value("p95"));

form.calc("personarcissique", "Personnalité narcissique", form.value("p5") + form.value("p18") + form.value("p31") + form.value("p44") + form.value("p57") + form.value("p68") + form.value("p73") + form.value("p79") + form.value("p92"));

form.calc("persoschizotypique", "Personnalité schizoptypique", form.value("p10") + form.value("p23") + form.value("p36") + form.value("p48") + form.value("p61") + form.value("p72") + form.value("p74") + form.value("p60") + form.value("p86"));

form.calc("persoborderline", "Personnalité borderline", form.value("p6") + form.value("p19") + form.value("p32") + ((form.value("p98") || []).length >= 2) + form.value("p45") + form.value("p58") + form.value("p69") + form.value("p78") + form.value("p93"));

form.calc("persodependante", "Personnalité dépendante", form.value("p2") + form.value("p15") + form.value("p27") + form.value("p40") + form.value("p53") + form.value("p65") + form.value("p82") + form.value("p88"));


if (typeof data !== 'undefined')
    data.makeFormFooter(nav, page)
