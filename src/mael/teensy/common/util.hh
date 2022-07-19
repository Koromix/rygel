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

#include <Arduino.h>
#include <assert.h>

#define DEG2RAD 0.01745329251
#define RAD2DEG 57.2957795457

#define STRINGIFY_(A) #A
#define STRINGIFY(A) STRINGIFY_(A)
#define CONCAT_(A, B) A ## B
#define CONCAT(A, B) CONCAT_(A, B)
#define UNIQUE_NAME(Prefix) CONCAT(Prefix, __COUNTER__)

#define STATIC_ASSERT(Cond) \
    static_assert((Cond), STRINGIFY(Cond))

template <typename T, unsigned N>
char (&ComputeArraySize(T const (&)[N]))[N];
#define LEN(Array) sizeof(ComputeArraySize(Array))

class WaitFor {
    unsigned long delay;
    unsigned long last_execution;

public:
    WaitFor(long delay_us) : delay((unsigned long)delay_us) {}

    bool Test()
    {
        unsigned long now = micros();

        if (last_execution < now - delay) {
            last_execution += delay;
            return true;
        } else {
            return false;
        }
    };
};

#define PROCESS_EVERY_(VarName, DelayUs) \
    static WaitFor VarName(DelayUs); \
    if (!VarName.Test()) \
        return;
#define PROCESS_EVERY(DelayUs) PROCESS_EVERY_(UNIQUE_NAME(wf_), (DelayUs))

template <typename Fun>
class DeferGuard {
    DeferGuard(const DeferGuard&) = delete;
    DeferGuard &operator=(const DeferGuard&) = delete;

    Fun f;
    bool enabled;

public:
    DeferGuard() = delete;
    DeferGuard(Fun f_, bool enable = true) : f(std::move(f_)), enabled(enable) {}
    ~DeferGuard()
    {
        if (enabled) {
            f();
        }
    }

    DeferGuard(DeferGuard &&other)
        : f(std::move(other.f)), enabled(other.enabled)
    {
        other.enabled = false;
    }

    void Disable() { enabled = false; }
};

// Honestly, I don't understand all the details in there, this comes from Andrei Alexandrescu.
// https://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
struct DeferGuardHelper {};
template <typename Fun>
DeferGuard<Fun> operator+(DeferGuardHelper, Fun &&f)
{
    return DeferGuard<Fun>(std::forward<Fun>(f));
}

// Write 'DEFER { code };' to do something at the end of the current scope, you
// can use DEFER_N(Name) if you need to disable the guard for some reason, and
// DEFER_NC(Name, Captures) if you need to capture values.
#define DEFER \
    auto UNIQUE_NAME(defer) = DeferGuardHelper() + [&]()
#define DEFER_N(Name) \
    auto Name = DeferGuardHelper() + [&]()
#define DEFER_C(...) \
    auto UNIQUE_NAME(defer) = DeferGuardHelper() + [&, __VA_ARGS__]()
#define DEFER_NC(Name, ...) \
    auto Name = DeferGuardHelper() + [&, __VA_ARGS__]()
