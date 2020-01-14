// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"

namespace RG {

template <typename T>
struct Animation {
    T start_time;
    T end_time = std::numeric_limits<T>::min();
    T (*animator)(T relative_time);

    Animation() = default;
    Animation(T start_time, T end_time, T (*animator)(T relative_time))
        : start_time(start_time), end_time(end_time), animator(animator) {}

    bool Running(T time) const { return (time < end_time); }

    template<typename U>
    U Compute(U start_value, U end_value, T time) const
    {
        T relative_time = (time - start_time) / (end_time - start_time);
        return start_value + (U)(animator(relative_time) * (end_value - start_value));
    }
};

template <typename T, typename U>
T Animate(const Animation<U> &animation, T start_value, T end_value, U time)
{
    if (time >= animation.end_time) {
        return end_value;
    } else if (time <= animation.start_time) {
        return start_value;
    } else {
        return animation.Compute(start_value, end_value, time);
    }
}

template <typename T, typename U>
class AnimatedValue {
public:
    T value;
    T start_value, end_value;
    Animation<U> animation;

    AnimatedValue(T value = {}) : value(value), end_value(value) {}
    AnimatedValue(T start_value, T end_value, U start_time, U end_time,
                  U (*animator)(U relative_time))
        : value(start_value), start_value(start_value), end_value(end_value),
          animation(start_time, end_time, animator) {}

    operator T() const { return value; }

    void Update(U time)
    {
        value = Animate(animation, start_value, end_value, time);
    }
};

template <typename T, typename U>
AnimatedValue<T, U> MakeAnimatedValue(T start_value, T end_value, U start_time, U end_time,
                                      U (*animator)(U relative_time))
{
    return AnimatedValue<T, U>(start_value, end_value, start_time, end_time, animator);
}
template <typename T, typename U>
AnimatedValue<T, U> MakeAnimatedValue(AnimatedValue<T, U> start_value, T end_value,
                                      U start_time, U end_time, U (*animator)(U relative_time))
{
    return AnimatedValue<T, U>(start_value.value, end_value, start_time, end_time, animator);
}

template <typename T>
T TweenInQuad(T relative_time)
{
    return relative_time * relative_time;
}

template <typename T>
T TweenOutQuad(T relative_time)
{
    return relative_time * -(relative_time - 2.0);
}

template <typename T>
T TweenInOutQuad(T relative_time)
{
    if (relative_time < 0.5f) {
        relative_time *= 2.0f;
        return 0.5f * relative_time * relative_time;
    } else {
        relative_time = (relative_time - 0.5f) * 2.0f;
        return 0.5f + (0.5f * relative_time * -(relative_time - 2.0f));
    }
}

}
