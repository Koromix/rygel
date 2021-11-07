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

#pragma once

// Encoder pins
#define ENC0_PIN_INT 24
#define ENC0_PIN_DIR 25
#define ENC1_PIN_INT 26
#define ENC1_PIN_DIR 27
#define ENC2_PIN_INT 28
#define ENC2_PIN_DIR 29
#define ENC3_PIN_INT 30
#define ENC3_PIN_DIR 31

// DC driver pins
#define DC0_PIN_DIR 2
#define DC0_PIN_PWM 3
#define DC1_PIN_DIR 4
#define DC1_PIN_PWM 5
#define DC2_PIN_DIR 6
#define DC2_PIN_PWM 7
#define DC3_PIN_DIR 8
#define DC3_PIN_PWM 9

// RF24 config
#define RF24_SPI SPI
#define RF24_PIN_CE 0
#define RF24_PIN_CSN 1
#define RF24_ADDR_HTOR 0xDEAD4321ul
#define RF24_ADDR_RTOH 0xDEAD1234ul
#define RF24_PAYLOAD_SIZE 32
