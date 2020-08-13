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

#include "../grisu_exact.h"

#include <cmath>
#include <iostream>
#include "bigint.h"

void verify_log_computation()
{
	using jkj::grisu_exact_detail::bigint;

	std::cout << "[Verifying log computation...]\n";

	// Verify floor_log10_pow2
	int maximum_valid_exp = 0;
	bool succeeded = true;
	for (int e = 1; e <= 4096; ++e) {
		// Take lower 20 bits of 0x4d104 * e
		auto lower = (std::int32_t(e) * 0x4d104) & 0xfffff;

		// Verify the lower bits can never overflow
		if (lower + 0xd28 >= 0x100000) {
			std::cout << "floor_log10_pow2: overflow detected [e = " << e << "]\n";

			// If there might be overflow, compute directly to verify
			bigint<4097> number = bigint<4097>::power_of_2(e);
			int true_value = 0;
			while (number >= 10) {
				number = number.long_division(10);
				++true_value;
			}

			auto computed = jkj::grisu_exact_detail::floor_log10_pow2<false>(e);
			if (computed != true_value) {
				std::cout << "floor_log10_pow2: mismatch! [e = " << e
					<< ", true_value = " << true_value
					<< ", computed = " << computed << "]\n";
				succeeded = false;
			}
			computed = jkj::grisu_exact_detail::floor_log10_pow2<false>(-e);
			true_value = -true_value - 1;	// log10(2^e) is never an integer
			if (computed != true_value) {
				std::cout << "floor_log10_pow2: mismatch! [e = " << -e
					<< ", true_value = " << true_value
					<< ", computed = " << computed << "]\n";
				succeeded = false;
			}
		}
		
		if (succeeded) {
			maximum_valid_exp = e;
		}
	}
	std::cout << "floor_log10_pow2 is valid up to |e| <= " << maximum_valid_exp << ".\n\n";


	// Verify floor_log2_pow10
	maximum_valid_exp = 0;
	succeeded = true;
	for (int e = 1; e <= 1024; ++e) {
		// Take lower 20 bits of 0x35269e * e
		auto lower = (std::int32_t(e) * 0x35269e) & 0xfffff;

		// Verify the lower bits can never overflow
		if (lower + 0x130 >= 0x100000) {
			std::cout << "floor_log2_pow10: overflow detected [e = " << e << "]\n";
			
			// If there might be overflow, compute directly to verify
			bigint<4 * 4096> number = bigint<4 * 4096>::power_of_2(e);
			for (int i = 0; i < e; ++i) {
				number.multiply_5();
			}
			auto true_value = int(number.leading_one_pos.element_pos * decltype(number)::element_number_of_bits
				+ number.leading_one_pos.bit_pos - 1);

			auto computed = jkj::grisu_exact_detail::floor_log2_pow10<false>(e);
			if (computed != true_value) {
				std::cout << "floor_log2_pow10: mismatch! [e = " << e
					<< ", true_value = " << true_value
					<< ", computed = " << computed << "]\n";
				succeeded = false;
			}
			computed = jkj::grisu_exact_detail::floor_log2_pow10<false>(-e);
			true_value = -true_value - 1;	// log2(10^e) is never an integer
			if (computed != true_value) {
				std::cout << "floor_log2_pow10: mismatch! [e = " << -e
					<< ", true_value = " << true_value
					<< ", computed = " << computed << "]\n";
				succeeded = false;
			}
		}

		if (succeeded) {
			maximum_valid_exp = e;
		}
	}
	std::cout << "floor_log2_pow10 is valid up to |e| <= " << maximum_valid_exp << ".\n\n";


	// Verify floor_log5_pow2
	maximum_valid_exp = 0;
	succeeded = true;
	for (int e = 1; e <= 65536; ++e) {
		// Take lower 32 bits of 0x6e40d1a4 * e
		auto lower = (std::int64_t(e) * 0x6e40d1a4) & 0xffffffff;

		// Verify the lower bits can never overflow
		if (lower + 0x143e >= 0x100000000) {
			std::cout << "floor_log5_pow2: overflow detected [e = " << e << "]\n";

			// If there might be overflow, compute directly to verify
			bigint<65537> number = bigint<65537>::power_of_2(e);
			int true_value = 0;
			while (number >= 5) {
				number = number.long_division(5);
				++true_value;
			}

			auto computed = jkj::grisu_exact_detail::floor_log5_pow2<false>(e);
			if (computed != true_value) {
				std::cout << "floor_log5_pow2: mismatch! [e = " << e
					<< ", true_value = " << true_value
					<< ", computed = " << computed << "]\n";
				succeeded = false;
			}
			computed = jkj::grisu_exact_detail::floor_log5_pow2<false>(-e);
			true_value = -true_value - 1;	// log10(2^e) is never an integer
			if (computed != true_value) {
				std::cout << "floor_log5_pow2: mismatch! [e = " << -e
					<< ", true_value = " << true_value
					<< ", computed = " << computed << "]\n";
				succeeded = false;
			}
		}

		if (succeeded) {
			maximum_valid_exp = e;
		}
	}
	std::cout << "floor_log5_pow2 is valid up to |e| <= " << maximum_valid_exp << ".\n\n";

	std::cout << "Done.\n\n\n";
}