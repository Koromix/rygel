
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
    Float,
    String
};
static const char *const TypeNames[] = {
    "Null",
    "Bool",
    "Int",
    "Float",
    "String"
};

struct VariableInfo {
    const char *name;
    Type type;
    bool global;
    bool readonly;
    bool poisoned;

    Size offset;

    Size defined_pos; // Token
    Size defined_idx; // IR

    RG_HASHTABLE_HANDLER(VariableInfo, name);
};

struct FunctionInfo {
    struct Parameter {
        const char *name;
        Type type;
    };

    const char *name;
    const char *signature;

    LocalArray<Parameter, 16> params;
    bool variadic;
    Type ret;
    Size ret_pop;
    bool intrinsic;

    // Linked list
    FunctionInfo *overload_prev;
    FunctionInfo *overload_next;

    Size defined_pos; // Token
    Size inst_idx; // IR

    // Used to prevent dangerous forward calls (if relevant globals are not defined yet)
    Size earliest_call_pos;
    Size earliest_call_idx;

    RG_HASHTABLE_HANDLER(FunctionInfo, name);
};

union Value {
    bool b;
    int64_t i;
    double d;
    const char *str;
};

}
