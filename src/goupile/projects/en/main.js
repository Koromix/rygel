// Enable per-variable annotaton feature
app.annotate = true

app.form("project", "Project title", () => {
    // This files defiens a project with 3 pages

    app.form("intro", "Introduction")
    app.form("advanced", "Advanced")
    app.form("layout", "Page layout")
})
