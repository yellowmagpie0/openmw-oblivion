# Kvatch main-quest course — current handoff

Target: the user's route from outside Kvatch's main gates through its first
Oblivion Gate and onward through original content. Choose subsequent bounded
chunks from observed failures on that route. Gate entry, interior progression
and full main-quest acceptance remain open.

Verified slices: [gate weather commands](KVATCH-WEATHER-VERIFICATION.json),
committed as `f558bb120c`, and
[gate creature spawning/lifetime](KVATCH-GATE-SPAWN-VERIFICATION.json).
[Spawn contract](KVATCH-GATE-SPAWN-CONTRACT.json) and
[independent original-game observations](KVATCH-GATE-SPAWN-ORACLE.json).
The latter implements default single-actor `PlaceAtMe` at the original marker,
using the original leveled list. Selected Scamp identity/stats/AI remain intact;
script/factions belong to each spawned reference and survive saves.

Normal evidence covers actual births, fresh-process retention, three timed
spawns, natural creature attacks and armor wear, an ordinary club kill, and the
original death script incrementing the gate count once. Corpse/counter cold
continuity passes. SAN covers actual births, own-save cold, timed-three actor/AI
continuation and restoration of the normal Player-caused death. No extra SAN
Player-kill claim. All nine primary runtime continuations and reviewed scene
captures pass. Engine inventory: 2,310 cases per mode; Python: 621. SAN units
use leak1, rendered SAN leak0. Producer archives are hash-verified. Retained
failures include the initial root-template rejection, a loading-overlay image,
an unsupported diagnostic query, SDL fixture preflight, and an ENOSPC unit link;
corrected attempts and archive-quota recovery remain in `kvatch-gate-spawn`.

**Current next chunk: approach and activate the actual gate through normal
controls.** Observe its authored teleport, script flags and weather release;
check the interior and cold continuity, then implement the next encountered gap.
Use the healthy save from `normal-first-gate-creature-03`:
`build/oblivion-compat/kvatch-gate-spawn/normal-first-gate-creature-03/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `7441a92e01ebdc990eb7e2599cc90be7d2024755a22807cbeed9adea196102aa`.
It retains full Player health and one genuinely spawned Scamp. Timers and
remaining original spawns continue naturally. Avoid damaged corpse-proof saves
for entry and **never use weather diagnostic count-3/missing-creature saves**
as legitimate spawn continuations.

Source inventory is the separate genuine tutorial/earned-Amulet branch,
`build/oblivion-compat/quest-items/normal-earned-amulet-owned-cold-drop-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `0d820b5cb21300bb43344ce2c4c8db1c815578f21560b84580ee6aeae4a8d292`.
MQ01 is stage100, MQ02 stage20; MQ03 was not artificially advanced. Keep this
inventory separate from Chorrol/Vilverin. The declared one-time Player MoveTo
to original marker `00cef7` established the requested starting location, not
physical travel or full quest prerequisites. Pristine staged approach SHA256
`28dd5a527b7bdb6f4dd239670ed102c206dfafa5126887e9f010b6442f92dd1f`
(count0) remains available for independent birth checks.

Original gate `01e8a3`, base `1778c8`, script `1778c5`, marker `091229`, list
`008bfd`, level-one leaf `03e9cb`, template `09122a` and script `09122b`.
Paired gate `01e8a5` is in authored cell `01e897`, landing near
[1375.229,-1281.649,765.794]. Breaking the Siege is quest `0224d8`; Find the
Heir `01e725`. Winning-record audits confirm no installed DLC overrides for the
spawn records. Raw provenance stays under `kvatch-approach`, `kvatch-weather`
and `kvatch-gate-spawn`.

[Chorrol throne work](CHORROL-THRONE-SEAT-HANDOFF.md) is complete at `4d63f80e20`;
the Countess investigation remains deferred. The requested Windows integration
is already merged; [its receipt](WINDOWS-MERGE-VERIFICATION.json) retains the
unchanged 3,310-per-mode component coverage. Do not call these bounded slices
complete Kvatch or main-quest acceptance.
