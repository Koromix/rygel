// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "util.hh"
#include "drive.hh"

static int ticks[4] = {};
static int target[4] = {};
static int speed[4] = {};

// PID state
static float error_acc[4] = {};
static float error_last[4] = {};

void InitDrive()
{
    attachInterrupt(digitalPinToInterrupt(12), []() { ticks[0]++; }, FALLING);
    attachInterrupt(digitalPinToInterrupt(13), []() { ticks[1]++; }, FALLING);
    attachInterrupt(digitalPinToInterrupt(14), []() { ticks[2]++; }, FALLING);
    attachInterrupt(digitalPinToInterrupt(15), []() { ticks[3]++; }, FALLING);

    pinMode(22, OUTPUT);
    pinMode(23, OUTPUT);
    pinMode(24, OUTPUT);
    pinMode(25, OUTPUT);
}

void ProcessDrive()
{
    PROCESS_EVERY(5000);

    static const float kp = 1.0f;
    static const float ki = 0.0f;
    static const float kd = 0.0f;

    for (int i = 0; i < 4; i++) {
        float error = target[i] - ticks[i];
        float delta = error - error_last[i];

        ticks[i] = 0;
        error_acc[i] += error;
        error_last[i] = error;

        speed[i] += (int)(kp * error + ki * error_acc[i] + kd * delta);
        speed[i] = constrain(speed[i], 0, 255);
    }

    analogWrite(22, speed[0]);
    analogWrite(23, speed[1]);
    analogWrite(24, speed[2]);
    analogWrite(25, speed[3]);
}

void SetDriveSpeed(float x, float y, float w)
{
    static const float kl = 1.0f;
    static const float kw = 1.0f;

    x *= kl;
    y *= kl;
    w *= kw;

    // Forward kinematics matrix:
    // -sin((45 + 90)°)  | cos((45 + 90)°)  | 1
    // -sin((135 + 90)°) | cos((135 + 90)°) | 1
    // -sin((225 + 90)°) | cos((225 + 90)°) | 1
    // -sin((315 + 90)°) | cos((315 + 90)°) | 1
    //
    // Inverse kinematics matrix:
    // -1/sqrt(2) | -1/sqrt(2) | 1
    //  1/sqrt(2) | -1/sqrt(2) | 1
    //  1/sqrt(2) |  1/sqrt(2) | 1
    // -1/sqrt(2) |  1/sqrt(2) | 1

    target[0] = (int)(x * -0.7071f + y * -0.7071f + w * 1.0f);
    target[1] = (int)(x *  0.7071f + y * -0.7071f + w * 1.0f);
    target[2] = (int)(x *  0.7071f + y *  0.7071f + w * 1.0f);
    target[3] = (int)(x * -0.7071f + y *  0.7071f + w * 1.0f);
}
