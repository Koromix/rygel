<div class="banner">
    <div class="title">Espace détente</div>
</div>

<div class="cards">
    <div class="card" style="flex: 8;">
        <img src="{{ ASSET static/detente/coherence_cardiaque.webp }}" alt="" />
        <div>
            <p class="title">Cohérence cardiaque</p>
            <p>La cohérence cardiaque est une <b>technique de respiration simple</b>, mais très efficace, visant à apaiser le corps et l’esprit <b>par le contrôle du rythme cardiaque</b>.</p>
            <div class="buttons">
                <a href="/coherence">Commencer</a>
            </div>
        </div>
    </div>
    <div class="card" style="flex: 7;">
        <img src="{{ ASSET static/detente/etirements_doux.webp }}" alt="" />
        <div>
            <p class="title">Étirements doux</p>
            <p>Ces exercices simples peuvent être réalisés <b>en toute autonomie</b> et apportent une <b>sensation de légèreté et de bien-être</b>.</p>
            <div class="buttons">
                <a class="disabled">Non disponible</a>
            </div>
        </div>
    </div>
    <div class="card" style="flex: 5;">
        <img src="{{ ASSET static/detente/jeu_snake.webp }}" alt="" />
        <div>
            <p class="title">Jouez à un jeu</p>
            <p>Lancez une partie pour vous changer les idées !<br><br></p>
            <div class="buttons">
                <a href="https://koromix.dev/test/staks">Jouer</a>
            </div>
        </div>
    </div>
    <div class="card" style="flex: 8;">
        <img src="{{ ASSET static/detente/ecriture_expressive.webp }}" alt="" />
        <div>
            <p class="title">Écrivez dans votre journal</p>
            <p>L'écriture expressive est un exercice qui consiste à <b>écrire librement ses pensées</b>, émotions, ou ressentis, sans se soucier de la structure ou de la forme.</p>
            <div class="buttons">
                <a class="disabled">Connectez-vous</a>
            </div>
        </div>
    </div>
</div>

<style>
    .cards {
        display: flex;
        flex-wrap: wrap;
        gap: 1em;
    }
    .card {
        display: flex;
        position: relative;
        min-width: 40%;
        align-items: end;
    }
    .card > img {
        position: absolute;
        left: 0;
        top: 0;
        width: 100%;
        height: 100%;
        object-fit: cover;
        border-radius: 16px;
    }
    .card > div {
        width: 100%;
        margin-top: 150px;
        padding: 1em;
        z-index: 2;
        background: #ffffff88;
        backdrop-filter: blur(16px);
        border-radius: 0 0 16px 16px;
    }
    .card .title {
        color: #364b9b;
        text-transform: uppercase;
        font-size: 1.1em;
        font-weight: bold;
    }
</style>
