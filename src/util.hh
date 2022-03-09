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

struct InstanceData;

template <typename T, typename... Args>
void ThrowError(Napi::Env env, const char *msg, Args... args)
{
    char buf[1024];
    Fmt(buf, msg, args...);

    T::New(env, buf).ThrowAsJavaScriptException();
}

static inline Size AlignLen(Size len, Size align)
{
    Size aligned = (len + align - 1) / align * align;
    return aligned;
}

template <typename T>
static inline T *AlignUp(T *ptr, Size align)
{
    uint8_t *aligned = (uint8_t *)(((uintptr_t)ptr + align - 1) / align * align);
    return (T *)aligned;
}

template <typename T>
static inline T *AlignDown(T *ptr, Size align)
{
    uint8_t *aligned = (uint8_t *)((uintptr_t)ptr / align * align);
    return (T *)aligned;
}

// Can be slow, only use for error messages
const char *GetValueType(const InstanceData *instance, Napi::Value value);

void SetValueTag(const InstanceData *instance, Napi::Value value, const void *marker);
bool CheckValueTag(const InstanceData *instance, Napi::Value value, const void *marker);

class CallData {
    Napi::Env env;
    InstanceData *instance;
    const FunctionInfo *func;

    Span<uint8_t> *stack_mem;
    Span<uint8_t> *heap_mem;

    Span<uint8_t> old_stack_mem;
    Span<uint8_t> old_heap_mem;

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
        uint8_t *ptr = heap_mem->ptr;
        Size len = ptr - old_heap_mem.ptr;

        return MakeSpan(ptr, len);
    }

    template <typename T>
    bool AllocStack(Size size, Size align, T **out_ptr);
    template <typename T>
    bool AllocHeap(Size size, Size align, T **out_ptr);

    const char *PushString(const Napi::Value &value);
    bool PushObject(const Napi::Object &obj, const TypeInfo *type, uint8_t *dest);

    void DumpDebug() const;
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

    *out_ptr = (T *)ptr;
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

    *out_ptr = (T *)ptr;
    return true;
}

template <typename T>
T CopyNumber(const Napi::Value &value)
{
    RG_ASSERT(value.IsNumber() || value.IsBigInt());

    if (RG_LIKELY(value.IsNumber())) {
        return (T)value.As<Napi::Number>();
    } else if (value.IsBigInt()) {
        Napi::BigInt bigint = value.As<Napi::BigInt>();

        bool lossless;
        return (T)bigint.Uint64Value(&lossless);
    }

    RG_UNREACHABLE();
}

Napi::Object PopObject(Napi::Env env, const uint8_t *ptr, const TypeInfo *type);

}
