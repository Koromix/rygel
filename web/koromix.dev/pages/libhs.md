# Overview

libhs is a C library to **enumerate and interact with USB HID and USB serial devices** and interact with them. It is:

- Single-file: one header is all you need to make it work.
- Public domain: use it, hack it, do whatever you want.
- Multiple platforms: Windows (≥ XP), macOS (≥ 10.9) and Linux.
- Multiple compilers: MSVC (≥ 2015), GCC and Clang.
- Driverless: uses native OS-provided interfaces and does not require custom drivers.

# Build

Just [download libhs.h](https://github.com/Koromix/libraries) from the GitHub repository. This file provides both the interface and the implementation. To instantiate the implementation, `#define HS_IMPLEMENTATION` in *ONE* source file, before including libhs.h.

libhs depends on **a few OS-provided libraries** that you need to link:

OS                  | Dependencies
------------------- | --------------------------------------------------------------------------------
Windows (MSVC)      | Nothing to do, libhs uses `#pragma comment(lib)`
Windows (MinGW-w64) | Link `-luser32 -ladvapi32 -lsetupapi -lhid`
OSX (Clang)         | Link _CoreFoundation and IOKit_
Linux (GCC)         | Link `-ludev`

This library is developed as part of the TyTools project where you can find the original [libhs source code](https://codeberg.org/Koromix/rygel/src/branch/master/src/tytools/libhs). The amalgamated header file is automatically produced by CMake scripts.

Look at [Sean Barrett's excellent stb libraries](https://github.com/nothings/stb) for the reasoning behind this mode of distribution.

# Contribute

You can clone the code and report bugs on the [TyTools GitHub repository](https://github.com/Koromix/tytools).

# Examples

You can find a few complete [working examples](https://codeberg.org/Koromix/rygel/src/branch/master/src/tytools/libhs/examples) in the GitHub repository.

The following code uses libhs to enumerate serial and HID devices:

```c
/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

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
```

# License

All the code related to these programs is under **public domain**, you can do whatever you want with it. See the LICENSE file or [unlicense.org](https://unlicense.org/) more more information.
