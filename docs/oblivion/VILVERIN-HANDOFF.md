# Player Flare and Vilverin — current handoff

Current bounded chunk: Player instant fire/shock target casts. Normal original
content is accepted: Flare uses target animation, debits14 Magicka once, visibly
flies using authored SpecialIdle_Projectile and consumes a geometric terrain miss
without actor damage. Saved flight resumes in a fresh process without relaunch
or another debit. Two normal casts kill original goblin03b526 (10→4.72→dead),
conserve Player80 Health/inventory/quests, and persist the Player killer/death
state without replay. No original records or old sewer course manifests changed.

The normal depleted-resource course exposed empty legacy error feedback. The
TES4 path now shows a visible enough-Magicka message; read-only held-key observation
proves input arrived while native state proves no cast/projectile/debit occurred.
Normal integration at fff4a4dc33 passes2257 exact engine cases. Reuse it for unchanged
mechanics/rendering after the two message-literal replacements; current normal
runtime verifies those replacements. Final sanitizer acceptance is still open.

Evidence under `build/oblivion-compat/vilverin/`: `flare-visible-launch-02`,
`flare-flight-visible-reload-01`, `flare-goblin-{select-02,fire-01,victory-01}`,
`flare-goblin-victory-reload-01`, `flare-insufficient-01`, and
`flare-insufficient-reload-02`. Audits: `normal-semantic-verification-in-progress.json`
and `normal-restart-verification.json`. Review screenshots as well as scenario
results. ASan engine01/02 were intentionally superseded for the discovered visual
phase/UI dependencies; their stop reasons and earlier fixture failures remain.
Source reproducible courses are `oblivion_player_flare_*`.

This is not full M16: general skill progression, additional tutorial Destruction
tracking, harmful-spell provocation/crime and complete FX/death visual fidelity
remain separate. Existing accepted sewer/Hackdirt evidence stays frozen. The
original Vilverin bandit references06c2aa/06c2ab use LVLC bases; the loader currently
skips them. That is the next content-driven chunk after this slice closes. The
course revision, inputs, asset identity and scope are explicit in
[VILVERIN-FLARE-CONTRACT.json](VILVERIN-FLARE-CONTRACT.json).

Next action: finish the prepared ASan/UBSan build, run final exact engine gate and
instrumented original Flare launch, flight restart and goblin hit. Audit and commit
the acceptance receipt, then implement original Vilverin leveled spawns.
