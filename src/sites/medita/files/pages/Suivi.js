function addChart(id, echelles) {
    let colors = ['#236b20', '#24579d', 'rgb(187, 35, 35)', 'black'];
    let canvas = scratch.canvas;

    if (!canvas) {
        canvas = document.createElement('canvas');
        scratch.canvas = canvas;
    }

    let p = Promise.all([
        window.Chart || util.loadScript(`${env.base_url}files/resources/chart.bundle.min.js`),
        ...echelles.map(echelle => virt_data.loadAll(echelle.form))
    ]);

    p.then(tables => {
        // Remove parasite script thingy
        tables.shift();

        let chart = scratch.chart;

        if (!chart) {
            chart = new Chart(canvas.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: []
                },
                options: {
                    legend: {
                        position: 'bottom',
                        align: 'start'
                    },
                    scales: {
                        xAxes: [{
                            type: 'time',
                            ticks: {
                                // XXX: Fix chart X-axis date format
                                source: 'data'
                            }
                        }],
                        yAxes: [{type: 'linear'}]
                    }
                }
            });
            scratch.chart = chart;
        }
        chart.data.datasets.length = 0;

        for (let i = 0; i < tables.length; i++) {
            let records = tables[i].filter(record => record.values.id === id);
            let echelle = echelles[i];
            let keys = echelle.keys;

            if (records.length) {
                for (let j = 0; j < keys.length; j++) {
                    let dataset = {
                        label: keys[j] === 'score' ? echelle.name : `${echelle.name} [${keys[j]}]`,
                        borderColor: colors[j % colors.length],
                        fill: false,
                        pointBorderColor: [],
                        pointBackgroundColor:[],
                        data: []
                    };
                    chart.data.datasets.push(dataset);

                    for (let record of records) {
                        dataset.pointBorderColor.push(colors[j % colors.length]);
                        dataset.pointBackgroundColor.push(colors[j % colors.length]);

                        dataset.data.push({
                            x: new Date(util.decodeULIDTime(record.id)),
                            y: record.values[keys[j]]
                        });
                    }
                }
            }
        }

        canvas.style.display = chart.data.datasets.length ? 'block' : 'none';
        chart.update({duration: 0});
    });

    page.output(canvas);
}

data.makeFormHeader("Suivi d'échelles", page)
page.pushOptions({compact: true})

let id = form.find("id");

// Sélecteur d'échelles
{
    let tsel = new TreeSelector;

    tsel.clickHandler = (e, values) => {
        scratch.echelles = values;
        page.changeHandler(page);
    };
    tsel.setPrefix('Échelles : ');

    for (let i = 0; i < data.echelles.length; i++) {
        let echelle = data.echelles[i];
        tsel.addOption([echelle.category, echelle.name], echelle.form,
                       {selected: !scratch.echelles || scratch.echelles.has(echelle.form)});
    }

    page.output(tsel.render());
}

if (id.value) {
    let echelles = scratch.echelles ? data.echelles.filter(echelle => scratch.echelles.has(echelle.form)) : data.echelles;
    addChart(id.value, echelles);
}

form.output(html`
    <br/>
    <button class="md_button" @click=${e => go("Formulaires")}>Formulaires</button>
    <button class="md_button" @click=${e => go("Accueil")}>Retour à l'accueil</button>
`)
data.makeFooter(page)
