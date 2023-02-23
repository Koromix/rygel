// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "src/core/libcc/libcc.hh"

#include <napi.h>

namespace RG {

static const Size DefaultSyncStackSize = Mebibytes(1);
static const Size DefaultSyncHeapSize = Mebibytes(2);
static const Size DefaultAsyncStackSize = Kibibytes(256);
static const Size DefaultAsyncHeapSize = Kibibytes(512);
static const int DefaultResidentAsyncPools = 2;
static const int DefaultMaxAsyncCalls = 64;
static const Size DefaultMaxTypeSize = Mebibytes(64);

static const int MaxAsyncCalls = 256;
static const Size MaxParameters = 32;
static const Size MaxOutParameters = 16;
static const Size MaxTrampolines = 1024;

enum class PrimitiveKind {
    Void,
    Bool,
    Int8,
    UInt8,
    Int16,
    Int16S,
    UInt16,
    UInt16S,
    Int32,
    Int32S,
    UInt32,
    UInt32S,
    Int64,
    Int64S,
    UInt64,
    UInt64S,
    String,
    String16,
    Pointer,
    Record,
    Union,
    Array,
    Float32,
    Float64,
    Prototype,
    Callback
};
static const char *const PrimitiveKindNames[] = {
    "Void",
    "Bool",
    "Int8",
    "UInt8",
    "Int16",
    "Int16S",
    "UInt16",
    "UInt16S",
    "Int32",
    "Int32S",
    "UInt32",
    "UInt32S",
    "Int64",
    "Int64S",
    "UInt64",
    "UInt64S",
    "String",
    "String16",
    "Pointer",
    "Record",
    "Union",
    "Array",
    "Float32",
    "Float64",
    "Prototype",
    "Callback"
};

struct TypeInfo;
struct RecordMember;
struct FunctionInfo;

typedef void DisposeFunc (Napi::Env env, const TypeInfo *type, const void *ptr);

enum class ArrayHint {
    Array,
    Typed,
    String
};
static const char *const ArrayHintNames[] = {
    "Array",
    "Typed",
    "String"
};

struct TypeInfo {
    const char *name;

    PrimitiveKind primitive;
    int32_t size;
    int16_t align;

    DisposeFunc *dispose;
    Napi::FunctionReference dispose_ref;

    HeapArray<RecordMember> members; // Record only
    union {
        const void *marker;
        const TypeInfo *type; // Pointer or array
        const FunctionInfo *proto; // Callback only
    } ref;
    ArrayHint hint; // Array only

    mutable Napi::FunctionReference construct; // Union only
    mutable Napi::ObjectReference defn;

    RG_HASHTABLE_HANDLER(TypeInfo, name);
};

struct RecordMember {
    const char *name;
    const TypeInfo *type;
    int32_t offset;
};

struct LibraryHolder {
    void *module = nullptr; // HMODULE on Windows
    mutable std::atomic_int refcount { 1 };

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
    int8_t vec_bytes; // ARM64
#elif defined(__i386__) || defined(_M_IX86)
    bool trivial; // Only matters for return value
    bool fast;
#elif __riscv_xlen == 64
    bool use_memory;
    int8_t gpr_count;
    int8_t vec_count;
    bool gpr_first; // Only for structs
    int8_t reg_size[2];
#endif
};

struct ValueCast {
    Napi::Reference<Napi::Value> ref;
    const TypeInfo *type;
};

// Also used for callbacks, even though many members are not used in this case
struct FunctionInfo {
    mutable std::atomic_int refcount { 1 };

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
    Span<uint8_t> stack0;
    Span<uint8_t> heap;

    uint16_t generation; // Can wrap without risk

    std::atomic_bool busy;
    bool temporary;
    int depth;
};

struct InstanceData {
    ~InstanceData();

    BucketArray<TypeInfo> types;
    HashMap<const char *, const TypeInfo *> types_map;
    BucketArray<FunctionInfo> callbacks;

    bool debug;
    uint64_t tag_lower;

    const TypeInfo *void_type;
    const TypeInfo *char_type;
    const TypeInfo *char16_type;

    std::mutex memories_mutex;
    LocalArray<InstanceMemory *, 9> memories;
    int temporaries = 0;

    std::thread::id main_thread_id;
    napi_threadsafe_function broker = nullptr;

#ifdef _WIN32
    void *main_stack_max;
    void *main_stack_min;
#endif

    HashMap<void *, int16_t> trampolines_map;

    BlockAllocator str_alloc;

    struct {
        Size sync_stack_size = DefaultSyncStackSize;
        Size sync_heap_size = DefaultSyncHeapSize;
        Size async_stack_size = DefaultAsyncStackSize;
        Size async_heap_size = DefaultAsyncHeapSize;
        int resident_async_pools = DefaultResidentAsyncPools;
        int max_temporaries = DefaultMaxAsyncCalls - DefaultResidentAsyncPools;
        Size max_type_size = DefaultMaxTypeSize;
    } config;

    struct {
        int64_t disposed = 0;
    } stats;
};
RG_STATIC_ASSERT(DefaultResidentAsyncPools <= RG_LEN(InstanceData::memories.data) - 1);
RG_STATIC_ASSERT(DefaultMaxAsyncCalls >= DefaultResidentAsyncPools);
RG_STATIC_ASSERT(MaxAsyncCalls >= DefaultMaxAsyncCalls);

struct TrampolineInfo {
    const FunctionInfo *proto;
    Napi::FunctionReference func;
    Napi::Reference<Napi::Value> recv;

    int32_t generation;
};

struct SharedData {
    std::mutex mutex;

    TrampolineInfo trampolines[MaxTrampolines];
    LocalArray<int16_t, MaxTrampolines> available;

    SharedData()
    {
        available.len = MaxTrampolines;

        for (int16_t i = 0; i < MaxTrampolines; i++) {
            available[i] = i;
        }
    }
};
RG_STATIC_ASSERT(MaxTrampolines <= INT16_MAX);

extern SharedData shared;

}
