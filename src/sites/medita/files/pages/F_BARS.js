if (typeof data !== 'undefined')
    data.makeHeader("Barnes Akathisia Rating Scale (BARS)", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true,
                                       hidden: goupile.isLocked()}).value

form.output(html`
    <p>Patient should be observed while they are seated, and then standing while engaged in neutral
conversation (for a minimum of two minutes in each position). Symptoms observed in other situations, for
example while engaged in activity on the ward, may also be rated. Subsequently, the subjective phenomena
should be elicited by direct questioning.</p>
`)
form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("Objective", () => {
    form.enumRadio("objective", "", [
        [0, "0 : Normal, occasional fidgety movements of the limbs."],
        [1, "1 : Presence of characteristic restless movements: shuffling or tramping movements of the legs/feet, or swinging of one leg while sitting, and/or rocking from foot to foot or “walking on the spot” when standing, but movements present for less than half the time observed"],
        [2, "2 : Observed phenomena, as described in (1) above, which are present for at least half the observation period."],
        [3, "3 : Patient is constantly engaged in characteristic restless movements, and/or has the inability to remain seated or standing without walking or pacing, during the time observed"],
            ])
})

form.section("Subjective", () => {
    form.enumRadio("Awarenessofrestlessness", "Awareness of restlessness", [
        [0, "0 : Absence of inner restlessness"],
        [1, "1 : Non-specific sense of inner restlessness"],
        [2, "2 : The patient is aware of an inability to keep the legs still, or a desire to move the legs, and/or complains of inner restlessness aggravated specifically by being required to stand still"],
        [3, "3 : Awareness of intense compulsion to move most of the time and/or reports strong desire to walk or pace most of the time"],

    ])
    form.enumRadio("Distressrelatedtorestlessness", "Distress related to restlessness", [
        [0, "0 : No distress"],
        [1, "1 : Mild"],
        [2, "2 : Moderate"],
        [3, "3 : Severe"],
    ])
})

form.section("Global Clinical Assessment of Akathisia", () => {

    form.enumRadio("GlobalClinicalAssessmentofAkathisia", "", [
        [0, "0 : Absent. No evidence of awareness of restlessness. Observation of characteristic movements of akathisia in the absence of a subjective report of inner restlessness or compulsive desire to move the legs should be classified as pseudoakathisia"],
        [1, "1 : Questionable. Non-specific inner tension and fidgety movements"],
        [2, "2 : Mild akathisia. Awareness of restlessness in the legs and/or inner restlessness worse when required to stand still. Fidgety movements present, but characteristic restless movements of akathisia not necessarily observed. Condition causes little or no distress."],
        [3, "3 : Moderate akathisia. Awareness of restlessness as described for mild akathisia above, combined with characteristic restless movements such as rocking from foot to foot when standing. Patient finds the condition distressing"],
        [4, "4 : Marked akathisia. Subjective experience of restlessness includes a compulsive desire to walk or pace. However, the patient is able to remain seated for at least five minutes. The condition is obviously distressing."],
        [5, "5 : Severe akathisia. The patient reports a strong compulsion to pace up and down most of the time. Unable to sit or lie down for more than a few minutes. Constant restlessness which is associated with intense distress and insomnia."],
    ])
})



let score = form.value("objective") +
            form.value("Awarenessofrestlessness") +
            form.value("Distressrelatedtorestlessness") +
            form.value("GlobalClinicalAssessmentofAkathisia")
form.calc("score", "Score total", score, {hidden: goupile.isLocked()})

if (typeof data !== 'undefined')
    data.makeFormFooter(nav, page)
