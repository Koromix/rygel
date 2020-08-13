// Copyright 2020 Junekey Jeon
//
// The contents of this file may be used under the terms of
// the Apache License v2.0 with LLVM Exceptions.
//
//    (See accompanying file LICENSE-Apache or copy at
//     https://llvm.org/foundation/relicensing/LICENSE.txt)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

#include "benchmark.h"
#include "../fp_to_chars.h"

namespace {
	void grisu_exact_wo_correct_rounding_float_to_chars(float x, char* buffer)
	{
		jkj::fp_to_chars(x, buffer,
			jkj::grisu_exact_rounding_modes::nearest_to_even{},
			jkj::grisu_exact_correct_rounding::do_not_care{});
	}
	void grisu_exact_wo_correct_rounding_double_to_chars(double x, char* buffer)
	{
		jkj::fp_to_chars(x, buffer,
			jkj::grisu_exact_rounding_modes::nearest_to_even{},
			jkj::grisu_exact_correct_rounding::do_not_care{});
	}
	register_function_for_benchmark dummy("Grisu-Exact (w/o correct rounding)",
		grisu_exact_wo_correct_rounding_float_to_chars,
		grisu_exact_wo_correct_rounding_double_to_chars);
}