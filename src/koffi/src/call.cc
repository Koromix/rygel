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

#include "src/core/libcc/libcc.hh"
#include "call.hh"
#include "ffi.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

struct RelayContext {
    CallData *call;

    Size idx;
    uint8_t *own_sp;
    uint8_t *caller_sp;
    BackRegisters *out_reg;

    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
};

CallData::CallData(Napi::Env env, InstanceData *instance, InstanceMemory *mem)
    : env(env), instance(instance),
      mem(mem), old_stack_mem(mem->stack), old_heap_mem(mem->heap)
{
    mem->generation += !mem->depth;
    mem->depth++;

    RG_ASSERT(AlignUp(mem->stack.ptr, 16) == mem->stack.ptr);
    RG_ASSERT(AlignUp(mem->stack.end(), 16) == mem->stack.end());
}

CallData::~CallData()
{
    for (const OutArgument &out: out_arguments) {
        napi_delete_reference(env, out.ref);
    }

    mem->stack = old_stack_mem;
    mem->heap = old_heap_mem;

    if (used_trampolines.len) {
        std::lock_guard<std::mutex> lock(shared.mutex);

        for (Size i = used_trampolines.len - 1; i >= 0; i--) {
            int16_t idx = used_trampolines[i];
            TrampolineInfo *trampoline = &shared.trampolines[idx];

            RG_ASSERT(!trampoline->func.IsEmpty());

            trampoline->func.Reset();
            trampoline->recv.Reset();

            shared.available.Append(idx);
        }
    }

    instance->temporaries -= mem->temporary;

    if (!--mem->depth && mem->temporary) {
        delete mem;
    }

    instance = nullptr;
}

void CallData::RelaySafe(Size idx, uint8_t *own_sp, uint8_t *caller_sp, BackRegisters *out_reg)
{
    if (std::this_thread::get_id() != instance->main_thread_id) {
        // JS/V8 is single-threaded, and runs on main_thread_id. Forward the call
        // to the JS event loop.

        RelayContext ctx;

        ctx.call = this;
        ctx.idx = idx;
        ctx.own_sp = own_sp;
        ctx.caller_sp = caller_sp;
        ctx.out_reg = out_reg;

        napi_call_threadsafe_function(instance->broker, &ctx, napi_tsfn_blocking);

        // Wait until it executes
        std::unique_lock<std::mutex> lock(ctx.mutex);
        while (!ctx.done) {
            ctx.cv.wait(lock);
        }
    } else {
        Relay(idx, own_sp, caller_sp, false, out_reg);
    }
}

void CallData::RelayAsync(napi_env, napi_value, void *, void *udata)
{
    RelayContext *ctx = (RelayContext *)udata;

    ctx->call->Relay(ctx->idx, ctx->own_sp, ctx->caller_sp, true, ctx->out_reg);

    // We're done!
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->done = true;
    ctx->cv.notify_one();
}

bool CallData::PushString(Napi::Value value, int directions, const char **out_str)
{
    if (value.IsString()) {
        if (RG_UNLIKELY(directions & 2)) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected string", GetValueType(instance, value));
            return false;
        }

        PushStringValue(value, out_str);
        return true;
    } else if (IsNullOrUndefined(value)) {
        *out_str = nullptr;
        return true;
    } else if (value.IsArray()) {
        Napi::Array array = value.As<Napi::Array>();

        if (RG_UNLIKELY(!(directions & 2))) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected string", GetValueType(instance, value));
            return false;
        }
        if (RG_UNLIKELY(array.Length() != 1)) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected [string]", GetValueType(instance, value));
            return false;
        }

        value = array[0u];

        if (RG_UNLIKELY(!value.IsString())) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected [string]", GetValueType(instance, array[0u]));
            return false;
        }

        Size len = PushStringValue(value, out_str);
        if (RG_UNLIKELY(len < 0))
            return false;

        // Create array type
        TypeInfo *type;
        {
            type = AllocateOne<TypeInfo>(&call_alloc, (int)AllocFlag::Zero);

            type->name = "<temporary>";

            type->primitive = PrimitiveKind::Array;
            type->align = 1;
            type->size = (int32_t)len;
            type->ref.type = instance->char_type;
            type->hint = TypeInfo::ArrayHint::String;
        }

        // Prepare output argument
        {
            OutArgument *out = out_arguments.AppendDefault();

            napi_status status = napi_create_reference(env, array, 1, &out->ref);
            RG_ASSERT(status == napi_ok);

            out->ptr = (const uint8_t *)*out_str;
            out->type = type;
        }

        return true;
    } else {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected string", GetValueType(instance, value));
        return false;
    }
}

Size CallData::PushStringValue(Napi::Value value, const char **out_str)
{
    Span<char> buf;
    size_t len = 0;
    napi_status status;

    buf.ptr = (char *)mem->heap.ptr;
    buf.len = std::max((Size)0, mem->heap.len - Kibibytes(32));

    status = napi_get_value_string_utf8(env, value, buf.ptr, (size_t)buf.len, &len);
    RG_ASSERT(status == napi_ok);

    len++;

    if (RG_LIKELY(len < (size_t)buf.len)) {
        mem->heap.ptr += (Size)len;
        mem->heap.len -= (Size)len;
    } else {
        status = napi_get_value_string_utf8(env, value, nullptr, 0, &len);
        RG_ASSERT(status == napi_ok);

        buf = AllocateSpan<char>(&call_alloc, (Size)len + 1);

        status = napi_get_value_string_utf8(env, value, buf.ptr, (size_t)buf.len, &len);
        RG_ASSERT(status == napi_ok);

        len++;
    }

    *out_str = buf.ptr;
    return (Size)len;
}

bool CallData::PushString16(Napi::Value value, int directions, const char16_t **out_str16)
{
    if (value.IsString()) {
        if (RG_UNLIKELY(directions & 2)) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected string", GetValueType(instance, value));
            return false;
        }

        PushString16Value(value, out_str16);
        return true;
    } else if (IsNullOrUndefined(value)) {
        *out_str16 = nullptr;
        return true;
    }  else if (value.IsArray()) {
        Napi::Array array = value.As<Napi::Array>();

        if (RG_UNLIKELY(!(directions & 2))) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected string", GetValueType(instance, value));
            return false;
        }
        if (RG_UNLIKELY(array.Length() != 1)) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected [string]", GetValueType(instance, value));
            return false;
        }

        value = array[0u];

        if (RG_UNLIKELY(!value.IsString())) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected [string]", GetValueType(instance, array[0u]));
            return false;
        }

        Size len = PushString16Value(value, out_str16);
        if (RG_UNLIKELY(len < 0))
            return false;

        // Create array type
        TypeInfo *type;
        {
            type = AllocateOne<TypeInfo>(&call_alloc, (int)AllocFlag::Zero);

            type->name = "<temporary>";

            type->primitive = PrimitiveKind::Array;
            type->align = 1;
            type->size = (int32_t)(len * 2);
            type->ref.type = instance->char16_type;
            type->hint = TypeInfo::ArrayHint::String;
        }

        // Prepare output argument
        {
            OutArgument *out = out_arguments.AppendDefault();

            napi_status status = napi_create_reference(env, array, 1, &out->ref);
            RG_ASSERT(status == napi_ok);

            out->ptr = (const uint8_t *)*out_str16;
            out->type = type;
        }

        return true;
    } else {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected string", GetValueType(instance, value));
        return false;
    }
}

Size CallData::PushString16Value(Napi::Value value, const char16_t **out_str16)
{
    Span<char16_t> buf;
    size_t len = 0;
    napi_status status;

    buf.ptr = (char16_t *)mem->heap.ptr;
    buf.len = std::max((Size)0, mem->heap.len - Kibibytes(32)) / 2;

    status = napi_get_value_string_utf16(env, value, buf.ptr, (size_t)buf.len, &len);
    RG_ASSERT(status == napi_ok);

    len++;

    if (RG_LIKELY(len < (size_t)buf.len)) {
        mem->heap.ptr += (Size)len * 2;
        mem->heap.len -= (Size)len * 2;
    } else {
        status = napi_get_value_string_utf16(env, value, nullptr, 0, &len);
        RG_ASSERT(status == napi_ok);

        buf = AllocateSpan<char16_t>(&call_alloc, ((Size)len + 1) * 2);

        status = napi_get_value_string_utf16(env, value, buf.ptr, (size_t)buf.len, &len);
        RG_ASSERT(status == napi_ok);

        len++;
    }

    *out_str16 = buf.ptr;
    return (Size)len;
}

bool CallData::PushObject(Napi::Object obj, const TypeInfo *type, uint8_t *origin, int16_t realign)
{
    RG_ASSERT(IsObject(obj));
    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    for (Size i = 0; i < type->members.len; i++) {
        const RecordMember &member = type->members[i];
        Napi::Value value = obj.Get(member.name);

        if (RG_UNLIKELY(value.IsUndefined())) {
            ThrowError<Napi::TypeError>(env, "Missing expected object property '%1'", member.name);
            return false;
        }

        Size offset = realign ? (i * realign) : member.offset;
        uint8_t *dest = origin + offset;

        switch (member.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                if (RG_UNLIKELY(!value.IsBoolean())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected boolean", GetValueType(instance, value));
                    return false;
                }

                bool b = value.As<Napi::Boolean>();
                *(bool *)dest = b;
            } break;
            case PrimitiveKind::Int8: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                int8_t v = GetNumber<int8_t>(value);
                *(int8_t *)dest = v;
            } break;
            case PrimitiveKind::UInt8: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                uint8_t v = GetNumber<uint8_t>(value);
                *(uint8_t *)dest = v;
            } break;
            case PrimitiveKind::Int16: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                int16_t v = GetNumber<int16_t>(value);
                *(int16_t *)dest = v;
            } break;
            case PrimitiveKind::Int16S: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                int16_t v = GetNumber<int16_t>(value);
                *(int16_t *)dest = ReverseBytes(v);
            } break;
            case PrimitiveKind::UInt16: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                uint16_t v = GetNumber<uint16_t>(value);
                *(uint16_t *)dest = v;
            } break;
            case PrimitiveKind::UInt16S: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                uint16_t v = GetNumber<uint16_t>(value);
                *(uint16_t *)dest = ReverseBytes(v);
            } break;
            case PrimitiveKind::Int32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                int32_t v = GetNumber<int32_t>(value);
                *(int32_t *)dest = v;
            } break;
            case PrimitiveKind::Int32S: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                int32_t v = GetNumber<int32_t>(value);
                *(int32_t *)dest = ReverseBytes(v);
            } break;
            case PrimitiveKind::UInt32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                uint32_t v = GetNumber<uint32_t>(value);
                *(uint32_t *)dest = v;
            } break;
            case PrimitiveKind::UInt32S: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                uint32_t v = GetNumber<uint32_t>(value);
                *(uint32_t *)dest = ReverseBytes(v);
            } break;
            case PrimitiveKind::Int64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                int64_t v = GetNumber<int64_t>(value);
                *(int64_t *)dest = v;
            } break;
            case PrimitiveKind::Int64S: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                int64_t v = GetNumber<int64_t>(value);
                *(int64_t *)dest = ReverseBytes(v);
            } break;
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                uint64_t v = GetNumber<uint64_t>(value);
                *(uint64_t *)dest = v;
            } break;
            case PrimitiveKind::UInt64S: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                uint64_t v = GetNumber<uint64_t>(value);
                *(uint64_t *)dest = ReverseBytes(v);
            } break;
            case PrimitiveKind::String: {
                const char *str;
                if (RG_UNLIKELY(!PushString(value, 1, &str)))
                    return false;

                *(const char **)dest = str;
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16;
                if (RG_UNLIKELY(!PushString16(value, 1, &str16)))
                    return false;

                *(const char16_t **)dest = str16;
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr;
                if (RG_UNLIKELY(!PushPointer(value, member.type, 1, &ptr)))
                    return false;

                *(void **)dest = ptr;
            } break;
            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!IsObject(value))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected object", GetValueType(instance, value));
                    return false;
                }

                Napi::Object obj2 = value.As<Napi::Object>();
                if (!PushObject(obj2, member.type, dest, realign))
                    return false;
            } break;
            case PrimitiveKind::Array: {
                if (value.IsArray()) {
                    Napi::Array array = value.As<Napi::Array>();
                    Size len = (Size)member.type->size / member.type->ref.type->size;

                    if (!PushNormalArray(array, len, member.type, dest, realign))
                        return false;
                } else if (value.IsTypedArray()) {
                    Napi::TypedArray array = value.As<Napi::TypedArray>();
                    Size len = (Size)member.type->size / member.type->ref.type->size;

                    if (!PushTypedArray(array, len, member.type, dest, realign))
                        return false;
                } else if (value.IsString() && !realign) {
                    if (!PushStringArray(value, member.type, dest))
                        return false;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected array", GetValueType(instance, value));
                    return false;
                }
            } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                float f = GetNumber<float>(value);
                *(float *)dest = f;
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                    return false;
                }

                double d = GetNumber<double>(value);
                *(double *)dest = d;
            } break;
            case PrimitiveKind::Callback: {
                void *ptr;

                if (value.IsFunction()) {
                    Napi::Function func = value.As<Napi::Function>();

                    ptr = ReserveTrampoline(member.type->ref.proto, func);
                    if (RG_UNLIKELY(!ptr))
                        return false;
                } else if (CheckValueTag(instance, value, member.type->ref.marker)) {
                    Napi::External<void> external = value.As<Napi::External<void>>();
                    ptr = external.Data();
                } else if (IsNullOrUndefined(value)) {
                    ptr = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected %2", GetValueType(instance, value), member.type->name);
                    return false;
                }

                *(void **)dest = ptr;
            } break;

            case PrimitiveKind::Prototype: { RG_UNREACHABLE(); } break;
        }
    }

    return true;
}

bool CallData::PushNormalArray(Napi::Array array, Size len, const TypeInfo *type, uint8_t *origin, int16_t realign)
{
    RG_ASSERT(array.IsArray());

    const TypeInfo *ref = type->ref.type;

    if (RG_UNLIKELY(array.Length() != (size_t)len)) {
        ThrowError<Napi::Error>(env, "Expected array of length %1, got %2", len, array.Length());
        return false;
    }

    Size offset = 0;

#define PUSH_ARRAY(Check, Expected, GetCode) \
        do { \
            for (Size i = 0; i < len; i++) { \
                Napi::Value value = array[(uint32_t)i]; \
                 \
                int16_t align = std::max(ref->align, realign); \
                 \
                offset = AlignLen(offset, align); \
                uint8_t *dest = origin + offset; \
                 \
                if (RG_UNLIKELY(!(Check))) { \
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected %2", GetValueType(instance, value), (Expected)); \
                    return false; \
                } \
                 \
                GetCode \
                 \
                offset += ref->size; \
            } \
        } while (false)

    switch (ref->primitive) {
        case PrimitiveKind::Void: {
            ThrowError<Napi::TypeError>(env, "Ambigous parameter type %1, use koffi.as(value, type)", type->name); \
            return false;
        } break;

        case PrimitiveKind::Bool: {
            PUSH_ARRAY(value.IsBoolean(), "boolean", {
                bool b = value.As<Napi::Boolean>();
                *(bool *)dest = b;
            });
        } break;
        case PrimitiveKind::Int8: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                int8_t v = GetNumber<int8_t>(value);
                *(int8_t *)dest = v;
            });
        } break;
        case PrimitiveKind::UInt8: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                uint8_t v = GetNumber<uint8_t>(value);
                *(uint8_t *)dest = v;
            });
        } break;
        case PrimitiveKind::Int16: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                int16_t v = GetNumber<int16_t>(value);
                *(int16_t *)dest = v;
            });
        } break;
        case PrimitiveKind::Int16S: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                int16_t v = GetNumber<int16_t>(value);
                *(int16_t *)dest = ReverseBytes(v);
            });
        } break;
        case PrimitiveKind::UInt16: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                uint16_t v = GetNumber<uint16_t>(value);
                *(uint16_t *)dest = v;
            });
        } break;
        case PrimitiveKind::UInt16S: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                uint16_t v = GetNumber<uint16_t>(value);
                *(uint16_t *)dest = ReverseBytes(v);
            });
        } break;
        case PrimitiveKind::Int32: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                int32_t v = GetNumber<int32_t>(value);
                *(int32_t *)dest = v;
            });
        } break;
        case PrimitiveKind::Int32S: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                int32_t v = GetNumber<int32_t>(value);
                *(int32_t *)dest = ReverseBytes(v);
            });
        } break;
        case PrimitiveKind::UInt32: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                uint32_t v = GetNumber<uint32_t>(value);
                *(uint32_t *)dest = v;
            });
        } break;
        case PrimitiveKind::UInt32S: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                uint32_t v = GetNumber<uint32_t>(value);
                *(uint32_t *)dest = ReverseBytes(v);
            });
        } break;
        case PrimitiveKind::Int64: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                int64_t v = GetNumber<int64_t>(value);
                *(int64_t *)dest = v;
            });
        } break;
        case PrimitiveKind::Int64S: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                int64_t v = GetNumber<int64_t>(value);
                *(int64_t *)dest = ReverseBytes(v);
            });
        } break;
        case PrimitiveKind::UInt64: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                uint64_t v = GetNumber<uint64_t>(value);
                *(uint64_t *)dest = v;
            });
        } break;
        case PrimitiveKind::UInt64S: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                uint64_t v = GetNumber<uint64_t>(value);
                *(uint64_t *)dest = ReverseBytes(v);
            });
        } break;
        case PrimitiveKind::String: {
            PUSH_ARRAY(true, "string", {
                const char *str;
                if (RG_UNLIKELY(!PushString(value, 1, &str)))
                    return false;

                *(const char **)dest = str;
            });
        } break;
        case PrimitiveKind::String16: {
            PUSH_ARRAY(true, "string", {
                const char16_t *str16;
                if (RG_UNLIKELY(!PushString16(value, 1, &str16)))
                    return false;

                *(const char16_t **)dest = str16;
            });
        } break;
        case PrimitiveKind::Pointer: {
            PUSH_ARRAY(true, ref->name, {
                void *ptr;
                if (RG_UNLIKELY(!PushPointer(value, ref, 1, &ptr)))
                    return false;

                *(const void **)dest = ptr;
            });
        } break;
        case PrimitiveKind::Record: {
            PUSH_ARRAY(IsObject(value), "object", {
                Napi::Object obj2 = value.As<Napi::Object>();
                if (!PushObject(obj2, ref, dest, realign))
                    return false;
            });
        } break;
        case PrimitiveKind::Array: {
            for (Size i = 0; i < len; i++) {
                Napi::Value value = array[(uint32_t)i];

                int16_t align = std::max(ref->align, realign);
                offset = AlignLen(offset, align);

                uint8_t *dest = origin + offset;

                if (value.IsArray()) {
                    Napi::Array array2 = value.As<Napi::Array>();
                    Size len2 = (Size)ref->size / ref->ref.type->size;

                    if (!PushNormalArray(array2, len2, ref, dest, realign))
                        return false;
                } else if (value.IsTypedArray()) {
                    Napi::TypedArray array2 = value.As<Napi::TypedArray>();
                    Size len2 = (Size)ref->size / ref->ref.type->size;

                    if (!PushTypedArray(array2, len2, ref, dest, realign))
                        return false;
                } else if (value.IsString() && !realign) {
                    if (!PushStringArray(value, ref, dest))
                        return false;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected array", GetValueType(instance, value));
                    return false;
                }

                offset += ref->size;
            }
        } break;
        case PrimitiveKind::Float32: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                float f = GetNumber<float>(value);
                *(float *)dest = f;
            });
        } break;
        case PrimitiveKind::Float64: {
            PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                double d = GetNumber<double>(value);
                *(double *)dest = d;
            });
        } break;
        case PrimitiveKind::Callback: {
            for (Size i = 0; i < len; i++) {
                Napi::Value value = array[(uint32_t)i];

                int16_t align = std::max(ref->align, realign);
                offset = AlignLen(offset, align);

                uint8_t *dest = origin + offset;

                void *ptr;

                if (value.IsFunction()) {
                    Napi::Function func = value.As<Napi::Function>();

                    ptr = ReserveTrampoline(ref->ref.proto, func);
                    if (RG_UNLIKELY(!ptr))
                        return false;
                } else if (CheckValueTag(instance, value, ref->ref.marker)) {
                    Napi::External<void> external = value.As<Napi::External<void>>();
                    ptr = external.Data();
                } else if (IsNullOrUndefined(value)) {
                    ptr = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected %2", GetValueType(instance, value), ref->name);
                    return false;
                }

                *(void **)dest = ptr;

                offset += ref->size;
            }
        } break;

        case PrimitiveKind::Prototype: { RG_UNREACHABLE(); } break;
    }

#undef PUSH_ARRAY

    return true;
}

bool CallData::PushTypedArray(Napi::TypedArray array, Size len, const TypeInfo *type, uint8_t *origin, int16_t realign)
{
    RG_ASSERT(array.IsTypedArray());

    const TypeInfo *ref = type->ref.type;

    if (RG_UNLIKELY(array.ElementLength() != (size_t)len)) {
        ThrowError<Napi::Error>(env, "Expected array of length %1, got %2", len, array.ElementLength());
        return false;
    }

    const uint8_t *buf = (const uint8_t *)array.ArrayBuffer().Data();

    if (RG_UNLIKELY(array.TypedArrayType() != GetTypedArrayType(ref) &&
                    ref != instance->void_type)) {
        ThrowError<Napi::TypeError>(env, "Cannot use %1 value for %2 array", GetValueType(instance, array), ref->name);
        return false;
    }

    if (realign) {
        Size offset = 0;
        Size size = (Size)array.ElementSize();

        for (Size i = 0; i < len; i++) {
            offset = AlignLen(offset, realign);

            uint8_t *dest = origin + offset;
            const uint8_t *src = buf + i * size;

            memcpy(dest, src, size);
            offset += size;
        }
    } else {
        memcpy_safe(origin, buf, (size_t)array.ByteLength());
    }

    return true;
}

bool CallData::PushStringArray(Napi::Value obj, const TypeInfo *type, uint8_t *origin)
{
    RG_ASSERT(obj.IsString());
    RG_ASSERT(type->primitive == PrimitiveKind::Array);

    size_t encoded = 0;

    switch (type->ref.type->primitive) {
        case PrimitiveKind::Int8: {
            napi_status status = napi_get_value_string_utf8(env, obj, (char *)origin, type->size, &encoded);
            RG_ASSERT(status == napi_ok);
        } break;
        case PrimitiveKind::Int16: {
            napi_status status = napi_get_value_string_utf16(env, obj, (char16_t *)origin, type->size / 2, &encoded);
            RG_ASSERT(status == napi_ok);

            encoded *= 2;
        } break;

        default: {
            ThrowError<Napi::TypeError>(env, "Strings cannot be converted to %1 array", type->ref.type->name);
            return false;
        } break;
    }

    memset_safe(origin + encoded, 0, type->size - encoded);

    return true;
}

bool CallData::PushPointer(Napi::Value value, const TypeInfo *type, int directions, void **out_ptr)
{
    if (CheckValueTag(instance, value, &CastMarker)) {
        Napi::External<ValueCast> external = value.As<Napi::External<ValueCast>>();
        ValueCast *cast = external.Data();

        value = cast->ref.Value();
        type = cast->type;
    }

    switch (value.Type()) {
        case napi_undefined:
        case napi_null: {
            *out_ptr = nullptr;
            return true;
        } break;

        case napi_external: {
            RG_ASSERT(type->primitive == PrimitiveKind::Pointer);

            if (RG_UNLIKELY(!CheckValueTag(instance, value, type->ref.marker) &&
                            !CheckValueTag(instance, value, instance->void_type) &&
                            type->ref.type != instance->void_type))
                goto unexpected;

            *out_ptr = value.As<Napi::External<uint8_t>>().Data();
            return true;
        } break;

        case napi_object: {
            uint8_t *ptr = nullptr;

            if (value.IsArray()) {
                Napi::Array array = value.As<Napi::Array>();

                Size len = (Size)array.Length();
                Size size = len * type->ref.type->size;

                ptr = AllocHeap(size, 16);

                if (directions & 1) {
                    if (!PushNormalArray(array, len, type, ptr))
                        return false;
                } else {
                    memset(ptr, 0, size);
                }
            } else if (value.IsTypedArray()) {
                Napi::TypedArray array = value.As<Napi::TypedArray>();

                Size len = (Size)array.ElementLength();
                Size size = (Size)array.ByteLength();

                ptr = AllocHeap(size, 16);

                if (directions & 1) {
                    if (!PushTypedArray(array, len, type, ptr))
                        return false;
                } else {
                    if (RG_UNLIKELY(array.TypedArrayType() != GetTypedArrayType(type->ref.type) &&
                                    type->ref.type != instance->void_type)) {
                        ThrowError<Napi::TypeError>(env, "Cannot use %1 value for %2 array", GetValueType(instance, array), type->ref.type->name);
                        return false;
                    }

                    memset(ptr, 0, size);
                }
            } else if (RG_LIKELY(type->ref.type->primitive == PrimitiveKind::Record)) {
                Napi::Object obj = value.As<Napi::Object>();
                RG_ASSERT(IsObject(value));

                ptr = AllocHeap(type->ref.type->size, 16);

                if (directions & 1) {
                    if (!PushObject(obj, type->ref.type, ptr))
                        return false;
                } else {
                    memset(ptr, 0, type->size);
                }
            } else {
                goto unexpected;
            }

            if (directions & 2) {
                OutArgument *out = out_arguments.AppendDefault();

                napi_status status = napi_create_reference(env, value, 1, &out->ref);
                RG_ASSERT(status == napi_ok);

                out->ptr = ptr;
                out->type = type->ref.type;
            }

            *out_ptr = ptr;
            return true;
        } break;

        default: {} break;
    }

unexpected:
    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected %2", GetValueType(instance, value), type->name);
    return false;
}

static inline Napi::Value GetReferenceValue(Napi::Env env, napi_ref ref)
{
    napi_value value;

    napi_status status = napi_get_reference_value(env, ref, &value);
    RG_ASSERT(status == napi_ok);

    return Napi::Value(env, value);
}

void CallData::PopOutArguments()
{
    for (const OutArgument &out: out_arguments) {
        Napi::Value value = GetReferenceValue(env, out.ref);
        RG_ASSERT(!value.IsEmpty());

        if (value.IsArray()) {
            Napi::Array array(env, value);
            DecodeNormalArray(array, out.ptr, out.type);
        } else if (value.IsTypedArray()) {
            Napi::TypedArray array(env, value);
            DecodeTypedArray(array, out.ptr, out.type);
        } else {
            Napi::Object obj(env, value);
            DecodeObject(obj, out.ptr, out.type);
        }

        if (out.type->dispose) {
            out.type->dispose(env, out.type, out.ptr);
        }
    }
}

void *CallData::ReserveTrampoline(const FunctionInfo *proto, Napi::Function func)
{
    int16_t idx;
    {
        std::lock_guard<std::mutex> lock(shared.mutex);

        if (RG_UNLIKELY(!shared.available.len)) {
            ThrowError<Napi::Error>(env, "Too many callbacks are in use (max = %1)", MaxTrampolines);
            return env.Null();
        }
        if (RG_UNLIKELY(!used_trampolines.Available())) {
            ThrowError<Napi::Error>(env, "This call uses too many temporary callbacks (max = %1)", RG_LEN(used_trampolines.data));
            return env.Null();
        }

        idx = shared.available.data[--shared.available.len];
        used_trampolines.Append(idx);
    }

    TrampolineInfo *trampoline = &shared.trampolines[idx];

    trampoline->proto = proto;
    trampoline->func.Reset(func, 1);
    trampoline->recv.Reset();
    trampoline->generation = (int32_t)mem->generation;

    void *ptr = GetTrampoline(idx, proto);

    return ptr;
}

void CallData::DumpForward(const FunctionInfo *func) const
{
    PrintLn(stderr, "%!..+---- %1 (%2) ----%!0", func->name, CallConventionNames[(int)func->convention]);

    if (func->parameters.len) {
        PrintLn(stderr, "Parameters:");
        for (Size i = 0; i < func->parameters.len; i++) {
            const ParameterInfo &param = func->parameters[i];
            PrintLn(stderr, "  %1 = %2 (%3)", i, param.type->name, FmtMemSize(param.type->size));
        }
    }
    PrintLn(stderr, "Return: %1 (%2)", func->ret.type->name, FmtMemSize(func->ret.type->size));

    Span<const uint8_t> stack = MakeSpan(mem->stack.end(), old_stack_mem.end() - mem->stack.end());
    Span<const uint8_t> heap = MakeSpan(old_heap_mem.ptr, mem->heap.ptr - old_heap_mem.ptr);

    DumpMemory("Stack", stack);
    DumpMemory("Heap", heap);
}

}
