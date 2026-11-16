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

**Latest accepted chunk: MS48 stage10 ModFactionReaction.** The original
agreement adds20 to winning KvatchGuards `0224d7` → PlayerFaction `01dbcd`
reaction20, producing40; reverse remains0. The fork uses the existing directed
RuntimeState owner (v81 within current schema106), with no shared FACT mutation.
Normal/SAN actual agreements and own-save cold getters/resaves all pass; other
reactions, inventory ownership/counts and quest stage10 persist. Engine:2,310 per
mode; Python:621. [Contract](KVATCH-FACTION-REACTION-CONTRACT.json),
[original oracle](KVATCH-FACTION-REACTION-ORACLE.json),
[verification](KVATCH-FACTION-REACTION-VERIFICATION.json).

**Next chunk: actual gate approach and entry from the completed agreement.**
Use the normal cold save:
`build/oblivion-compat/kvatch-faction-reaction/normal-savlian-agreement-cold-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `703fcbfb42cbd614ab841a897e6d1db83788cb7a8b00e533b19d345792b14e42`.
It has an alive Player, earned inventory, three genuine timed Scamps, MS48
stage10 and guard reaction40. Aim at the original gate and move/activate through
normal controls. Do not replay Help or inject quest/stat writes. If combat
blocks travel, use actual earned resources and inspect the observed failure.
Check authored teleport, script flags, weather release and cold continuity;
then choose the next gap from actual interior gameplay.

Earlier direct gate attempts met ordinary combat deaths/obstruction. The latest
`kvatch-gate-entry/normal-help-gate-entry-01` also exposed the now-fixed stage10
command before a later death; no causal claim that the command caused that death.
Savlian's friendly Ambush force-greeting already works (actor `028a7a`, base
`028007`, package `17c463`) at the barricade south of the original staging marker.
The original completed voiced response changes faction state after speaking;
no identical voice timing is claimed for the fork's text UI.

Reusable compact normal/SAN producers and standalone verified archives are under
`build/oblivion-compat/kvatch-faction-reaction`. Their loaded ELF sections/entry
match the full builds. Earlier accepted full-debug producers remain recoverable
from their original durable archives; their executable caches were retired.
Failed probes/archive/input attempts stay in their ignored course directories.
Keep generated data/images outside Git and retain original test provenance.

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
