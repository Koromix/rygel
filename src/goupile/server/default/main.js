// Permettre d'annoter chaque variable définit dans les formulaires du projet
app.annotate = true;

app.form("projet", "Titre du projet", () => {
    // Ce fichier définit un formulaire composé de 3 pages

    app.form("introduction", "Introduction")
    app.form("avance", "Avancé")
    app.form("mise_en_page", "Mise en page")
})
