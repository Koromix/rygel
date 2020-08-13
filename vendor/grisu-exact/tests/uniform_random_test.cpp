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
#include "../benchmark/ryu/ryu.h"
#include <iostream>
#include <string_view>

template <class Float, class TypenameString>
void uniform_random_test(std::size_t number_of_tests, TypenameString&& type_name_string)
{
	using extended_significand_type =
		typename jkj::grisu_exact_detail::common_info<Float>::extended_significand_type;

	char buffer1[41];
	char buffer2[41];
	auto rg = generate_correctly_seeded_mt19937_64();
	bool succeeded = true;
	for (std::size_t test_idx = 0; test_idx < number_of_tests; ++test_idx) {
		auto x = uniformly_randomly_generate_general_float<Float>(rg);

		// Check if the output is identical to that of Ryu
		jkj::fp_to_chars(x, buffer1);
		if constexpr (std::is_same_v<Float, float>) {
			f2s_buffered(x, buffer2);
		}
		else {
			d2s_buffered(x, buffer2);
		}

		std::string_view view1(buffer1);
		std::string_view view2(buffer2);

		if (view1 != view2) {
			std::cout << "Error detected! [Ryu = " << buffer2
				<< ", Grisu-Exact = " << buffer1 << "]\n";
			succeeded = false;
		}
	}

	if (succeeded) {
		std::cout << "Uniform random test for " << type_name_string
			<< " with " << number_of_tests << " examples succeeded.\n";
	}
}

void uniform_random_test_float(std::size_t number_of_tests) {
	std::cout << "[Testing uniformly randomly generated float inputs...]\n";
	uniform_random_test<float>(number_of_tests, "float");
	std::cout << "Done.\n\n\n";
}
void uniform_random_test_double(std::size_t number_of_tests) {
	std::cout << "[Testing uniformly randomly generated double inputs...]\n";
	uniform_random_test<double>(number_of_tests, "double");
	std::cout << "Done.\n\n\n";
}