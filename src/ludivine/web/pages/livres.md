<div class="tabbar">
    <a class="active">Livres</a>
    <a href="/videos">Vidéos</a>
</div>

<div class="tab">
    <div class="cardset">
        <div class="card">
            <img src="{{ ASSET ../assets/resources/traiter_la_depression.jpg }}" alt="" />
            <div>
                <p class="reference">Traiter la dépression et les troubles de l'humeur</p>
                <div class="actions">
                    <a href="https://www.dunod.com/sciences-humaines-et-sociales/traiter-depression-et-troubles-humeur-10-cas-pratiques-en-tcc" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/les_troubles_bipolaires.jpg }}" alt="" />
            <div>
                <p class="reference">Les troubles bipolaires : de la cyclothymie au syndrome maniaco-dépressif</p>
                <div class="actions">
                    <a href="https://www.dunod.com/sciences-humaines-et-sociales/troubles-bipolaires-cyclothymie-au-syndrome-maniaco-depressif" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/apprendre_a_soigner_les_depressions.jpg }}" alt="" />
            <div>
                <p class="reference">Apprendre à soigner les dépressions</p>
                <div class="actions">
                    <a href="https://www.dunod.com/sciences-humaines-et-sociales/apprendre-soigner-depressions-avec-therapies-comportementales-et-0" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/humeur_normale_a_la_depression.jpg }}" alt="" />
            <div>
                <p class="reference">De l'humeur normale à la dépression</p>
                <div class="actions">
                    <a href="https://www.deboecksuperieur.com/ouvrage/9782353273546-de-l-humeur-normale-la-depression-en-psychologie-cognitive-neurosciences-et" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/psychoeducation_dans_depression.jpg }}" alt="" />
            <div>
                <p class="reference">Mettre en oeuvre un programme de psychoéducation pour la dépression</p>
                <div class="actions">
                    <a href="https://www.dunod.com/sciences-humaines-et-sociales/mettre-en-oeuvre-un-programme-psychoeducation-pour-depression" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
    </div>
    <button id="randomize" style="display: none;" onclick="app.randomCard('.cardset')"><img src="{{ ASSET ../assets/ui/dice.webp }}" alt="Choix aléatoire" /></button>
</div>

<script>
    let button = document.querySelector('#randomize');
    button.style.display = 'block';
</script>
