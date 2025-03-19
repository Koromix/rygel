<div class="banner">
    <div class="title">Ressources</div>
    <div class="intro">
        <p>Vous souhaitez vous informer sur le psychotraumatisme et les thématiques associés ?
        <p>Vous trouverez sur cette page cinq onglets : <b>vidéos, livres, films, musiques, et des recommandations de professionnels</b>.
        <p>N'hésitez pas à parcourir le <a href="https://cn2r.fr" target="_blank">site du Cn2r</a> pour découvrir davantage de ressources.
    </div>
</div>

<div class="tabbar">
    <a href="/videos" class="active" title="Vidéos"><img src="{{ ASSET ../assets/ui/video.png }}" alt="Vidéos" /></a>
    <a href="/livres" title="Livres"><img src="{{ ASSET ../assets/ui/book.png }}" alt="Livres" /></a>
    <a href="/films" title="Films"><img src="{{ ASSET ../assets/ui/movie.png }}" alt="Films" /></a>
    <a href="/musiques" title="Musiques"><img src="{{ ASSET ../assets/ui/music.png }}" alt="Musiques" /></a>
    <a href="/pros" title="Recos des pros"><img src="{{ ASSET ../assets/ui/paper.png }}" alt="Recos des pros" /></a>
</div>

<div class="tab">
    <div class="cardset">
        <div class="card">
            <img src="{{ ASSET ../assets/resources/video_cn2r.jpg }}" alt="" />
            <div>
                <div class="reference">Les blessures invisibles – Cn2r</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=LCAv1ru9P8Q" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/video_legriguer.jpg }}" alt="" />
            <div>
                <div class="reference">Les thérapies pour les victimes de violences sexuelles – Cn2r</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=oQzIev3u5QQ&t=5s" target="_blank">Voir la vidéo</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/video_fenvac.jpg }}" alt="" />
            <div>
                <div class="reference">Le risque suicidaire – FENVAC</div>
                <div class="actions">
                    <a href="https://www.youtube.com/watch?v=OJUJyt8B9jU&list=PL5r6RtmFqyWRLJY2h5S3_SUaLe6mZXDe0&index=20" target="_blank">Voir la vidéo</a>
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
