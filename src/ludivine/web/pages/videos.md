<div class="banner">
    <div class="title">Ressources</div>
    <div class="intro">
        <p>Vous souhaitez vous informer sur le psychotraumatisme et les thématiques associés ? Vous trouverez sur cette page cinq onglets : <b>livres, articles, films et musique et des numéros utiles (onglet SOS)</b>.
        <p>En cas d'urgence, utilisez le <b>bouton SOS</b> ou rapprochez-vous d'une personne de confiance et/ou d'un professionnel compétent.
    </div>
</div>

<div class="tabbar">
    <a href="/livres">Livres</a>
    <a href="/videos" class="active">Vidéos</a>
</div>

<div class="tab">
    <div class="cardset">
        <div class="card">
            <img src="{{ ASSET ../assets/web/videos/trauma_vicariant.webp }}" alt="" />
            <div>
                <div class="reference">5 choses à savoir sur le traumatisme vicariant - Youtube Cn2r</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=uqbGbUkTEcY" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/web/videos/violences_sexuelles_enfance.webp }}" alt="" />
            <div>
                <div class="reference">Violences sexuelles dans l’enfance et conséquences à l’âge adulte - Youtube Cn2r</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=ZMAn4F-udNM" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/web/videos/blessures_invisibles.webp }}" alt="" />
            <div>
                <div class="reference">Les blessures invisibles #TSPT - Youtube Cn2r</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=pqQdnFVp3z4" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/web/videos/trop_trop_peur.webp }}" alt="" />
            <div>
                <div class="reference">Parler du psychotraumatisme aux enfants : Trop trop peur #TSPT - Youtube Cn2r</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=swVzn-zhkqw" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/web/videos/bloque_bloquee.webp }}" alt="" />
            <div>
                <div class="reference">Bloqué⸱e #TSPT - Youtube Cn2r</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=3BZT3CjelXU" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
    </div>
    <button id="randomize" onclick="app.randomCard('.cardset')"><img src="{{ ASSET ../assets/web/misc/dice.webp }}" alt="Choix aléatoire" /></button>
</div>

<script>
    let button = document.querySelector('#randomize');
    button.style.display = 'block';
</script>
