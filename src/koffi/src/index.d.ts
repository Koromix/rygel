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

declare module 'koffi' {
    export function load(path: string): IKoffiLib;

    interface IKoffiCType { __brand: 'IKoffiCType' }
    interface IKoffiPointerCast { __brand: 'IKoffiPointerCast' }
    interface IKoffiRegisteredCallback { __brand: 'IKoffiRegisteredCallback' }

    type TypeSpec = string | IKoffiCType;
    type TypeSpecWithAlignment = TypeSpec | [number, TypeSpec];
    type TypeInfo = {
        name: string;
        primitive: string;
        size: number;
        alignment: number;
        disposable: boolean;
        length: number;
        ref: IKoffiCType;
        members: Record<string, { name: string, type: IKoffiCType, offset: number }>;
    };
    type KoffiFunction = Function & { async: Function };

    export interface IKoffiLib {
        func(definition: string): KoffiFunction;
        func(name: string, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;

        cdecl(definition: string): KoffiFunction;
        cdecl(name: string, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;

        stdcall(definition: string): KoffiFunction;
        stdcall(name: string, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;

        fastcall(definition: string): KoffiFunction;
        fastcall(name: string, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;

        thiscall(definition: string): KoffiFunction;
        thiscall(name: string, result: TypeSpec, arguments: TypeSpec[]): KoffiFunction;
    }

    export function struct(name: string, def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
    export function struct(def: Record<string, TypeSpecWithAlignment>): IKoffiCType;

    export function pack(name: string, def: Record<string, TypeSpecWithAlignment>): IKoffiCType;
    export function pack(def: Record<string, TypeSpecWithAlignment>): IKoffiCType;

    export function opaque(name: string): IKoffiCType;
    export function opaque(): IKoffiCType;

    export function pointer(value: TypeSpec): IKoffiCType;
    export function pointer(value: TypeSpec, asteriskCount: number): IKoffiCType;
    export function pointer(name: string, value: TypeSpec, asteriskCount: number): IKoffiCType;

    export function out(value: TypeSpec): IKoffiCType;
    export function inout(value: TypeSpec): IKoffiCType;

    export function as(value: any, type: TypeSpec): IKoffiPointerCast;

    export function disposable(type: TypeSpec): IKoffiCType;
    export function disposable(name: string, type: TypeSpec): IKoffiCType;
    export function disposable(name: string, type: TypeSpec, freeFunction: Function): IKoffiCType;

    export function callback(definition: string): IKoffiCType;
    export function callback(name: string, result: TypeSpec, arguments: TypeSpec[]): IKoffiCType;

    export function register(callback: Function, type: TypeSpec): IKoffiRegisteredCallback;
    export function register(thisValue: any, callback: Function, type: TypeSpec): IKoffiRegisteredCallback;
    export function unregister(callback: IKoffiRegisteredCallback): void;

    export function decode(value: any, type: TypeSpec): any;
    export function decode(value: any, type: TypeSpec, len: number): any;
    export function decode(value: any, offset: number, type: TypeSpec): any;
    export function decode(value: any, offset: number, type: TypeSpec, len: number): any;

    export function sizeof(type: TypeSpec): number;
    export function alignof(type: TypeSpec): number;
    export function offsetof(type: TypeSpec): number;
    export function resolve(type: TypeSpec): IKoffiCType;
    export function introspect(type: TypeSpec): TypeInfo;

    export function alias(name: string, type: TypeSpec): IKoffiCType;

    export function config(): Record<string, unknown>;
    export function config(cfg: Record<string, unknown>): Record<string, unknown>;
}
