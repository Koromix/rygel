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

class CallData {
    Napi::Env env;
    InstanceData *instance;
    const FunctionInfo *func;

    struct OutObject {
        Napi::Object obj;
        const uint8_t *ptr;
        const TypeInfo *type;
    };

    Span<uint8_t> *stack_mem;
    Span<uint8_t> *heap_mem;
    LocalArray<OutObject, MaxOutParameters> out_objects;
    BlockAllocator big_alloc;

    Span<uint8_t> old_stack_mem;
    Span<uint8_t> old_heap_mem;

    union {
        uint32_t u32;
        uint64_t u64;
        float f;
        double d;
        void *ptr;
        uint8_t buf[32];
    } result;
    uint8_t *return_ptr = nullptr;

public:
    CallData(Napi::Env env, InstanceData *instance, const FunctionInfo *func);
    ~CallData();

    Span<uint8_t> GetStack() const
    {
        uint8_t *sp = stack_mem->end();
        Size len = old_stack_mem.end() - sp;

        return MakeSpan(sp, len);
    }
    uint8_t *GetSP() const { return stack_mem->end(); };

    Span<uint8_t> GetHeap() const
    {
        uint8_t *ptr = old_heap_mem.ptr;
        Size len = heap_mem->ptr - ptr;

        return MakeSpan(ptr, len);
    }

    bool Prepare(const Napi::CallbackInfo &info);
    void Execute();
    Napi::Value Complete();

    Napi::Value Run(const Napi::CallbackInfo &info)
    {
        if (!RG_UNLIKELY(Prepare(info)))
            return env.Null();

        if (instance->debug) {
            DumpDebug();
        }

        Execute();
        return Complete();
    }

    void DumpDebug() const;

private:
    template <typename T = void>
    bool AllocStack(Size size, Size align, T **out_ptr = nullptr);
    template <typename T = void>
    bool AllocHeap(Size size, Size align, T **out_ptr = nullptr);

    const char *PushString(const Napi::Value &value);
    const char16_t *PushString16(const Napi::Value &value);
    bool PushObject(const Napi::Object &obj, const TypeInfo *type, uint8_t *dest);

    void PopObject(Napi::Object obj, const uint8_t *ptr, const TypeInfo *type);
    Napi::Object PopObject(const uint8_t *ptr, const TypeInfo *type);
};

template <typename T>
bool CallData::AllocStack(Size size, Size align, T **out_ptr)
{
    uint8_t *ptr = AlignDown(stack_mem->end() - size, align);
    Size delta = stack_mem->end() - ptr;

    if (RG_UNLIKELY(stack_mem->len < delta)) {
        ThrowError<Napi::Error>(env, "FFI call is taking up too much memory");
        return false;
    }

    if (instance->debug) {
        memset(ptr, 0, delta);
    }

    stack_mem->len -= delta;

    if (out_ptr) {
        *out_ptr = (T *)ptr;
    }
    return true;
}

template <typename T>
bool CallData::AllocHeap(Size size, Size align, T **out_ptr)
{
    uint8_t *ptr = AlignUp(heap_mem->ptr, align);
    Size delta = size + (ptr - heap_mem->ptr);

    if (RG_UNLIKELY(delta > heap_mem->len)) {
        ThrowError<Napi::Error>(env, "FFI call is taking up too much memory");
        return false;
    }

    if (instance->debug) {
        memset(heap_mem->ptr, 0, (size_t)delta);
    }

    heap_mem->ptr += delta;
    heap_mem->len -= delta;

    if (out_ptr) {
        *out_ptr = (T *)ptr;
    }
    return true;
}

}
