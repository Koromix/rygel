// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "ffi.hh"

#include <napi.h>

namespace K {

#if defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
#else
    #define FORCE_INLINE __attribute__((always_inline)) inline
#endif
#if defined(UNITY_BUILD) && (defined(__clang__) || defined(_MSC_VER))
    #define INLINE_IF_UNITY FORCE_INLINE
#else
    #define INLINE_IF_UNITY
#endif

#if defined(__GNUC__) || defined(__clang__)
    #if  __has_attribute(musttail) && __has_attribute(preserve_none)
        #define MUST_TAIL __attribute__((musttail))
        #define PRESERVE_NONE __attribute__((preserve_none))
    #endif
#endif

extern const napi_type_tag TypeInfoMarker;
extern const napi_type_tag CastMarker;
extern const napi_type_tag MagicUnionMarker;

class MagicUnion: public Napi::ObjectWrap<MagicUnion> {
    InstanceData *instance;
    const TypeInfo *type;

    Size active_idx = -1;

    HeapArray<uint8_t> raw;

public:
    static Napi::Function InitClass(Napi::Env env, const TypeInfo *type);

    MagicUnion(const Napi::CallbackInfo &info);

    const TypeInfo *GetType() { return type; }
    const RecordMember *GetMember() const { return (active_idx >= 0) ? &type->members[active_idx] : nullptr; }

    void SetRaw(const uint8_t *ptr);
    const uint8_t *GetRaw() const { return raw.ptr; }

private:
    Napi::Value Getter(const Napi::CallbackInfo &info);
    void Setter(const Napi::CallbackInfo &info, const Napi::Value &value);
};

template <typename T, typename... Args>
void ThrowError(Napi::Env env, const char *msg, Args... args)
{
    char buf[1024];
    Fmt(buf, msg, args...);

    auto err = T::New(env, buf);
    err.ThrowAsJavaScriptException();
}

static FORCE_INLINE bool IsInteger(const TypeInfo *type)
{
    bool integer = ((int)type->primitive >= (int)PrimitiveKind::Int8 &&
                    (int)type->primitive <= (int)PrimitiveKind::UInt64);
    return integer;
}

static FORCE_INLINE bool IsFloat(const TypeInfo *type)
{
    bool fp = (type->primitive == PrimitiveKind::Float32 ||
               type->primitive == PrimitiveKind::Float64);
    return fp;
}

static FORCE_INLINE bool IsRegularSize(Size size, Size max)
{
    bool regular = (size <= max && !(size & (size - 1)));
    return regular;
}

int ResolveDirections(Span<const char> str);
const TypeInfo *ResolveType(Napi::Value value, int *out_directions = nullptr);
const TypeInfo *ResolveType(Napi::Env env, Span<const char> str);

TypeInfo *MakePointerType(InstanceData *instance, const TypeInfo *ref, int count = 1);
TypeInfo *MakeArrayType(InstanceData *instance, const TypeInfo *ref, Size len);
TypeInfo *MakeArrayType(InstanceData *instance, const TypeInfo *ref, Size len, ArrayHint hint);

Napi::External<TypeInfo> WrapType(Napi::Env env, const TypeInfo *type);

bool CanPassType(const TypeInfo *type, int directions);
bool CanReturnType(const TypeInfo *type);
bool CanStoreType(const TypeInfo *type);

static FORCE_INLINE napi_valuetype GetKindOf(napi_env env, napi_value value)
{
    napi_valuetype kind = napi_undefined;
    napi_typeof(env, value, &kind);

    return kind;
}

static FORCE_INLINE napi_valuetype GetKindOf(Napi::Value value)
{
    return GetKindOf(value.Env(), value);
}

// Can be slow, only use for error messages
const char *GetValueType(const InstanceData *instance, napi_value value);

void SetValueTag(napi_env env, napi_value value, const void *marker);
bool CheckValueTag(napi_env env, napi_value value, const void *marker);

static FORCE_INLINE bool IsNullOrUndefined(napi_valuetype kind)
{
    return kind == napi_null || kind == napi_undefined;
}

static FORCE_INLINE bool IsNullOrUndefined(napi_env env, napi_value value)
{
    napi_valuetype kind = GetKindOf(env, value);
    return IsNullOrUndefined(kind);
}

static FORCE_INLINE bool IsArray(napi_env env, napi_value value)
{
    bool array = false;
    napi_is_array(env, value, &array);

    return array;
}

static FORCE_INLINE bool IsObject(napi_env env, napi_value value)
{
    if (GetKindOf(env, value) != napi_object)
        return false;
    if (IsArray(env, value))
        return false;

    return true;
}

static FORCE_INLINE bool IsTypedArray(napi_env env, napi_value value)
{
    bool typedarray = false;
    napi_is_typedarray(env, value, &typedarray);
    return typedarray;
}

static FORCE_INLINE bool IsArrayBuffer(napi_env env, napi_value value)
{
    bool arraybuffer = false;
    napi_is_arraybuffer(env, value, &arraybuffer);
    return arraybuffer;
}

static FORCE_INLINE bool IsBuffer(napi_env env, napi_value value)
{
    bool buffer = false;
    napi_is_buffer(env, value, &buffer);
    return buffer;
}

template <typename T>
static FORCE_INLINE bool TryNumber(napi_env env, napi_value value, T *out_value)
{
    T v;
    napi_status status;

    // Assume number first
    if constexpr (std::is_same_v<T, double>) {
        status = napi_get_value_double(env, value, &v);
    } else if constexpr (std::is_same_v<T, float>) {
        double d;
        status = napi_get_value_double(env, value, &d);
        v = (float)d;
    } else {
        int64_t i64;
        status = napi_get_value_int64(env, value, &i64);
        v = (T)i64;
    }
    if (status == napi_ok) [[likely]] {
        *out_value = v;
        return true;
    }

    // Maybe a BigInt?
    if constexpr (std::is_signed_v<T>) {
        int64_t i64;
        bool lossless;
        status = napi_get_value_bigint_int64(env, value, &i64, &lossless);
        v = (T)i64;
    } else {
        uint64_t u64;
        bool lossless;
        status = napi_get_value_bigint_uint64(env, value, &u64, &lossless);
        v = (T)u64;
    }
    if (status == napi_ok) {
        *out_value = v;
        return true;
    }

    return false;
}

static FORCE_INLINE bool TryPointer(napi_env env, napi_value value, void **out_ptr)
{
    if (uintptr_t ptr = 0; TryNumber(env, value, &ptr)) {
        *out_ptr = (void *)ptr;
        return true;
    }

    napi_valuetype kind = GetKindOf(env, value);

    if (IsNullOrUndefined(kind)) {
        *out_ptr = nullptr;
        return true;
    } else if (IsTypedArray(env, value)) {
        napi_get_typedarray_info(env, value, nullptr, nullptr, out_ptr, nullptr, nullptr);
        return true;
    } else if (kind == napi_external) {
        Napi::External<void> external = Napi::External<void>(env, value);

        *out_ptr = (void *)external.Data();
        return true;
    } else if (IsArrayBuffer(env, value)) {
        Napi::ArrayBuffer buffer = Napi::ArrayBuffer(env, value);

        *out_ptr = (void *)buffer.Data();
        return true;
    }

    return false;
}

static FORCE_INLINE bool TryPointer(napi_env env, napi_value value, void **out_ptr, bool *out_external)
{
    if (uintptr_t ptr = 0; TryNumber(env, value, &ptr)) {
        *out_ptr = (void *)ptr;
        *out_external = false;

        return true;
    }

    napi_valuetype kind = GetKindOf(env, value);

    if (IsNullOrUndefined(kind)) {
        *out_ptr = nullptr;
        *out_external = false;

        return true;
    } else if (IsTypedArray(env, value)) {
        napi_get_typedarray_info(env, value, nullptr, nullptr, out_ptr, nullptr, nullptr);
        *out_external = false;

        return true;
    } else if (kind == napi_external) {
        Napi::External<void> external = Napi::External<void>(env, value);

        *out_ptr = (void *)external.Data();
        *out_external = true;

        return true;
    } else if (IsArrayBuffer(env, value)) {
        Napi::ArrayBuffer buffer = Napi::ArrayBuffer(env, value);

        *out_ptr = (void *)buffer.Data();
        *out_external = false;

        return true;
    }

    return false;
}

static FORCE_INLINE bool TryBuffer(napi_env env, napi_value value, Span<uint8_t> *out_buffer)
{
    // Before somewhere around Node 20.12, napi_get_buffer_info() would assert/crash
    // when used with something it did not support, instead of returning napi_invalid_arg.
    // So we need to call napi_is_buffer(), at least for now.

    if (IsBuffer(env, value)) {
        void *ptr = nullptr;
        size_t length = 0;

        // Assume it works
        napi_get_buffer_info(env, value, &ptr, &length);

        *out_buffer = MakeSpan((uint8_t *)ptr, (Size)length);
        return true;
    } else if (IsArrayBuffer(env, value)) {
        Napi::ArrayBuffer buffer = Napi::ArrayBuffer(env, value);

        *out_buffer = MakeSpan((uint8_t *)buffer.Data(), (Size)buffer.ByteLength());
        return true;
    }

    return false;
}

int GetTypedArrayType(const TypeInfo *type);

template <typename T>
Size NullTerminatedLength(const T *ptr)
{
    Size len = 0;
    while (ptr[len]) {
        len++;
    }
    return len;
}
template <typename T>
Size NullTerminatedLength(const T *ptr, Size max)
{
    Size len = 0;
    while (len < max && ptr[len]) {
        len++;
    }
    return len;
}

Napi::String MakeStringFromUTF32(Napi::Env env, const char32_t *ptr, Size len);
static inline Napi::String MakeStringFromUTF32(Napi::Env env, const char32_t *ptr)
    { return MakeStringFromUTF32(env, ptr, NullTerminatedLength(ptr)); }

Napi::Object DecodeObject(Napi::Env env, const uint8_t *origin, const TypeInfo *type);
void DecodeObject(Napi::Object obj, const uint8_t *origin, const TypeInfo *type);
Napi::Value DecodeArray(Napi::Env env, const uint8_t *origin, const TypeInfo *type, uint32_t len);
Napi::Value DecodeArray(Napi::Env env, const uint8_t *origin, const TypeInfo *type);
void DecodeNormalArray(Napi::Array array, const uint8_t *origin, const TypeInfo *ref);
void DecodeBuffer(Span<uint8_t> buffer, const uint8_t *origin, const TypeInfo *ref);

Napi::Value Decode(Napi::Value value, Size offset, const TypeInfo *type, const Size *len = nullptr);
Napi::Value Decode(Napi::Env env, const uint8_t *ptr, const TypeInfo *type, const Size *len = nullptr);

bool Encode(Napi::Value ref, Size offset, Napi::Value value, const TypeInfo *type, const Size *len = nullptr);
bool Encode(Napi::Env env, uint8_t *ptr, Napi::Value value, const TypeInfo *type, const Size *len = nullptr);

static FORCE_INLINE Napi::Value NewInt(Napi::Env env, int8_t i)
    { napi_value value; napi_create_int32(env, (int32_t)i, &value); return Napi::Value(env, value); }
static FORCE_INLINE Napi::Value NewInt(Napi::Env env, uint8_t i)
    { napi_value value; napi_create_uint32(env, (uint32_t)i, &value); return Napi::Value(env, value); }
static FORCE_INLINE Napi::Value NewInt(Napi::Env env, int16_t i)
    { napi_value value; napi_create_int32(env, (int32_t)i, &value); return Napi::Value(env, value); }
static FORCE_INLINE Napi::Value NewInt(Napi::Env env, uint16_t i)
    { napi_value value; napi_create_uint32(env, (uint32_t)i, &value); return Napi::Value(env, value); }
static FORCE_INLINE Napi::Value NewInt(Napi::Env env, int32_t i)
    { napi_value value; napi_create_int32(env, i, &value); return Napi::Value(env, value); }
static FORCE_INLINE Napi::Value NewInt(Napi::Env env, uint32_t i)
    { napi_value value; napi_create_uint32(env, i, &value); return Napi::Value(env, value); }

template <typename T>
static FORCE_INLINE Napi::Value NewInt(Napi::Env env, T i)
{
    static_assert(sizeof(T) == 8);

    if constexpr (std::is_signed_v<T>) {
        if (i <= 9007199254740992ll && i >= -9007199254740992ll) {
            napi_value value;
            napi_create_int64(env, (int64_t)i, &value);
            return Napi::Value(env, value);
        }
    } else {
        if (i <= 9007199254740992ull) {
            napi_value value;
            napi_create_int64(env, (int64_t)i, &value);
            return Napi::Value(env, value);
        }
    }

    return Napi::BigInt::New(env, i);
}

static FORCE_INLINE Napi::Array GetOwnPropertyNames(napi_env env, napi_value obj)
{
    K_ASSERT(IsObject(env, obj));

    napi_value result;
    napi_status status = napi_get_all_property_names(env, obj, napi_key_own_only,
                                                     (napi_key_filter)(napi_key_enumerable | napi_key_skip_symbols),
                                                     napi_key_numbers_to_strings, &result);
    K_ASSERT(status == napi_ok);

    return Napi::Array(env, result);
}

Napi::Function WrapFunction(Napi::Env env, const FunctionInfo *func);

Napi::Value INLINE_IF_UNITY WrapPointer(Napi::Env env, const TypeInfo *ref, void *ptr);
Napi::Value INLINE_IF_UNITY WrapCallback(Napi::Env env, const TypeInfo *ref, void *ptr);

bool DetectCallConvention(Span<const char> name, CallConvention *out_convention);

int AnalyseFlat(const TypeInfo *type, FunctionRef<void(const TypeInfo *type, int offset, int count)> func);

void DumpMemory(const char *type, Span<const uint8_t> bytes);

}
