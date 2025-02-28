<div class="banner">
    <div class="title">Ressources</div>
    <div class="intro">
        <p>Vous souhaitez vous informer sur le psychotraumatisme et les thématiques associés ?
        <p>Vous trouverez sur cette page cinq onglets : <b>vidéos, livres, films, musiques, et des recommandations de professionnels</b>.
    </div>
</div>

<div class="tabbar">
    <a href="/videos" title="Vidéos"><img src="{{ ASSET ../assets/ui/video.png }}" alt="Vidéos" /></a>
    <a href="/livres" class="active" title="Livres"><img src="{{ ASSET ../assets/ui/book.png }}" alt="Livres" /></a>
    <a href="/films" title="Films"><img src="{{ ASSET ../assets/ui/movie.png }}" alt="Films" /></a>
    <a href="/musiques" title="Musiques"><img src="{{ ASSET ../assets/ui/music.png }}" alt="Musiques" /></a>
    <a href="/pros" title="Recos des pros"><img src="{{ ASSET ../assets/ui/paper.png }}" alt="Recos des pros" /></a>
</div>

<div class="tab">
    <div class="cardset">
        <div class="card">
            <img src="{{ ASSET ../assets/resources/livre_jamoulle.jpg }}" alt="" />
            <div>
                <p class="reference">Je n'existais plus – Pascale Jamoulle</p>
                <div class="actions">
                    <a href="https://www.editionsladecouverte.fr/je_n_existais_plus-9782348065101" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/livre_lambda.jpg }}" alt="" />
            <div>
                <p class="reference">Tant pis pour l'amour – Sophie Lambda</p>
                <div class="actions">
                    <a href="https://www.editions-delcourt.fr/bd/series/serie-tant-pis-pour-l-amour-ou-comment-j-ai-survecu-un-manipulateur/album-tant-pis-pour-l-amour-ou-comment-j-ai-survecu-un-manipulateur" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/livre_delacourt.jpg }}" alt="" />
            <div>
                <p class="reference">L'enfant réparé – Grégoire Delacourt</p>
                <div class="actions">
                    <a href="https://www.grasset.fr/livre/lenfant-repare-9782246828846/" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/livre_vantran.jpg }}" alt="" />
            <div>
                <p class="reference">La thérapie de la dernière chance – Sophie Tran Van</p>
                <div class="actions">
                    <a href="https://www.odilejacob.fr/catalogue/psychologie/psychotherapie/therapie-de-la-derniere-chance_9782738149213.php" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/livre_cabrel.jpg }}" alt="" />
            <div>
                <p class="reference">Boza – Ulrich Cabrel et Etienne Longueville</p>
                <div class="actions">
                    <a href="https://www.jailu.com/boza/9782290239322" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
        <div class="card">
            <img src="{{ ASSET ../assets/resources/livre_alloinguillon.jpg }}" alt="" />
            <div>
                <p class="reference">Dans la secte – Louis Alloing et Patrice Guillon</p>
                <div class="actions">
                    <a href="https://www.la-boite-a-bulles.com/serie/11" target="_blank">En savoir plus</a>
                </div>
            </div>
        </div>
    </div>
    <button id="randomize" style="display: none;" onclick="app.randomCard('.cardset')"><img src="{{ ASSET ../assets/ui/dice.webp }}" alt="Choix aléatoire" title="Choix aléatoire" /></button>
</div>

<script>
    let button = document.querySelector('#randomize');
    button.style.display = 'block';
</script>
