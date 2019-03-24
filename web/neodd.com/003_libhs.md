<!-- Title: libhs
     Created: 2017-01-13 -->

#intro# Introduction

libhs is a C library to enumerate HID and serial devices and interact with them.

- **single-file**: one header is all you need to make it work.
- **public domain**: use it, hack it, do whatever you want.
- **multiple platforms**: Windows (≥ XP), Mac OS X (≥ 10.9) and Linux.
- **multiple compilers**: MSV (≥ 2015), GCC and Clang.
- **driverless**: uses native OS-provided interfaces and does not require custom drivers.

#build# Build

Just [download libhs.h from the GitHub repository](https://github.com/Koromix/libraries). This file
provides both the interface and the implementation. To instantiate the implementation, `#define
HS_IMPLEMENTATION` in *ONE* source file, before including libhs.h.

libhs depends on **a few OS-provided libraries** that you need to link:

OS                  | Dependencies
------------------- | --------------------------------------------------------------------------------
Windows (MSVC)      | Nothing to do, libhs uses `#pragma comment(lib)`
Windows (MinGW-w64) | Link _user32, advapi32, setupapi and hid_ `-luser32 -ladvapi32 -lsetupapi -lhid`
OSX (Clang)         | Link _CoreFoundation and IOKit_
Linux (GCC)         | Link _libudev_ `-ludev`

This library is developed as part of the TyTools project where you can find the original
[libhs source code](https://github.com/Koromix/tytools/tree/master/src/libhs). The
amalgamated header file is automatically produced by CMake scripts.

Look at [Sean Barrett's excellent stb libraries](https://github.com/nothings/stb) for the
reasoning behind this mode of distribution.

#license# License

libhs is in the public domain, or the equivalent where that is not possible. You can and should
do anything you want with it. You have no legal obligation to do anything else, although I
appreciate attribution.

#contribute# Contribute

You can clone the code and report bugs on the [TyTools GitHub
repository](https://github.com/Koromix/tytools).

#examples# Examples

You can find a few complete [working examples in the GitHub
repository](https://github.com/Koromix/tytools/tree/master/src/libhs/examples).

The following code uses libhs to enumerate serial and HID devices:

    /* libhs - public domain
       Niels Martignène <niels.martignene@gmail.com>
       https://neodd.com/libraries

       This software is in the public domain. Where that dedication is not
       recognized, you are granted a perpetual, irrevocable license to copy,
       distribute, and modify this file as you see fit.

       See the LICENSE file for more details. */

    #include <inttypes.h>
    #include <stdio.h>

    #define HS_IMPLEMENTATION
    #include "libhs.h"

    static int device_callback(hs_device *dev, void *udata)
    {
        (void)(udata);

        printf("+ %s@%"PRIu8" %04"PRIx16":%04"PRIx16" (%s)\n",
               dev->location, dev->iface_number, dev->vid, dev->pid,
               hs_device_type_strings[dev->type]);
        printf("  - device node:   %s\n", dev->path);
        if (dev->manufacturer_string)
            printf("  - manufacturer:  %s\n", dev->manufacturer_string);
        if (dev->product_string)
            printf("  - product:       %s\n", dev->product_string);
        if (dev->serial_number_string)
            printf("  - serial number: %s\n", dev->serial_number_string);

        /* If you return a non-zero value, the enumeration is aborted and this value is returned
           from the calling function. */
        return 0;
    }

    int main(void)
    {
        int r;

        /* Go through the device tree and call our callback for each device. The callback can abort
           the enumeration by returning a non-zero value. */
        r = hs_enumerate(NULL, 0, device_callback, NULL);
        if (r < 0)
            return -r;

        return 0;
    }
