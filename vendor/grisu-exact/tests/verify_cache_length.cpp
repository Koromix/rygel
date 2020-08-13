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

#include "bigint.h"

namespace jkj {
	namespace grisu_exact_detail {
		// Min-Max Euclid algorithm
		// Precondition: a, b, N are positive integers
		template <std::size_t array_size>
		struct minmax_euclid_return {
			bigint_impl<array_size> min;
			bigint_impl<array_size> max;
			std::uint64_t argmin;
			std::uint64_t argmax;
		};
		template <std::size_t array_size>
		minmax_euclid_return<array_size>
			minmax_euclid(bigint_impl<array_size> const& a, bigint_impl<array_size> const& b, std::uint64_t N)
		{
			using bigint_t = bigint_impl<array_size>;

			minmax_euclid_return<array_size> ret;
			ret.max = b;

			bigint_t ai = a;
			bigint_t bi = b;
			std::uint64_t si = 1;
			std::uint64_t ui = 0;

			while (true) {
				// Update ui and bi
				auto new_b = bi;
				auto qi = new_b.long_division(ai);
				if (new_b == 0) {
					assert(qi > 0);
					--qi;
					new_b = ai;
				}
				auto new_u = qi * si;
				new_u += ui;

				if (new_u > N) {
					// Find 0 < k < qi such that ui + k*si <= N < ui + (k+1)*si
					auto k = (N - ui) / si;

					// si <= N < new_u
					ret.min = ai;
					ret.argmin = si;
					ret.max -= bi;
					ret.max += k * ai;
					ret.argmax = ui + k * si;

					break;
				}
				assert(new_u.leading_one_pos.element_pos == 0);

				// Update si and ai
				auto new_a = ai;
				auto pi = new_a.long_division(new_b);
				if (new_a == 0) {
					assert(pi > 0);
					--pi;
					new_a = new_b;
				}
				auto new_s = pi * new_u;
				new_s += si;

				if (new_s > N) {
					// Find 0 < k < pi such that si + k*u(i+1) <= N < si + (k+1)*u(i+1)
					auto k = (N - si) / new_u.elements[0];

					// new_u <= N < new_s
					ret.min = ai;
					ret.min -= k * new_b;
					ret.argmin = si + k * new_u.elements[0];
					ret.max -= new_b;
					ret.argmax = new_u.elements[0];

					break;
				}
				assert(new_s.leading_one_pos.element_pos == 0);

				if (new_b == bi && new_a == ai) {
					// Reached to the gcd
					assert(ui == new_u.elements[0]);
					assert(si == new_s.elements[0]);

					ret.max -= new_b;
					ret.argmax = new_u.elements[0];

					auto sum_idx = new_s;
					sum_idx += new_u;
					if (sum_idx > N) {
						ret.min = new_a;
						ret.argmin = new_s.elements[0];
					}
					else {
						assert(sum_idx.leading_one_pos.element_pos == 0);
						ret.min = 0;
						ret.argmin = sum_idx.elements[0];
					}

					break;
				}

				bi = new_b;
				ui = new_u.elements[0];
				ai = new_a;
				si = new_s.elements[0];
			}

			return ret;
		}

		template <class Float>
		struct further_info {
			// When k < 0,
			// we should be able to hold 5^-k and 2^(q + e + k + 1).
			// For the former, the necessary number of bits are
			// floor(-k * log2(5)) + 1 = floor(-k * log2(10)) + k + 1,
			// and for the latter, the necessary number of bits are q + e + k + 2.
			// Since k = ceil((alpha-e-1) * log10(2)), we can show that
			// floor(-k * log2(10)) <= e + 1 - alpha, so
			// the necessary bits for the former is at most e + k + 2 - alpha.
			// On the other hand, e + k is an increasing function of e, so
			// the following is an upper bound:
			static constexpr std::size_t negative_k_max_bits =
				std::max(int(common_info<Float>::extended_precision + 2),
					int(2 - common_info<Float>::alpha)) +
				common_info<Float>::max_exponent + common_info<Float>::min_k;


			// When k >= 0,
			// we should be able to hold 5^k * 2^(p+2) and 2^(-e - k - (q-p-3)).
			// For the former, the necessary number of bits are
			// floor(k * log2(5)) + p + 3 = floor(k * log2(10)) - k + p + 3,
			// and for the latter, the necessary number of bits are
			// -e - k - (q-p-3) + 1 = -e - k + p + 4 - q.
			// Since k = ceil((alpha-e-1) * log10(2)), we can show that
			// floor(k * log2(10)) <= (alpha-e-1) + log2(10) < alpha - e + 3, so
			// the necessary bits for the former is at most -e - k + p + 5 + alpha.
			// On the other hand, -e - k is a decreasing function on e, so
			// the following is an upper bound:
			static constexpr std::size_t positive_k_max_bits =
				common_info<Float>::precision +
				std::max(-int(common_info<Float>::extended_precision - 4),
					int(5 + common_info<Float>::alpha))
				- common_info<Float>::min_exponent - common_info<Float>::max_k;


			// Useful constants
			static constexpr auto range =
				(std::uint64_t(1) << (common_info<Float>::precision + 2));
			static constexpr auto q_mp_m2 =
				common_info<Float>::extended_precision - common_info<Float>::precision - 2;
		};

		template <class Float, class F1, class F2>
		void verify_cache_length_single_type_negative_k(F1&& on_each, F2&& on_max)
		{
			using bigint_type = bigint<further_info<Float>::negative_k_max_bits>;

			std::size_t max_required_bits = 0;

			bigint_type power_of_5 = 1;
			int prev_k = 0;
			for (int e = common_info<Float>::alpha + 3; e <= common_info<Float>::max_exponent; ++e) {
				auto k = -floor_log10_pow2(e + 1 - common_info<Float>::alpha);
				if (k != prev_k) {
					assert(k == prev_k - 1);
					power_of_5.multiply_5();
					prev_k = k;
				}

				auto mod_minmax = minmax_euclid(
					bigint_type::power_of_2(further_info<Float>::q_mp_m2 + e + k),
					power_of_5, further_info<Float>::range);
				auto& mod_min = mod_minmax.min;
				auto& mod_max = mod_minmax.max;

				auto divisor = power_of_5;
				divisor -= mod_max;
				auto dividend = power_of_5;
				auto division_res = dividend.long_division(divisor);

				auto log2_res_p1 = division_res.leading_one_pos.element_pos *
					division_res.element_number_of_bits + division_res.leading_one_pos.bit_pos;

				auto required_bits = common_info<Float>::extended_precision +
					e + floor_log2_pow10(k) + 1 + log2_res_p1;

				mod_minmax = minmax_euclid(
					bigint_type::power_of_2(further_info<Float>::q_mp_m2 + e + k + 2),
					power_of_5, further_info<Float>::range / 2);
				mod_min = mod_minmax.min;
				mod_max = mod_minmax.max;

				divisor = power_of_5;
				divisor -= mod_max;
				dividend = power_of_5;
				division_res = dividend.long_division(divisor);

				log2_res_p1 = division_res.leading_one_pos.element_pos *
					division_res.element_number_of_bits + division_res.leading_one_pos.bit_pos;

				auto two_y_result = common_info<Float>::extended_precision +
					e + floor_log2_pow10(k) + 1 + log2_res_p1;

				if (two_y_result > required_bits)
					required_bits = two_y_result;
 
				auto edge_case_a = bigint_type::power_of_2(further_info<Float>::q_mp_m2 + e + k - 1);
				edge_case_a *= (further_info<Float>::range - 1);
				edge_case_a.long_division(power_of_5);

				divisor = power_of_5;
				divisor -= edge_case_a;
				dividend = power_of_5;
				division_res = dividend.long_division(divisor);

				log2_res_p1 = division_res.leading_one_pos.element_pos *
					division_res.element_number_of_bits + division_res.leading_one_pos.bit_pos;

				auto edge_case_result = common_info<Float>::extended_precision +
					e + floor_log2_pow10(k) + log2_res_p1;

				if (edge_case_result > required_bits)
					required_bits = edge_case_result;

				if (required_bits > max_required_bits)
					max_required_bits = required_bits;

				on_each(e, required_bits);
			}

			on_max(max_required_bits);
		}

		template <class Float, class F1, class F2>
		void verify_cache_length_single_type_positive_k(F1&& on_each, F2&& on_max)
		{
			using bigint_type = bigint<further_info<Float>::positive_k_max_bits>;

			std::size_t max_required_bits = 0;

			bigint_type power_of_5 = 1;
			int prev_k = 0;
			for (int e = common_info<Float>::alpha + 2; e >= common_info<Float>::min_exponent; --e) {
				auto k = -floor_log10_pow2(e + 1 - common_info<Float>::alpha);
				if (k != prev_k) {
					assert(k == prev_k + 1);
					power_of_5.multiply_5();
					prev_k = k;
				}

				auto required_bits_base = std::size_t(floor_log2_pow10(k) - k + 1);
				auto required_bits = required_bits_base;

				int exp_of_2 = -e - k - int(further_info<Float>::q_mp_m2);

				if (exp_of_2 > 0) {
					auto mod_minmax = minmax_euclid(
						power_of_5,
						bigint_type::power_of_2(exp_of_2),
						further_info<Float>::range);
					auto& mod_min = mod_minmax.min;

					if (mod_min.leading_one_pos.bit_pos != 0) {
						auto log2_res = mod_min.leading_one_pos.element_pos *
							mod_min.element_number_of_bits + mod_min.leading_one_pos.bit_pos - 1;

						if (log2_res > common_info<Float>::precision + 2) {
							required_bits -= (log2_res - common_info<Float>::precision - 2);
						}
					}
				}

				exp_of_2 -= 2;
				if (exp_of_2 > 0) {
					auto mod_minmax = minmax_euclid(
						power_of_5,
						bigint_type::power_of_2(exp_of_2),
						further_info<Float>::range / 2);
					auto& mod_min = mod_minmax.min;

					if (mod_min.leading_one_pos.bit_pos != 0) {
						auto log2_res = mod_min.leading_one_pos.element_pos *
							mod_min.element_number_of_bits + mod_min.leading_one_pos.bit_pos - 1;

						if (log2_res > common_info<Float>::precision + 1) {
							auto two_y_result = required_bits_base - log2_res + common_info<Float>::precision + 1;

							if (two_y_result > required_bits)
								required_bits = two_y_result;
						}
					}
				}

				exp_of_2 += 3;
				if (exp_of_2 > 0) {
					auto edge_case_a = power_of_5;
					edge_case_a *= (further_info<Float>::range - 1);
					edge_case_a.long_division(bigint_type::power_of_2(exp_of_2));

					if (edge_case_a.leading_one_pos.bit_pos != 0) {
						auto log2_res = edge_case_a.leading_one_pos.element_pos *
							edge_case_a.element_number_of_bits + edge_case_a.leading_one_pos.bit_pos - 1;

						if (log2_res > common_info<Float>::precision + 2) {
							auto edge_case_result = required_bits_base - log2_res + common_info<Float>::precision + 2;

							if (edge_case_result > required_bits)
								required_bits = edge_case_result;
						}
					}
				}

				if (required_bits > max_required_bits)
					max_required_bits = required_bits;

				on_each(e, required_bits);
			}

			on_max(max_required_bits);
		}
	}
}

#include <fstream>
#include <iostream>

void verify_cache_length()
{
	std::cout << "[Verifying cache length upper bound...]\n";

	std::ofstream out;
	auto on_each = [&out](auto e, auto required_bits) {
		out << e << "," << required_bits << std::endl;
	};
	auto on_max = [&out](auto max_required_bits) {
		std::cout << "Maximum required bits: " << max_required_bits << std::endl;
	};

	std::cout << "\nVerify for IEEE-754 binary32 (float) type for negative k...\n";	
	out.open("test_results/binary32_negative_k.csv");
	out << "e,required_bits\n";
	jkj::grisu_exact_detail::verify_cache_length_single_type_negative_k<float>(on_each, on_max);
	out.close();

	std::cout << "\nVerify for IEEE-754 binary32 (float) type for positive k...\n";
	out.open("test_results/binary32_positive_k.csv");
	out << "e,required_bits\n";
	jkj::grisu_exact_detail::verify_cache_length_single_type_positive_k<float>(on_each, on_max);
	out.close();

	std::cout << "\nVerify for IEEE-754 binary64 (double) type for negative k...\n";
	out.open("test_results/binary64_negative_k.csv");
	out << "e,required_bits\n";
	jkj::grisu_exact_detail::verify_cache_length_single_type_negative_k<double>(on_each, on_max);
	out.close();

	std::cout << "\nVerify for IEEE-754 binary64 (double) type for positive k...\n";
	out.open("test_results/binary64_positive_k.csv");
	out << "e,required_bits\n";
	jkj::grisu_exact_detail::verify_cache_length_single_type_positive_k<double>(on_each, on_max);
	out.close();

	std::cout << std::endl;
	std::cout << "Done.\n\n\n";
}