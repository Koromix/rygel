// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "util.hh"
#include "config.hh"
#include "imu.hh"
#include "serial.hh"
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

static Vec3 position;

void InitIMU()
{
    if (!bno.begin()) {
        Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
        while (1);
    }

    delay(1000);
}

void ProcessIMU()
{
    PROCESS_EVERY(5000);

    // Get IMU data
    sensors_event_t orient;
    sensors_event_t accel;
    bno.getEvent(&orient, Adafruit_BNO055::VECTOR_EULER);
    bno.getEvent(&accel, Adafruit_BNO055::VECTOR_LINEARACCEL);

    // Compute position (Euler integration)
    // pos = 0.5 * accel * dt^2
    position.x += 0.5 * (double)accel.acceleration.x * (1.0 / 1000.0) * (1.0 / 1000.0);
    position.y += 0.5 * (double)accel.acceleration.y * (1.0 / 1000.0) * (1.0 / 1000.0);
    position.z += 0.5 * (double)accel.acceleration.z * (1.0 / 1000.0) * (1.0 / 1000.0);

    // Fill basic IMU data
    ImuParameters imu = {};
    imu.position = position;
    imu.orientation = {(double)orient.orientation.x * DEG2RAD, (double)orient.orientation.y * DEG2RAD, (double)orient.orientation.z * DEG2RAD};
    imu.acceleration = {(double)accel.acceleration.x, (double)accel.acceleration.y, (double)accel.acceleration.z};

    PostMessage(MessageType::Imu, &imu);
}
