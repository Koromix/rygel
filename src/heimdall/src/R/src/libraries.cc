#ifdef _WIN32
    #define WINVER 0x0602
    #define _WIN32_WINNT 0x0602
#endif

#include "../../../lib/imgui/imgui.cpp"
#include "../../../lib/imgui/imgui_draw.cpp"
#include "../../../lib/imgui/imgui_demo.cpp"

#define HEIMDALL_IMPLEMENTATION
#define KUTIL_NO_MINIZ
#include "../../heimdall/libheimdall.hh"
