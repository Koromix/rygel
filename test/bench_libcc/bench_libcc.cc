// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../src/libcc/libcc.hh"
#include "bench_libcc.hh"

namespace RG {

void BenchFmt();

int RunBenchLibcc(int, char **)
{
    BenchFmt();

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunBenchLibcc(argc, argv); }
