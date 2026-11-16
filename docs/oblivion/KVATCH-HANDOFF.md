# Kvatch main-quest course — current handoff

User-directed target: start outside Kvatch's main gates at the first Oblivion
Gate, enter it, and proceed through the original content. Choose subsequent
bounded implementation chunks from observed failures on this route. Do not
substitute unrelated furniture or inventory work. Full quest completion remains
unproven until the actual course and its save/reload continuations pass.

The previous [Chorrol throne chunk](CHORROL-THRONE-SEAT-HANDOFF.md) is committed
as `4d63f80e20`. Its normal/SAN lifecycle, canonical back-entry regression,
2,310 engine tests per mode and 621 Python checks pass. The Countess arrival
investigation is deferred; evidence remains in
`build/oblivion-compat/next-npc-throne`.

Verified bounded slice: original gate storm weather commands. `GetIsCurrentWeather`
reads the incoming identity once its fade starts; `SetWeather` requests the
authored transition with persistent override without restarting the same fade.
Normal/SAN actual gate weather observations and diagnostic cold continuity pass.
[Contract](KVATCH-WEATHER-CONTRACT.json),
[independent native observations](KVATCH-WEATHER-ORACLE.json),
[verification](KVATCH-WEATHER-VERIFICATION.json).
Engine coverage is 2,310 cases per mode and 621 Python checks. SAN unit leaks are
enabled; rendered SAN leaks disabled. Exact producer identities have verified
chunk archives against retained full Back anchors.

**The original gate course still fails** at `PlaceAtMe` while spawning creatures.
Do not relabel the failed transition scenarios or diagnostic cold saves as valid
controller, approach, entry or quest acceptance. The latter inherit count 3 with
missing creatures from that failed script and are weather evidence only.

Current next chunk: original gate leveled creature creation/lifetime/cold ownership.
Resume pristine staged approach `28dd5a52...` (count 0), not weather diagnostic
continuations. At this character's level 1, original `KvatchExteriorCreature`
list `008bfd` selects Stunted Scamp `03e9cb`. Existing native-reference allocator,
leveled selector and saved NPC/creature reconstruction provide reusable support;
the host `PlaceAtMe` dispatch/creation integration is missing. Read-only original
list audit: `build/oblivion-compat/kvatch-gate-spawn/master-list-audit.json`.
Keep subsequent work on the user-directed gate/interior route.

Source: the separate genuine tutorial/earned-Amulet branch,
`build/oblivion-compat/quest-items/normal-earned-amulet-owned-cold-drop-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `0d820b5cb21300bb43344ce2c4c8db1c815578f21560b84580ee6aeae4a8d292`.
Tutorial MQ01 is stage 100, Deliver the Amulet MQ02 is stage 20; MQ03 has not
been artificially advanced. Keep this inventory separate from the Chorrol
reward and Vilverin branches. The staging manifest declares one engine MoveTo
of Player to original map marker `00cef7`, before gate `01e8a3`. This establishes
the requested starting location; it does not certify physical travel or main-
quest prerequisites. No item grants, quest edits, NPC repositioning or added
records. Subsequent entry/progression uses normal controls and original scripts.

Original records: gate `01e8a3` (`MS48KvatchGate`), base `1778c8`
(`MS48OblivionGate`), enable parent `091229`, teleport pair `01e8a5` in authored
cell `01e897`. Breaking the Siege of Kvatch is quest `0224d8`; Find the Heir is
`01e725`. Raw metadata/provenance is retained in `master-route-audit.json`.
Audit winning installed records and inspect actual enabled/script/runtime state
before attributing a failure to an implementation gap.

Next action: implement and verify the actual gate spawn dependency from pristine
approach; confirm real actor appearance, identity, AI/combat/life and cold count/
state without duplicate spawns. Then approach/activate the real gate and continue
inside. Weather-slice completion does not close this gameplay course.

The diagnostic staging save is structurally and semantically verified despite
its failed runtime scenario: inventory, Player values, reputation, quests and
allocator match the earned source; actual native save equals its captured live
snapshot. SHA256 `28dd5a527b7bdb6f4dd239670ed102c206dfafa5126887e9f010b6442f92dd1f`.
Its first image caught loading UI. Positive checks must wait for actual Player
control/unpaused world and rendered scene. The original-game weather probe exited
0 with exact documents/plugins/pristine-save restoration and no owned processes.
