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

void verify_incorrect_rounding_removal()
{
	std::cout << "[Verifying incorrect rounding removal...]\n";

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

			// Carefully compute the rounded-down value of y (y^(rd) in the paper)
			// Since y = 2^(e+q-1) * 10^k, we compute the ceiling of
			// (2^(e+q) * 10^(k-kappa) - 1) / 2
			// First, compute floor(2^(e+q) * 10^k) = floor(2^(q+beta) * phi_k * 2^-Q),
			// which is the first q+beta bits of phi_k
			extended_significand_type two_yi;
			if constexpr (sizeof(float_type) == 4) {
				two_yi = extended_significand_type(cache >>
					(int(common_info<float_type>::extended_precision) - beta));
			}
			else {
				static_assert(sizeof(float_type) == 8);
				two_yi = cache.high() >> -beta;
			}

			// Next, write 2^(e+q) * 10^k = a * 10^kappa + b for some
			// b in [0,10^kappa), so that
			// 2^(e+q) * 10^(k-kappa) = a + b * 10^-kappa, so
			// y^(rd) = ceil( (a-1)/2 + (b*10^-kappa)/2 )
			auto const a = two_yi / divisor;
			auto const bi = two_yi % divisor;

			extended_significand_type rounded_down;
			// If a-1 is odd, then y^(rd) = floor((a-1)/2) + 1
			if ((a - 1) % 2 == 1) {
				rounded_down = (a - 1) / 2 + 1;
			}
			// Otherwise, y^(rd) = floor((a-1)/2) + 1 if b != 0 and
			// y^(rd) = floor((a-1)/2) if b == 0
			else {
				if (bi != 0) {
					rounded_down = (a - 1) / 2 + 1;
				}
				else {
					// Check if b is an integer, or equivalently,
					// 2^(e+q) * 10^k is an integer
					if (e + common_info<float_type>::extended_precision + k >= 0 && k >= 0) {
						rounded_down = (a - 1) / 2;
					}
					else {
						rounded_down = (a - 1) / 2 + 1;
					}
				}
			}

			// Check if the distance from floor(z/10^kappa) is exactly 1
			auto const fr = common_info<float_type>::sign_bit_mask | common_info<float_type>::boundary_bit;
			auto const zi = grisu_exact_impl<float_type>::compute_mul(fr, cache, -beta);
			auto const deltai = grisu_exact_impl<float_type>::template compute_delta<
				jkj::grisu_exact_rounding_modes::to_nearest_tag>(true, cache, -beta);
			auto const approx_x = zi - deltai;
			auto const right_bdy = zi / divisor;
			auto const r = zi % divisor;

			if (right_bdy == rounded_down + 1) {
				// In this case, compare r_kappa + 10^kappa + z^(f) with delta
				// We are interested in the case when the integer part of those two are the same
				auto const distancei = r + divisor;
				if (distancei == deltai) {
					std::cout << "Coincidence of integer parts detected (x = "
						<< std::hex << std::setfill('0');

					if constexpr (sizeof(float_type) == 4) {
						std::cout << std::setprecision(9) << x << " [0x" << std::setw(8);
					}
					else {
						static_assert(sizeof(float_type) == 8);
						std::cout << std::setprecision(17) << x << " [0x" << std::setw(16);
					}

					std::cout << bit_rep << "], e = " << std::dec << e << "): ";

					// Now, compare the fractional parts
					auto const fl = common_info<float_type>::sign_bit_mask -
						common_info<float_type>::edge_case_boundary_bit;

					if ((grisu_exact_impl<float_type>::compute_mul(fl, cache, -beta) & 1) != (approx_x & 1))
					{
						std::cout << "z^(f) < delta^(f)\n";
						
						if constexpr (sizeof(float_type) == 4) {
							if (e == 59) {
								success = false;
							}
						}
						else {
							if (e == -203) {
								success = false;
							}
						}
					}
					else if (grisu_exact_impl<float_type>::template equal_fractional_parts<
						jkj::grisu_exact_rounding_modes::to_nearest_tag>(fl, e, -k))
					{
						std::cout << "z^(f) == delta^(f)\n";

						if constexpr (sizeof(float_type) == 4) {
							if (e == 59) {
								success = false;
							}
						}
						else {
							if (e == -203) {
								success = false;
							}
						}
					}
					else
					{
						std::cout << "z^(f) > delta^(f)\n";

						if constexpr (sizeof(float_type) == 4) {
							if (e != 59) {
								success = false;
							}
						}
						else {
							if (e != -203) {
								success = false;
							}
						}
					}
				}
			}
		}

		return success;
	};

	if (verify_single_type(common_info<float>{})) {
		std::cout << "Incorrect rounding removal for binary32: verified." << std::endl;
	}
	else {
		std::cout << "Incorrect rounding removal for binary32: failed." << std::endl;
	}

	if (verify_single_type(common_info<double>{})) {
		std::cout << "Incorrect rounding removal for binary64: verified." << std::endl;
	}
	else {
		std::cout << "Incorrect rounding removal for binary64: failed." << std::endl;
	}

	std::cout << "Done.\n\n\n";
}