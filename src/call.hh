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
#include "ffi.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

bool AnalyseFunction(Napi::Env env, InstanceData *instance, FunctionInfo *func);

struct BackRegisters;

// I'm not sure why the alignas(8), because alignof(CallData) is 8 without it.
// But on Windows i386, without it, the alignment may not be correct (compiler bug?).
class alignas(8) CallData {
    struct OutArgument {
        napi_ref ref;
        const uint8_t *ptr;
        const TypeInfo *type;
    };

    Napi::Env env;
    InstanceData *instance;
    const FunctionInfo *func;

    InstanceMemory *mem;
    Span<uint8_t> old_stack_mem;
    Span<uint8_t> old_heap_mem;

    int16_t used_trampolines = 0;

    LocalArray<OutArgument, MaxOutParameters> out_arguments;

    uint8_t *new_sp;
    uint8_t *old_sp;

    union {
        int8_t i8;
        uint8_t u8;
        int16_t i16;
        uint16_t u16;
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
        void *ptr;
        uint8_t buf[32];
    } result;
    uint8_t *return_ptr = nullptr;

    LinkedAllocator call_alloc;

public:
    CallData(Napi::Env env, InstanceData *instance, const FunctionInfo *func, InstanceMemory *mem);
    ~CallData();

    bool Prepare(const Napi::CallbackInfo &info);
    void Execute();
    Napi::Value Complete();

    void Relay(Size idx, uint8_t *own_sp, uint8_t *caller_sp, BackRegisters *out_reg);

    void DumpForward() const;

private:
    template <typename T>
    bool AllocStack(Size size, Size align, T **out_ptr);
    template <typename T = uint8_t>
    T *AllocHeap(Size size, Size align);

    const char *PushString(Napi::Value value);
    const char16_t *PushString16(Napi::Value value);
    bool PushObject(Napi::Object obj, const TypeInfo *type, uint8_t *origin, int16_t realign = 0);
    bool PushNormalArray(Napi::Array array, Size len, const TypeInfo *ref, uint8_t *origin, int16_t realign = 0);
    bool PushTypedArray(Napi::TypedArray array, Size len, const TypeInfo *ref, uint8_t *origin, int16_t realign = 0);
    bool PushStringArray(Napi::Value value, const TypeInfo *type, uint8_t *origin);
    bool PushPointer(Napi::Value value, const ParameterInfo &param, void **out_ptr);

    void PopObject(Napi::Object obj, const uint8_t *origin, const TypeInfo *type, int16_t realign = 0);
    Napi::Object PopObject(const uint8_t *origin, const TypeInfo *type, int16_t realign = 0);
    void PopNormalArray(Napi::Array array, const uint8_t *origin, const TypeInfo *ref, int16_t realign = 0);
    void PopTypedArray(Napi::TypedArray array, const uint8_t *origin, const TypeInfo *ref, int16_t realign = 0);
    Napi::Value PopArray(const uint8_t *origin, const TypeInfo *type, int16_t realign = 0);

    void PopOutArguments();

    void *ReserveTrampoline(const FunctionInfo *proto, Napi::Function func);
};
RG_STATIC_ASSERT(MaxTrampolines <= 32);

template <typename T>
inline bool CallData::AllocStack(Size size, Size align, T **out_ptr)
{
    uint8_t *ptr = AlignDown(mem->stack.end() - size, align);
    Size delta = mem->stack.end() - ptr;

    // Keep 512 bytes for redzone (required in some ABIs)
    if (RG_UNLIKELY(mem->stack.len - 512 < delta)) {
        ThrowError<Napi::Error>(env, "FFI call is taking up too much memory");
        return false;
    }

#ifdef RG_DEBUG
    memset(ptr, 0, delta);
#endif

    mem->stack.len -= delta;

    *out_ptr = (T *)ptr;
    return true;
}

template <typename T>
inline T *CallData::AllocHeap(Size size, Size align)
{
    uint8_t *ptr = AlignUp(mem->heap.ptr, align);
    Size delta = size + (ptr - mem->heap.ptr);

    if (RG_LIKELY(size < 4096 && delta <= mem->heap.len)) {
#ifdef RG_DEBUG
        memset(mem->heap.ptr, 0, (size_t)delta);
#endif

        mem->heap.ptr += delta;
        mem->heap.len -= delta;

        return ptr;
    } else {
#ifdef RG_DEBUG
        int flags = (int)Allocator::Flag::Zero;
#else
        int flags = 0;
#endif

        ptr = (uint8_t *)Allocator::Allocate(&call_alloc, size + align, flags);
        ptr = AlignUp(ptr, align);

        return ptr;
    }
}

void *GetTrampoline(Size idx, const FunctionInfo *proto);

}
