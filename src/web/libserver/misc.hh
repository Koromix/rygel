// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "http.hh"
#include "../../libwrap/json.hh"

namespace RG {

const char *http_GetMimeType(Span<const char> extension);

uint32_t http_ParseAcceptableEncodings(Span<const char> encodings);

class http_JsonPageBuilder: public json_Writer {
    HeapArray<uint8_t> buf;
    StreamWriter st;

public:
    http_JsonPageBuilder(CompressionType compression_type) :
        json_Writer(&st), st(&buf, nullptr, compression_type) {}

    void Finish(http_IO *io);
};

}
