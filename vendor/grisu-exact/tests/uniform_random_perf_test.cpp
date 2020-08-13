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

#include "../fp_to_chars.h"
#include "random_float.h"
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <string_view>

template <class Float>
void uniform_random_perf_test(std::size_t number_of_tests)
{
	using extended_significand_type =
		typename jkj::grisu_exact_detail::common_info<Float>::extended_significand_type;

	char buffer[41];
	auto rg = generate_correctly_seeded_mt19937_64();
	for (std::size_t test_idx = 0; test_idx < number_of_tests; ++test_idx) {
		auto x = uniformly_randomly_generate_general_float<Float>(rg);
		jkj::fp_to_chars(x, buffer);
	}
}

void uniform_random_perf_test_float(std::size_t number_of_tests) {
	std::cout << "[Running the algorithm with uniformly randomly generated float inputs...]\n";
	uniform_random_perf_test<float>(number_of_tests);
	std::cout << "Done.\n\n\n";
}
void uniform_random_perf_test_double(std::size_t number_of_tests) {
	std::cout << "[Running the algorithm with uniformly randomly generated double inputs...]\n";
	uniform_random_perf_test<double>(number_of_tests);
	std::cout << "Done.\n\n\n";
}