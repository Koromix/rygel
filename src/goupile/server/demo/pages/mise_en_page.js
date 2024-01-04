form.section("Tableau de nombres", () => {
    form.number("a1", "Variable A1")
    form.sameLine(true); form.number("b1", "Variable B1")
    form.sameLine(true); form.number("c1", "Variable C1")

    form.number("a2", "Variable A2")
    form.sameLine(true); form.number("b2", "Variable B2")
    form.sameLine(true); form.number("c2", "Variable C2")

    form.number("a3", "Variable A3")
    form.sameLine(true); form.number("b3", "Variable B3")
    form.sameLine(true); form.number("c3", "Variable C3")
})

form.section("Tableau de variables oui/non", () => {
    form.binary("d1", "Variable D1")
    form.sameLine(true); form.binary("e1", "Variable E1")
    form.sameLine(true); form.binary("f1", "Variable F1")
    form.sameLine(true); form.binary("g1", "Variable G1")

    form.binary("d2", "Variable D2")
    form.sameLine(true); form.binary("e2", "Variable E2")
    form.sameLine(true); form.binary("f2", "Variable F2")
    form.sameLine(true); form.binary("g2", "Variable G2")

    form.binary("d3", "Variable D3")
    form.sameLine(true); form.binary("e3", "Variable E3")
    form.sameLine(true); form.binary("f3", "Variable F3")
    form.sameLine(true); form.binary("g3", "Variable G3")
})

form.section("Tableau de variables énumérées", () => {
    form.enum("h1", "Variable H1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enum("h2", "Variable H2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])

    form.enum("i1", "Variable I1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enum("i2", "Variable I2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])

    form.enum("j1", "Variable J1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enum("j2", "Variable J2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
})

form.section("Tableau de listes déroulantes", () => {
    form.enumDrop("k1", "Variable K1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enumDrop("k2", "Variable K2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enumDrop("k3", "Variable K3", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])

    form.enumDrop("l1", "Variable L1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enumDrop("l2", "Variable L2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enumDrop("l3", "Variable L3", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])

    form.enumDrop("m1", "Variable M1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enumDrop("m2", "Variable M2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    form.sameLine(true); form.enumDrop("m3", "Variable M3", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
})
