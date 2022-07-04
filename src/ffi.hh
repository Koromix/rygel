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

static const Size DefaultSyncStackSize = Mebibytes(1);
static const Size DefaultSyncHeapSize = Mebibytes(2);
static const Size DefaultAsyncStackSize = Kibibytes(256);
static const Size DefaultAsyncHeapSize = Kibibytes(512);
static const int DefaultResidentAsyncPools = 2;
static const int DefaultMaxAsyncCalls = 64;

static const int MaxAsyncCalls = 256;
static const Size MaxParameters = 32;
static const Size MaxOutParameters = 4;
static const Size MaxTrampolines = 16;

extern const int TypeInfoMarker;

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
    String,
    String16,
    Pointer,
    Record,
    Array,
    Float32,
    Float64,
    Callback
};
static const char *const PrimitiveKindNames[] = {
    "Void",
    "Bool",
    "Int8",
    "UInt8",
    "Int16",
    "UInt16",
    "Int32",
    "UInt32",
    "Int64",
    "UInt64",
    "String",
    "String16",
    "Pointer",
    "Record",
    "Array",
    "Float32",
    "Float64",
    "Callback"
};

struct TypeInfo;
struct RecordMember;
struct FunctionInfo;

typedef void DisposeFunc (Napi::Env env, const TypeInfo *type, const void *ptr);

struct TypeInfo {
    enum class ArrayHint {
        Array,
        TypedArray,
        String
    };

    const char *name;

    PrimitiveKind primitive;
    int16_t size;
    int16_t align;

    DisposeFunc *dispose;
    Napi::FunctionReference dispose_ref;

    HeapArray<RecordMember> members; // Record only
    const TypeInfo *ref; // Pointer or array
    ArrayHint hint; // Array only
    const FunctionInfo *proto; // Callback only

    Napi::ObjectReference defn;

    RG_HASHTABLE_HANDLER(TypeInfo, name);
};

struct RecordMember {
    const char *name;
    const TypeInfo *type;
    int16_t align;
};

struct LibraryHolder {
    void *module = nullptr; // HMODULE on Windows
    mutable std::atomic_int refcount {1};

    LibraryHolder(void *module) : module(module) {}
    ~LibraryHolder();

    const LibraryHolder *Ref() const;
    void Unref() const;
};

enum class CallConvention {
    Cdecl,
    Stdcall,
    Fastcall,
    Thiscall
};
static const char *const CallConventionNames[] = {
    "Cdecl",
    "Stdcall",
    "Fastcall",
    "Thiscall"
};

struct ParameterInfo {
    const TypeInfo *type;
    int directions;
    bool variadic;
    int8_t offset;

    // ABI-specific part

#if defined(_M_X64)
    bool regular;
#elif defined(__x86_64__)
    bool use_memory;
    int8_t gpr_count;
    int8_t xmm_count;
    bool gpr_first;
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM64)
    bool use_memory; // Only used for return value on ARM32
    int8_t gpr_count;
    int8_t vec_count;
#elif defined(__i386__) || defined(_M_IX86)
    bool trivial; // Only matters for return value
    bool fast;
#elif __riscv_xlen == 64
    bool use_memory;
    int8_t gpr_count;
    int8_t vec_count;
    bool gpr_first; // Only for structs
#endif
};

// Also used for callbacks, even though many members are not used in this case
struct FunctionInfo {
    mutable std::atomic_int refcount {1};

    const char *name;
    const char *decorated_name; // Only set for some platforms/calling conventions
    const LibraryHolder *lib = nullptr;

    void *func;
    CallConvention convention;

    ParameterInfo ret;
    HeapArray<ParameterInfo> parameters;
    int8_t out_parameters;
    bool variadic;

    // ABI-specific part

    Size args_size;
#if defined(__i386__) || defined(_M_IX86)
    bool fast;
#else
    bool forward_fp;
#endif

    ~FunctionInfo();

    const FunctionInfo *Ref() const;
    void Unref() const;
};

struct InstanceMemory {
    ~InstanceMemory();

    Span<uint8_t> stack;
    Span<uint8_t> heap;

    int depth;
    bool temporary;
};

struct TrampolineInfo {
    const FunctionInfo *proto;
    Napi::Function func;
};

struct InstanceData {
    ~InstanceData();

    BucketArray<TypeInfo> types;
    HashTable<const char *, TypeInfo *> types_map;
    BucketArray<FunctionInfo> callbacks;

    bool debug;
    uint64_t tag_lower;

    LocalArray<InstanceMemory *, 9> memories;
    int temporaries = 0;

    TrampolineInfo trampolines[MaxTrampolines];
    uint32_t free_trampolines = UINT32_MAX;

    BlockAllocator str_alloc;

    Size sync_stack_size = DefaultSyncStackSize;
    Size sync_heap_size = DefaultSyncHeapSize;
    Size async_stack_size = DefaultAsyncStackSize;
    Size async_heap_size = DefaultAsyncHeapSize;
    int resident_async_pools = DefaultResidentAsyncPools;
    int max_temporaries = DefaultMaxAsyncCalls - DefaultResidentAsyncPools;
};
RG_STATIC_ASSERT(DefaultResidentAsyncPools <= RG_LEN(InstanceData::memories.data) - 1);
RG_STATIC_ASSERT(DefaultMaxAsyncCalls >= DefaultResidentAsyncPools);
RG_STATIC_ASSERT(MaxAsyncCalls >= DefaultMaxAsyncCalls);
RG_STATIC_ASSERT(MaxTrampolines <= 16);

}
