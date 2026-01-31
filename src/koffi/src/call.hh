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
    const FunctionInfo *func;
    void *native;

    Span<uint8_t> old_stack_mem;
    Span<uint8_t> old_heap_mem;
    uint8_t *saved_sp;

    uint8_t *async_base;
    const AbiInstruction *async_ip;

    LocalArray<int16_t, 16> used_trampolines;
    HeapArray<OutArgument> out_arguments;

    BlockAllocator call_alloc;

    CallData(Napi::Env env, InstanceData *instance, InstanceMemory *mem, const FunctionInfo *func, void *native);
    ~CallData();

#if defined(UNITY_BUILD) && (defined(__clang__) || defined(_MSC_VER))
    #define INLINE_IF_UNITY FORCE_INLINE
#else
    #define INLINE_IF_UNITY
#endif

    void Dispose();

    INLINE_IF_UNITY Napi::Value Run(const Napi::CallbackInfo &info);

    bool PrepareAsync(const Napi::CallbackInfo &info);
    void ExecuteAsync();
    Napi::Value EndAsync();

    void Relay(Size idx, uint8_t *own_sp, uint8_t *caller_sp, bool switch_stack, BackRegisters *out_reg);
    void RelaySafe(Size idx, uint8_t *own_sp, uint8_t *caller_sp, bool outside_call, BackRegisters *out_reg);
    static void RelayAsync(napi_env, napi_value, void *, void *udata);

    void DumpForward() const;

    INLINE_IF_UNITY bool PushString(Napi::Value value, int directions, const char **out_str);
    INLINE_IF_UNITY bool PushString16(Napi::Value value, int directions, const char16_t **out_str16);
    INLINE_IF_UNITY bool PushString32(Napi::Value value, int directions, const char32_t **out_str32);
    Size PushStringValue(Napi::Value value, const char **out_str);
    Size PushString16Value(Napi::Value value, const char16_t **out_str16);
    Size PushString32Value(Napi::Value value, const char32_t **out_str32);
    bool PushObject(Napi::Object obj, const TypeInfo *type, uint8_t *origin);
    bool PushNormalArray(Napi::Array array, const TypeInfo *type, Size size, uint8_t *origin);
    INLINE_IF_UNITY void PushBuffer(Span<const uint8_t> buffer, const TypeInfo *type, uint8_t *origin);
    bool PushStringArray(Napi::Value value, const TypeInfo *type, uint8_t *origin);
    INLINE_IF_UNITY bool PushPointer(Napi::Value value, const TypeInfo *type, int directions, void **out_ptr);
    bool PushCallback(Napi::Value value, const TypeInfo *type, void **out_ptr);
    Size PushIndirectString(Napi::Array array, const TypeInfo *ref, void **out_ptr);

    void *ReserveTrampoline(const FunctionInfo *proto, Napi::Function func);

    BlockAllocator *GetAllocator() { return &call_alloc; }

    template <typename T>
    T *AllocStack(Size size);
    template <typename T = uint8_t>
    T *AllocHeap(Size size, Size align);

    bool CheckDynamicLength(Napi::Object obj, Size element, const char *countedby, Napi::Value value);

    INLINE_IF_UNITY void PopOutArguments();

#undef INLINE_IF_UNITY
};

template <typename T>
inline T *CallData::AllocStack(Size size)
{
    uint8_t *ptr = AlignDown(mem->stack.end(), 16) - size;
    Size delta = mem->stack.end() - ptr;

    // Keep 512 bytes for redzone (required in some ABIs)
    if (mem->stack.len - 512 < delta) [[unlikely]] {
        ThrowError<Napi::Error>(env, "FFI call is taking up too much memory");
        return nullptr;
    }

#if defined(K_DEBUG)
    MemSet(ptr, 0, delta);
#endif

    mem->stack.len -= delta;

    return (T *)ptr;
}

template <typename T>
inline T *CallData::AllocHeap(Size size, Size align)
{
    uint8_t *ptr = AlignUp(mem->heap.ptr, align);
    Size delta = size + (ptr - mem->heap.ptr);

    if (size < 4096 && delta <= mem->heap.len) [[likely]] {
#if defined(K_DEBUG)
        MemSet(mem->heap.ptr, 0, delta);
#endif

        mem->heap.ptr += delta;
        mem->heap.len -= delta;

        return ptr;
    } else {
#if defined(K_DEBUG)
        int flags = (int)AllocFlag::Zero;
#else
        int flags = 0;
#endif

        ptr = (uint8_t *)AllocateRaw(&call_alloc, size + align, flags);
        ptr = AlignUp(ptr, align);

        return ptr;
    }
}

void *GetTrampoline(int16_t idx);

}
