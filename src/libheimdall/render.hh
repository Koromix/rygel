// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"

PUSH_NO_WARNINGS()
#include "../../lib/imgui/imgui.h"
#include "../../lib/imgui/imgui_internal.h"
POP_NO_WARNINGS()

bool StartRender();
void Render();

void ReleaseRender();
