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

#if defined(__i386__) || defined(_M_IX86)

#include "vendor/libcc/libcc.hh"
#include "ffi.hh"
#include "call.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

extern "C" uint64_t ForwardCallG(const void *func, uint8_t *sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp);
extern "C" double ForwardCallD(const void *func, uint8_t *sp);

bool AnalyseFunction(FunctionInfo *func)
{
#ifdef _WIN32
    if (func->convention == CallConvention::Stdcall) {
        Size total = 0;
        for (const ParameterInfo &param: func->parameters) {
            total += param.type->size;
        }

        func->decorated_name = Fmt(&func->lib->str_alloc, "_%1@%2", func->name, total).ptr;
    }
#endif

    if (IsIntegral(func->ret.type->primitive)) {
        func->ret.trivial = true;
    } else if (func->ret.type->members.len == 1 && IsIntegral(func->ret.type->members[0].type->primitive)) {
        func->ret.trivial = true;
#ifdef _WIN32
    } else if (func->ret.type->members.len == 2 && IsIntegral(func->ret.type->members[0].type->primitive)
                                                && IsIntegral(func->ret.type->members[1].type->primitive)) {
        func->ret.trivial = true;
#endif
    }

    return true;
}

Napi::Value TranslateCall(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    FunctionInfo *func = (FunctionInfo *)info.Data();
    LibraryData *lib = func->lib.get();

    RG_DEFER { lib->tmp_alloc.ReleaseAll(); };

    // Sanity checks
    if (info.Length() < (uint32_t)func->parameters.len) {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", func->parameters.len, info.Length());
        return env.Null();
    }

    // Stack pointer and register
    uint8_t *top_ptr = lib->stack.end();
    uint8_t *return_ptr = nullptr;
    uint8_t *args_ptr = nullptr;
    uint8_t *sp_ptr = nullptr;

    // Reserve space for return value if needed
    if (func->ret.trivial) {
        args_ptr = top_ptr - func->scratch_size;
        sp_ptr = args_ptr;
    } else {
        return_ptr = top_ptr - AlignLen(func->ret.type->size, 16);
        args_ptr = return_ptr - func->scratch_size;
        sp_ptr = args_ptr;

        *(uint32_t *)args_ptr = (uint32_t)return_ptr;
        args_ptr += 4;
    }

    RG_ASSERT(AlignUp(lib->stack.ptr, 16) == lib->stack.ptr);
    RG_ASSERT(AlignUp(lib->stack.end(), 16) == lib->stack.end());
    RG_ASSERT(AlignUp(args_ptr, 16) == args_ptr);

    // Push arguments
    for (Size i = 0; i < func->parameters.len; i++) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[i];

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                if (RG_UNLIKELY(!value.IsBoolean())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected boolean", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                bool b = value.As<Napi::Boolean>();
                *(bool *)args_ptr = b;
                args_ptr += 4;
            } break;
            case PrimitiveKind::Int8:
            case PrimitiveKind::UInt8:
            case PrimitiveKind::Int16:
            case PrimitiveKind::UInt16:
            case PrimitiveKind::Int32:
            case PrimitiveKind::UInt32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                int32_t v = CopyNodeNumber<int32_t>(value);
                *(uint32_t *)args_ptr = (uint32_t)v;
                args_ptr += 4;
            } break;
            case PrimitiveKind::Int64:
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                int64_t v = CopyNodeNumber<int64_t>(value);
                *(uint64_t *)args_ptr = (uint64_t)v;
                args_ptr += 8;
            } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                float f = CopyNodeNumber<float>(value);
                *(float *)args_ptr = f;
                args_ptr += 4;
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                double d = CopyNodeNumber<double>(value);
                *(double *)args_ptr = d;
                args_ptr += 8;
            } break;
            case PrimitiveKind::String: {
                if (RG_UNLIKELY(!value.IsString())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected string", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                const char *str = CopyNodeString(value, &lib->tmp_alloc);
                *(const char **)args_ptr = str;
                args_ptr += 4;
            } break;
            case PrimitiveKind::Pointer: {
                if (RG_UNLIKELY(!CheckValueTag(instance, value, param.type))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected %3", GetValueType(instance, value), i + 1, param.type->name);
                    return env.Null();
                }

                void *ptr = value.As<Napi::External<void>>();
                *(void **)args_ptr = ptr;
                args_ptr += 4;
            } break;

            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!value.IsObject())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected object", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                Napi::Object obj = value.As<Napi::Object>();

                args_ptr = AlignUp(args_ptr, param.type->align);
                if (!PushObject(obj, param.type, &lib->tmp_alloc, args_ptr))
                    return env.Null();
                args_ptr += param.type->size;
            } break;
        }
    }

    // DumpStack(func, MakeSpan(sp_ptr, top_ptr - sp_ptr));

    // Execute and convert return value
    switch (func->ret.type->primitive) {
        case PrimitiveKind::Float32: {
            float f = ForwardCallF(func->func, sp_ptr);

            return Napi::Number::New(env, (double)f);
        } break;

        case PrimitiveKind::Float64: {
            double d = ForwardCallD(func->func, sp_ptr);

            return Napi::Number::New(env, d);
        } break;

        default: {
            // We can't directly use the struct as a return value, because not all platforms
            // treat it the same: it is trivial only on Windows (see AnalyseFunction).
            uint64_t raw = ForwardCallG(func->func, sp_ptr);
            struct {
                uint32_t rax;
                uint32_t rdx;
            } ret;
            memcpy(&ret, &raw, RG_SIZE(raw));

            switch (func->ret.type->primitive) {
                case PrimitiveKind::Void: return env.Null();
                case PrimitiveKind::Bool: return Napi::Boolean::New(env, ret.rax);
                case PrimitiveKind::Int8: return Napi::Number::New(env, (double)ret.rax);
                case PrimitiveKind::UInt8: return Napi::Number::New(env, (double)ret.rax);
                case PrimitiveKind::Int16: return Napi::Number::New(env, (double)ret.rax);
                case PrimitiveKind::UInt16: return Napi::Number::New(env, (double)ret.rax);
                case PrimitiveKind::Int32: return Napi::Number::New(env, (double)ret.rax);
                case PrimitiveKind::UInt32: return Napi::Number::New(env, (double)ret.rax);
                case PrimitiveKind::Int64: return Napi::BigInt::New(env, (int64_t)raw);
                case PrimitiveKind::UInt64: return Napi::BigInt::New(env, raw);
                case PrimitiveKind::Float32: { RG_UNREACHABLE(); } break;
                case PrimitiveKind::Float64: { RG_UNREACHABLE(); } break;
                case PrimitiveKind::String: return Napi::String::New(env, (const char *)ret.rax);
                case PrimitiveKind::Pointer: {
                    void *ptr = (void *)ret.rax;

                    Napi::External<void> external = Napi::External<void>::New(env, ptr);
                    SetValueTag(instance, external, func->ret.type);

                    return external;
                } break;

                case PrimitiveKind::Record: {
                    const uint8_t *ptr = return_ptr ? return_ptr : (const uint8_t *)&ret;
                    Napi::Object obj = PopObject(env, ptr, func->ret.type);
                    return obj;
                } break;
            }
        } break;
    }

    RG_UNREACHABLE();
}

}

#endif
