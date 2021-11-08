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
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

static RF24 rf24;

static void InitRadio()
{
    while (!rf24.begin(&(RF24_SPI), RF24_PIN_CE, RF24_PIN_CSN)) {
        Serial.println("Radio hardware not responding!!");
        delay(2000);
    }

    rf24.setPALevel(RF24_PA_LOW);
    rf24.setPayloadSize(RF24_PAYLOAD_SIZE);
    rf24.setAutoAck(false);
    rf24.setRetries(0, 0);
    rf24.disableCRC();

    rf24.openWritingPipe(RF24_ADDR);
    rf24.openReadingPipe(1, RF24_ADDR ^ 1);

    rf24.startListening();
}

void setup()
{
    Serial.begin(9600);

    SPI.begin();
    InitRadio();
}

void loop()
{
    if (rf24.failureDetected) {
        rf24.failureDetected = false;
        InitRadio();
    }

    if (rf24.available()) {
        uint8_t buf[RF24_PAYLOAD_SIZE];
        rf24.read(buf, sizeof(buf));

        if (buf[0] <= sizeof(buf) - 1) {
            Serial.write(buf + 1, buf[0]);
        }
    }

    if (Serial.available()) {
        rf24.stopListening();
        DEFER { rf24.startListening(); };

        uint8_t buf[RF24_PAYLOAD_SIZE];

        int i = 0;
        while (Serial.available() && i < RF24_PAYLOAD_SIZE - 1) {
            uint8_t c = Serial.read();
            buf[++i] = c;
        }
        buf[0] = (uint8_t)i;

        rf24.writeFast(buf, sizeof(buf));
        rf24.txStandBy(0);
    }
}
