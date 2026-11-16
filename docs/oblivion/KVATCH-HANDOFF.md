# Kvatch main-quest course — current handoff

Target: outside Kvatch's main gates → original Savlian encounter → first Oblivion
Gate → original interior progression. Gate entry, interior progression and full
main-quest acceptance remain open. Choose chunks from observed route failures.

Accepted slices: [weather commands](KVATCH-WEATHER-VERIFICATION.json),
[gate creature spawning/lifetime](KVATCH-GATE-SPAWN-VERIFICATION.json) at
`58f203eedb`, and [Savlian INFO-added topics](KVATCH-TOPIC-LEARNING-VERIFICATION.json).
The topic chunk consumes reply `098143`'s two authored NAME subjects, Help and
Oblivion Gate, through existing Player knowledge/DIAS persistence. Normal and
SAN original greeting/reply teach both (26 → 28); fresh processes retain both
and select Help, opening INFO `098144` and advancing MS48 to stage8. Cold saves
are made before that selection and remain stage5/donowchoice1. Engine: 2,310 per
mode; final Python: 621. Rendered SAN uses leak0, engine SAN leak1. Ambient
Say/SayTo and NPC-to-NPC topic exposure remain outside this accepted slice.
[Contract](KVATCH-TOPIC-LEARNING-CONTRACT.json),
[independent original UI oracle](KVATCH-TOPIC-LEARNING-ORACLE.json).

**Next chunk: accept Help, then approach and activate the actual gate.**
Use the healthy, earned-inventory SAN learning save as a normal continuation:
`build/oblivion-compat/kvatch-savlian/sanitized-savlian-topics-learned-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `34f7656b436941a165bd87298da78247506189cdb671544526417b730281e7c3`.
It has three genuine timed Scamps and MS48 stage5. Select retained Help, accept
its first answer (INFO `028030`, authored stage10), then walk to the gate.
Private prepared course: `kvatch-savlian/help-agree-gate-entry.json`; publish
its tested editable manifest with the next slice. No quest/topic/stat injections.
Check the authored teleport, script flags, weather release and cold continuity;
then take the next gap from actual interior gameplay.

Direct gate attempts met ordinary Scamp deaths/obstruction, without a new
unsupported command. Savlian's existing friendly Ambush force-greeting already
works; it needs no reimplementation. Actor `028a7a`, base `028007`, package
`17c463`, stands at the barricade south of the original staging marker.
Retained topic failures were input timing, camera aim and controller Goodbye
focus; explicit held movement, live aiming and first-topic focus corrected them.
All failures remain under `build/oblivion-compat/kvatch-savlian`. Producer source
hashes and durable archives are in its verification receipt. Current executable
caches remain available. Keep generated data/images outside Git.

The separate genuine tutorial/earned-Amulet source is
`build/oblivion-compat/quest-items/normal-earned-amulet-owned-cold-drop-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `0d820b5cb21300bb43344ce2c4c8db1c815578f21560b84580ee6aeae4a8d292`.
MQ01 stage100, MQ02 stage20; MQ03 was not artificially advanced. One declared
Player MoveTo to marker `00cef7` established the requested location, not physical
cross-country travel or full quest prerequisites. Keep this inventory separate
from Chorrol/Vilverin. Never use weather diagnostic count3/missing-creature
saves as legitimate spawn continuations. Earlier source/evidence details are
linked from the spawn contract and receipt rather than repeated here.

Gate `01e8a3`, base `1778c8`, script `1778c5`, marker `091229`, list `008bfd`,
leaf `03e9cb`, template `09122a`/script `09122b`. Paired gate `01e8a5` is in
cell `01e897`, world `01d0bc`. Portal pose [1375.229,-1281.649,765.794] differs
from the incoming XTEL landing [1372.577,-1088.240,661.154], facing north.
Breaking the Siege: quest `0224d8`; Find the Heir: `01e725`. Original content,
records and probe provenance remain in ignored course evidence directories.

[Chorrol throne work](CHORROL-THRONE-SEAT-HANDOFF.md) is complete at `4d63f80e20`;
Countess investigation is deferred. The requested Windows integration is
already merged: [receipt](WINDOWS-MERGE-VERIFICATION.json), including unchanged
3,310-per-mode component coverage. Do not repeat that merge or call these slices
complete Kvatch/main-quest acceptance.
