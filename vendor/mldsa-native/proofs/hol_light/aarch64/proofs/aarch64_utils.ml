(*
 * Copyright (c) The mldsa-native project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 *)

(* Merge 4 x bytes32 reads into bytes128 reads *)
let MEMORY_128_FROM_32_TAC =
  let a_tm = `a:int64` and n_tm = `n:num` and i64_ty = `:int64`
  and pat = `read (memory :> bytes128(word_add a (word n))) s0` in
  fun v boff n ->
    let pat' = subst[mk_var(v,i64_ty),a_tm] pat in
    let f i =
      let itm = mk_small_numeral(boff + 16*i) in
      READ_MEMORY_MERGE_CONV 2 (subst[itm,n_tm] pat') in
    MP_TAC(end_itlist CONJ (map f (0--(n-1))));;

(* Utility for executing until target PC is reached *)
let MAP_UNTIL_TARGET_PC f n = fun (asl, w) ->
  let is_pc_condition = can (term_match [] `read PC some_state = some_value`) in
  let extract_target_pc_from_goal goal =
    let _, insts, _ = term_match [] `eventually arm (\s'. P) some_state` goal in
    insts |> rev_assoc `P: bool` |> conjuncts |> find is_pc_condition in
  let extract_pc_assumption asl =
    try Some (find (is_pc_condition o concl o snd) asl |> snd |> concl) with _ -> None in
  let has_matching_pc_assumption asl target_pc =
    match extract_pc_assumption asl with
     | None -> false
     | Some(asm) -> can (term_match [`returnaddress: 64 word`; `pc: num`] target_pc) asm in
  let target_pc = extract_target_pc_from_goal w in
  let TARGET_PC_REACHED_TAC target_pc = fun (asl, w) ->
    if has_matching_pc_assumption asl target_pc then ALL_TAC (asl, w)
    else NO_TAC (asl, w) in
  let rec core n (asl, w) =
    (TARGET_PC_REACHED_TAC target_pc ORELSE (f n THEN core (n + 1))) (asl, w)
  in core n (asl, w);;
