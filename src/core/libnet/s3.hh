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

#include "src/core/libcc/libcc.hh"

struct curl_slist;

namespace RG {

class s3_Session {
    const char *scheme = nullptr;
    const char *host = nullptr;
    const char *bucket = nullptr;
    const char *region = nullptr;

    const char *access_id = nullptr;
    const char *access_key = nullptr;

    bool open = false;

    BlockAllocator str_alloc;

public:
    ~s3_Session() { Close(); }

    bool Open(const char *url, const char *id, const char *key);
    bool Open(const char *host, const char *region, const char *bucket, const char *id, const char *key);
    void Close();

    bool PutObject(Span<const char> key, Span<const uint8_t> data, const char *mimetype = nullptr);
    bool DeleteObject(Span<const char> key);

private:
    bool OpenAccess(const char *id, const char *key);
    bool DetermineRegion(const char *url);

    Size PrepareHeaders(const char *method, const char *path, Span<const uint8_t> body,
                        Allocator *alloc, Span<curl_slist> out_headers);
    Span<char> MakeAuthorization(const uint8_t signature[32], const TimeSpec &date, Allocator *alloc);
    void MakeSignature(const char *method, const char *path, const char *query,
                       const TimeSpec &date, const uint8_t sha256[32], uint8_t out_signature[32]);

    Span<const char> MakeURL(Span<const char> key, Allocator *alloc, Span<const char> *out_path = nullptr);
};

}
