// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

#include "src/core/base/base.hh"
#include "crc64fast_nvme.h"

namespace K {

static std::atomic_int errors = 0;

static uint64_t ComputeWithRust(Span<const uint8_t> buf)
{
    DigestHandle *handle = digest_new();
    K_DEFER { digest_free(handle); };

    digest_write(handle, (const char *)buf.ptr, (uintptr_t)buf.len);
    return digest_sum64(handle);
}

static void TestForever()
{
    for (Size i = 0;; i++) {
        BlockAllocator temp_alloc;

        if (i % 10 == 0) {
            LogInfo("%1 :: iteration %2", Async::GetWorkerIdx(), i);
        }

        for (Size j = 0; j < 100; j++) {
            Size size = GetRandomInt(0, Mebibytes(4));
            Size offset = GetRandomInt(0, 512);

            Span<uint8_t> mem = AllocateSpan<uint8_t>(&temp_alloc, offset + size);
            Span<uint8_t> buf = mem.Take(offset, size);

            FillRandomSafe(buf);

            uint64_t hash1 = CRC64nvme(0, buf);
            uint64_t hash2 = ComputeWithRust(buf);

            if (hash1 != hash2) {
                int idx = ++errors;

                char path[256];
                Fmt(path, "failure%1", idx);
                WriteFile(buf, path);

                LogError("%1 :: %2/%3", idx, FmtHex(hash1).Pad0(-16), FmtHex(hash2).Pad0(-16));
            }
        }
    }
}

int Main(int, char **)
{
    Async async;

    for (int i = 0; i < async.GetWorkerCount(); i++) {
        async.Run([]() { TestForever(); return true; });
    }

    // Never ends
    async.Sync();

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
