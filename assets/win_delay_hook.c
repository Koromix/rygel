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

#include <stdlib.h>
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <delayimp.h>

static FARPROC WINAPI self_exe_hook(unsigned int event, DelayLoadInfo *info)
{
    if (event == dliNotePreLoadLibrary && !stricmp(info->szDll, "node.exe")) {
        HMODULE h = GetModuleHandle(NULL);
        return (FARPROC)h;
    }

    return NULL;
}

const PfnDliHook __pfnDliNotifyHook2 = self_exe_hook;
