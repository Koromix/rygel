declare module "koffi" {

    export function load(path: string): IKoffiLib;

    interface IKoffiCType { _brand: "IKoffiCType" }
    type CType = string | IKoffiCType;
    type CTypeWithAlignment = CType | [number, CType];
    type KoffiFunction = Function | { async: Function };


    export interface IKoffiLib {
        func(definition: string): KoffiFunction;
        func(name: string, result: CType, arguments: CType[]): KoffiFunction;

        cdecl(definition: string): KoffiFunction;
        cdecl(name: string, result: CType, arguments: CType[]): KoffiFunction;

        stdcall(definition: string): KoffiFunction;
        stdcall(name: string, result: CType, arguments: CType[]): KoffiFunction;

        fastcall(definition: string): KoffiFunction;
        fastcall(name: string, result: CType, arguments: CType[]): KoffiFunction;

        thiscall(definition: string): KoffiFunction;
        thiscall(name: string, result: CType, arguments: CType[]): KoffiFunction;
    }

    export function struct(name: string, def: Record<string, CTypeWithAlignment>): CType;
    export function struct(def: Record<string, CTypeWithAlignment>): CType;

    export function pack(name: string, def: Record<string, CTypeWithAlignment>): CType;
    export function pack(def: Record<string, CTypeWithAlignment>): CType;

    export function opaque(name: string): CType;
    export function opaque(): CType;

    export function pointer(value: CType): CType;
    export function pointer(value: CType, asteriskCount: number): CType;
    export function pointer(name: string, value: CType, asteriskCount: number): CType;

    export function out(value: CType): CType;
    export function inout(value: CType): CType;

    export function as(value: {}, type: CType): {};

    export function disposable(type: CType): CType;
    export function disposable(name: string, type: CType): CType;
    export function disposable(name: string, type: CType, freeFunction: Function): CType;

    export function callback(definition: string): CType;
    export function callback(name: string, result: CType, arguments: CType[]): CType;

    interface IKoffiRegisteredCallback { __brand: "IKoffiRegisteredCallback" }
    export function register(callback: Function, type: CType): IKoffiRegisteredCallback;
    export function register(thisValue: any, callback: Function, type: CType): IKoffiRegisteredCallback;
    export function unregister(callback: IKoffiRegisteredCallback): void;

    export function decode(value: {}, type: CType): {};
    export function decode(value: {}, type: CType, len: number): {};
    export function decode(value: {}, offset: number, type: CType): {};
    export function decode(value: {}, offset: number, type: CType, len: number): {};

    export function sizeof(type: CType): number;
    export function alignof(type: CType): number;
    export function offsetof(type: CType): number;
    export function resolve(type: CType): {};
    export function introspect(type: CType): { name: string, primitive: string, size: number, alignment: number, members: Record<string, { name: string, type: {}, offset: number }> };

    export function alias(name: string, type: CType);

    export function config(): {}
    export function config({ }): {}
}
