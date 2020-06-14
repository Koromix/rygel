data.makeHeader("Suivi d'échelles", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

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

// Graphique et tableaux
if (id.value) {
    let forms = new Set(scratch.echelles || data.echelles.map(echelle => echelle.form));
    let echelles = data.echelles.filter(echelle => forms.delete(echelle.form));

    let p = Promise.all([
        window.Chart || net.loadScript(`${env.base_url}files/resources/chart.bundle.min.js`),
        ...echelles.map(echelle => virt_data.loadAll(echelle.form))
    ]);

    scratch.canvas = scratch.canvas || document.createElement('canvas');
    scratch.div = scratch.div || document.createElement('div');
    page.output(scratch.canvas);
    page.output(scratch.div);

    p.then(tables => {
        // Remove parasite script thingy
        tables.shift();
        tables = tables.map(table => table.filter(record => record.values.id === id.value));

        updateChart(echelles, tables);
        updateTables(echelles, tables);
    });
}

function updateChart(echelles, tables) {
    let canvas = scratch.canvas;
    let chart = scratch.chart;

    let colors = ['#236b20', '#24579d', 'rgb(187, 35, 35)', 'black'];

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

                        time: {
                            unit: 'day',
                            unitStepSize: 1,
                            displayFormats: {
                               day: 'DD/MM HH:mm'
                            }
                        },

                        ticks: {
                            source: 'data',
                            minRotation: 90,
                            maxRotation: 90
                        }
                    }],
                    yAxes: [{type: 'linear'}]
                },
                onClick: function(x, e) { console.log(x, e) }
            }
        });
        scratch.chart = chart;
    }
    chart.data.datasets.length = 0;

    for (let i = 0; i < echelles.length; i++) {
        let echelle = echelles[i];
        let keys = echelle.keys;
        let records = tables[i];

        if (records.length) {
            for (let j = 0; j < keys.length; j++) {
                let dataset = {
                    label: keys[j] === 'score' ? echelle.name : `${echelle.name} [${keys[j]}]`,
                    borderColor: colors[i % colors.length],
                    fill: false,
                    lineTension: 0.1,
                    borderDash: j ? [5, 15] : [],
                    pointBorderColor: [],
                    pointBackgroundColor:[],
                    data: []
                };
                chart.data.datasets.push(dataset);

                for (let record of records) {
                    dataset.pointBorderColor.push(colors[i % colors.length]);
                    dataset.pointBackgroundColor.push(colors[i % colors.length]);

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
}

function updateTables(echelles, tables) {
    let div = scratch.div;

    render(echelles.map((echelle, idx) => {
        let records = tables[idx];
        let keys = echelle.keys;

        if (records.length) {
            return html`
                <fieldset class="af_container af_section">
                    <legend>${echelle.name}</legend>
                    <table style="width: 100%; table-layout: fixed;">
                        <thead>
                            <tr>
                                <th style="text-align: left;">Date</th>
                                ${keys.map(key => html`<th style="text-align: left;">${key}</th>`)}
                                <th style="width: 200px;"></th>
                            </tr>
                        </thead>
                        <tbody>${records.map(record => {
                            let date = new Date(util.decodeULIDTime(record.id));
                            let values = keys.map(key => record.values[key]);

                            return html`<tr>
                                <td>${date.toLocaleString()}</td>
                                ${values.map(value => html`<td>${value}</td>`)}
                                <td style="text-align: right;">
                                    <a href=${nav.link(echelle.form, null, record)}>Modifier</a> |
                                    <a href="#" @click=${e => {showDeleteDialog(e, echelle.form, record.id); e.preventDefault();}}>Supprimer</a>
                                </td>
                            </tr>`;
                        })}</tbody>
                    </table>
                </fieldset>
            `;
        } else {
            return '';
        }
    }), div);
}

function showDeleteDialog(e, form, id) {
    ui.popup(e, popup => {
        popup.output('Voulez-vous vraiment supprimer cet enregistrement ?');

        popup.submitHandler = async () => {
            popup.close();

            await virt_data.delete(form, id);
            page.restart();
        };
        popup.buttons(popup.buttons.std.ok_cancel('Supprimer'));

    });
}

form.output(html`
    <br/>
    <button class="md_button" @click=${e => go("Formulaires")}>Formulaires</button>
    <button class="md_button" @click=${e => go("Accueil")}>Retour à l'accueil</button>
`)
