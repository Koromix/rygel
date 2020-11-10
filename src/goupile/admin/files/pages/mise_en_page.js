page.section("Tableau de nombres", () => {
    page.number("a1", "Variable A1")
    page.sameLine(true); page.number("b1", "Variable B1")
    page.sameLine(true); page.number("c1", "Variable C1")

    page.number("a2", "Variable A2")
    page.sameLine(true); page.number("b2", "Variable B2")
    page.sameLine(true); page.number("c2", "Variable C2")

    page.number("a3", "Variable A3")
    page.sameLine(true); page.number("b3", "Variable B3")
    page.sameLine(true); page.number("c3", "Variable C3")
})

page.section("Tableau de variables oui/non", () => {
    page.binary("d1", "Variable D1")
    page.sameLine(true); page.binary("e1", "Variable E1")
    page.sameLine(true); page.binary("f1", "Variable F1")
    page.sameLine(true); page.binary("g1", "Variable G1")

    page.binary("d2", "Variable D2")
    page.sameLine(true); page.binary("e2", "Variable E2")
    page.sameLine(true); page.binary("f2", "Variable F2")
    page.sameLine(true); page.binary("g2", "Variable G2")

    page.binary("d3", "Variable D3")
    page.sameLine(true); page.binary("e3", "Variable E3")
    page.sameLine(true); page.binary("f3", "Variable F3")
    page.sameLine(true); page.binary("g3", "Variable G3")
})

page.section("Tableau de variables énumérées", () => {
    page.enum("h1", "Variable H1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enum("h2", "Variable H2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])

    page.enum("i1", "Variable I1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enum("i2", "Variable I2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])

    page.enum("j1", "Variable J1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enum("j2", "Variable J2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
})

page.section("Tableau de listes déroulantes", () => {
    page.enumDrop("k1", "Variable K1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enumDrop("k2", "Variable K2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enumDrop("k3", "Variable K3", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])

    page.enumDrop("l1", "Variable L1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enumDrop("l2", "Variable L2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enumDrop("l3", "Variable L3", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])

    page.enumDrop("m1", "Variable M1", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enumDrop("m2", "Variable M2", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
    page.sameLine(true); page.enumDrop("m3", "Variable M3", [
        [1, "Choix 1"],
        [2, "Choix 2"],
        [3, "Choix 3"]
    ])
})
