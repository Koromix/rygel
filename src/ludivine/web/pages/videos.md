<div class="tabbar">
    <a href="/videos" class="active">Vidéos</a>
    <a href="/livres">Livres</a>
    <a href="/films">Films</a>
    <a href="/musiques">Musiques</a>
    <a href="/pros">Recos des pros</a>
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
    <button id="randomize" onclick="app.randomCard('.cardset')"><img src="{{ ASSET ../assets/ui/dice.webp }}" alt="Choix aléatoire" /></button>
</div>

<script>
    let button = document.querySelector('#randomize');
    button.style.display = 'block';
</script>
