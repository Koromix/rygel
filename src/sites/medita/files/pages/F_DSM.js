if (shared.makeHeader)
    shared.makeHeader("Catatonie", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("STUPEUR", () => {
    form.binary("stupeur","Absence d'activité psychomotrice, pas de relation active avec l'environnement.",[""])
})
form.section("CATALEPSIE", () => {
    form.binary("catalepsie", "Maintien contre la gravité de postures imposées par l'examinateur.",[""])
})
form.section("FLEXIBILITE CIREUSE", () => {
    form.binary("flexibiliteCireuse", "Résistance légère ou nette lors du positionnement induit par l'examinateur.",[""])
})
form.section("MUTISME", () => {
    form.binary("mutisme", "Absence ou quasi-absence de réponse verbale [exclure si secondaire à une aphasie connue].")
})
form.section("NÉGATIVISME", () => {
    form.binary("negativisme", "Opposition ou absence de réponse à des instructions ou à des stimuli extérieurs.")
})
form.section("PRISE DE POSTURE", () => {
    form.binary("priseDePosture", "Maintien actif, contre la gravité, d'une posture adoptée spontanément.")
})
form.section("MANIERISME", () => {
    form.binary("manierisme", "Caricatures bizarres ou solennelles d'actions ordinaires.")
})
form.section("STÉRÉOTYPIES", () => {
    form.binary("stereotypies", "Mouvements non dirigés vers un but, répétitifs et anormalement fréquents.")
})
form.section("AGITATION", () => {
    form.binary("agitation", "Non influencée par des stimuli externes.")
})
form.section("EXPRESSIONS FACIALES GRIMAÇANTES", () => {
    form.binary("grimaces", "")
})
form.section("ÉCHOLALIE", () => {
    form.binary("echolalie", "Répétition des paroles de l'examinateur.")
})
form.section("ÉCHOPRAXIE", () => {
    form.binary("echopraxie", "Reproduction des mouvements de l'examinateur.")
})

form.calc("score", "Score total", form.value("stupeur") +
                                  form.value("catalepsie") +
                                  form.value("flexibiliteCireuse") +
                                  form.value("mutisme") +
                                  form.value("negativisme") +
                                  form.value("priseDePosture") +
                                  form.value("manierisme") +
                                  form.value("stereotypies") +
                                  form.value("agitation") +
                                  form.value("grimaces") +
                                  form.value("echolalie") +
                                  form.value("echopraxie"))

form.output(html`
    <p>Si présence de plus de 3 signes => syndrome catatonique.</p>
    <p>En cas de syndrome catatonique : prendre les constantes du patient (tableau ci-dessous) et passer à l'évaluation complète du syndrome catatonique avec l'échelle de Bush Francis.</p>
`)

if (shared.makeHeader)
    shared.makeFormFooter(nav, page)
