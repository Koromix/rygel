// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html } from '../../../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Net } from '../../../../../web/core/base.js';
import { loadTexture } from '../../../core/misc.js';
import { ASSETS } from './images/images.js';

const BACKGROUND = '#7f7f7f';
const IMAGES = 24;
const WAIT_MIN = 500;
const WAIT_MAX = 1500;
const IMAGE_DELAY = 2000;
const RECALIBRATION_DELAY = 20000;

async function run(meta, draw, tracker) {
    draw.event('preload');

    let images = await loadImages(meta.gender, (loaded, total) => draw.frame(ctx => {
        ctx.font = '32px sans-serif';
        ctx.fillStyle = '#000000';

        let text = `Préchargement des images... ${Math.floor(100 * loaded / total)}%`;
        draw.text(draw.width / 2, draw.height / 2, 5, text);
    }));

    draw.event('help');
    await draw.test({
        setup: () => {
            draw.on(' ', () => { return true; });
        },

        frame: ctx => {
            drawBackground(draw, ctx);

            ctx.font = '32px sans-serif';
            ctx.fillStyle = '#000000';

            draw.text(draw.width / 2, draw.height / 2, 5,
                      "Vous aller voir défiler des images suivies d'un point\n\n" +
                      "Appuyez sur la touche 'Ctrl' (située en bas à gauche) si vous voyez un point à gauche\n" +
                      "ou sur la 'Flèche droite' (située en bas à droite) s'il est à droite\n\n" +
                      "Appuyez sur Espace pour commencer");
        }
    });

    let tests = makeTests(IMAGES);

    draw.event('start', tests);
    await draw.repeat(2000, ctx => drawBackground(draw, ctx));

    for (let i = 0; i < tests.length; i++) {
        let test = tests[i];

        draw.event('focus');
        {
            let start = performance.now();

            let center = {
                x: draw.width / 2,
                y: draw.height / 2
            };

            let prev_timestamp = null;
            let focus = 0;

            let target = Util.getRandomInt(WAIT_MIN, WAIT_MAX + 1);

            while (focus < target) {
                await draw.frame(ctx => {
                    drawBackground(draw, ctx);

                    ctx.fillStyle = '#000000';

                    let length = 160;
                    let width = 1;

                    ctx.fillRect(draw.width / 2 - width / 2, draw.height / 2 - length / 2, width, length);
                    ctx.fillRect(draw.width / 2 - length / 2, draw.height / 2 - width / 2, length, width);
                });

                let gaze = tracker.gaze;

                if (gaze != null) {
                    let dist = distance(gaze, center);

                    if (dist < 120) {
                        if (prev_timestamp != null)
                            focus += gaze.timestamp - prev_timestamp;
                        prev_timestamp = gaze.timestamp;
                    } else {
                        focus = 0;
                        prev_timestamp = null;
                    }
                } else {
                    focus = 0;
                    prev_timestamp = null;
                }

                if (performance.now() - start >= RECALIBRATION_DELAY) {
                    draw.event('recalibrate');

                    await tracker.calibrate(draw);
                    start = performance.now();

                    focus = 0;
                    prev_timestamp = null;
                }
            }
        }

        let negative = images.negatives[test.negative];
        let neutral = images.neutrals[test.neutral];
        let left = test.neutral_first ? neutral : negative;
        let right = test.neutral_first ? negative : neutral;

        let start = null;
        let delay = null;
        let choice = null;

        draw.event('test', {
            test: i,
            info: test
        });
        await draw.repeat(IMAGE_DELAY, ctx => {
            drawBackground(draw, ctx);

            ctx.font = '48px sans-serif';
            ctx.fillStyle = '#000000';

            let rect1 = computeLeftRect(draw, left);
            let rect2 = computeRightRect(draw, right);

            draw.event('images', {
                left: rect1,
                right: rect2
            });

            ctx.drawImage(left, 0, 0, left.width, left.height, rect1.left, rect1.top, rect1.width, rect1.height);
            ctx.drawImage(right, 0, 0, right.width, right.height, rect2.left, rect2.top, rect2.width, rect2.height);
        });

        draw.event('target');
        await draw.test({
            setup: () => {
                start = performance.now();

                draw.on('Control', () => {
                    delay = performance.now() - start;
                    choice = 'left';
                    return true;
                });
                draw.on('ArrowRight', () => {
                    delay = performance.now() - start;
                    choice = 'right';
                    return true;
                });
            },

            frame: ctx => {
                drawBackground(draw, ctx);

                ctx.font = '48px sans-serif';
                ctx.fillStyle = '#000000';

                let x = draw.width / 5 * (test.neutral_first != test.is_congruent ? 1 : 4);
                let y = draw.height / 2;

                ctx.beginPath();
                ctx.arc(x, y, 20, 0, 2 * Math.PI, true);
                ctx.fill();
            }
        });

        draw.event('choice', {
            test: i,
            delay: delay,
            choice: choice
        });
    }

    draw.event('done');
}

function makeTests(n) {
    let tests = [];

    // Select image pairs
    {
        let neutrals = shuffle(sequence(n));
        let negatives = shuffle(sequence(n));

        for (let i = 0; i < n; i++) {
            let test = {
                neutral: neutrals.pop(),
                negative: negatives.pop(),
                is_congruent: false,
                neutral_first: false
            };

            tests.push(test);
        }
    }

    tests = [
        ...tests.map(test => Object.assign({}, test, { is_congruent: false, neutral_first: false })),
        ...tests.map(test => Object.assign({}, test, { is_congruent: false, neutral_first: true })),
        ...tests.map(test => Object.assign({}, test, { is_congruent: true, neutral_first: false })),
        ...tests.map(test => Object.assign({}, test, { is_congruent: true, neutral_first: true }))
    ];
    tests = shuffle(tests);

    return tests;
}

function sequence(n) {
    let seq = (new Array(n)).fill(0).map((v, idx) => idx);
    return seq;
}

function shuffle(array) {
    let copy = array.slice();
    let shuffled = [];

    while (copy.length) {
        let idx = Util.getRandomInt(0, copy.length);

        shuffled.push(copy[idx]);

        copy[idx] = copy[copy.length - 1];
        copy.length--;
    }

    return shuffled;
}

function drawBackground(draw, ctx) {
    ctx.fillStyle = '#7f7f7f';
    ctx.fillRect(0, 0, draw.width, draw.height);
}

function computeLeftRect(size, img) {
    let center = size.width / 5 * 1;

    let width = size.width / 4;
    let height = img.height * (width / img.width);
    let left = center - width / 2;
    let top = size.height / 2 - height / 2;

    return { left, top, width, height };
}

function computeRightRect(size, img) {
    let center = size.width / 5 * 4;

    let width = size.width / 4;
    let height = img.height * (width / img.width);
    let left = center - width / 2;
    let top = size.height / 2 - height / 2;

    return { left, top, width, height };
}

async function interpret(meta, events) {
    let results = {
        Tests: [],
        Echantillons: []
    };

    // Assume original screen size until we know better
    let size = {
        width: 1280,
        height: 800
    };

    let gaze = null;
    let test = null;
    let start = null;

    let looking_at = null;
    let looking_since = null;
    let rect1 = null;
    let rect2 = null;

    let images = await loadImages(meta.gender, (loaded, total) => Util.waitFor(0));

    for (let evt of events) {
        switch (evt.type) {
            case 'resize': { size = evt.data; } break;

            case 'gaze': {
                if (looking_at != null) {
                    let gaze = evt.data;

                    if (rect1 == null) {
                        let negative = images.negatives[test.negative];
                        let neutral = images.neutrals[test.neutral];
                        let left = test.neutral_first ? neutral : negative;
                        let right = test.neutral_first ? negative : neutral;

                        rect1 = computeLeftRect(size, left);
                        rect2 = computeRightRect(size, right);
                    }

                    let now_at = inside(gaze, rect1, 1.2) ? 'left' :
                                 inside(gaze, rect2, 1.2) ? 'right' : 'outside';

                    if (looking_at != null) {
                        test[looking_at] += evt.timestamp - looking_since;

                        if (now_at != looking_at && test.hasOwnProperty(now_at + 's'))
                            test[now_at + 's']++;

                        if (test.latency == null && looking_at != 'outside')
                            test.latency = evt.timestamp - start;

                        if (test.lefts + test.rights == 1 && looking_at != 'outside') {
                            test.first = looking_at;
                            test.fixation = evt.timestamp - looking_since;
                        }

                        if (looking_at != 'outside') {
                            for (let i = looking_since; i < evt.timestamp; i++) {
                                let timestamp = i - start;
                                let step = Math.min(Math.max(Math.floor(timestamp / 500), 0), 3);

                                test.steps[step][looking_at]++;
                            }
                        }
                    }

                    looking_at = now_at;
                }

                looking_since = evt.timestamp;
            } break;

            case 'test': {
                test = {
                    test: evt.data.test,
                    ...evt.data.info,

                    target: 0,
                    latency: null,
                    left: 0,
                    right: 0,
                    outside: 0,

                    first: 0,
                    fixation: 0,
                    lefts: 0,
                    rights: 0,

                    steps: [{ left: 0, right: 0 }, { left: 0, right: 0 }, { left: 0, right: 0 }, { left: 0, right: 0 }],

                    choice: null,
                    correct: null
                };

                start = evt.timestamp;

                looking_at = 'outside';
                rect1 = null;
                rect2 = null;
            } break;

            case 'images': {
                rect1 = evt.data.left;
                rect2 = evt.data.right;
            } break;

            case 'target': {
                test.target = evt.timestamp - start;
                looking_at = null;
            } break;

            case 'choice': {
                let expected = (test.neutral_first != test.is_congruent) ? 'left' : 'right';

                test.choice = evt.timestamp - start - test.target;
                test.correct = (evt.data.choice == expected);

                let samples = test.steps.map((step, idx) => ({
                    Test: test.test,

                    Debut: idx * 500,
                    Fin: (idx + 1) * 500,

                    Temps_Gauche: step.left,
                    Temps_Droite: step.right,
                    Temps_Neutre: test.neutral_first ? step.left : step.right,
                    Temps_Negative: test.neutral_first ? step.right : step.left
                }));

                test = {
                    Test: test.test,
                    Image_Neutre: test.neutral,
                    Image_Negative: test.negative,
                    Congruence: test.is_congruent,
                    Affichage_Neutre: test.neutral_first ? 'Gauche' : 'Droite',

                    Temps_Images: test.target,
                    Temps_Dehors: test.outside,
                    Temps_Latence1: test.latency,
                    Temps_Gauche: test.left,
                    Temps_Droite: test.right,
                    Temps_Neutre: test.neutral_first ? test.left : test.right,
                    Temps_Negative: test.neutral_first ? test.right : test.left,

                    Premiere_Direction: ({ left: 'Gauche', right: 'Droite' })[test.first] ?? null,
                    Premiere_Image: ({ left: test.neutral_first ? 'Neutre' : 'Negative',
                                       right: test.neutral_first ? 'Negative' : 'Neutre' })[test.first] ?? null,
                    Premiere_Fixation: test.fixation,

                    Fixations_Gauche: test.lefts,
                    Fixations_Droite: test.rights,
                    Fixations_Neutre: test.neutral_first ? test.lefts : test.rights,
                    Fixations_Negative: test.neutral_first ? test.rights : test.lefts,

                    Delai_Choix: test.choice,
                    Choix_Correct: test.correct
                };

                results.Tests.push(test);
                results.Echantillons.push(...samples);

                looking_at = null;
            } break;
        }
    }

    return results;
}

async function loadImages(genre, progress) {
    let images = {
        negatives: 'emotions',
        neutrals: 'neutres'
    };

    let loaded = 0;
    let total = 0;
    let failed = false;

    for (let key in images) {
        let array = [];

        for (let i = 0; i < IMAGES; i++) {
            let url = findImage(genre, images[key], i);

            loadTexture(url)
                .then(img => { array[i] = img; loaded++; })
                .catch(err => { failed = true });
            total++;
        }

        images[key] = array;
    }

    while (loaded < total) {
        await progress(loaded, total);

        if (failed)
            throw new Error('Échec de préchargement');
    }

    return images;
}

function findImage(genre, type, idx) {
    let key = `${genre == 'H' ? 'hommes' : 'femmes'}/${type}/${idx + 1}`;
    return ASSETS[key];
}

function inside(p, rect, factor) {
    let center = {
        x: rect.left + rect.width / 2,
        y: rect.top + rect.height / 2
    };

    if (p.x < center.x - rect.width / 2 * factor)
        return false;
    if (p.x > center.x + rect.width / 2 * factor)
        return false;
    if (p.y < center.y - rect.height / 2 * factor)
        return false;
    if (p.y > center.y + rect.height / 2 * factor)
        return false;

    return true;
}

function distance(p1, p2) {
    let dx = p1.x - p2.x;
    let dy = p1.y - p2.y;

    return Math.sqrt(dx * dx + dy * dy);
}

export {
    run,
    interpret
}
