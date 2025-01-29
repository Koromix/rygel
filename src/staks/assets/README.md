# Assets

## Backgrounds

* `backgrounds/aurora.webp`: Aurora Night by psiipilehto: https://www.deviantart.com/psiipilehto/art/Aurora-Night-608184628
* `backgrounds/winter.webp`: Winter scenery by psiipilehto: https://www.deviantart.com/psiipilehto/art/winter-scenery-346519787
* `backgrounds/somewhere.webp`: Somewhere by psiipilehto: https://www.deviantart.com/psiipilehto/art/Somewhere-269008123

## Soundtracks

* `musics/abstract_piano_ambient.webm`: Abstract Piano Ambient by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/tomorrow/abstract-piano-ambient/
* `musics/ambient_future_tech.webm`: Ambient Future Tech by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/sci-fi-times/ambient-future-tech/
* `musics/architecture.webm`: Architecture by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/motivational-style/architecture-1/
* `musics/medical.webm`: Medical by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/sci-fi-times/medical-2/
* `musics/motivational_day.webm`: Motivational Day by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/motivational-style/motivational-day-1/
* `musics/motivational_stylish_background.webm`: Motivational Stylish Background by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/pop-inspiration/motivational-stylish-background/
* `musics/serious_corporate.webm`: Serious Corporate by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/corporate/serious-corporate/
* `musics/summer_beauty.webm`: Summer Beauty by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/life/summer-beauty/
* `musics/summer_every_day.webm`: Summer Every Day by AudioCoffee: https://freemusicarchive.org/music/audiocoffee/pop-inspiration/summer-every-day/
* `musics/tech_corporation.webm`: Tech Corporation by AudioCoffee: https://freemusicarchive.org/music/becouse-heart/technology-1/tech-corporation

## SFX

* `sounds/clear.webm`: powerup by BananaMilkshake: https://freesound.org/people/BananaMilkshake/sounds/632702/
* `sounds/gameover.webm`: game over by Leszek_Szary: https://freesound.org/people/Leszek_Szary/sounds/133283/
* `sounds/hold.webm`: Notification Sound 1 by deadrobotmusic: https://freesound.org/people/deadrobotmusic/sounds/750607/
* `sounds/levelup.webm`: You find a treasure by xkeril: https://freesound.org/people/xkeril/sounds/632661/
* `sounds/lock.webm`: Blip 1 by HenKonen: https://freesound.org/people/HenKonen/sounds/757175/
* `sounds/move.webm`: Simple GUI click by qubodup: https://freesound.org/people/qubodup/sounds/159697/

# Commands

## Sound conversion

* WAV volume adjustment: `sox -v FACTOR input.wav output.wav`
* Music opus encoding: `opusenc --music --discard-comments --discard-pictures input.wav output.opus`
* Sound opus encoding: `opusenc --downmix-mono --discard-comments --discard-pictures input.wav output.opus`
* Convert Ogg (Opus) container to WebM: `ffmpeg -i input.opus -acodec copy output.webm`
* Convert audio to MP3 for older browsers: `ffmpeg -i input.webm -codec:a libmp3lame -qscale:a 4 output.mp3`
