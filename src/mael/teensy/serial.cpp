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

#ifdef __MK64FX512__

#include "util.hh"
#include "config.hh"
#include "drive.hh"
#include "serial.hh"
#include <FastCRC.h>
#include <SPI.h>
#include <RF24.h>

static RF24 rf24;

static bool recv_started = false;
alignas(uint64_t) static uint8_t recv_buf[1024];
static size_t recv_buf_len = 0;

static uint8_t send_buf[1024];
static size_t send_buf_write = 0;
static size_t send_buf_send = 0;

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

    rf24.openWritingPipe(RF24_ADDR ^ 1);
    rf24.openReadingPipe(1, RF24_ADDR);

    rf24.startListening();
}

void InitSerial()
{
    Serial.begin(9600);

    SPI.begin();
    InitRadio();
}

static bool ExecuteCommand(MessageType type, const void *data)
{
    switch (type) {
        case MessageType::Drive: {
            const DriveParameters &args = *(const DriveParameters *)data;
            SetDriveSpeed(args.speed.x, args.speed.y, args.rotation);
            return true;
        } break;

        default: {
            Serial.println("Unexpected packet");
            return false;
        } break;
    }
}

static void ReceivePacket()
{
    while (rf24.available()) {
        uint8_t c;
        rf24.read(&c, 1);

        if (!recv_started) {
            recv_started = (c == 0xA);
            recv_buf_len = 0;
        } else if (c != 0xA) {
            if (recv_buf_len >= sizeof(recv_buf)) {
                recv_started = false;
                continue;
            }

            recv_buf[recv_buf_len++] = c;
        } else {
            recv_started = false;

            size_t len = 0;
            for (size_t i = 0; i < recv_buf_len; i++, len++) {
                recv_buf[len] = recv_buf[i];

                if (recv_buf[i] == 0xD) {
                    if (i >= recv_buf_len - 1)
                        goto malformed;

                    recv_buf[len] = recv_buf[++i] ^ 0x8;
                }
            }

            // Fix start/end inversion
            if (!len) {
                recv_started = true;
                goto malformed;
            }

            PacketHeader hdr;
            if (len < sizeof(hdr))
                goto malformed;
            memcpy(&hdr, recv_buf, sizeof(hdr));

            if (hdr.payload != len - sizeof(hdr))
                goto malformed;
            if (hdr.type > LEN(PacketSizes))
                goto malformed;
            if (hdr.payload != PacketSizes[hdr.type])
                goto malformed;
            if (hdr.crc32 != FastCRC32().crc32(recv_buf + 4, len - 4))
                goto malformed;

            ExecuteCommand((MessageType)hdr.type, recv_buf + sizeof(hdr));
        }

        continue;
    }

    return;

malformed:
    Serial.println("Malformed packet");
}

void ProcessSerial()
{
    if (rf24.failureDetected) {
        rf24.failureDetected = false;
        InitRadio();
    }

    // Process incoming packets
    ReceivePacket();

    // Send pending packets
    if (send_buf_send != send_buf_write) {
        rf24.stopListening();

        while (send_buf_send != send_buf_write) {
            uint8_t buf[RF24_PAYLOAD_SIZE] = {};
            int buf_len = 1;

            while (send_buf_send != send_buf_write && buf_len < sizeof(buf)) {
                buf[buf_len++] = send_buf[send_buf_send];
                send_buf_send = (send_buf_send + 1) % sizeof(send_buf);
            }
            buf[0] = (uint8_t)(buf_len - 1);

            rf24.writeFast(buf, sizeof(buf));
            rf24.txStandBy(0);
        }

        rf24.startListening();
    }
}

static inline bool WriteByte(uint8_t c, bool escape)
{
    if (escape && (c == 0xA || c == 0xD)) {
        size_t next = (send_buf_write + 1) % sizeof(send_buf);

        if (next == send_buf_send)
            return false;

        send_buf[send_buf_write] = 0xD;
        send_buf_write = (send_buf_write + 1) % sizeof(send_buf);
        c ^= (uint8_t)0x8;
    }

    size_t next = (send_buf_write + 1) % sizeof(send_buf);

    if (next == send_buf_send)
        return false;

    send_buf[send_buf_write] = c;
    send_buf_write = (send_buf_write + 1) % sizeof(send_buf);

    return true;
}

bool PostMessage(MessageType type, const void *args)
{
    assert((size_t)type >= 0 && (size_t)type < LEN(PacketSizes));

    PacketHeader hdr = {};
    size_t prev_write = send_buf_write;

    // Fill basic header information
    hdr.type = (uint16_t)type;
    hdr.payload = PacketSizes[hdr.type];

    // Compute checksum
    {
        FastCRC32 crc32;

        crc32.crc32((const uint8_t *)&hdr + 4, sizeof(hdr) - 4);
        hdr.crc32 = crc32.crc32_upd((const uint8_t *)args, PacketSizes[hdr.type]);
    }

    // Write packet to send buffer
    if (!WriteByte(0xA, false))
        goto overflow;
    for (size_t i = 0; i < sizeof(hdr); i++) {
        uint8_t c = ((const uint8_t *)&hdr)[i];
        if (!WriteByte(c, true))
            goto overflow;
    }
    for (size_t i = 0; i < PacketSizes[hdr.type]; i++) {
        const uint8_t *bytes = (const uint8_t *)args;
        if (!WriteByte(bytes[i], true))
            goto overflow;
    }
    if (!WriteByte(0xA, false))
        goto overflow;

    return true;

overflow:
    send_buf_write = prev_write;

    Serial.println("Send overflow, dropping packet");
    return false;
}

#endif
