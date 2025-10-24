// Uncomment the following line to display fields (text, numeric, etc.) to the right of the label.
// form.pushOptions({ compact: true })

form.output(html`
    <p>A function consists of a name and several parameters and allows you to provide an <b>input field</b> (text field, dropdown menu, etc.).
    <p>Example: the function <span style="color: #24579d;">form.text("inclusion_num", "Inclusion number")</span> provides a text input field titled <i>Inclusion number</i> and stores it in the variable <i>inclusion_num</i>.
    <p>Many <b>other widgets (or fields)</b> can be used: <span style="color: #24579d;">form.date(), form.number(), form.enum(), form.binary(), form.enumDrop(), form.enumRadio(), form.multiCheck(), form.slider()...</span><br/>
    <p>These widgets can be configured using <b>a few options</b>: <i>value</i> (default value), <i>min/max</i> (for numeric values and scales), <i>compact</i> (to display the label and field on the same line), <i>help</i> (to provide details), <i>prefix/suffix</i>, etc.
`)

form.section("Simple widgets", () => {
    form.date("*inclusion_date", "Inclusion date")

    form.number("*age", "Age", {
        min: 0,
        max: 120
    })

    form.enum("smoking", "Smoking status", [
        ["active", "Current smoker"],
        ["stopped", "Former smoker"],
        ["no", "Non-smoker"]
    ])

    form.binary("hypertension", "Hypertension")

    form.enumDrop("csp", "Socio-professional category", [
        [1, "Farm operator"],
        [2, "Artisan, shopkeeper, or business owner"],
        [3, "Executive or higher intellectual profession"],
        [4, "Intermediate profession"],
        [5, "Employee"],
        [6, "Worker"],
        [7, "Retired"],
        [8, "Other or no professional activity"]
    ])

    form.enumRadio("living_place", "Living place", [
        ["house", "House"],
        ["apartment", "Apartment"]
    ])

    form.multiCheck("sommeil", "Sleep disorder(s)", [
        [1, "Sleep onset insomnia"],
        [2, "Sleep maintenance insomnia"],
        [3, "Early morning awakening"],
        [4, "Non-restorative sleep"],
        [null, "None of the above"]
    ])

    form.slider("eva", "Sleep quality", {
        min: 1,
        max: 10,
        help: "Rate sleep quality with a score between 0 (poor) and 10 (very good sleep)"
    })
})

form.output(html`
    <p>Go to the <b>"Advanced" page</b> using the main menu at the top of the page to continue.
`)
