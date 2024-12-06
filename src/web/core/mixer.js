// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// ------------------------------------------------------------------------
// CRC-32
// ------------------------------------------------------------------------

const CRC32 = new function() {
    const CRC32_TABLE = [
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    ];

    this.compute = function(buf) {
        let crc32 = 0xFFFFFFFF;

        for (let byte of buf)
            crc32 = CRC32_TABLE[(crc32 ^ byte) & 0xFF] ^ (crc32 >>> 8);

        return (crc32 ^ (-1)) >>> 0;
    };
};

// ------------------------------------------------------------------------
// Base 64
// ------------------------------------------------------------------------

const Base64 = new function() {
    const BaseChars = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
                       'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b',
                       'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
                       'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
                       '4', '5', '6', '7', '8', '9', '+', '/'];
    const BaseCodes = [null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
                       null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
                       null, null, null, null, null, null, null, null, null, null, null, 0x3E, null, null, null, 0x3F,
                       0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, null, null, null, 0x00, null, null,
                       null, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                       0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, null, null, null, null, null,
                       null, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                       0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33];

    const UrlChars = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
                      'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b',
                      'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
                      'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
                      '4', '5', '6', '7', '8', '9', '-', '_'];
    const UrlCodes = [null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
                      null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
                      null, null, null, null, null, null, null, null, null, null, null, null, null, 0x3E, null, null,
                      0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, null, null, null, null, null, null,
                      null, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                      0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, null, null, null, null, 0x3F,
                      null, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                      0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33];

    if (Uint8Array.prototype.toBase64 != null) {
        this.toBase64 = function(bytes) {
            if (bytes instanceof ArrayBuffer)
                bytes = new Uint8Array(bytes);

            return bytes.toBase64({ alphabet: 'base64', omitPadding: false });
        };
        this.toBase64Url = function(bytes) {
            if (bytes instanceof ArrayBuffer)
                bytes = new Uint8Array(bytes);

            return bytes.toBase64({ alphabet: 'base64url', omitPadding: true });
        };
    } else {
        this.toBase64 = function(bytes) {
            if (bytes instanceof ArrayBuffer)
                bytes = new Uint8Array(bytes);

            return convertToBase64(bytes, BaseChars, true);
        };
        this.toBase64Url = function(bytes) {
            if (bytes instanceof ArrayBuffer)
                bytes = new Uint8Array(bytes);

            return convertToBase64(bytes, UrlChars, false);
        };
    }

    function convertToBase64(bytes, chars, pad) {
        if (bytes instanceof ArrayBuffer)
            bytes = new Uint8Array(bytes);

        let result = '';
        let length = bytes.length;

        let i = 0;

        for (i = 2; i < length; i += 3) {
            result += chars[bytes[i - 2] >> 2];
            result += chars[(bytes[i - 2] & 0x03) << 4 | bytes[i - 1] >> 4];
            result += chars[(bytes[i - 1] & 0x0F) << 2 | bytes[i] >> 6];
            result += chars[bytes[i] & 0x3F];
        }

        if (i == length + 1) {
            result += chars[bytes[i - 2] >> 2];
            result += chars[(bytes[i - 2] & 0x03) << 4];
            result += pad ? '==' : '';
        } else if (i == length) {
            result += chars[bytes[i - 2] >> 2];
            result += chars[(bytes[i - 2] & 0x03) << 4 | bytes[i - 1] >> 4];
            result += chars[(bytes[i - 1] & 0x0F) << 2];
            result += pad ? '=' : '';
        }

        return result;
    }

    if (Uint8Array.fromBase64 != null) {
        this.toBytes = function(str) { return Uint8Array.fromBase64(str, { alphabet: 'base64', lastChunkHandling: 'strict' }); };
        this.toBytesUrl = function(str) { return Uint8Array.fromBase64(str, { alphabet: 'base64url', lastChunkHandling: 'loose' }); };
    } else {
        this.toBytes = function(str) { return convertToBytes(str, BaseCodes, true); };
        this.toBytesUrl = function(str) { return convertToBytes(str, UrlCodes, false); };
    }

    function convertToBytes(str, codes, pad) {
        let missing = 0;
        let length = str.length;

        if (pad) {
            if (str.length % 4 != 0)
                throw new Error('Invalid base64 string');

            missing = (str[str.length - 1] == '=') +
                      (str[str.length - 2] == '=') +
                      (str[str.length - 3] == '=');
        } else {
            missing = (4 - (str.length & 3)) & 3;
            length += missing;
        }

        if (missing > 2)
            throw new Error('Invalid base64 string');

        let result = new Uint8Array(3 * (length / 4));

        for (let i = 0, j = 0; i < length; i += 4, j += 3) {
            let tmp = (getBase64Code(codes, str.charCodeAt(i)) << 18) |
                      (getBase64Code(codes, str.charCodeAt(i + 1)) << 12) |
                      (getBase64Code(codes, str.charCodeAt(i + 2)) << 6) |
                      (getBase64Code(codes, str.charCodeAt(i + 3)) << 0);

            result[j] = tmp >> 16;
            result[j + 1] = tmp >> 8 & 0xFF;
            result[j + 2] = tmp & 0xFF;
        }

        return result.subarray(0, result.length - missing);
    }

    function getBase64Code(codes, c) {
        if (isNaN(c))
            return 0;

        let code = codes[c];
        if (code == null)
            throw new Error('Invalid base64 string');
        return code;
    }
};

// ------------------------------------------------------------------------
// SHA-256
// ------------------------------------------------------------------------

function Sha256(data) {
    let self = this;

    // Hash constant words K
    let K256 = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    ];

    // Global arrays
    let ihash = [
        0x6a09e667,
        0xbb67ae85,
        0x3c6ef372,
        0xa54ff53a,
        0x510e527f,
        0x9b05688c,
        0x1f83d9ab,
        0x5be0cd19
    ];
    let count = [0, 0];
    let buffer = new Array(64);
    let finalized = false;

    // Read the next chunk of data and update the SHA256 computation
    this.update = function(data) {
        if (typeof data === 'string') {
            data = (new TextEncoder()).encode(data);
        } else if (data instanceof ArrayBuffer) {
            data = new Uint8Array(data);
        }

        // Compute number of bytes mod 64
        let index = ((count[0] >> 3) & 0x3f);
        let remainder = (data.length & 0x3f);
        let curpos = 0;

        // Update number of bits
        if ((count[0] += (data.length << 3)) < (data.length << 3))
            count[1]++;
        count[1] += (data.length >> 29);

        // Transform as many times as possible
        for (let i = 0; i < data.length - 63; i += 64) {
            for (let j = index; j < 64; j++)
                buffer[j] = data[curpos++];
            transform();
            index = 0;
        }

        // Buffer remaining input
        for (let i = 0; i < remainder; i++)
            buffer[i] = data[curpos++];
    };

    // Finish the computation by operations such as padding
    this.finalize = function() {
        if (!finalized) {
            let index = ((count[0] >> 3) & 0x3f);

            buffer[index++] = 0x80;
            if (index <= 56) {
                for (let i = index; i < 56; i++)
                    buffer[i] = 0;
            } else {
                for (let i = index; i < 64; i++)
                    buffer[i] = 0;
                transform();
                for (let i = 0; i < 56; i++)
                    buffer[i] = 0;
            }
            buffer[56] = (count[1] >>> 24) & 0xff;
            buffer[57] = (count[1] >>> 16) & 0xff;
            buffer[58] = (count[1] >>> 8) & 0xff;
            buffer[59] = count[1] & 0xff;
            buffer[60] = (count[0] >>> 24) & 0xff;
            buffer[61] = (count[0] >>> 16) & 0xff;
            buffer[62] = (count[0] >>> 8) & 0xff;
            buffer[63] = count[0] & 0xff;
            transform();

            finalized = true;
        }

        return encodeToHex();
    };

    // Transform a 512-bit message block
    function transform() {
        let W = new Array(16);

        // Initialize registers with the previous intermediate value
        let a = ihash[0];
        let b = ihash[1];
        let c = ihash[2];
        let d = ihash[3];
        let e = ihash[4];
        let f = ihash[5];
        let g = ihash[6];
        let h = ihash[7];

        // make 32-bit words
        for (let i = 0; i < 16; i++)
            W[i] = (buffer[(i << 2) + 3]) | (buffer[(i << 2) + 2] << 8) |
                   (buffer[(i << 2) + 1] << 16) | (buffer[i << 2] << 24);

        for (let i = 0; i < 64; i++) {
            let T1 = h + Sigma1(e) + choice(e, f, g) + K256[i] +
                     ((i < 16) ? W[i] : expand(W, i));
            let T2 = Sigma0(a) + majority(a, b, c);

            h = g;
            g = f;
            f = e;
            e = addSafe(d, T1);
            d = c;
            c = b;
            b = a;
            a = addSafe(T1, T2);
        }

        // Compute the current intermediate hash value
        ihash[0] += a;
        ihash[1] += b;
        ihash[2] += c;
        ihash[3] += d;
        ihash[4] += e;
        ihash[5] += f;
        ihash[6] += g;
        ihash[7] += h;
    }

    // SHA256 logical functions
    function rotateRight(n, x) {
        return ((x >>> n) | (x << (32 - n)));
    }
    function choice(x, y, z) {
        return ((x & y) ^ (~x & z));
    }
    function majority(x, y, z) {
        return ((x & y) ^ (x & z) ^ (y & z));
    }
    function Sigma0(x) {
        return (rotateRight(2, x) ^ rotateRight(13, x) ^ rotateRight(22, x));
    }
    function Sigma1(x) {
        return (rotateRight(6, x) ^ rotateRight(11, x) ^ rotateRight(25, x));
    }
    function expand(W, j) {
        return (W[j & 0x0f] += sigma1(W[(j + 14) & 0x0f]) + W[(j + 9) & 0x0f] +
                               sigma0(W[(j + 1) & 0x0f]));
    }
    function sigma0(x) {
        return (rotateRight(7, x) ^ rotateRight(18, x) ^ (x >>> 3));
    }
    function sigma1(x) {
        return (rotateRight(17, x) ^ rotateRight(19, x) ^ (x >>> 10));
    }

    // Add 32-bit integers with 16-bit operations (bug in some JS-interpreters: overflow)
    function addSafe(x, y) {
        let lsw = (x & 0xffff) + (y & 0xffff);
        let msw = (x >> 16) + (y >> 16) + (lsw >> 16);
        return (msw << 16) | (lsw & 0xffff);
    }

    // Get the internal hash as a hex string
    function encodeToHex() {
        let hex_digits = '0123456789ABCDEF';

        let output = '';
        for (let i = 0; i < 8; i++) {
            for (let j = 28; j >= 0; j -= 4)
                output += hex_digits.charAt((ihash[i] >>> j) & 0x0f);
        }
        return output;
    };

    // Quick API: call Sha256(str) directly
    if (!new.target && data != null) {
        this.update(data);
        return this.finalize();
    }
};

// Javascript Blob/File API sucks, plain and simple
Sha256.async = async function(blob) {
    let sha256 = new Sha256;

    for (let offset = 0; offset < blob.size; offset += 65536) {
        let piece = blob.slice(offset, offset + 65536);
        let buf = await new Promise((resolve, reject) => {
            let reader = new FileReader;

            reader.onload = e => resolve(e.target.result);
            reader.onerror = e => {
                reader.abort();
                reject(new Error(e.target.error));
            };

            reader.readAsArrayBuffer(piece);
        });

        sha256.update(buf);
    }

    return sha256.finalize();
};

export {
    CRC32,
    Base64,
    Sha256
}
