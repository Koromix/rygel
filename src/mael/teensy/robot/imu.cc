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

#include "../common/util.hh"
#include "../common/config.hh"
#include "../common/protocol.hh"
#include "imu.hh"
#include "serial.hh"
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

static Vec3 speed;
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
    PROCESS_EVERY(10000);

    // Get IMU data
    sensors_event_t orient;
    sensors_event_t accel;
    bno.getEvent(&orient, Adafruit_BNO055::VECTOR_EULER);
    bno.getEvent(&accel, Adafruit_BNO055::VECTOR_LINEARACCEL);

    speed.x += (double)accel.acceleration.x * (1.0 / 1000.0);
    speed.y += (double)accel.acceleration.y * (1.0 / 1000.0);
    speed.z += (double)accel.acceleration.z * (1.0 / 1000.0);
    position.x += speed.x * 10.0;
    position.y += speed.y * 10.0;
    position.z += speed.z * 10.0;

    PROCESS_EVERY(50000);

    // Fill basic IMU data
    ImuParameters imu = {};
    imu.orientation = {(double)orient.orientation.x * DEG2RAD, (double)orient.orientation.y * DEG2RAD, (double)orient.orientation.z * DEG2RAD};
    imu.acceleration = {(double)accel.acceleration.x, (double)accel.acceleration.y, (double)accel.acceleration.z};
    imu.speed = speed;
    imu.position = position;

    PostMessage(MessageType::Imu, &imu);
}
