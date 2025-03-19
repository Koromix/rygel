<div class="banner">
    <div class="title">Ressources</div>
    <div class="intro">
        <p>Vous souhaitez vous informer sur le psychotraumatisme et les thématiques associés ?
        <p>Vous trouverez sur cette page cinq onglets : <b>vidéos, livres, films, musiques, et des recommandations de professionnels</b>.
        <p>N'hésitez pas à parcourir le <a href="https://cn2r.fr" target="_blank">site du Cn2r</a> pour découvrir davantage de ressources.
    </div>
</div>

<div class="tabbar">
    <a href="/videos" title="Vidéos"><img src="{{ ASSET ../assets/ui/video.png }}" alt="Vidéos" /></a>
    <a href="/livres" title="Livres"><img src="{{ ASSET ../assets/ui/book.png }}" alt="Livres" /></a>
    <a href="/films" title="Films"><img src="{{ ASSET ../assets/ui/movie.png }}" alt="Films" /></a>
    <a href="/musiques" class="active" title="Musiques"><img src="{{ ASSET ../assets/ui/music.png }}" alt="Musiques" /></a>
    <a href="/pros" title="Recos des pros"><img src="{{ ASSET ../assets/ui/paper.png }}" alt="Recos des pros" /></a>
</div>

<div class="tab">
    <div class="cardset">
        <div class="card">
            <img src="{{ ASSET ../assets/resources/musique_blueoctober.jpg }}" alt="" />
            <div>
                <div class="reference">Fear - Blue October</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=Q3b0-i1T8Hk" target="_blank">Écouter sur Youtube</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/musique_gherbo.jpg }}" alt="" />
            <div>
                <div class="reference">PTSD – G Herbo</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=k3-fAXbCa44" target="_blank">Écouter sur Youtube</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/musique_jamesarthur.jpg }}" alt="" />
            <div>
                <div class="reference">Recovery – James Arthur</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=m9DO3zpdWqw" target="_blank">Écouter sur Youtube</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/musique_thecranberries.jpg }}" alt="" />
            <div>
                <div class="reference">Zombie - The Cranberries</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=6Ejga4kJUts" target="_blank">Écouter sur Youtube</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/musique_ladygaga.jpg }}" alt="" />
            <div>
                <div class="reference">Til it happens to you – Lady Gaga</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=ZmWBrN7QV6Y" target="_blank">Écouter sur Youtube</a>
                </div>
            </div>
        </div>
    </div>
    <button id="randomize" onclick="app.randomCard('.cardset')"><img src="{{ ASSET ../assets/ui/dice.webp }}" alt="Choix aléatoire" title="Choix aléatoire" /></button>
</div>

<script>
    let button = document.querySelector('#randomize');
    button.style.display = 'block';
</script>
