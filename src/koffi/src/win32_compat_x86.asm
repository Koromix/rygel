; SPDX-License-Identifier: MIT
; SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

; The static MSVC STL (14.51 and later) imports GetSystemTimePreciseAsFileTime directly, which does
; not exist before Windows 8. Defining the import slot here means the linker does not pull the
; kernel32 import, and the STL calls KoffiGetSystemTimePrecise() (see win32.cc) instead.

.386
.model flat

extern _KoffiGetSystemTimePrecise@4:proc

public __imp__GetSystemTimePreciseAsFileTime@4

.data

__imp__GetSystemTimePreciseAsFileTime@4 dd _KoffiGetSystemTimePrecise@4

end
