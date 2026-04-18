// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "ffi.hh"
#include "util.hh"

#include <napi.h>

namespace K {

bool AnalyseFunction(Napi::Env env, InstanceData *instance, FunctionInfo *func);

struct BackRegisters;

// I'm not sure why the alignas(8), because alignof(CallData) is 8 without it.
// But on Windows i386, without it, the alignment may not be correct (compiler bug?).
struct alignas(8) CallData {
    struct OutArgument {
        enum class Kind {
            Array,
            Buffer,
            String,
            String16,
            String32,
            Object
        };

        Kind kind;

        napi_ref ref;
        const uint8_t *ptr;
        const TypeInfo *type;

        Size max_len; // Only for indirect strings
    };

    Napi::Env env;
    InstanceData *instance;
    InstanceMemory *mem;

    uint8_t *prev_stack;
    uint8_t *prev_heap;
    uint8_t *saved_sp;
    bool release_alloc = false;

    void *native;
    uint8_t *async_base;
    const AbiInstruction *async_ip;

    LocalArray<int16_t, 16> used_trampolines;
    LocalArray<OutArgument, MaxParameters> out_arguments;

#if defined(K_DEBUG)
    bool finalized = false;
#endif

#if defined(UNITY_BUILD) && (defined(__clang__) || defined(_MSC_VER))
    #define INLINE_IF_UNITY FORCE_INLINE
#else
    #define INLINE_IF_UNITY
#endif

    CallData(Napi::Env env, InstanceData *instance, InstanceMemory *mem)
        : env(env), instance(instance), mem(mem),
          prev_stack(mem->stack.end), prev_heap(mem->heap.ptr) {}
#if defined(K_DEBUG)
    ~CallData();
#endif

    INLINE_IF_UNITY Napi::Value Run(const FunctionInfo *func, void *native, napi_value *args);

    bool PrepareAsync(const FunctionInfo *func, napi_value *args);
    void ExecuteAsync(void *native);
    Napi::Value EndAsync();

    INLINE_IF_UNITY void Finalize();
    INLINE_IF_UNITY void FinalizeFast();

    void Relay(Size idx, uint8_t *sp);
    void RelayAsync(Size idx, uint8_t *sp);

    INLINE_IF_UNITY bool PushString(Napi::Value value, int directions, const char **out_str);
    INLINE_IF_UNITY bool PushString16(Napi::Value value, int directions, const char16_t **out_str16);
    INLINE_IF_UNITY bool PushString32(Napi::Value value, int directions, const char32_t **out_str32);
    INLINE_IF_UNITY Size PushStringValue(Napi::Value value, const char **out_str);
    INLINE_IF_UNITY Size PushString16Value(Napi::Value value, const char16_t **out_str16);
    INLINE_IF_UNITY Size PushString32Value(Napi::Value value, const char32_t **out_str32);
    bool PushObject(Napi::Object obj, const TypeInfo *type, uint8_t *origin);
    bool PushNormalArray(Napi::Array array, const TypeInfo *type, Size size, uint8_t *origin);
    INLINE_IF_UNITY void PushBuffer(Span<const uint8_t> buffer, const TypeInfo *type, uint8_t *origin);
    bool PushStringArray(Napi::Value value, const TypeInfo *type, uint8_t *origin);
    INLINE_IF_UNITY bool PushPointer(Napi::Value value, const TypeInfo *type, int directions, void **out_ptr);
    bool PushCallback(Napi::Value value, const TypeInfo *type, void **out_ptr);
    Size PushIndirectString(Napi::Array array, const TypeInfo *ref, void **out_ptr);

    void *ReserveTrampoline(const FunctionInfo *proto, Napi::Function func);

    template <typename T>
    T *AllocStack(Size size);
    template <typename T = uint8_t>
    T *AllocHeap(Size size);

    bool CheckDynamicLength(Napi::Object obj, Size element, const char *countedby, Napi::Value value);

#undef INLINE_IF_UNITY
};

template <typename T>
inline T *CallData::AllocStack(Size size)
{
    uint8_t *ptr = AlignDown(mem->stack.end, 16) - size;

    // Keep 512 bytes for redzone (required in some ABIs)
    if (ptr < mem->stack.ptr + 512) [[unlikely]] {
        ThrowError<Napi::Error>(env, "FFI call is taking up too much memory");
        return nullptr;
    }

#if defined(K_DEBUG)
    Size len = mem->stack.end - ptr;
    MemSet(ptr, 0, len);
#endif

    mem->stack.end = ptr;

    return (T *)ptr;
}

template <typename T>
inline T *CallData::AllocHeap(Size size)
{
    K_ASSERT(AlignUp(mem->heap.ptr, 16) == mem->heap.ptr);

    uint8_t *ptr = mem->heap.ptr;
    uint8_t *end = AlignUp(ptr + size, 16);

    if (size < 4096 && end <= mem->heap.end) [[likely]] {
#if defined(K_DEBUG)
        MemSet(ptr, 0, size);
#endif

        mem->heap.ptr = end;

        return ptr;
    } else {
#if defined(K_DEBUG)
        int flags = (int)AllocFlag::Zero;
#else
        int flags = 0;
#endif

        ptr = (uint8_t *)AllocateRaw(&mem->allocator, size + 16, flags);
        ptr = AlignUp(ptr, 16);
        release_alloc |= (prev_stack == mem->stack.end);

        return ptr;
    }
}

void PerformAsyncRelay(napi_env env, napi_value callback, void *ctx, void *udata);

void *GetTrampoline(int idx);

}
