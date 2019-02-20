// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let report = (function() {
    let self = this;

    function createRow(title, value)
    {
        return html('tr',
            html('th', title),
            html('td', value)
        );
    }

    this.refreshReport = function(rows) {
        let plid = query('#inpl_option_plid').value;
        let row = rows.find(row => row.rdv_plid == plid);

        let report = query('#inpl_report');

        report.replaceContent(
            html('h1', 'Identité'),
            row ? html('div',
                html('table',
                    createRow('Nom', row.consultant_nom),
                    createRow('Prénom', row.consultant_prenom),
                    createRow('Sexe', {M: 'Homme', F: 'Femme'}[row.consultant_sexe]),
                    createRow('Date de naissance', row.consultant_date_naissance)
                )
            ) : null,
            html('h1', 'Biologie médicale'),
            html('h1', 'Examen général'),
            html('h1', 'Électrocardiogramme (ECG)'),
            html('h1', 'Examen de la vision'),
            html('h1', 'Examen de l\'audition'),
            html('h1', 'Examen de spirométrie'),
            html('h1', 'Examen de densitométrie osseuse'),
            html('h1', 'Entretien avec neuro-psychologue'),
            html('h1', 'Entretien avec nutritionniste'),
            html('h1', 'Entretien avec EMS')
        );
    };

    this.generateDocument = function(rows) {
        let file = document.querySelector('#inpl_option_template').files[0];
        let plid = query('#inpl_option_plid').value;
        let row = rows.find(row => row.rdv_plid == plid);

        if (file && row) {
            let reader = new FileReader();

            reader.onload = function() {
                let zip = new JSZip(reader.result);
                let doc = new Docxtemplater().loadZip(zip);

                try {
                    doc.setOptions({
                        parser: function(tag) {
                            tag = tag.replace(/[’‘]/g, "'");
                            return { get: (it) => eval(tag) };
                        }
                    });
                    doc.setData(row);
                    doc.render();

                    let out = doc.getZip().generate({
                        type: 'blob',
                        mimeType: 'application/vnd.openxmlformats-officedocument.wordprocessingml.document'
                    });

                    saveBlob(out, `CR_${row.rdv_plid}.docx`);
                } catch (err) {
                    alert(err.message);
                }
            };

            reader.readAsBinaryString(file);
        } else if (row) {
            alert('You must specify a template document');
        }
    };

    return this;
}).call({});
