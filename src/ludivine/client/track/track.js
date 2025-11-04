// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';
import * as UI from '../../../core/web/base/ui.js';
import { DrawAPI, InputAPI } from './api.js';
import { calibrate } from './trackers/mouse.js';
import experiments from './experiments/experiments.json';

import './track.css';

const VIDEO_WIDTH = 800;
const VIDEO_HEIGHT = 600;
const COMMIT_INTERVAL = 10000;

function TrackModule(db, test, experiment) {
    let self = this;

    let target_el = null;

    this.start = async function(el) {
        target_el = el;

        let meta = await db.fetch1('SELECT gender FROM meta');

        render(html`<canvas class="trk_canvas"></canvas>`, target_el);
        let canvas = target_el.querySelector('canvas');

        let input = new InputAPI(canvas);
        let draw = new DrawAPI(canvas, db, test);

        let video = null;
        let tracker = null;

        let commit_timer = setTimeout(commit, COMMIT_INTERVAL);
        await db.exec('BEGIN');

        async function commit() {
            await db.exec(`COMMIT; BEGIN;`);
            commit_timer = setTimeout(commit, COMMIT_INTERVAL);
        }

        try {
            input.start();

            draw.frame(ctx => {
                ctx.font = '32px sans-serif';
                ctx.fillStyle = '#000000';

                draw.text(draw.width / 2, draw.height / 2, 5,
                          'Préparation du test...');
            });

            draw.event('video');
            video = await startVideo();

            draw.event('fullscreen');
            document.documentElement.requestFullscreen();
            document.documentElement.style.cursor = 'none';

            tracker = await calibrate(video, draw, input);
            draw.event('calibration', tracker.calibration);

            draw.setInput(input);
            draw.setTracker(tracker);

            draw.event('experiment');
            await experiment.run(meta, draw, tracker);

            draw.event('done');
        } finally {
            clearTimeout(commit_timer);

            draw.frame(ctx => {
                ctx.font = '32px sans-serif';
                ctx.fillStyle = '#000000';

                draw.text(draw.width / 2, draw.height / 2, 5,
                          'Expérience terminée, enregistrement en cours...');
            });

            if (video != null) {
                video.pause();

                let stream = video.srcObject;

                for (let track of stream.getTracks())
                    track.stop();
            }
            if (tracker != null)
                tracker.stop();
            input.stop();

            await db.exec('COMMIT');

            try {
                document.exitFullscreen();
            } catch (err) {
                console.error(err);
            }
            document.documentElement.style.cursor = 'auto';
            render('', target_el);
        }
    };

    this.stop = function() {
        Log.error('Not implemented');
    };

    this.hasUnsavedData = function() {
        return true;
    };

    this.interpret = async function() {
        let meta = await db.fetch1('SELECT gender FROM meta');
        let events = await db.fetchAll(`SELECT sequence, timestamp, type, data
                                        FROM events
                                        WHERE test = ?
                                        ORDER BY sequence`, test.id);

        for (let evt of events) {
            if (evt.data != null)
                evt.data = JSON.parse(evt.data);
        }

        let objects = await experiment.interpret(meta, events);

        let wb = XLSX.utils.book_new();
        let wb_name = `${meta.identifier}_${new Date(meta.date).toISOString()}.xlsx`;

        for (let sheet in objects) {
            let rows = objects[sheet];

            for (let row of rows) {
                for (let key in row) {
                    let value = row[key];

                    if (value == null) {
                        row[key] = 'NA';
                    } else if (typeof value == 'boolean') {
                        row[key] = 0 + value;
                    }
                }
            }

            let ws = XLSX.utils.json_to_sheet(rows);
            XLSX.utils.book_append_sheet(wb, ws, sheet);
        }

        XLSX.writeFile(wb, wb_name);
    };

    async function startVideo() {
        let video = null;

        let stream = await navigator.mediaDevices.getUserMedia({
            video: {
                width: VIDEO_WIDTH,
                height: VIDEO_HEIGHT,
                facingMode: 'user'
            },
            audio: false
        });

        try {
            video = document.createElement('video');
            video.width = VIDEO_WIDTH;
            video.height = VIDEO_HEIGHT;

            await new Promise((resolve, reject) => {
                video.srcObject = stream;
                video.play();

                video.addEventListener('playing', resolve);
                setTimeout(() => reject(new Error('Cannot start webcam play')), 5000);
            });

            let canvas = document.createElement('canvas');

            canvas.width = video.width;
            canvas.height = video.height;

            let ctx = canvas.getContext('2d', { willReadFrequently: true });

            async function snapshot() {
                if (video.paused)
                    return;

                ctx.drawImage(video, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT);

                let blob = await new Promise((resolve, reject) => {
                    canvas.toBlob(blob => {
                        if (blob != null) {
                            resolve(blob);
                        } else {
                            reject(new Error('Failed to capture image'));
                        }
                    }, 'image/webp', 0.9);
                });
                let png = await blob.arrayBuffer();

                db.exec('INSERT INTO snapshots (test, timestamp, image) VALUES (?, ?, ?)',
                        test.id, performance.now(), png);

                setTimeout(snapshot, 0);
            }
            setTimeout(snapshot, 0);
        } catch (err) {
            for (let track of stream.getTracks())
                track.stop();

            throw err;
        }

        return video;
    }
}

export { TrackModule }
