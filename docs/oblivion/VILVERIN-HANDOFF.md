# Player Flare and Vilverin — current handoff

Current bounded chunk: Player instant fire/shock target casts. Original Flare now
launches with the native target animation, debits14 Magicka once and owns saved
physical flight. Two normal casts kill original goblin03b526 (10→4.72→dead) with
Player80 Health conserved and the Player recorded as killer. A lakeshore terrain
miss applies no actor damage. Fresh flight continuation preserves ownership.

Real visual inspection found that Fireball.NIF initially plays its Cast phase.
The renderer now selects authored SpecialIdle_Projectile and loops by saved flight
age; `flare-visible-launch-02` visibly shows the fireball. A fresh visible restart
and final normal/sanitizer integration remain open. General natural skill
progression and additional tutorial skill-use tracking are separate gaps.

The original Vilverin bandit references06c2aa/06c2ab use LVLC bases; the cell
loader skips native leveled actors. That is the next content-driven chunk after Flare closes.
The explicit course revision and immutable lakeshore/goblin inputs are in
[VILVERIN-FLARE-CONTRACT.json](VILVERIN-FLARE-CONTRACT.json). Existing sewer manifests
and the accepted Hackdirt reward are unchanged.

Reusable evidence under `build/oblivion-compat/vilverin/`: baseline03 reproduces
the old rejection; focused02 passes14 existing checks; normal-engine01 passes2257
engine tests before the visual selector. ASan engine01 was intentionally stopped
before expensive world-test compilation for this discovered dependency; retain
its stop reason and rerun after the visual fix. Prior fixture/input/version
failures are preserved. Source gameplay manifests are `oblivion_player_flare_*`.

Next action: fresh visible flight, depleted-Magicka rejection and completed-goblin
restart; commit the asset-phase fix and manifests, then run final normal/sanitizer
gates and instrumented real-content courses. Close this slice only when those
gaps are empty, then implement original Vilverin leveled spawns.
