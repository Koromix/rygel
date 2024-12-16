<div class="banner">
    <div class="title">Ressources</div>
</div>

<div class="cards">
    <div class="card">
        <img src="{{ ASSET static/livres/traiter_la_depression.jpg }}" alt="" />
        <div>
            <div class="title">Traiter la dépression et les troubles de l'humeur</div>
            <div class="buttons">
                <a href="https://www.dunod.com/sciences-humaines-et-sociales/traiter-depression-et-troubles-humeur-10-cas-pratiques-en-tcc" target="_blank">En savoir plus</a>
            </div>
        </div>
    </div>
    <div class="card">
        <img src="{{ ASSET static/livres/les_troubles_bipolaires.jpg }}" alt="" />
        <div>
            <div class="title">Les troubles bipolaires : de la cyclothymie au syndrome maniaco-dépressif</div>
            <div class="buttons">
                <a href="https://www.dunod.com/sciences-humaines-et-sociales/troubles-bipolaires-cyclothymie-au-syndrome-maniaco-depressif" target="_blank">En savoir plus</a>
            </div>
        </div>
    </div>
    <div class="card">
        <img src="{{ ASSET static/livres/apprendre_a_soigner_les_depressions.jpg }}" alt="" />
        <div>
            <div class="title">Apprendre à soigner les dépressions</div>
            <div class="buttons">
                <a href="https://www.dunod.com/sciences-humaines-et-sociales/apprendre-soigner-depressions-avec-therapies-comportementales-et-0" target="_blank">En savoir plus</a>
            </div>
        </div>
    </div>
    <div class="card">
        <img src="{{ ASSET static/livres/humeur_normale_a_la_depression.jpg }}" alt="" />
        <div>
            <div class="title">De l'humeur normale à la dépression</div>
            <div class="buttons">
                <a href="https://www.deboecksuperieur.com/ouvrage/9782353273546-de-l-humeur-normale-la-depression-en-psychologie-cognitive-neurosciences-et" target="_blank">En savoir plus</a>
            </div>
        </div>
    </div>
    <div class="card">
        <img src="{{ ASSET static/livres/psychoeducation_dans_depression.jpg }}" alt="" />
        <div>
            <div class="title">Mettre en oeuvre un programme de psychoéducation pour la dépression</div>
            <div class="buttons">
                <a href="https://www.dunod.com/sciences-humaines-et-sociales/mettre-en-oeuvre-un-programme-psychoeducation-pour-depression" target="_blank">En savoir plus</a>
            </div>
        </div>
    </div>
</div>

<style>
    .cards {
        display: flex;
        flex-wrap: wrap;
        gap: 1em;
        justify-content: center;
        align-items: center;
    }
    .card {
        display: flex;
        position: relative;
        width: 400px;
        height: 400px;
        align-items: end;
        overflow: hidden;
        border-radius: 16px;
        user-select: none;
    }
    .card > img {
        position: absolute;
        left: 0;
        top: 0;
        width: 100%;
    }
    .card > div {
        width: 100%;
        margin-top: 150px;
        padding: 1em;
        z-index: 2;
        background: #ffffff88;
        backdrop-filter: blur(16px);
    }
    .card .title {
        color: #364b9b;
        font-weight: bold;
    }

    .js .cards { height: 400px; }
    .js .card {
        --index: calc(max(-1 * var(--position), var(--position)));

        position: absolute;
        margin-left: calc(var(--position) * 15%);
        filter: blur(calc(var(--index) * 2px));
        transform: scaleY(calc(1 - 0.05 * var(--index)));
        z-index: calc(10 - var(--index));
        transition: all 0.2s ease-in;
    }
    .js .card:not(.active) { cursor: pointer; }
</style>

<script>
    let root = document.querySelector('.cards');
    let cards = Array.from(root.querySelectorAll('.card'));

    function toggle(active) {
        let left = -Math.floor((cards.length - 1) / 2);
        let right = Math.floor(cards.length / 2);

        for (let i = -1; i >= left; i--) {
            let idx = active + i;
            if (idx < 0)
                idx = cards.length + idx;
            cards[idx].style.setProperty('--position', i);
            cards[idx].classList.toggle('active', i == 0);
        }
        for (let i = 0; i <= right; i++) {
            let idx = (active + i) % cards.length;
            cards[idx].style.setProperty('--position', i);
            cards[idx].classList.toggle('active', i == 0);
        }
    }

    for (let i = 0; i < cards.length; i++)
        cards[i].addEventListener('click', () => toggle(i));

    let middle = Math.floor((cards.length - 1) / 2);
    toggle(middle);
</script>
