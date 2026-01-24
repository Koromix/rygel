// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

#include <napi.h>

namespace K {

static const Size DefaultSyncStackSize = Mebibytes(1);
static const Size DefaultSyncHeapSize = Mebibytes(2);
static const Size DefaultAsyncStackSize = Kibibytes(128);
static const Size DefaultAsyncHeapSize = Kibibytes(128);
static const int DefaultResidentAsyncPools = 4;
static const int DefaultMaxAsyncCalls = 256;
static const Size DefaultMaxTypeSize = Mebibytes(64);

static const int MaxAsyncCalls = 4096;
static const Size MaxParameters = 64;
static const Size MaxTrampolines = 8192;

enum class PrimitiveKind {
    // Void is explictly not first so that it is not 0, for reasons related to N-API type tags.
    // Look at TypeInfo definition for more information!
    Bool,
    Void,
    Int8,
    UInt8,
    Int16,
    Int16S,  // Keep behind Int16
    UInt16,
    UInt16S, // Keep behind UInt16
    Int32,
    Int32S,  // Keep behind Int32
    UInt32,
    UInt32S, // Keep behind UInt32
    Int64,
    Int64S,  // Keep behind Int64
    UInt64,
    UInt64S, // Keep behind UInt64
    String,
    String16,
    String32,
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
    "Bool",
    "Void",
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
    "String32",
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
class CallData;

typedef void DisposeFunc (Napi::Env env, const TypeInfo *type, const void *ptr);

enum class TypeFlag {
    IsIncomplete = 1 << 0,
    HasTypedArray = 1 << 1,
    IsCharLike = 1 << 2
};

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

    // Make sure primitie ends up as the upper N-API tag value when we cast TypeInfo pointers to
    // napi_type_tag pointers. Yes, I want to do this. We don't do strict aliasing here.
    // N.B. Some node versions don't like when one of the two tag values is 0, so make sure
    // this does not happen! It would happen if primitive is 0 and size is 0. To avoid this
    // situation, PrimitiveKind::Void (the only type with size 0) is explictly not 0.
    alignas(8) PrimitiveKind primitive;
    int32_t size;
    int16_t align;
    uint16_t flags;

    DisposeFunc *dispose;
    Napi::FunctionReference dispose_ref;

    HeapArray<RecordMember> members; // Record only
    union {
        const void *marker;
        const TypeInfo *type; // Pointer or array
        const FunctionInfo *proto; // Callback only
    } ref;
    ArrayHint hint; // Array only
    const char *countedby; // Pointer or array

    mutable Napi::FunctionReference construct; // Union only
    mutable Napi::ObjectReference defn;

    K_HASHTABLE_HANDLER(TypeInfo, name);
};

struct RecordMember {
    const char *name;
    const TypeInfo *type;
    int32_t offset;
    Size countedby;
};

struct LibraryHolder {
    void *module = nullptr; // HMODULE on Windows
    mutable std::atomic_int refcount { 1 };

    LibraryHolder(void *module) : module(module) {}
    ~LibraryHolder() { Unload(); }

    void Unload();

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
    int8_t fast;
#elif __riscv_xlen == 64 || defined(__loongarch64)
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
#if defined(_WIN32)
    int ordinal_name = -1;
#endif
    const LibraryHolder *lib = nullptr;

    void *native;
    CallConvention convention;

    ParameterInfo ret;
    HeapArray<ParameterInfo> parameters;
    int8_t required_parameters;
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

    bool busy;
    bool temporary;
    int depth;
};

struct InstanceData {
    ~InstanceData();

    BucketArray<TypeInfo> types;
    HashMap<const char *, const TypeInfo *> types_map;
    BucketArray<FunctionInfo> callbacks;
    Size base_types_count;

    bool debug;

    const TypeInfo *void_type;
    const TypeInfo *char_type;
    const TypeInfo *char16_type;
    const TypeInfo *char32_type;
    const TypeInfo *str_type;
    const TypeInfo *str16_type;
    const TypeInfo *str32_type;

    Napi::Reference<Napi::Symbol> active_symbol;

    std::mutex mem_mutex;
    LocalArray<InstanceMemory *, 17> memories;
    int temporaries = 0;

    std::thread::id main_thread_id;
    napi_threadsafe_function broker = nullptr;

#if defined(_WIN32)
    void *main_stack_max;
    void *main_stack_min;

    uint32_t last_error = 0;
#endif

    BucketArray<BlockAllocator> encode_allocators;
    HashMap<void *, BlockAllocator *> encode_map;

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
static_assert(DefaultResidentAsyncPools <= K_LEN(InstanceData::memories.data) - 1);
static_assert(DefaultMaxAsyncCalls >= DefaultResidentAsyncPools);
static_assert(MaxAsyncCalls >= DefaultMaxAsyncCalls);

struct TrampolineInfo {
    InstanceData *instance;

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
static_assert(MaxTrampolines <= INT16_MAX);

extern SharedData shared;

InstanceMemory *AllocateMemory(InstanceData *instance, Size stack_size, Size heap_size);
void ReleaseMemory(InstanceData *instance, InstanceMemory *mem);

Napi::Value TranslateNormalCall(const Napi::CallbackInfo &info);
Napi::Value TranslateVariadicCall(const Napi::CallbackInfo &info);
Napi::Value TranslateAsyncCall(const Napi::CallbackInfo &info);

bool InitAsyncBroker(Napi::Env env, InstanceData *instance);

}
