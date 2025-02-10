<div class="tabbar">
    <a href="/videos">Vidéos</a>
    <a href="/livres">Livres</a>
    <a href="/films" class="active">Films</a>
    <a href="/musiques">Musiques</a>
    <a href="/pros">Recos des pros</a>
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
