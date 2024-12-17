<div class="banner">
    <div class="title">Ressources</div>
</div>

<div class="tabbar">
    <a href="/livres">Livres</a>
    <a href="/videos" class="active">Vidéos</a>
    <div style="flex: 1;"></div>
    <a href="https://3114.fr/je-suis-en-souffrance/" onclick="app.sos(event); return false" style="background: #d97069;">SOS</a>
</div>

<div class="tab">
    <div class="cardset">
        <div class="card">
            <img src="{{ ASSET static/videos/trauma_vicariant.webp }}" alt="" />
            <div>
                <div class="title">5 choses à savoir sur le traumatisme vicariant - Youtube Cn2r</div>
                <div class="buttons">
                    <a href="https://www.youtube.com/watch?v=uqbGbUkTEcY" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET static/videos/violences_sexuelles_enfance.webp }}" alt="" />
            <div>
                <div class="title">Violences sexuelles dans l’enfance et conséquences à l’âge adulte - Youtube Cn2r</div>
                <div class="buttons">
                    <a href="https://www.youtube.com/watch?v=ZMAn4F-udNM" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET static/videos/blessures_invisibles.webp }}" alt="" />
            <div>
                <div class="title">Les blessures invisibles #TSPT - Youtube Cn2r</div>
                <div class="buttons">
                    <a href="https://www.youtube.com/watch?v=pqQdnFVp3z4" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET static/videos/trop_trop_peur.webp }}" alt="" />
            <div>
                <div class="title">Parler du psychotraumatisme aux enfants : Trop trop peur #TSPT - Youtube Cn2r</div>
                <div class="buttons">
                    <a href="https://www.youtube.com/watch?v=swVzn-zhkqw" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET static/videos/bloque_bloquee.webp }}" alt="" />
            <div>
                <div class="title">Bloqué⸱e #TSPT - Youtube Cn2r</div>
                <div class="buttons">
                    <a href="https://www.youtube.com/watch?v=3BZT3CjelXU" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
    </div>
    <button onclick="app.randomCard('.cardset')"><img src="{{ ASSET static/misc/dice.webp }}" alt="Choix aléatoire" /></button>
</div>
