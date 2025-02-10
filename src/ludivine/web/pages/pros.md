<div class="tabbar">
    <a href="/videos">Vidéos</a>
    <a href="/livres">Livres</a>
    <a href="/films">Films</a>
    <a href="/musiques">Musiques</a>
    <a href="/pros" class="active">Recos des pros</a>
</div>

<div class="tab">
    <div class="cardset">
        <div class="card">
            <img src="{{ ASSET ../assets/resources/pro_hingrayelhage.jpg }}" alt="" />
            <div>
                <div class="reference">Le trauma comment s'en sortir – Coraline Hingray et Wissam El Hage</div>
                <div class="actions">
                    <a href="https://www.deboecksuperieur.com/ouvrage/9782807329409-le-trauma-comment-s-en-sortir" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/pro_brillon.jpg }}" alt="" />
            <div>
                <div class="reference">Se relever d'un traumatisme – Pascale Brillon</div>
                <div class="actions">
                    <a href="https://editionshomme.groupelivre.com/products/se-relever-dun-traumatisme-1?variant=43708227649793" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/pro_kolk.jpg }}" alt="" />
            <div>
                <div class="reference">Le corps n'oublie rien – Bessel Van Der Kolk</div>
                <div class="actions">
                    <a href="https://www.albin-michel.fr/le-corps-noublie-rien-9782226457486" target="_blank">En savoir plus</a>
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
