
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

enum class Type {
    Null,
    Bool,
    Int,
    Double,
    String
};
static const char *const TypeNames[] = {
    "Null",
    "Bool",
    "Int",
    "Double",
    "String"
};

struct VariableInfo {
    const char *name;
    Type type;
    bool global;
    bool readonly;

    Size offset;
    // Used to prevent dangerous forward calls, only for globals
    Size defined_at;

    RG_HASHTABLE_HANDLER(VariableInfo, name);
};

struct FunctionInfo {
    struct Parameter {
        const char *name;
        Type type;
    };

    const char *name;
    const char *full_name;
    LocalArray<Parameter, 16> params;
    Type ret;

    // Singly-linked list
    FunctionInfo *next_overload;

    Size inst_idx;
    // Used to prevent dangerous forward calls (if relevant globals are not defined yet)
    Size earliest_forward_call;

    RG_HASHTABLE_HANDLER(FunctionInfo, name);
};

}
