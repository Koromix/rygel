// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

type LoadOptions = {
    lazy?: boolean,
    global?: boolean,
    deep?: boolean
};

export function load(path: string | null, options?: LoadOptions): IKoffiLib;

interface IKoffiCType { __brand: 'IKoffiCType' }
interface IKoffiPointerCast { __brand: 'IKoffiPointerCast' }
interface IKoffiRegisteredCallback { __brand: 'IKoffiRegisteredCallback' }

type PrimitiveKind = 'Void' | 'Bool' | 'Int8' | 'UInt8' | 'Int16' | 'Int16S' | 'UInt16' | 'UInt16S' |
                     'Int32' | 'Int32S' | 'UInt32' | 'UInt32S' | 'Int64' | 'Int64S' | 'UInt64' | 'UInt64S' |
                     'String' | 'String16' | 'Pointer' | 'Record' | 'Union' | 'Array' | 'Float32' | 'Float64' |
                     'Prototype' | 'Callback';
type ArrayHint = 'Array' | 'Typed' | 'String';

type TypeSpec = string | IKoffiCType;
type TypeSpecWithAlignment = TypeSpec | [number, TypeSpec];
type TypeInfo = {
    name: string;
    primitive: PrimitiveKind;
    size: number;
    alignment: number;
    disposable: boolean;
    length?: number;
    hint?: ArrayHint;
    ref?: IKoffiCType;
    members?: Record<string, { name: string, type: IKoffiCType, offset: number }>;
};
type KoffiFunction = {
    (...args: any[]) : any;
    async: (...args: any[]) => any;
    info: {
        name: string,
        arguments: IKoffiCType[],
        result: IKoffiCType
    };
};

export type KoffiFunc<T extends (...args: any) => any> = T & {
   async: (...args: [...Parameters<T>, (err: any, result: ReturnType<T>) => void]) => void;
   info: {
      name: string;
      arguments: IKoffiCType[];
      result: IKoffiCType;
   };
};

export interface IKoffiLib {
    func(definition: string): KoffiFunction;
    func(name: string | number, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;
    func(convention: string, name: string | number, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;

    /** @deprecated */ cdecl(definition: string): KoffiFunction;
    /** @deprecated */ cdecl(name: string | number, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;
    /** @deprecated */ stdcall(definition: string): KoffiFunction;
    /** @deprecated */ stdcall(name: string | number, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;
    /** @deprecated */ fastcall(definition: string): KoffiFunction;
    /** @deprecated */ fastcall(name: string | number, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;
    /** @deprecated */ thiscall(definition: string): KoffiFunction;
    /** @deprecated */ thiscall(name: string | number, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;

    symbol(name: string, type: TypeSpec): any;

    unload(): void;
}

export function struct(name: string | null | undefined, def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
export function struct(ref: IKoffiCType, def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
export function struct(def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
export function pack(name: string | null | undefined, def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
export function pack(ref: IKoffiCType, def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
export function pack(def: Record<string, TypeSpecWithAlignment>): IKoffiCType;

export function union(name: string | null | undefined, def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
export function union(ref: IKoffiCType, def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
export function union(def: Record<string, TypeSpecWithAlignment>): IKoffiCType;

export class Union {
    constructor(type: TypeSpec);
    [s: string]: any;
}

export function array(ref: TypeSpec, len: number, hint?: ArrayHint | null): IKoffiCType;
export function array(ref: TypeSpec, countedBy: string, maxLen: number, hint?: ArrayHint | null): IKoffiCType;

export function opaque(name: string | null | undefined): IKoffiCType;
export function opaque(): IKoffiCType;
/** @deprecated */ export function handle(name: string | null | undefined): IKoffiCType;
/** @deprecated */ export function handle(): IKoffiCType;

export function pointer(ref: TypeSpec): IKoffiCType;
export function pointer(ref: TypeSpec, count: number): IKoffiCType;
export function pointer(name: string | null | undefined, ref: TypeSpec, countedBy?: string | null): IKoffiCType;
export function pointer(name: string | null | undefined, ref: TypeSpec, count: number): IKoffiCType;

export function out(type: TypeSpec): IKoffiCType;
export function inout(type: TypeSpec): IKoffiCType;

export function disposable(type: TypeSpec): IKoffiCType;
export function disposable(type: TypeSpec, freeFunction: Function): IKoffiCType;
export function disposable(name: string | null | undefined, type: TypeSpec): IKoffiCType;
export function disposable(name: string | null | undefined, type: TypeSpec, freeFunction: Function): IKoffiCType;

export function proto(definition: string): IKoffiCType;
export function proto(result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;
export function proto(convention: string, result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;
export function proto(name: string | null | undefined, result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;
export function proto(convention: string, name: string | null | undefined, result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;
/** @deprecated */ export function callback(definition: string): IKoffiCType;
/** @deprecated */ export function callback(result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;
/** @deprecated */ export function callback(convention: string, result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;
/** @deprecated */ export function callback(name: string | null | undefined, result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;
/** @deprecated */ export function callback(convention: string, name: string | null | undefined, result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;

export function register(callback: Function, type: TypeSpec): IKoffiRegisteredCallback;
export function register(thisValue: any, callback: Function, type: TypeSpec): IKoffiRegisteredCallback;
export function unregister(callback: IKoffiRegisteredCallback): void;

export function as(value: any, type: TypeSpec): IKoffiPointerCast;
export function decode(value: any, type: TypeSpec): any;
export function decode(value: any, type: TypeSpec, len: number): any;
export function decode(value: any, offset: number, type: TypeSpec): any;
export function decode(value: any, offset: number, type: TypeSpec, len: number): any;
export function address(value: any): bigint;
export function call(value: any, type: TypeSpec, ...args: any[]): any;
export function encode(ref: any, type: TypeSpec, value: any): void;
export function encode(ref: any, type: TypeSpec, value: any, len: number): void;
export function encode(ref: any, offset: number, type: TypeSpec): void;
export function encode(ref: any, offset: number, type: TypeSpec, value: any): void;
export function encode(ref: any, offset: number, type: TypeSpec, value: any, len: number): void;
export function view(ref: any, len: number): ArrayBuffer;

export function sizeof(type: TypeSpec): number;
export function alignof(type: TypeSpec): number;
export function offsetof(type: TypeSpec, member_name: string): number;
export function resolve(type: TypeSpec): IKoffiCType;
export function introspect(type: TypeSpec): TypeInfo;

export function alias(name: string, type: TypeSpec): IKoffiCType;

type KoffiConfig = {
    sync_stack_size: number
    sync_heap_size: number
    async_stack_size: number
    async_heap_size: number
    resident_async_pools: number
    max_async_calls: number
    max_type_size: number
};
type KoffiStats = {
    disposed: number
};

export function config(): KoffiConfig;
export function config(cfg: KoffiConfig): KoffiConfig;
export function stats(): KoffiStats;

export function alloc(type: TypeSpec, length: number): any;
export function free(value: any): void;

export function errno(): number;
export function errno(value: number): number;

export function reset(): void;

export const internal: Boolean;
export const extension: String;

export const os: {
    errno: Record<string, number>
};

// https://koffi.dev/input#standard-types
type PrimitiveTypes =
    | 'bool'
    | 'char'
    | 'char16_t'
    | 'char16'
    | 'char32_t'
    | 'char32'
    | 'double'
    | 'float'
    | 'float32'
    | 'float64'
    | 'int'
    | 'int8_t'
    | 'int8'
    | 'int16_be_t'
    | 'int16_be'
    | 'int16_le_t'
    | 'int16_le'
    | 'int16_t'
    | 'int16'
    | 'int32_be_t'
    | 'int32_be'
    | 'int32_le_t'
    | 'int32_le'
    | 'int32_t'
    | 'int32'
    | 'int64_be_t'
    | 'int64_be'
    | 'int64_le_t'
    | 'int64_le'
    | 'int64_t'
    | 'int64'
    | 'intptr_t'
    | 'intptr'
    | 'long long'
    | 'long'
    | 'longlong'
    | 'short'
    | 'size_t'
    | 'str'
    | 'str16'
    | 'str32'
    | 'string'
    | 'string16'
    | 'string32'
    | 'uchar'
    | 'uint'
    | 'uint8_t'
    | 'uint8'
    | 'uint16_be_t'
    | 'uint16_be'
    | 'uint16_le_t'
    | 'uint16_le'
    | 'uint16_t'
    | 'uint16'
    | 'uint32_be_t'
    | 'uint32_be'
    | 'uint32_le_t'
    | 'uint32_le'
    | 'uint32_t'
    | 'uint32'
    | 'uint64_be_t'
    | 'uint64_be'
    | 'uint64_le_t'
    | 'uint64_le'
    | 'uint64_t'
    | 'uint64'
    | 'uintptr_t'
    | 'uintptr'
    | 'ulong'
    | 'ulonglong'
    | 'unsigned char'
    | 'unsigned int'
    | 'unsigned long long'
    | 'unsigned long'
    | 'unsigned short'
    | 'ushort'
    | 'void'
    | 'wchar'
    | 'wchar_t';
export const types: Record<PrimitiveTypes, IKoffiCType>;

export interface IKoffiPollOptions {
    readable?: boolean;
    writable?: boolean;
    disconnect?: boolean;
}

export interface IKoffiPollEvents {
    readable: boolean;
    writable: boolean;
    disconnect: boolean;
}

export namespace node {
    export const env: { __brand: 'IKoffiNodeEnv' };

    export class PollHandle {
        start(opts: IKoffiPollOptions, callback: (ev: IKoffiPollEvents) => void): void;
        start(callback: (ev: IKoffiPollEvents) => void): void;
        stop(): void;
        close(): void;
        unref(): void;
        ref(): void;
    }

    export function poll(fd: number, opts: IKoffiPollOptions, callback: (ev: IKoffiPollEvents) => void): PollHandle;
    export function poll(fd: number, callback: (ev: IKoffiPollEvents) => void): PollHandle;
}
