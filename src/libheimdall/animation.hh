// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"

template <typename T, typename U>
struct AnimationData {
    T start_value;
    T value_change;
    U start_time;
    U duration = {};

    AnimationData() = default;
    AnimationData(T start_value, T end_value, U start_time, U duration)
        : start_value(start_value), value_change(end_value - start_value),
          start_time(start_time), duration(duration) {}
};
template <typename T, typename U>
AnimationData<T, U> MakeAnimationData(T start_value, T end_value, U start_time, U duration)
    { return AnimationData<T, U>(start_value, end_value, start_time, duration); }

template <typename T, typename U, typename Fun>
void Tween(T *value, AnimationData<T, U> *animation, U time, Fun f)
{
    if (!animation->duration)
        return;

    double relative_time = (double)(time - animation->start_time) / (double)animation->duration;
    if (relative_time < 1.0) {
        *value = f(relative_time);
    } else {
        *value = animation->start_value + animation->value_change;
        animation->duration = {};
    }
}

template <typename T, typename U>
void TweenInQuad(T *value, AnimationData<T, U> *animation, U time)
{
    Tween(value, animation, time, [&](double relative_time) {
        return animation->start_value +
               (T)(relative_time * relative_time * animation->value_change);
    });
}

template <typename T, typename U>
void TweenOutQuad(T *value, AnimationData<T, U> *animation, U time)
{
    Tween(value, animation, time, [&](double relative_time) {
        return animation->start_value +
               (T)(relative_time * -(relative_time - 2.0) * animation->value_change);
    });
}

template <typename T, typename U>
void TweenInOutQuad(T *value, AnimationData<T, U> *animation, U time)
{
    Tween(value, animation, time, [&](double relative_time) {
        if (relative_time < 0.5) {
            relative_time *= 2.0;
            return animation->start_value +
                   relative_time * relative_time * (animation->value_change / (T)2);
        } else {
            relative_time = (relative_time - 0.5) * 2.0;
            return animation->start_value + (animation->value_change / (T)2) +
                   (T)(relative_time * -(relative_time - 2.0) * (animation->value_change / (T)2));
        }
    });
}
