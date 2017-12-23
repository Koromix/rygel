// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"

GCC_PUSH_IGNORE(-Wconversion)
#include "../../lib/imgui/imgui.h"
GCC_POP_IGNORE()

bool StartRender();
void Render();

void ReleaseRender();
