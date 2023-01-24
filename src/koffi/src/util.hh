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

struct InstanceData;
struct TypeInfo;
struct FunctionInfo;

template <typename T, typename... Args>
void ThrowError(Napi::Env env, const char *msg, Args... args)
{
    char buf[1024];
    Fmt(buf, msg, args...);

    auto err = T::New(env, buf);
    err.ThrowAsJavaScriptException();
}

static inline bool IsInteger(const TypeInfo *type)
{
    bool integer = ((int)type->primitive >= (int)PrimitiveKind::Int8 &&
                    (int)type->primitive <= (int)PrimitiveKind::UInt64);
    return integer;
}

static inline bool IsFloat(const TypeInfo *type)
{
    bool fp = (type->primitive == PrimitiveKind::Float32 ||
               type->primitive == PrimitiveKind::Float64);
    return fp;
}

static inline bool IsRegularSize(Size size, Size max)
{
    bool regular = (size <= max && !(size & (size - 1)));
    return regular;
}

const TypeInfo *ResolveType(Napi::Value value, int *out_directions = nullptr);
const TypeInfo *ResolveType(InstanceData *instance, Span<const char> str, int *out_directions = nullptr);
const TypeInfo *MakePointerType(InstanceData *instance, const TypeInfo *type, int count = 1);

bool CanPassType(const TypeInfo *type, int directions);
bool CanReturnType(const TypeInfo *type);
bool CanStoreType(const TypeInfo *type);

// Can be slow, only use for error messages
const char *GetValueType(const InstanceData *instance, Napi::Value value);

void SetValueTag(const InstanceData *instance, Napi::Value value, const void *marker);
bool CheckValueTag(const InstanceData *instance, Napi::Value value, const void *marker);

static inline bool IsNullOrUndefined(Napi::Value value)
{
    return value.IsNull() || value.IsUndefined();
}

static inline bool IsObject(Napi::Value value)
{
    return value.IsObject() && !IsNullOrUndefined(value) && !value.IsArray();
}

static inline bool IsRawBuffer(Napi::Value value)
{
    return value.IsTypedArray() || value.IsArrayBuffer();
}

static inline Span<uint8_t> GetRawBuffer(Napi::Value value)
{
    if (value.IsTypedArray()) {
        Napi::TypedArray array = value.As<Napi::TypedArray>();
        Napi::ArrayBuffer buffer = array.ArrayBuffer();
        Size offset = array.ByteOffset();

        return MakeSpan((uint8_t *)buffer.Data() + offset, (Size)array.ByteLength());
    } else if (value.IsArrayBuffer()) {
        Napi::ArrayBuffer buffer = value.As<Napi::ArrayBuffer>();

        return MakeSpan((uint8_t *)buffer.Data(), (Size)buffer.ByteLength());
    }

    RG_UNREACHABLE();
}

int GetTypedArrayType(const TypeInfo *type);

template <typename T>
T GetNumber(Napi::Value value)
{
    RG_ASSERT(value.IsNumber() || value.IsBigInt());

    if (RG_LIKELY(value.IsNumber())) {
        return (T)value.As<Napi::Number>().DoubleValue();
    } else if (value.IsBigInt()) {
        Napi::BigInt bigint = value.As<Napi::BigInt>();

        bool lossless;
        return (T)bigint.Uint64Value(&lossless);
    }

    RG_UNREACHABLE();
}

Napi::Object DecodeObject(Napi::Env env, const uint8_t *origin, const TypeInfo *type, int16_t realign = 0);
void DecodeObject(Napi::Object obj, const uint8_t *origin, const TypeInfo *type, int16_t realign = 0);
Napi::Value DecodeArray(Napi::Env env, const uint8_t *origin, const TypeInfo *type, int16_t realign = 0);
void DecodeNormalArray(Napi::Array array, const uint8_t *origin, const TypeInfo *ref, int16_t realign = 0);
void DecodeBuffer(Span<uint8_t> buffer, const uint8_t *origin, const TypeInfo *ref, int16_t realign = 0);

static inline Napi::Value NewBigInt(Napi::Env env, int64_t value)
{
    if (value <= 9007199254740992ll && value >= -9007199254740992ll) {
        double d = (double)value;
        return Napi::Number::New(env, d);
    } else {
        return Napi::BigInt::New(env, value);
    }
}

static inline Napi::Value NewBigInt(Napi::Env env, uint64_t value)
{
    if (value <= 9007199254740992ull) {
        double d = (double)value;
        return Napi::Number::New(env, d);
    } else {
        return Napi::BigInt::New(env, value);
    }
}

int AnalyseFlat(const TypeInfo *type, FunctionRef<void(const TypeInfo *type, int offset, int count)> func);

int IsHFA(const TypeInfo *type, int min, int max);

void DumpMemory(const char *type, Span<const uint8_t> bytes);

}
