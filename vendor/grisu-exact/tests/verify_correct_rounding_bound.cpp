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

#include <iostream>
#include <iomanip>

void verify_correct_rounding_bound()
{
	std::cout << "[Verifying correct rounding bound...]\n";

	using namespace jkj::grisu_exact_detail;

	auto verify_single_type = [](auto type_tag) {
		using float_type = typename decltype(type_tag)::float_type;
		using extended_significand_type = typename common_info<float_type>::extended_significand_type;

		bool success = true;

		constexpr auto max_exponent_shifted =
			((int)(1) << common_info<float_type>::exponent_bits) - 1;
		for (int e_shifted = 1; e_shifted < max_exponent_shifted; ++e_shifted)
		{
			// Compose bits
			auto const bit_rep = extended_significand_type(e_shifted) << common_info<float_type>::precision;
			float_type x;
			std::memcpy(&x, &bit_rep, sizeof(bit_rep));

			// Compute e, k, and beta
			auto const e = e_shifted + common_info<float_type>::exponent_bias
				- int(common_info<float_type>::extended_precision) + 1;
			auto const k = -floor_log10_pow2(e + 1 - common_info<float_type>::alpha);
			int const beta = e + floor_log2_pow10(k) + 1;

			// Run Grisu-Exact without correct rounding search to inspect the possible range of kappa
			// Since the significand is always even, nearest-to-odd is the most harsh condition, and
			// nearest-to-even is the most generous condition
			auto const grisu_exact_result_harsh = jkj::grisu_exact(x,
				jkj::grisu_exact_rounding_modes::nearest_to_odd{},
				jkj::grisu_exact_correct_rounding::do_not_care{});
			auto const grisu_exact_result_generous = jkj::grisu_exact(x,
				jkj::grisu_exact_rounding_modes::nearest_to_even{},
				jkj::grisu_exact_correct_rounding::do_not_care{});

			auto const kappa_min = grisu_exact_result_harsh.exponent + k;
			auto const kappa_max = grisu_exact_result_generous.exponent + k;

			if (kappa_min != kappa_max) {
				std::cout << "Detected mismatch between kappa's for different rounding modes!\n";
				return false;
			}
			auto const kappa = kappa_min;
			auto divisor = extended_significand_type(1);
			for (int i = 0; i < kappa; ++i) {
				divisor *= 10;
			}

			auto const& cache = get_cache<float_type>(k);
			assert(-beta < common_info<float_type>::extended_precision);

			// To get n', we need to subtract 1, except when N = 10^kappa * n
			// Thus, compute N first
			auto fr = common_info<float_type>::sign_bit_mask | common_info<float_type>::boundary_bit;
			auto zi = grisu_exact_impl<float_type>::compute_mul(fr, cache, -beta);
			auto epsiloni = grisu_exact_impl<float_type>::template compute_delta<
				jkj::grisu_exact_rounding_modes::left_closed_directed_tag>(
				false, cache, -beta + 1);
			auto displacement = (zi % divisor) + divisor / 2;

			int np;
			if (displacement > epsiloni) {
				np = -1;
			}
			else {
				np = int((epsiloni - displacement) / divisor);
			}

			if (np >= 5 && kappa != 0) {
				std::cout << "n' = " << np
					<< " (e = " << e << ", x = ";
				std::cout << std::hex << std::setfill('0');				

				if constexpr (sizeof(float_type) == 4) {
					std::cout << std::setprecision(9) << x << " [0x" << std::setw(8);
					success = false;
				}
				else {
					static_assert(sizeof(float_type) == 8);
					std::cout << std::setprecision(17) << x << " [0x" << std::setw(16);
					if (np >= 6) {
						success = false;
					}
				}

				std::cout << bit_rep << "])\n" << std::dec;
			}
		}

		return success;
	};

	if (verify_single_type(common_info<float>{}))
		std::cout << "correct rounding bound computation for binary32: verified." << std::endl;
	else
		std::cout << "correct rounding bound computation for binary32: failed." << std::endl;

	if (verify_single_type(common_info<double>{}))
		std::cout << "correct rounding bound computation for binary64: verified." << std::endl;
	else
		std::cout << "correct rounding bound computation for binary64: failed." << std::endl;

	std::cout << "Done.\n\n\n";
}