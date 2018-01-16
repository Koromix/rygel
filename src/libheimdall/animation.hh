// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"

template <typename T, typename U = T>
struct AnimationData {
    T start_time;
    T duration = {};
    U start_value;
    U value_change;

    AnimationData() = default;
    AnimationData(T start_time, T duration, U start_value, U end_value)
        : start_time(start_time), duration(duration), start_value(start_value),
          value_change(end_value - start_value) {}
};
template <typename T, typename U>
AnimationData<T, U> MakeAnimationData(T start_time, T duration, U start_value, U end_value)
    { return AnimationData<T, U>(start_time, duration, start_value, end_value); }

template <typename T, typename U, typename Fun>
void Tween(T *value, AnimationData<T, U> *animation, U time, Fun f)
{
    if (!animation->duration)
        return;

    float relative_time = ((float)time - (float)animation->start_time) /
                          (float)animation->duration;
    if (relative_time < 1.0f) {
        *value = f(relative_time);
    } else {
        *value = animation->start_value + animation->value_change;
        animation->duration = {};
    }
}

template <typename T, typename U>
void TweenInQuad(T *value, AnimationData<T, U> *animation, U time)
{
    Tween(value, animation, time, [&](U relative_time) {
        return animation->start_value +
               relative_time * relative_time * animation->value_change;
    });
}

template <typename T, typename U>
void TweenOutQuad(T *value, AnimationData<T, U> *animation, U time)
{
    Tween(value, animation, time, [&](U relative_time) {
        return animation->start_value +
               relative_time * -(relative_time - 2.0f) * animation->value_change;
    });
}

template <typename T, typename U>
void TweenInOutQuad(AnimationData<T, U> *animation, T *value, U time)
{
    Tween(value, animation, time, [&](U relative_time) {
        if (relative_time < 0.5f) {
            relative_time *= 2.0f;
            return animation->start_value +
                   relative_time * relative_time * (animation->value_change / (T)2);
        } else {
            relative_time = (relative_time - 0.5f) * 2.0f;
            return animation->start_value + (animation->value_change / (T)2) +
                   relative_time * -(relative_time - 2.0f) * (animation->value_change / (T)2);
        }
    });
}
