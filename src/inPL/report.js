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

    function refreshConsultant(data)
    {
        let report = query('#inpl_report');

        report.replaceContent(
            html('h1', 'Identité'),
            data ? html('div',
                html('table',
                    createRow('Nom', data.cs_nom),
                    createRow('Prénom', data.cs_prenom),
                    createRow('Sexe', {M: 'Homme', F: 'Femme'}[data.cs_sexe]),
                    createRow('Date de naissance', data.cs_date_naissance)
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

    this.refreshReport = function(rows) {
        let plid = query('#inpl_option_plid').value;
        let row = rows.find(row => row.plid == plid);

        refreshConsultant(row);
    };

    return this;
}).call({});
