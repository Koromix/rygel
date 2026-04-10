(* Copyright (c) The mldsa-native project authors
   SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT *)

theory Rounding
  imports HOL.Rat
begin

section \<open>Round-half-down\<close>

text \<open>The definition of the compression routine in ML-DSA involves rounding with half being rounded
down. This can be expressed as follows:\<close>

definition round_half_down :: \<open>rat \<Rightarrow> int\<close> where
  \<open>round_half_down x = - round (-x)\<close>

text \<open>Monotonicity of round-half-down.\<close>
lemma round_half_down_mono:
  assumes \<open>x \<le> y\<close>
  shows \<open>round_half_down x \<le> round_half_down y\<close>
  by (simp add: assms round_half_down_def round_mono)

text \<open>Predicate for half-multiples (values of the form n + 1/2).\<close>
definition is_half_multiple :: \<open>rat \<Rightarrow> bool\<close> where
  \<open>is_half_multiple x \<equiv> \<exists>y. x = rat_of_int y + 1/2\<close>

text \<open>Equivalent characterization.\<close>
lemma is_half_multiple_alt:
  shows \<open>is_half_multiple x \<longleftrightarrow> (\<exists>y. x = rat_of_int y - 1/2)\<close>
proof -
  have \<open>\<And>y. rat_of_int y - 1/2 = rat_of_int (y - 1) + 1/2\<close> 
    by simp
  then show ?thesis
    unfolding is_half_multiple_def by (metis add_diff_cancel_right')
qed

text \<open>Characterization of standard rounding.\<close>
lemma round_eq_iff:
  shows \<open>round x = y \<longleftrightarrow> (-1/2 \<le> x - rat_of_int y \<and> x - rat_of_int y < 1/2)\<close>
  by (metis add.commute diff_le_eq diff_less_eq minus_diff_eq minus_divide_left minus_le_iff 
    of_int_round_gt of_int_round_le round_unique)

text \<open>Characterization of round-half-down.\<close>
lemma round_half_down_eq_iff:
  shows \<open>round_half_down x = y \<longleftrightarrow> (-1/2 < x - rat_of_int y \<and> x - rat_of_int y \<le> 1/2)\<close>
proof -
  have \<dagger>: \<open>\<And>x y. - round x = y \<longleftrightarrow> round x = -y\<close> 
    by auto
  show ?thesis
    unfolding round_half_down_def by (auto simp add: round_eq_iff \<dagger>) 
qed

text \<open>Relationship between round-half-down and standard rounding.\<close>
lemma round_half_down_up:
  shows \<open>is_half_multiple x \<Longrightarrow> round_half_down x = round x - 1\<close>
    and \<open>\<not> (is_half_multiple x) \<Longrightarrow> round_half_down x = round x\<close>
proof -
  assume \<open>is_half_multiple x\<close>
  then obtain y where *: \<open>x = rat_of_int y + 1/2\<close> 
    unfolding is_half_multiple_def by force
  then have \<open>round_half_down x = y\<close> and \<open>round x = y + 1\<close>
    by (auto simp add: round_half_down_eq_iff round_eq_iff)
  then show \<open>round_half_down x = round x - 1\<close>
    by simp 
next
  assume \<open>\<not> (is_half_multiple x)\<close>
  obtain \<epsilon> where \<epsilon>: \<open>\<epsilon> = x - rat_of_int (round x)\<close> by simp
  obtain y where y: \<open>y = round x\<close> by simp
  have x: \<open>x = rat_of_int y + \<epsilon>\<close> 
    unfolding \<epsilon> y by simp
  have \<open>-1/2 \<le> \<epsilon>\<close> and \<open>\<epsilon> < 1/2\<close>
    using round_eq_iff unfolding \<epsilon> by auto
  moreover { 
    assume \<open>\<epsilon> = -1/2\<close>
    then have \<open>is_half_multiple x\<close> 
      by (subst x; auto simp add: is_half_multiple_def) 
         (metis add.commute diff_add_cancel of_int_1 of_int_add)
    then have False 
      using \<open>\<not> (is_half_multiple x)\<close> by simp
  }
  ultimately have \<open>-1/2 < \<epsilon>\<close> and \<open>\<epsilon> \<le> 1/2\<close>
    using less_eq_rat_def by blast+
  then have \<open>round_half_down x = y\<close>
    by (auto simp add: round_half_down_eq_iff \<epsilon> y)
  then show \<open>round_half_down x = round x\<close>
    by (simp flip: y)
qed

corollary round_half_down_up':
  shows \<open>round x - round_half_down x \<in> {0,1}\<close>
    and \<open>round x = round_half_down x \<longleftrightarrow> \<not> (is_half_multiple x)\<close>
  by (cases \<open>is_half_multiple x\<close>; simp add: round_half_down_up)+

text \<open>If rounding is changed by a small disturbance, then there must
be a half-multiple that's as close to the original value as the disturbance.\<close>
lemma round_half_down_changed_find_half_multiple:
  assumes \<open>\<epsilon> \<ge> 0\<close>
    and \<open>round_half_down x \<noteq> round_half_down (x - \<epsilon>)\<close>
  obtains y where \<open>is_half_multiple y\<close> \<open>y < x\<close> and \<open>x - y \<le> \<epsilon>\<close>
proof -
  obtain r where r: \<open>r = rat_of_int (round_half_down x)\<close> by simp
  from r have *: \<open>-1/2 < x - r\<close> \<open>x - r \<le> 1/2\<close> 
    using round_half_down_eq_iff by blast+
  obtain h where h: \<open>h = r - 1/2\<close> by simp
  then have **: \<open>h < x\<close> \<open>x - h \<le> 1\<close>
    using * h by linarith+
  moreover have \<open>is_half_multiple h\<close> 
    using is_half_multiple_alt h r by blast
  {
    assume \<open>x - h > \<epsilon>\<close>
    then have \<open>h < x - \<epsilon>\<close> and \<open>(x - \<epsilon>) - h \<le> 1\<close> 
      using assms ** by auto
    then have \<open>-1/2 < (x - \<epsilon>) - r\<close> \<open>(x - \<epsilon>) - r \<le> 1/2\<close>  
      using h by auto
    then have \<open>round_half_down (x - \<epsilon>) = round_half_down x\<close>
      using round_half_down_eq_iff r by blast
    then have False
      using assms by simp
  }
  then have \<open>x - h \<le> \<epsilon>\<close> 
    by force
  then show ?thesis 
    using \<open>is_half_multiple h\<close> ** that by blast
qed

text \<open>Conversely, if any non-equal half-multiple is farther away than the disturbance,
then rounding is unchanged:\<close>
corollary round_half_down_unchanged:
  assumes \<open>\<epsilon> \<ge> 0\<close>
      and \<open>\<And>y. is_half_multiple y \<Longrightarrow> x \<noteq> y \<Longrightarrow> abs (x - y) > \<epsilon>\<close>
    shows \<open>round_half_down (x - \<epsilon>) = round_half_down x\<close> 
      and \<open>\<epsilon> > 0 \<Longrightarrow> round_half_down (x - \<epsilon>) = round (x - \<epsilon>)\<close>
proof -
  show \<open>round_half_down (x - \<epsilon>) = round_half_down x\<close>
    by (metis abs_of_nonneg assms(1,2) diff_ge_0_iff_ge less_eq_rat_def linorder_not_less 
      round_half_down_changed_find_half_multiple)
next
  assume \<open>\<epsilon> > 0\<close>
  have \<open>\<not> (is_half_multiple (x - \<epsilon>))\<close>
    using \<open>0 < \<epsilon>\<close> assms(2) by fastforce
  then show \<open>round_half_down (x - \<epsilon>) = round (x - \<epsilon>)\<close>
    by (simp add: round_half_down_up)
qed

text \<open>For even quotients, the distance from half multiples is easy to find:\<close>
corollary half_distance_concrete:
  assumes \<open>2 dvd b\<close> and \<open>b > 0\<close> and x: \<open>x = (rat_of_int a) / (rat_of_int b)\<close>
  shows \<open>\<And>y. is_half_multiple y \<Longrightarrow> y \<noteq> x \<Longrightarrow> abs (x - y) \<ge> 1 / (rat_of_int b)\<close>
proof -
  from assms obtain k where k: \<open>b = 2 * k\<close> \<open>k \<noteq> 0\<close>
    by auto
  fix y
  assume \<open>is_half_multiple y\<close> and \<open>y \<noteq> x\<close>
  obtain l where y: \<open>y = (2 * rat_of_int l + 1) / 2\<close>
    using \<open>is_half_multiple y\<close> is_half_multiple_def by auto
  then have y': \<open>y = rat_of_int (2 * l * k + k) / rat_of_int b\<close> 
    using k y by (simp add: add_divide_distrib)
  then have \<open>2 * l * k + k \<noteq> a\<close>
    using \<open>y \<noteq> x\<close> x by blast
  then have \<open>abs (2 * l * k + k - a) \<ge> 1\<close>
    by auto
  moreover have \<open>abs (x - y) = rat_of_int (abs (2 * l * k + k - a)) / rat_of_int b\<close> 
    by (simp add: x y' abs_div_pos abs_minus_commute assms(2) diff_divide_distrib)
  ultimately show \<open>abs (x - y) \<ge> 1 / (rat_of_int b)\<close>
    by (metis assms(2) divide_right_mono linorder_not_less
        nle_le of_int_0_less_iff of_int_1_le_iff)
qed

lemma round_half_down_unchanged_concrete:
  assumes \<open>2 dvd b\<close> and \<open>b > 0\<close> and \<open>\<epsilon> \<ge> 0\<close> and \<open>\<epsilon> < 1 / rat_of_int b\<close>
      and \<open>x = rat_of_int a / rat_of_int b\<close>
    shows \<open>round_half_down (x - \<epsilon>) = round_half_down x\<close> 
      and \<open>\<epsilon> > 0 \<Longrightarrow> round_half_down (x - \<epsilon>) = round (x - \<epsilon>)\<close>
  using assms by (intro round_half_down_unchanged; 
    simp add: half_distance_concrete order_less_le_trans)+

end
