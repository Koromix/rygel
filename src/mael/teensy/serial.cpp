// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "util.hh"
#include "config.hh"
#include "drive.hh"
#include "serial.hh"

void InitSerial()
{
    Serial.begin(9600);
}

static bool ParseLong(const char *str, long min, long max, long *out_value)
{
    long value = 0;

    bool neg = (str[0] == '-');
    str += neg;

    for (int pos = 0; str[pos]; pos++) {
        unsigned int digit = (unsigned int)(str[pos] - '0');
        if (digit > 9) {
            Serial.println("Malformed integer");
            return false;
        }

        value = (value * 10) - digit;
    }
    value = neg ? value : -value;

    if (value < min || value > max) {
        Serial.println("Too small or too big");
        return false;
    }

    *out_value = value;
    return true;
}

static bool ExecuteCommand(char *cmd, char *arg0, char *arg1, char *arg2, char *arg3)
{
    if (!strcmp(cmd, "drive") && arg0 && arg1 && arg2) {
        long x;
        long y;
        long w;
        if (!ParseLong(arg0, 0, 1000, &x))
            return false;
        if (!ParseLong(arg1, 0, 1000, &y))
            return false;
        if (!ParseLong(arg2, 0, 1000, &w))

        SetDriveSpeed((float)x, (float)y, (float)w);
        return true;
    } else {
        Serial.println("Invalid command or arguments");
        return false;
    }
}

void ProcessSerial()
{
    enum class ParseMode {
        Start,
        Command,
        Arguments,
        Skip
    };

    static ParseMode mode = ParseMode::Start;
    static char cmd_buf[16];
    static uint8_t cmd_buf_len;
    static unsigned int arg_count;
    static char arg_end_char;
    static char arg_buf[4][32];
    static uint8_t arg_buf_len;

    while (Serial.available()) {
        int c = Serial.read();

        switch (mode) {
            case ParseMode::Start: {
                cmd_buf_len = 0;
                arg_count = 0;
                arg_end_char = ' ';
                arg_buf_len = 0;

                mode = ParseMode::Command;
            } // fallthrough

            case ParseMode::Command: {
                if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                    if (cmd_buf_len < sizeof(cmd_buf) - 1) {
                        cmd_buf[cmd_buf_len++] = c;
                    } else {
                        Serial.println("Excessive command name length");
                        mode = ParseMode::Skip;
                    }
                } else if (c == ' ') {
                    if (cmd_buf_len) {
                        cmd_buf[cmd_buf_len] = 0;
                        mode = ParseMode::Arguments;
                    }
                } else if (c == '\n' || c == '\r') {
                    if (cmd_buf_len) {
                        cmd_buf[cmd_buf_len] = 0;
                        ExecuteCommand(cmd_buf, nullptr, nullptr, nullptr, nullptr);
                        mode = ParseMode::Start;
                    }
                } else {
                    Serial.println("Syntax error");
                    mode = ParseMode::Skip;
                }
            } break;

            case ParseMode::Arguments: {
                if (c == arg_end_char || c == '\n' || c == '\r') {
                    if (arg_buf_len || arg_end_char != ' ') {
                        arg_buf[arg_count][arg_buf_len] = 0;
                        arg_count++;

                        if (arg_end_char != ' ' && c != arg_end_char) {
                            Serial.println("Missing end quote");
                            mode = ParseMode::Skip;
                            break;
                        }
                    }

                    if (c == '\n' || c == '\r') {
                        ExecuteCommand(cmd_buf, arg_count >= 1 ? arg_buf[0] : nullptr,
                                                arg_count >= 2 ? arg_buf[1] : nullptr,
                                                arg_count >= 3 ? arg_buf[2] : nullptr,
                                                arg_count >= 4 ? arg_buf[3] : nullptr);
                        mode = ParseMode::Start;
                    }

                    arg_end_char = ' ';
                    arg_buf_len = 0;
                } else if (arg_count < 4) {
                    if (!arg_buf_len && (c == '\'' || c == '"')) {
                        arg_end_char = c;
                    } else {
                        if (arg_buf_len < sizeof(arg_buf) - 1) {
                            arg_buf[arg_count][arg_buf_len++] = c;
                        } else {
                            Serial.println("Excessive argument length");
                            mode = ParseMode::Skip;
                        }
                    }
                } else {
                    Serial.println("Too many arguments");
                    mode = ParseMode::Skip;
                }
            } break;

            case ParseMode::Skip: {
                if (c == '\n' || c == '\r') {
                    mode = ParseMode::Start;
                }
            } break;
        }
    }
}
