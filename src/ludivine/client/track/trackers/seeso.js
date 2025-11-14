// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, svg, ref } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from 'src/core/web/libjs/common.js';
import * as UI from 'src/core/web/base/ui.js';
import Seeso, { CalibrationAccuracyCriteria, InitializationErrorType } from 'vendor/seeso/seeso.js';

let seeso = null;

async function calibrate(video, draw, input) {
    let license = null;

    try {
        license = JSON.parse(localStorage.getItem('seeso'));
    } catch (err) {
        console.error(err);
    }

    if (!license) {
        license = {
            key: '',
            success: false
        };
    }

    if (seeso != null) {
        seeso.deinitialize();
        seeso = null;
    }

    for (;;) {
        if (!license.success) {
            document.documentElement.style.cursor = 'auto';
            input.stop();

            await UI.dialog({
                overlay: true,

                run: (render, close) => html`
                    <div class="title">
                        Clé SeeSo
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.insist(close)}>✖\uFE0E</button>
                    </div>

                    <div class="main">
                        <label>
                            <span>Clé</span>
                            <input name="key" type="text" value=${license.key || ''} required />
                        </label>
                    </div>

                    <div class="footer"></span>
                        <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                        <button type="submit">${T.confirm}</button>
                    </div>
                `,

                submit: (elements) => {
                    license.key = elements.key.value;
                }
            });

            input.start();
            document.documentElement.style.cursor = 'none';
        }

        try {
            seeso = new Seeso();

            let ret = await seeso.initialize(license.key, null);

            if (ret == InitializationErrorType.ERROR_NONE)
                break;

            switch (ret) {
                case InitializationErrorType.ERROR_INIT: throw new Error('SeeSo initialization error');
                case InitializationErrorType.ERROR_CAMERA_PERMISSION: throw new Error('Cannot access camera feed');
                case InitializationErrorType.AUTH_INVALID_PACKAGE_NAME: throw new Error('Package name invalid');
                case InitializationErrorType.AUTH_INVALID_ACCESS: throw new Error('Invalid access');
                case InitializationErrorType.AUTH_UNKNOWN_ERROR: throw new Error('Unknown error');
                case InitializationErrorType.AUTH_SERVER_ERROR: throw new Error('Server error');
                case InitializationErrorType.AUTH_CANNOT_FIND_HOST: throw new Error('Cannot find host');
                case InitializationErrorType.AUTH_WRONG_LOCAL_TIME: throw new Error('Wrong local time');

                case InitializationErrorType.AUTH_INVALID_KEY:
                case InitializationErrorType.AUTH_INVALID_ENV_USED_DEV_IN_PROD:
                case InitializationErrorType.AUTH_INVALID_ENV_USED_PROD_IN_DEV: Log.error('Invalid license key');
                case InitializationErrorType.AUTH_INVALID_APP_SIGNATURE: Log.error('App signature invalid');
                case InitializationErrorType.AUTH_EXCEEDED_FREE_TIER: Log.error('Exceeded SeeSo free tier');
                case InitializationErrorType.AUTH_DEACTIVATED_KEY: Log.error('License key deactivated');
                case InitializationErrorType.AUTH_INVALID_KEY_FORMAT: Log.error('License key format invalid');
                case InitializationErrorType.AUTH_EXPIRED_KEY: Log.error('License key expired');
            }

            license.success = false;
            continue;
        } catch (err) {
            seeso = null;
            throw err;
        }
    }

    license.success = true;
    localStorage.setItem('seeso', JSON.stringify(license));

    let tracker = new Tracker(video.srcObject, seeso);
    await tracker.calibrate(draw);

    return tracker;
}

function Tracker(stream, seeso) {
    let self = this;

    let tracking = false;

    let calibrating = null;
    let calibration = null;

    let handlers = [];
    let gaze = null;

    Object.defineProperties(this, {
        calibration: { get: () => calibration, enumerable: true },
        gaze: { get: () => gaze, enumerable: true }
    });

    seeso.addCalibrationNextPointCallback(handleCalibrationPoint);
    seeso.addCalibrationProgressCallback(handleCalibrationProgress);
    seeso.addCalibrationFinishCallback(handleCalibrationFinish);
    seeso.addGazeCallback(handleGaze);

    this.calibrate = async function(draw) {
        for (;;) {
            try {
                if (!tracking) {
                    await seeso.startTracking(stream);
                    tracking = true;
                }

                await draw.repeat(4000, ctx => {
                    ctx.font = '32px sans-serif';
                    ctx.fillStyle = '#000000';

                    draw.text(draw.width / 2, draw.height / 2, 5,
                              'La calibration va bientôt commencer...\n' +
                              'Fixez les points qui s\'affichent');
                });

                let p = new Promise((resolve, reject) => {
                    calibrating = {
                        draw: draw,
                        resolve: resolve,
                        reject: reject,

                        x: null,
                        y: null,
                        progress: null,

                        timer: null
                    };

                    if (!seeso.startCalibration(5, CalibrationAccuracyCriteria.DEFAULT))
                        reject('Failed to start calibration');
                });

                calibration = await p;
            } catch (err) {
                if (err == null) {
                    seeso.stopCalibration();
                    continue;
                }

                throw err;
            } finally {
                if (calibrating?.timer != null)
                   clearTimeout(calibrating.timer);

                calibrating = null;
                seeso.stopCalibration();
            }

            break;
        }
    }

    this.stop = async function() {
        if (tracking) {
            seeso.stopTracking();
            tracking = false;
        }

        handlers.length = 0;
    };

    this.on = function(func) { handlers.push(func); };

    function handleGaze(data) {
        if (data.x != null && !Number.isNaN(data.x)) {
            gaze = {
                timestamp: performance.now(),
                x: data.x,
                y: data.y
            };

            for (let handler of handlers)
                handler(gaze);
        } else {
            gaze = null;
        }
    }

    function handleCalibrationPoint(x, y) {
        if (calibrating.timer != null)
           clearTimeout(calibrating.timer);
        calibrating.timer = setTimeout(() => calibrating.reject(null), 10000);

        calibrating.x = x;
        calibrating.y = y;

        seeso.startCollectSamples();
    }

    function handleCalibrationProgress(progress) {
        calibrating.progress = progress;
        drawCalibrationPoint();
    }

    function drawCalibrationPoint() {
        let draw = calibrating.draw;

        if (calibrating.x == null || calibrating.y == null || calibrating.progress == null)
            return;

        draw.frame(ctx => {
            let x = calibrating.x;
            let y = calibrating.y;
            let size = 5 + calibrating.progress * 10;

            ctx.fillStyle = '#FF0000';

            ctx.beginPath();
            ctx.arc(x, y, size, 0, Math.PI * 2, true);
            ctx.fill();
        });
    }

    function handleCalibrationFinish(calibration) {
        calibrating.resolve(calibration);
    }
}

export { calibrate }
