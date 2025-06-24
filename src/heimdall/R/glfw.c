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

#if defined(_WIN32)
    #define _GLFW_WIN32
#elif defined(__linux__)
    #define _GLFW_X11
    #define _GNU_SOURCE
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #define _GLFW_X11
#elif defined(__APPLE__)
    #define _GLFW_COCOA
    #define _GLFW_USE_MENUBAR
    #define _GLFW_USE_RETINA
#endif

#define _GLFW_NO_NULL

// Common modules to all platforms
#include "vendor/glfw/src/init.c"
#include "vendor/glfw/src/platform.c"
#include "vendor/glfw/src/context.c"
#include "vendor/glfw/src/monitor.c"
#include "vendor/glfw/src/window.c"
#include "vendor/glfw/src/input.c"
#include "vendor/glfw/src/vulkan.c"

#if defined(_WIN32) || defined(__CYGWIN__)
    #include "vendor/glfw/src/win32_init.c"
    #include "vendor/glfw/src/win32_module.c"
    #include "vendor/glfw/src/win32_monitor.c"
    #include "vendor/glfw/src/win32_window.c"
    #include "vendor/glfw/src/win32_joystick.c"
    #include "vendor/glfw/src/win32_time.c"
    #include "vendor/glfw/src/win32_thread.c"
    #include "vendor/glfw/src/wgl_context.c"

    #include "vendor/glfw/src/egl_context.c"
    #include "vendor/glfw/src/osmesa_context.c"
#endif

#if defined(__linux__)
    #include "vendor/glfw/src/posix_module.c"
    #include "vendor/glfw/src/posix_thread.c"
    #include "vendor/glfw/src/posix_time.c"
    #include "vendor/glfw/src/posix_poll.c"
    #include "vendor/glfw/src/linux_joystick.c"
    #include "vendor/glfw/src/xkb_unicode.c"

    #include "vendor/glfw/src/egl_context.c"
    #include "vendor/glfw/src/osmesa_context.c"

    #include "vendor/glfw/src/x11_init.c"
    #include "vendor/glfw/src/x11_monitor.c"
    #include "vendor/glfw/src/x11_window.c"
    #include "vendor/glfw/src/glx_context.c"
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined( __NetBSD__) || defined(__DragonFly__)
    #include "vendor/glfw/src/posix_module.c"
    #include "vendor/glfw/src/posix_thread.c"
    #include "vendor/glfw/src/posix_time.c"
    #include "vendor/glfw/src/posix_poll.c"
    #include "vendor/glfw/src/null_joystick.c"
    #include "vendor/glfw/src/xkb_unicode.c"

    #include "vendor/glfw/src/x11_init.c"
    #include "vendor/glfw/src/x11_monitor.c"
    #include "vendor/glfw/src/x11_window.c"
    #include "vendor/glfw/src/glx_context.c"

    #include "vendor/glfw/src/egl_context.c"
    #include "vendor/glfw/src/osmesa_context.c"
#endif

#if defined(__APPLE__)
    #include "vendor/glfw/src/posix_module.c"
    #include "vendor/glfw/src/posix_thread.c"
    #include "vendor/glfw/src/cocoa_init.m"
    #include "vendor/glfw/src/cocoa_joystick.m"
    #include "vendor/glfw/src/cocoa_monitor.m"
    #include "vendor/glfw/src/cocoa_window.m"
    #include "vendor/glfw/src/cocoa_time.c"
    #include "vendor/glfw/src/nsgl_context.m"

    #include "vendor/glfw/src/egl_context.c"
    #include "vendor/glfw/src/osmesa_context.c"
#endif
