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

bool AnalyseFunction(InstanceData *instance, FunctionInfo *func);

struct BackRegisters;

// I'm not sure why the alignas(8), because alignof(CallData) is 8 without it.
// But on Windows i386, without it, the alignment may not be correct (compiler bug?).
class alignas(8) CallData {
    struct OutObject {
        Napi::ObjectReference ref;
        const uint8_t *ptr;
        const TypeInfo *type;
    };

    Napi::Env env;
    InstanceData *instance;
    const FunctionInfo *func;

    InstanceMemory *mem;
    Span<uint8_t> old_stack_mem;
    Span<uint8_t> old_heap_mem;
    uint32_t used_trampolines = 0;

    LocalArray<OutObject, MaxOutParameters> out_objects;

    uint8_t *new_sp;
    uint8_t *old_sp;

    union {
        uint32_t u32;
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

    void DumpDebug() const;

private:
    template <typename T = void>
    bool AllocStack(Size size, Size align, T **out_ptr);
    template <typename T = void>
    bool AllocHeap(Size size, Size align, T **out_ptr);

    const char *PushString(const Napi::Value &value);
    const char16_t *PushString16(const Napi::Value &value);
    bool PushObject(const Napi::Object &obj, const TypeInfo *type, uint8_t *dest, int16_t realign = 0);
    bool PushArray(const Napi::Value &obj, const TypeInfo *type, uint8_t *dest, int16_t realign = 0);

    void PopObject(Napi::Object obj, const uint8_t *src, const TypeInfo *type, int16_t realign = 0);
    Napi::Object PopObject(const uint8_t *src, const TypeInfo *type, int16_t realign = 0);
    Napi::Value PopArray(const uint8_t *src, const TypeInfo *type, int16_t realign = 0);

    Size ReserveTrampoline(const FunctionInfo *proto, Napi::Function func);
};

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
inline bool CallData::AllocHeap(Size size, Size align, T **out_ptr)
{
    uint8_t *ptr = AlignUp(mem->heap.ptr, align);
    Size delta = size + (ptr - mem->heap.ptr);

    if (RG_UNLIKELY(delta > mem->heap.len)) {
        ThrowError<Napi::Error>(env, "FFI call is taking up too much memory");
        return false;
    }

#ifdef RG_DEBUG
    memset(mem->heap.ptr, 0, (size_t)delta);
#endif

    mem->heap.ptr += delta;
    mem->heap.len -= delta;

    *out_ptr = (T *)ptr;
    return true;
}

void *GetTrampoline(Size idx, const FunctionInfo *proto);

}
