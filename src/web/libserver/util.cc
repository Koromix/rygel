// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "util.hh"

namespace RG {

const char *http_GetMimeType(Span<const char> extension)
{
    if (extension == ".txt") {
        return "text/plain";
    } else if (extension == ".css") {
        return "text/css";
    } else if (extension == ".html") {
        return "text/html";
    } else if (extension == ".ico") {
        return "image/vnd.microsoft.icon";
    } else if (extension == ".js") {
        return "application/javascript";
    } else if (extension == ".json") {
        return "application/json";
    } else if (extension == ".png") {
        return "image/png";
    } else if (extension == ".svg") {
        return "image/svg+xml";
    } else if (extension == ".map") {
        return "application/json";
    } else if (extension == ".woff") {
        return "font/woff";
    } else if (extension == ".woff2") {
        return "font/woff2";
    } else if (extension == ".manifest") {
        return "application/manifest+json";
    } else if (extension == "") {
        return "application/octet-stream";
    } else {
        LogError("Unknown MIME type for extension '%1'", extension);
        return "application/octet-stream";
    }
}

}
