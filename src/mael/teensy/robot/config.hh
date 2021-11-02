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
#define PIN_ENC0_INT 12
#define PIN_ENC0_DIR 16
#define PIN_ENC1_INT 13
#define PIN_ENC1_DIR 17
#define PIN_ENC2_INT 14
#define PIN_ENC2_DIR 18
#define PIN_ENC3_INT 15
#define PIN_ENC3_DIR 19

// DC driver pins
#define PIN_DC0_DIR 22
#define PIN_DC0_PWM 26
#define PIN_DC1_DIR 23
#define PIN_DC1_PWM 27
#define PIN_DC2_DIR 24
#define PIN_DC2_PWM 28
#define PIN_DC3_DIR 25
#define PIN_DC3_PWM 29

// RF24 config
#define RF24_SPI SPI
#define RF24_PIN_CE 4
#define RF24_PIN_CSN 3
#define RF24_ADDR_HTOR 0xDEAD4321ul
#define RF24_ADDR_RTOH 0xDEAD1234ul
#define RF24_PAYLOAD_SIZE 9