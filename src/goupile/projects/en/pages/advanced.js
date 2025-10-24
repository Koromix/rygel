// Uncomment the following line to display fields (text, numeric, etc.) to the right of the label.
// form.pushOptions({ compact: true })

form.output(html`
    <p>This page introduces <i>conditions</i>, <i>errors</i>, <i>calculated variables</i>, as well as some <i>layout</i> elements. Explanations will be provided as we go along.
`)

form.section("Conditions", () => {
    form.date("inclusion_date", "Inclusion date", {
        value: LocalDate.today(),
        help: "You can test the validity of one or more values with code. Here, a value later than today triggers an error. Very complex conditions can be programmed if needed."
    })
    if (form.value("inclusion_date") > LocalDate.today()) {
        form.error("inclusion_date", "The inclusion date cannot occur later than today")
    }

    form.number("age", "Age", {
        min: 0,
        max: 120,
        mandatory: true,
        suffix: value => value > 1 ? "years old" : "year old",
        help: "This field uses the mandatory option instead of the asterisk to make input mandatory. Additionally, it uses a dynamic suffix that updates based on the value (1 year / 3 years)."
    })

    form.enum("smoking", "Smoking Status", [
        ["active", "Current smoker"],
        ["stopped", "Former smoker"],
        ["no", "Non-Smoker"]
    ], {
        help: "Selecting 'Current smoker' triggers the appearance of another field."
    })

    if (form.value("smoking") == "active") {
        form.number("smoking_cig", "Number of cigarettes per day")
    }

    form.binary("hyperchol", "Hypercholesterolemia")
    form.sameLine(); form.binary("hypertension", "Hypertension")
    form.sameLine(); form.binary("diabete", "Diabetes", {
        help: "form.sameLine(); displays the field on the same line."
    })
})

form.section("Weight and height", () => {
    form.number("weight", "Weight", {
        min: 20,
        max: 400,
        suffix: "kg"
    })
    form.number("height", "Height", {
        min: 1,
        max: 3,
        decimals: 2,
        suffix: "m",
        help: "Enter a weight and height and the BMI and BMI class variables will be calculated automatically."
    })

    let bmi = form.value("weight") / (form.value("height") ** 2)
    form.calc("bmi", "BMI", bmi, {
        suffix: "kg/m²",
        help: ""
    })

    let bmi_cls;
    if (bmi_cls >= 30) {
        bmi_cls = "Obesity"
    } else if (bmi_cls >= 25) {
        bmi_cls = "Overweight"
    } else if (bmi_cls >= 18.5) {
        bmi_cls = "Normal"
    } else if (bmi_cls > 0) {
        bmi_cls = "Underweight"
    }
    form.sameLine(); form.calc("bmi_class", "BMI Class", bmi_cls)
})

form.section("Lifestyle", () => {
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
})
form.sameLine(); form.section("Sleep", () => {
    form.multiCheck("sleep", "Sleep disorder(s)", [
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
