(*
 * Copyright (c) 2020-2023 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 *)
Require Import bedrock.prelude.base.
Require Import iris.proofmode.proofmode.
From iris.bi.lib Require Import fractional.

Require Import bedrock.lang.cpp.bi.cfractional.
Require Import bedrock.lang.cpp.arith.z_to_bytes.
Require Import bedrock.lang.cpp.arith.builtins.
Require Import bedrock.lang.cpp.ast.
Require Import bedrock.lang.cpp.semantics.
From bedrock.lang.cpp.logic Require Import
     arr builtins heap_pred pred z_to_bytes.

(**
[rawR q r]: the argument pointer points to [r : raw_byte] within the
C++ abstract machine.
*)
mlock Definition rawR `{Σ : cpp_logic, σ : genv} (q : cQp.t) (r : raw_byte) : Rep :=
  tptsto_fuzzyR Tu8 q (Vraw r).
#[global] Arguments rawR {_ _ _} _ _ : assert.	(* mlock bug *)

(**
[rawsR q rs]: An array of raw bytes
*)
Definition rawsR `{Σ : cpp_logic, σ : genv} (q : cQp.t) (rs : list raw_byte) : Rep :=
  arrayR Tu8 (rawR q) rs.

(*
TODO: The axioms here should be at the level of [tptsto], stated in
pred.v, and proved in simple_pred.v with the properties of [primR] and
[anyR] derived.

Also, the proof of [raw_int_byte_primR] below suggest some TODOs for
[raw_bytes_of_val].
*)
Axiom primR_to_rawsR : ∀ `{Σ : cpp_logic, σ : genv} ty q v,
  primR ty q v -|-
  Exists rs, [| raw_bytes_of_val σ ty v rs |] ** type_ptrR ty ** rawsR q rs.

Definition decodes {σ : genv} (endianness : endian) (sgn : signed) (l : list N) (z : Z) : Prop :=
  List.Forall (fun v => has_type_prop (Vn v) Tu8) l /\
  _Z_from_bytes endianness sgn l = z.

Definition decodes_uint {σ : genv} (l : list N) (z : Z) : Prop :=
  Reduce (decodes (genv_byte_order σ) Unsigned l z).

(*
TODO (JH): Determine if we can axiomatize a more specific property and
use it to derive this reasoning principle.
*)
Axiom decode_uint_anyR : ∀ `{Σ : cpp_logic, σ : genv} q sz,
  anyR (Tnum sz Unsigned) q -|-
  anyR (Tarray Tuchar (bytesN sz)) q ** type_ptrR (Tnum sz Unsigned).

(* JH: TODO: Determine what new axioms we should add here. *)
Axiom raw_byte_of_int_eq : ∀ {σ : genv} sz x rs,
  raw_bytes_of_val σ (Tnum sz Unsigned) (Vint x) rs <->
  ∃ l, decodes_uint l x /\ raw_int_byte <$> l = rs /\ length l = bytesNat sz.

Section with_Σ.
  Context `{Σ : cpp_logic} {σ : genv}.

  (** [rawR] *)

  #[local] Notation PROPER R S := (
    Proper (R ==> eq ==> eq ==> S) (@rawR _ _)
  ) (only parsing).
  #[global] Instance rawR_mono : PROPER genv_leq bi_entails.
  Proof. rewrite rawR.unlock. solve_proper. Qed.
  #[global] Instance rawR_flip_mono : PROPER (flip genv_leq) (flip bi_entails).
  Proof. repeat intro. exact: rawR_mono. Qed.
  #[global] Instance rawR_proper : PROPER genv_eq equiv.
  Proof. intros σ1 σ2 [??] ??? ???. split'; exact: rawR_mono. Qed.

  #[global] Instance rawR_timeless : Timeless2 rawR.
  Proof. rewrite rawR.unlock. apply _. Qed.

  #[global] Instance rawR_cfractional : CFractional1 rawR.
  Proof. rewrite rawR.unlock. apply _. Qed.
  #[global] Instance rawR_as_cfractional : AsCFractional1 rawR.
  Proof. solve_as_cfrac. Qed.
  #[global] Instance rawR_cfrac_valid : CFracValid1 rawR.
  Proof. rewrite rawR.unlock. solve_cfrac_valid. Qed.

  #[global] Instance rawR_wellyped q r :
    Observe (pureR (has_type (Vraw r) Tu8)) (rawR q r).
  Proof.
    rewrite -has_type_or_undef_nonundef// rawR.unlock. apply _.
  Qed.
  #[global] Instance _at_rawR_wellyped (p : ptr) q r :
    Observe (has_type (Vraw r) Tu8) (p |-> rawR q r).
  Proof. apply _at_observe_pureR, _. Qed.

  #[global] Instance rawR_type_ptrR q r : Observe (type_ptrR Tu8) (rawR q r).
  Proof. rewrite rawR.unlock. apply _. Qed.

  #[global] Instance rawR_nonnull q r : Observe nonnullR (rawR q r).
  Proof. rewrite rawR.unlock. apply _. Qed.

  #[global] Instance rawR_agree : AgreeCF1 rawR.
  Proof.
    intros. rewrite rawR.unlock. iIntros "R1 R2".
    iDestruct (observe_2 [| val_related _ _ _ |] with "R1 R2") as %?.
    eauto using val_related_Vraw.
  Qed.

  Lemma rawR_tptsto_acc (p : ptr) q r :
    p |-> rawR q r |--
      Exists v', [| val_related Tu8 (Vraw r) v' |] ** tptsto Tu8 q p v' **
      (Forall p' q' v', [| val_related Tu8 (Vraw r) v' |] -* tptsto Tu8 q' p' v' -* p' |-> rawR q' r).
  Proof.
    rewrite rawR.unlock. by rewrite tptsto_fuzzyR_tptsto_acc.
  Qed.

  Lemma rawR_ptr_congP_transport p1 p2 q r :
    ptr_congP σ p1 p2 |-- p1 |-> rawR q r -* p2 |-> rawR q r.
  Proof.
    iIntros "#P R".
    iDestruct (rawR_tptsto_acc with "R") as "(%v' & V & R & HR)".
    iDestruct (tptsto_ptr_congP_transport with "P R") as "R".
    iApply ("HR" with "V R").
  Qed.

  Lemma rawR_offset_congP_transport (p : ptr) o1 o2 q r :
    offset_congP σ o1 o2 |--
    type_ptr Tu8 (p ,, o2) -*
    p ,, o1 |-> rawR q r -*
    p ,, o2 |-> rawR q r.
  Proof.
    iIntros "#O #T2 R".
    iDestruct (observe (type_ptr _ _) with "R") as "#T1".
    iApply (rawR_ptr_congP_transport with "[] R").
    by iApply offset_ptr_congP.
  Qed.

  (** [rawsR] *)

  (**
  NOTE: [rawsR] could have [Param] and [Proper] instances supporting
  [genv_le], [genv_eq].
  *)

  #[global] Instance rawsR_timeless q rs : Timeless (rawsR q rs).
  Proof. apply _. Qed.

  #[global] Instance rawsR_cfractional : CFractional1 rawsR.
  Proof. apply _. Qed.
  #[global] Instance rawsR_as_fractional : AsCFractional1 rawsR.
  Proof. solve_as_cfrac. Qed.

  Lemma rawsR_observe_frac_valid q rs :
    (0 < length rs) ->
    Observe [| cQp.frac q ≤ 1 |]%Qp (rawsR q rs).
  Proof.
    intros Hlen; rewrite /rawsR; induction rs;
      by [ simpl in Hlen; lia
         | rewrite arrayR_cons; apply _].
  Qed.

  Lemma rawsR_observe_agree q1 q2 rs1 rs2 :
    length rs1 = length rs2 ->
    Observe2 [| rs1 = rs2 |] (rawsR q1 rs1) (rawsR q2 rs2).
  Proof.
    generalize dependent rs2; induction rs1 as [| r1 ? ?]; intros * Hlen.
    - destruct rs2; [| simpl in Hlen; lia].
      rewrite /Observe2; iIntros "? ? !>"; by iPureIntro.
    - destruct rs2 as [| r2 ?]; [simpl in Hlen; lia |].
      rewrite !cons_length in Hlen; inversion Hlen.
      rewrite /rawsR !arrayR_cons;
        fold (rawsR q1 rs1);
        fold (rawsR q2 rs2).
      iIntros "(Htype_ptrR1 & HrawR1 & HrawsR1)
               (Htype_ptrR2 & HrawR2 & HrawsR2)".
      iDestruct (observe_2 [| r1 = r2 |] with "HrawR1 HrawR2") as %->.
      specialize (IHrs1 rs2 ltac:(auto)).
      iDestruct (observe_2 [| rs1 = rs2 |] with "HrawsR1 HrawsR2") as %->.
      iModIntro; by iPureIntro.
  Qed.

  Lemma raw_int_byte_primR' q r n :
    raw_int_byte n = r ->
    rawR q r -|- primR Tu8 q (Vn n).
  Proof.
    intros <-. rewrite primR_to_rawsR. split'.
    - iIntros "R". iExists [raw_int_byte n].
      rewrite /rawsR arrayR_singleton.
      iDestruct (observe (type_ptrR Tu8) with "R") as "#T". iFrame "R T".
      (**
      TODO: Missing axiom [raw_bytes_of_val σ Tu8 (Vn n) [raw_int_byte n]]
      *)
      admit.
    - iIntros "(% & %Hraw & #T & R)".
      (**
      TODO: Missing axioms allowing us to invert [raw_bytes_of_val σ
      Tu8 (Vn n) rs] to learn [rs] the singleton [raw_int_byte n ::
      nil].
      *)
      admit.
  Admitted.
  Lemma raw_int_byte_primR q n : rawR q (raw_int_byte n) -|- primR Tu8 q (Vn n).
  Proof. exact: raw_int_byte_primR'. Qed.

  (** TODO: determine whether this is correct with respect to pointers *)
  Lemma decode_uint_primR q sz (x : Z) :
    primR (Tnum sz Unsigned) q (Vint x) -|-
      Exists (rs : list raw_byte) (l : list N),
      (**
      Reconsider: We may only need one list and [raw_bytes_of_val σ
      (Tnum sz Unsigned) (Vint x) rs]
      *)
      [| decodes_uint l x |] **
      [| raw_int_byte <$> l = rs |] **
      [| length l = bytesNat sz |] **
      type_ptrR (Tnum sz Unsigned) **
      arrayR Tu8 (fun c => primR Tu8 q (Vint c)) (Z.of_N <$> l).
  Proof.
    rewrite primR_to_rawsR. f_equiv=>rs. rewrite raw_byte_of_int_eq. split'.
    - iIntros "(%Hraw & T & Rs)". destruct Hraw as (l & Hdec & Hrs & Hlen).
      iExists l. iFrame (Hdec Hrs Hlen) "T".
      rewrite -{}Hrs /rawsR arrayR_eq/arrayR_def. rewrite arrR_mono//.
      decompose_Forall. apply Forall_forall=>b ? /=.
      by rewrite raw_int_byte_primR.
    - iIntros "(%l & %Hdec & %Hrs & %Hlen & #T & Rs)". iFrame "T".
      iSplit; eauto. rewrite -{}Hrs /rawsR arrayR_eq/arrayR_def. rewrite arrR_mono//.
      decompose_Forall. apply Forall_forall=>b ? /=.
      by rewrite raw_int_byte_primR.
  Qed.

End with_Σ.

Module Endian.
  Section with_Σ.
    Context `{Σ : cpp_logic} {σ : genv}.

    Lemma decodes_uint_to_end :
      forall endianness sz l v,
        length l = bytesNat sz ->
        decodes endianness Unsigned l v ->
        decodes_uint l (to_end endianness sz v).
    Proof.
      move=> endianness sz l v Hsz [Hbyte Hdecode].
      rewrite /decodes in Hdecode.
      rewrite /decodes_uint/to_end/to_little_end/to_big_end.
      split; first assumption.
      repeat case_match; eauto;
        rewrite z_to_bytes._Z_from_bytes_eq/z_to_bytes._Z_from_bytes_def;
        rewrite z_to_bytes._Z_from_bytes_eq/z_to_bytes._Z_from_bytes_def in Hdecode;
        [ | replace l with (rev (rev l)) by (apply rev_involutive)];
        erewrite z_to_bytes._Z_from_bytes_unsigned_le_bswap; eauto;
        now rewrite rev_length.
    Qed.

    Lemma decodes_Z_to_bytes_Unsigned:
      forall (sz : bitsize) (n : nat)  (endianness : endian) (z : Z),
        (bytesNat sz = n)%nat ->
        has_type_prop z (Tnum sz Unsigned) ->
        decodes endianness Unsigned (_Z_to_bytes n endianness Unsigned z) z.
    Proof.
      intros * Hsz Hty; subst.
      rewrite /decodes.
      split.
      2: {
        erewrite _Z_from_to_bytes_roundtrip; try reflexivity.
        move: Hty.
        rewrite -has_int_type.
        rewrite/bound/min_val/max_val.
        destruct sz; rewrite/bytesNat; split; lia.
      }
      exact: _Z_to_bytes_has_type_prop.
    Qed.

    Lemma raw_bytes_of_val_to_end_raw_int_byte (endianness : endian) (sz : bitsize) (z : Z) :
      has_type_prop z (Tnum sz Unsigned) ->
      raw_bytes_of_val σ (Tnum sz Unsigned) (to_end endianness sz z)
        (map raw_int_byte (_Z_to_bytes (bytesNat sz) endianness Unsigned z)).
    Proof.
      rewrite raw_byte_of_int_eq.
      exists (_Z_to_bytes (bytesNat sz) endianness Unsigned z).
      intuition.
      2: by rewrite _Z_to_bytes_length //.
      apply: decodes_uint_to_end.
      { rewrite _Z_to_bytes_length; reflexivity. }
      apply: (decodes_Z_to_bytes_Unsigned sz); try reflexivity.
      assumption.
    Qed.

    Lemma raw_bytes_of_val_to_big_end_raw_int_byte (sz : bitsize) (z : Z) :
      has_type_prop z (Tnum sz Unsigned) ->
      raw_bytes_of_val σ (Tnum sz Unsigned) (to_big_end sz z)
        (map raw_int_byte (_Z_to_bytes (bytesNat sz) Big Unsigned z)).
    Proof.
      intros H; apply (raw_bytes_of_val_to_end_raw_int_byte Big) in H.
      by rewrite /to_end/= in H.
    Qed.

    Lemma raw_bytes_of_val_to_little_end_raw_int_byte (sz : bitsize) (z : Z) :
      has_type_prop z (Tnum sz Unsigned) ->
      raw_bytes_of_val σ (Tnum sz Unsigned) (to_little_end sz z)
        (map raw_int_byte (_Z_to_bytes (bytesNat sz) Little Unsigned z)).
    Proof.
      intros H; apply (raw_bytes_of_val_to_end_raw_int_byte Little) in H.
      by rewrite /to_end/= in H.
    Qed.
  End with_Σ.
End Endian.
