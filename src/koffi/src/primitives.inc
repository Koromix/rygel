// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if !defined(PRIMITIVE)
    #error Please define PRIMITIVE() before including primitives.inc
#endif

// Void is explictly not first so that it is not 0, for reasons related to N-API type tags.
// Look at TypeInfo definition for more information!

PRIMITIVE(Bool)
PRIMITIVE(Void)
PRIMITIVE(Int8)
PRIMITIVE(UInt8)
PRIMITIVE(Int16)
PRIMITIVE(Int16S)  // Keep behind Int16
PRIMITIVE(UInt16)
PRIMITIVE(UInt16S) // Keep behind UInt16
PRIMITIVE(Int32)
PRIMITIVE(Int32S)  // Keep behind Int32
PRIMITIVE(UInt32)
PRIMITIVE(UInt32S) // Keep behind UInt32
PRIMITIVE(Int64)
PRIMITIVE(Int64S)  // Keep behind Int64
PRIMITIVE(UInt64)
PRIMITIVE(UInt64S) // Keep behind UInt64
PRIMITIVE(String)
PRIMITIVE(String16)
PRIMITIVE(String32)
PRIMITIVE(Pointer)
PRIMITIVE(Record)
PRIMITIVE(Union)
PRIMITIVE(Array)
PRIMITIVE(Float32)
PRIMITIVE(Float64)
PRIMITIVE(Callback)
PRIMITIVE(Prototype)

#undef PRIMITIVE
