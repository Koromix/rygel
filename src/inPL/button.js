// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// This is the code directly executed when a user clicks on the "Générer un CR"
// button in Voozanoo. The code is directly embedded in the input tag in
// base64-encoded form:
//
// <input onclick="javascript:eval(atob('BASE64_CODE'));" value="Générer un CR"/>
//
// Yeah, this is beyond ugly. Voozanoo is complete crap.

function loadScript(url, func)
{
    let head = document.getElementsByTagName('head')[0];
    let script = document.createElement('script');

    script.type = 'text/javascript';
    script.src = url;
    script.onreadystatechange = func;
    script.onload = func;

    head.appendChild(script);
}

function generateCR(id)
{
    let script_url = 'https://koromix.dev/inPL/inPL.pk.js';
    let template_url = 'https://koromix.dev/inPL/template.docx';

    if (typeof bridge === 'undefined') {
        loadScript(script_url, () => generateCR(id));
    } else {
        fetch(template_url).then(response => response.blob().then(blob => {
            bridge.importFromVZV(id, row => report.generate(row, blob));
        }));
    }
}

let id = parseInt(document.querySelector('#inpl_id_rdv > div > span > span').innerHTML, 10);
generateCR(id);
