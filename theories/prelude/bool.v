(*
 * Copyright (C) BedRock Systems Inc. 2020-21
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 *)
Require Export bedrock.prelude.base.

Local Set Printing Coercions.

Lemma Is_true_is_true b : Is_true b ↔ is_true b.
Proof. by destruct b. Qed.
Lemma Is_true_eq b : Is_true b ↔ b = true.
Proof. by rewrite Is_true_is_true. Qed.

Global Instance orb_comm' : Comm (=) orb := orb_comm.
Global Instance orb_assoc' : Assoc (=) orb := orb_assoc.

Section implb.
  Implicit Types a b : bool.

  Lemma implb_True a b : implb a b ↔ (a → b).
  Proof. by rewrite !Is_true_is_true /is_true implb_true_iff. Qed.
  Lemma implb_prop_intro a b : (a → b) → implb a b.
  Proof. by rewrite implb_True. Qed.
  Lemma implb_prop_elim a b : implb a b → a → b.
  Proof. by rewrite implb_True. Qed.
End implb.
