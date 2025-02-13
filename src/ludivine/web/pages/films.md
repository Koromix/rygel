<div class="banner">
    <div class="title">Ressources</div>
    <div class="intro">
        <p>Vous souhaitez vous informer sur le psychotraumatisme et les thématiques associés ? Vous trouverez sur cette page cinq onglets : <b>vidéos, livres, films, musiques, et des recommandations de professionnels</b>.
    </div>
</div>

<div class="tabbar">
    <a href="/videos"><img src="{{ ASSET ../assets/ui/video.png }}" alt="Vidéos" /></a>
    <a href="/livres"><img src="{{ ASSET ../assets/ui/book.png }}" alt="Livres" /></a>
    <a href="/films" class="active"><img src="{{ ASSET ../assets/ui/movie.png }}" alt="Films" /></a>
    <a href="/musiques"><img src="{{ ASSET ../assets/ui/music.png }}" alt="Musiques" /></a>
    <a href="/pros"><img src="{{ ASSET ../assets/ui/paper.png }}" alt="Recos des pros" /></a>
</div>

<div class="tab">
    <div class="cardset">
        <div class="card">
            <img src="{{ ASSET ../assets/resources/film_scherfig.jpg }}" alt="" />
            <div>
                <div class="reference">The kindness of strangers – Lone Scherfig</div>
                <div class="actions">
                    <a href="https://www.allocine.fr/film/fichefilm_gen_cfilm=270236.html" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/film_suco.jpg }}" alt="" />
            <div>
                <div class="reference">Les éblouis – Sarah Suco</div>
                <div class="actions">
                    <a href="https://www.allocine.fr/film/fichefilm_gen_cfilm=258262.html" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/film_bescond.jpg }}" alt="" />
            <div>
                <div class="reference">Les chatouilles – Andréa Bescond et Eric Metayer</div>
                <div class="actions">
                    <a href="https://www.allocine.fr/film/fichefilm_gen_cfilm=256702.html" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/film_ozon.jpg }}" alt="" />
            <div>
                <div class="reference">Grâce à Dieu – François Ozon</div>
                <div class="actions">
                    <a href="https://www.allocine.fr/film/fichefilm_gen_cfilm=263132.html" target="_blank">En savoir plus</a>
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
