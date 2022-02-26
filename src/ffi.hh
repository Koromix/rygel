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

#pragma once

#include "vendor/libcc/libcc.hh"

#include <napi.h>

namespace RG {

enum class PrimitiveKind {
    Void,

    Bool,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float32,
    Float64,
    String,
    Pointer,

    Record
};

static inline bool IsIntegral(PrimitiveKind primitive)
{
    bool integral = ((int)primitive <= (int)PrimitiveKind::Pointer);
    return integral;
}

struct TypeInfo;
struct RecordMember;

struct TypeInfo {
    const char *name;
    napi_type_tag tag;

    PrimitiveKind primitive;
    int16_t size;
    int16_t align;

    HeapArray<RecordMember> members; // Record only
    const TypeInfo *ref; // Pointer only

    RG_HASHTABLE_HANDLER(TypeInfo, name);
};

struct RecordMember {
    const char *name;
    const TypeInfo *type;
};

class LibraryData {
public:
    ~LibraryData();

    void *module = nullptr; // HMODULE on Windows

    BlockAllocator tmp_alloc;
    HeapArray<uint8_t> stack;

    BlockAllocator str_alloc;
};

enum class CallConvention {
    Default,
    Stdcall
};

struct ParameterInfo {
    const TypeInfo *type;

    // ABI-specific part

#if defined(_WIN64)
    bool regular;
#elif defined(__x86_64__)
    bool use_memory;
    int8_t gpr_count;
    int8_t xmm_count;
    bool gpr_first;
#elif defined(__arm__) || defined(__aarch64__)
    bool use_memory;
    int8_t gpr_count;
    int8_t vec_count;
#elif defined(__i386__) || defined(_M_IX86)
    bool trivial; // Only matters for return value
#endif
};

struct FunctionInfo {
    const char *name;
    const char *decorated_name;
    std::shared_ptr<LibraryData> lib;

    void *func;
    CallConvention convention;

    ParameterInfo ret;
    HeapArray<ParameterInfo> parameters;
    Size scratch_size; // Total size needed if all arguments were copied together (with align = 16)

    // ABI-specific part

#if defined(__arm__) || defined(__aarch64__) || defined(__x86_64__) || defined(_WIN64)
    bool forward_fp;
#endif
};

struct InstanceData {
    BucketArray<TypeInfo> types;
    HashTable<const char *, TypeInfo *> types_map;

    uint64_t tag_lower;

    BlockAllocator str_alloc;
};

}
