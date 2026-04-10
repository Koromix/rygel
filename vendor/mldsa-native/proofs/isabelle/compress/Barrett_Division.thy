(* Copyright (c) The mldsa-native project authors
   SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT *)

theory Barrett_Division
  imports Rounding HOL.Bit_Operations
begin

section \<open>Barrett Division\<close>

text \<open>The Barrett magic constant approximates @{term "2^N / b"}.\<close>
definition barrett_magic :: \<open>int \<Rightarrow> nat \<Rightarrow> int\<close> where
  \<open>barrett_magic b N \<equiv> \<lfloor>(Rat.rat_of_int (2 ^ N)) / (rat_of_int b)\<rfloor>\<close>

text \<open>Rational approximation of @{term "1/b"} using the magic constant.\<close>
definition barrett_approximation :: \<open>int \<Rightarrow> nat \<Rightarrow> rat\<close> where
  \<open>barrett_approximation b N \<equiv> rat_of_int (barrett_magic b N) / rat_of_int (2 ^ N)\<close>

text \<open>Error in the Barrett approximation (always non-negative).\<close>
definition barrett_error :: \<open>int \<Rightarrow> nat \<Rightarrow> rat\<close> where
  \<open>barrett_error b N = 1.0 / (rat_of_int b) - barrett_approximation b N\<close>

text \<open>Floor of a rational is exact iff divisibility holds.\<close>
lemma rat_floor_exact_iff:
  assumes \<open>b \<noteq> 0\<close>
  shows \<open>rat_of_int \<lfloor>rat_of_int a / rat_of_int b\<rfloor> = rat_of_int a / rat_of_int b
         \<longleftrightarrow> b dvd a\<close>
proof -
  { 
    assume \<open>b dvd a\<close>
    then obtain k where \<open>a = k * b\<close> by force
    then have \<open>rat_of_int \<lfloor>rat_of_int a / rat_of_int b\<rfloor> = rat_of_int a / rat_of_int b\<close>
      by simp 
  }
  moreover {
    let ?k = \<open>\<lfloor>rat_of_int a / rat_of_int b\<rfloor>\<close>
    assume \<open>rat_of_int \<lfloor>rat_of_int a / rat_of_int b\<rfloor> = rat_of_int a / rat_of_int b\<close>
    then have \<open>a = ?k * b\<close>
      by (metis assms nonzero_eq_divide_eq of_int_eq_0_iff of_int_eq_iff of_int_mult)
    then have \<open>b dvd a\<close>
      by (metis dvd_triv_left mult.commute)
  }
  ultimately show ?thesis 
    by safe
qed

lemma barrett_error_zero_iff:
  assumes \<open>b > 0\<close>
  shows \<open>barrett_error b N = 0 \<longleftrightarrow> b dvd 2^N\<close>
proof -
  have \<open>barrett_error b N = 0 \<longleftrightarrow> rat_of_int (barrett_magic b N) = Rat.rat_of_int (2 ^ N) / (rat_of_int b)\<close>
    unfolding barrett_error_def barrett_approximation_def 
    using Fract_of_int_quotient divide_rat_def by fastforce
  moreover have \<open>rat_of_int (barrett_magic b N) = Rat.rat_of_int (2 ^ N) / (rat_of_int b) \<longleftrightarrow> b dvd 2^N\<close> 
    using rat_floor_exact_iff unfolding barrett_magic_def by (metis assms less_int_code(1))
  ultimately show ?thesis 
    by simp
qed

lemma barrett_approximation_leq:
  assumes \<open>b > 0\<close>
  shows \<open>barrett_approximation b N \<le> 1.0 / (rat_of_int b)\<close>
proof -
  note assms
  moreover from assms have \<open>\<lfloor>2 ^ N / rat_of_int b\<rfloor> * b \<le> 2 ^ N\<close> 
    by (metis floor_divide_lower of_int_mult of_int_numeral of_int_pos of_int_power_le_of_int_cancel_iff)  
  ultimately show ?thesis
    unfolding barrett_approximation_def barrett_magic_def by (simp add: divide_le_eq)
qed

lemma barrett_approximation_eq_iff:
  assumes \<open>b > 0\<close>
  shows \<open>barrett_approximation b N = 1.0 / (rat_of_int b) \<longleftrightarrow> b dvd 2^N\<close>
  by (metis assms barrett_error_def barrett_error_zero_iff right_minus_eq)

text \<open>Rational result of Barrett division (before rounding).\<close>
definition barrett_division_rat :: \<open>int \<Rightarrow> int \<Rightarrow> nat \<Rightarrow> rat\<close>
  where \<open>barrett_division_rat a b N \<equiv> rat_of_int a * barrett_approximation b N\<close>

text \<open>Barrett division: multiply by magic constant, then round.\<close>
definition barrett_division :: \<open>int \<Rightarrow> int \<Rightarrow> nat \<Rightarrow> int\<close>
  where \<open>barrett_division a b N \<equiv> round (barrett_division_rat a b N)\<close>

text \<open>Rounding division by a power of 2.\<close>
lemma round_div_by_power2:
  assumes \<open>N > 0\<close>
  shows \<open>round (rat_of_int a / 2^N) = (a + 2^(N-1)) div 2^N\<close>
proof -
  have \<open>\<And>x. floor (rat_of_int x / 2^N) = x div 2^N\<close> 
    by (metis floor_divide_of_int_eq of_int_eq_numeral_power_cancel_iff)
  moreover from \<open>N > 0\<close> have \<open>(1/2 :: rat) = 2^(N-1) / 2^N\<close> 
    by (metis (no_types, lifting) div_self divide_divide_eq_left power_eq_0_iff power_minus_mult zero_neq_numeral)
  moreover from this have \<open>rat_of_int a / 2^N + 1 / 2 = rat_of_int (a + 2^(N-1)) / 2^N\<close>
    using \<open>N > 0\<close> by (simp add: add_divide_distrib)
  ultimately show ?thesis
    by (metis round_def)
qed

text \<open>Concrete form of Barrett division.\<close>
lemma barrett_division:
  assumes \<open>N > 0\<close>
  shows \<open>barrett_division a b N = (a * barrett_magic b N + 2^(N-1)) div 2^N\<close>
  using assms by (simp add: round_div_by_power2 barrett_division_def barrett_division_rat_def 
    barrett_approximation_def flip: of_int_mult)
 
text \<open>Examples showing convergence as precision increases.\<close>
lemma
 \<open>round ((29837438.0 :: rat) / 386) = 77299\<close>
 \<open>barrett_division 29837438 386 10  = 58276\<close>
 \<open>barrett_division 29837438 386 15  = 76488\<close>
 \<open>barrett_division 29837438 386 20  = 77284\<close>
 \<open>barrett_division 29837438 386 25  = 77299\<close>
 by eval+

lemma barrett_division_rat_concrete:
  shows \<open>barrett_division_rat a b N = rat_of_int (a * barrett_magic b N) / 2^N\<close>
  unfolding barrett_division_rat_def barrett_approximation_def 
  by (simp add: Fract_of_int_quotient)

lemma rat_inverse_of_int:
  shows \<open>1 / rat_of_int b = Fract 1 b\<close>
  by (simp add: Fract_of_int_quotient) 

lemma barrett_division_leq:
  assumes \<open>a \<ge> 0\<close> and \<open>b > 0\<close>
  shows \<open>barrett_division_rat a b N \<le> rat_of_int a / rat_of_int b\<close>
proof - 
  have \<open>rat_of_int a * barrett_approximation b N \<le> rat_of_int a * (1 / rat_of_int b)\<close>    
    using barrett_approximation_leq assms by (simp add: mult_left_mono rat_inverse_of_int)
  then show ?thesis
    unfolding barrett_division_rat_def
    by (metis Fract_of_int_quotient mult.right_neutral of_int_1 times_divide_eq_right)
qed

lemma barrett_division_exact:
  assumes \<open>a > 0\<close> and \<open>b > 0\<close>
  shows \<open>barrett_division_rat a b N = rat_of_int a / rat_of_int b \<longleftrightarrow> (b dvd 2^N)\<close>
  using assms barrett_approximation_eq_iff barrett_division_rat_def divide_rat_def by auto

definition barrett_division_error :: \<open>int \<Rightarrow> int \<Rightarrow> nat \<Rightarrow> rat\<close> where
  \<open>barrett_division_error a b N \<equiv> rat_of_int a / rat_of_int b - barrett_division_rat a b N\<close>

lemma barrett_division_error:
  assumes \<open>a \<ge> 0\<close> and \<open>b > 0\<close>
  shows \<open>barrett_division_error a b N \<ge> 0\<close>
  by (simp add: assms barrett_division_error_def barrett_division_leq)

lemma barrett_division_error_zero:
  assumes \<open>a > 0\<close> and \<open>b > 0\<close>
  shows \<open>barrett_division_error a b N = 0 \<longleftrightarrow> b dvd 2^N\<close>
  by (metis assms barrett_division_error_def barrett_division_exact right_minus_eq)

lemma barrett_division_error_bound:
  assumes \<open>a \<ge> 0\<close> and \<open>b > 0\<close>
  shows \<open>barrett_division_error a b N \<le> rat_of_int a * barrett_error b N\<close>
  unfolding barrett_division_rat_def barrett_division_error_def barrett_error_def 
  by (simp add: right_diff_distrib')

text \<open>Main theorem: Barrett division equals round-half-down division when error is small enough.\<close>
theorem barrett_division_correct:
  assumes \<open>a \<ge> 0\<close> and \<open>b > 0\<close> and \<open>2 dvd b\<close> and \<open>\<not> (b dvd 2^N)\<close>
     and \<open>a < floor (1 / (barrett_error b N * rat_of_int b))\<close>
   shows \<open>round_half_down (rat_of_int a / rat_of_int b) = 
          barrett_division a b N\<close>
proof -
  from assms have bd: \<open>rat_of_int a < 1 / (barrett_error b N * rat_of_int b)\<close> 
    by linarith
  obtain q where q: \<open>q = rat_of_int a / rat_of_int b\<close> by simp
  obtain \<epsilon> where \<epsilon>: \<open>\<epsilon> = barrett_division_error a b N\<close> by simp
  from assms bd have \<open>rat_of_int a * barrett_error b N < (1 / (barrett_error b N * rat_of_int b)) * barrett_error b N\<close>
  by (metis linorder_not_le mult_le_0_iff mult_less_cancel_right nle_le of_int_nonneg 
    order_le_less_trans zero_less_divide_1_iff)
  also have \<open>\<dots> = 1 / rat_of_int b\<close>
    using calculation by fastforce
  finally have bd': \<open>rat_of_int a * barrett_error b N < 1 / rat_of_int b\<close>   
    by simp
  then have bd'': \<open>\<epsilon> \<le> 1 / rat_of_int b\<close>
    by (metis \<epsilon> assms(1,2) barrett_division_error_bound dual_order.strict_trans2 less_eq_rat_def)
  then have \<open>round_half_down q = round_half_down (q - \<epsilon>)\<close>
    by (metis \<epsilon> bd' assms barrett_division_error barrett_division_error_bound dual_order.strict_trans2 q
        round_half_down_unchanged_concrete)
  then have \<open>round_half_down (rat_of_int a / rat_of_int b) = 
        round_half_down (barrett_division_rat a b N)\<close>
    using \<epsilon> barrett_division_error_def q by auto
  moreover have \<open>round_half_down (q - \<epsilon>) = round (q - \<epsilon>)\<close>
  proof -
    { assume \<open>a = 0\<close>
      then have \<open>q - \<epsilon> = 0\<close>
        by (simp add: \<epsilon> barrett_division_error_def barrett_division_rat_def q)
      then have \<open>round_half_down (q - \<epsilon>) = round (q - \<epsilon>)\<close>
        by (simp add: round_half_down_def) 
    }
    moreover {
      assume \<open>a > 0\<close>
      then have \<open>\<epsilon> > 0\<close>
        by (simp add: \<epsilon> assms(2,4) barrett_division_error barrett_division_error_zero order_neq_le_trans)
      then have \<open>round_half_down (q - \<epsilon>) = round (q - \<epsilon>)\<close>
        by (metis \<epsilon> bd'' bd' assms(1,2,3) barrett_division_error_bound less_eq_rat_def linorder_not_less q
            round_half_down_unchanged_concrete(2))
    }
    ultimately show ?thesis 
      using \<open>a \<ge> 0\<close> by (cases \<open>a > 0\<close>; simp)
  qed
  then have \<open>round_half_down (barrett_division_rat a b N) = 
             round (barrett_division_rat a b N)\<close>
    using \<epsilon> barrett_division_error_def q by auto
  then show ?thesis 
    by (simp add: calculation barrett_division_def)
qed

text \<open>A convenience attribute for specializing the above theorem.\<close>
ML \<open>
fun eval_core_conv ctxt ct =
  let val t = Thm.term_of ct
  in if null (Term.add_frees t []) andalso null (Term.add_vars t [])
     then Code_Simp.dynamic_conv ctxt ct
     else raise CTERM ("eval_conv: not ground", [ct])
  end
fun eval_conv ctxt = Conv.try_conv (Conv.top_sweep_conv eval_core_conv ctxt) 
                          then_conv Simplifier.asm_full_rewrite ctxt
val eval_attr = (Context.proof_of #> eval_conv #> Conv.fconv_rule) |> Thm.rule_attribute [] 
\<close>
attribute_setup eval = \<open>Scan.succeed eval_attr\<close> "simplify theorem through evaluation"

end
