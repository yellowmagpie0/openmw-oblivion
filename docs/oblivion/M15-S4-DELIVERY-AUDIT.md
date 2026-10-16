# M15S4 delivery audit

Status: **incomplete**. The initial audit inspected `4996a2eb1a3a666751358f7435a459907e6e6345`
on 2026-10-15. This is a working requirement map, not acceptance. The controlling
scope is [S4 in the implementation plan](M15-COMBAT-STEALTH-CRIME-IMPLEMENTATION-PLAN.md#s4---deliver-a-normal-input-melee-slice),
including its tests and gate. Historical pure-rule, isolated body and service
checkpoints in the milestone ledger do not establish complete live gameplay.

## Required implementation

| S4 item | Current source evidence | Remaining delivery work |
| --- | --- | --- |
| 1. Input through contact for Player, NPC and creature | `CharacterController::updateOblivionWeaponState`, `dispatchOblivionMeleeContact`, `advanceOblivionMeleePlayback`; `Actors::update` handles paused input. Ordinary Player/NPC contact reaches `commitOblivionOrdinaryMeleeContact`. | Creature dispatch explicitly returns before contact. Trace and verify every actor route through production input, animation and class/service ownership. |
| 2. Blade/blunt/unarmed/creature, normal/directional power, geometry | `oblivionmelee.cpp` acquires collision/LOS contacts and resolves ordinary weapon/unarmed damage. `isOrdinaryMeleeContactPending` admits only Left/Right kinds. | Power selection can create animation/action ownership, but the ordinary contact predicate rejects power kinds. Directional/mastery selection is explicitly unfinished in the controller. Creature attacker and victim domains are rejected by ordinary adapters. Implement the native routes and verify geometry without a TES3 chance roll. |
| 3. Block, mitigation, wear, fatigue and reactions | `beginBlocking`/`endBlocking` own posture; ordinary contacts prepare values, conditions, RNG and timed knockback. | Basic equipped block now prepares shield/weapon mitigation, separate Fatigue cost and sequential condition wear through the owned contact. Expert/Master defensive procs, zero-absorption unarmed weapon-block reactions and zero/broken-after-wear items remain rejected. Complete proc/breakage/reaction publication and renderer/physics recovery once per window. |
| 4. Safe interruptions | Controller cancellation clears strike/input/posture before animation disable. Actors cancel Player and owned stationary intents on pause and control locks. Lifetime-guarded unequip notifications can now be staged for dispatch after controller/actor updates and retained across simulation rollback. | Verify all named transitions: weapon change/breakage, unequip, menu/control lock, stagger/knockdown, death, load, mount; release/held-input behavior and restarted ownership need live coverage. Existing guards alone are not proof of the entire matrix. |
| 5. Actual animation, HUD and sound | Native KF timing, swish, unarmored NPC hits and equipped-block sound selection are wired in the controller. Weapon-block stock PCM and fresh-process replay checks pass. | Armored ordinary hits, unarmed-only/creature sound branches and live shield audio remain open. Complete native reaction animation and related sound/HUD feedback across the required matrix. |
| 6. Command/query semantics | `OblivionCombatService` and World/script adapters share native values, process action and engagement. S3 accepts authority/persistence infrastructure. | Audit every melee-related command/query against the completed contact, mastery and reaction paths. S3 authority acceptance does not prove those later semantics. |
| 7. Minimal production stationary opponent | `updateOblivionStationaryMeleeAi` sets attack intent after production reach/LOS checks; authored deterministic CSTY fixture and prior runtime matrices exist. | Revalidate alongside completed Player blocking and recovery. It must continue through controller/contact, without injected damage or outcomes. Full S7 movement/style policy remains outside this item. |
| 8. Mastery, dropped items and narrow effects | S2 pure rules and prepared inventory/resource primitives exist. | Wire every melee/block/armor mastery branch to live contacts, dropped-item identities and required timed effects. Enchanted weapon contact currently returns nullopt; resolve the narrowly required combat hook rather than declaring all effects deferred to M16. |

Other deliberate ordinary-contact restrictions include sneaking, nonzero normal
weapon resistance, negative incoming damage, and remaining breakage branches.
They need their reviewed native branches before broader contact acceptance.
Removing a rejection without its complete consequences does not deliver a branch.

## Required verification

The section 8 minimum rows relevant to S4 are Melee/contact, Block/armor,
Mastery and Reactions. Cover discrete branches at L1 and appropriate pairwise /
high-risk crosses at L2/L3. Required live assertions include exact pre/post Health,
Fatigue, armor and weapon condition, contact identity and no replay.

The complete S4 course requires a named nonlethal target attacked with normal
input, blocking its production-policy attack, misses outside reach and behind a
wall, and recovery after stagger. The bounded equipped-block course now verifies
a production opponent attack and held-block restart, including stock audio.
Complete the combined course and repeat first/third person, keyboard/mouse and
gamepad, and FPS cases. Save/quit/fresh-process load during windup, blocking and
recovery must match uninterrupted continuation. These are distinct from idle
restart or direct rule/service tests. The final gate is input -> animation
contact -> one native delta -> visible/audible reaction. Also retain the plan's
sanitized synthetic melee course requirement.

## Current regression evidence

`build/oblivion-compat/m15/S4/current-melee-engine-01/verification.json` rebuilds
`openmw`, `openmw-tests` and `esmtool`, then passes all **49 selected engine cases**
with exact inventory/XML agreement, no failures and no skips. Filter:
`*Melee*:*Blocking*:*Weapon*:*Unarmed*:*PhysicalContact*`.
This is not the full engine inventory, component coverage, sanitizer coverage or
the complete S4 matrix. Tested source fingerprint:
`61f0c6034959308e06384f3bb02a355f7d13c30f1ee99a2e53a5d9858934f3e6`.

Prioritize complete live contact branches and their reaction integration over
additional isolated arithmetic coverage. The rejection sites above provide
concrete next integration points; preserve S2 native-rule provenance.

The same 49 selected cases pass under ASan/UBSan in
`S4/current-melee-engine-sanitized-01`, with exact inventory agreement and zero
failures/skips. ASan leak checking is disabled; UBSan halts on errors. Source
fingerprint: `7e2ba3a41a35cba1c8045c120644501911f0f0e71438769dddf4d7b425b11cb3`.
The intervening source change is this audit document; gameplay code is unchanged.

`S4/current-firstperson-restart-02/verification.json` records a current-build
normal-input unarmed miss and idle fresh-process restart. PIDs 427968 and 428183
are distinct and terminal. The schema-29 source migrates to schema46. Exactly
one action advances the counter2 -> 3, one miss spends Fatigue140 -> 133, and
no pending action or strike survives; Fatigue regenerates before saving.
Both outputs preserve all nine source native actor-value arrays, all four
Player inventory items (absent ownership defaults normalized), and combat RNG.
The second input hash matches the first output. Binary SHA256:
`86b283aa1bd04507cd1d6b8ccc23e45b4506ca597c07da970bd0c3262435fcc2`.
The report also records the loaded MyGUI library and source/output save hashes.

Actual captured PCM has exactly one matching stock `wpn_swishhand_02.wav`
cluster before restart (correlation0.9999204689) and none afterward, using the
unchanged prior acceptance threshold and hash-identified stock assets. Both
ready and restarted screenshots were directly reviewed: the dungeon and drawn
first-person unarmed hands render. The original source stays unchanged.
The preserved `current-firstperson-restart-01` attempt failed before engine
launch because `miss_fixture` was missing from the invocation; corrected02
supplies the declared fixture path. The failed attempt proves no gameplay.

This course covers a miss and **idle** restart only. It does not establish
nonlethal target damage, blocking an opponent, reactions, other weapons/views/
controls, power/mastery, or windup/block/recovery restart equivalence. All those
requirements above remain open. No gameplay implementation changed in this
audit checkpoint, and S4 is not delivered.

## Unarmed contact against knocked actors — implementation checkpoint

`resolveOblivionOrdinaryUnarmedContact` now admits nonpositive incoming hand
Fatigue and prepares the native armor-wear branch. The production contact adapter
forwards its prepared condition changes and RNG transition into the same owned
transaction as Health/Fatigue. Positive hand Fatigue preserves the existing
no-armor-wear route. This removes the prior blanket rejection of punches against
knocked actors. Armor breakage still rejects before publication; this checkpoint
does not replace the missing unequip/drop/reaction policy.

Independent original executable SHA256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`S4/unarmed-zero-fatigue-oracle-01` contains six original mitigation/selection/
condition observations, both x87 precision words, with three RNG seeds. Supplied
Health20, armor25, wear multiplier9 and novice heavy skill5 produce Health15,
wear45 and condition100.125 -> 32.625. Seed0/1/5540 consume2/3/1 draws and finish
at505908858/415139642/1188163031. Corpus SHA256:
`dac026c0609720371817f9ba28a79af035820e9867a7d676fb5cf068f3cf71af`.
These concatenate original paths with declared AV/item/TLS boundaries; they
are not live game contacts. Ten additional original5FF712..5FF7BD observations
vary the actual incoming Fatigue and prove positive values skip wear, including
signed-zero boundaries. Forty-eight corrected5FFBE3 dispatch observations check
armor fraction, wear and NPC/creature form gates through the actual base getter.
The retained first gate attempt mislabeled stack+38 as Fatigue; it is the armor
fraction. Only `gate-report-02.json` and `fatigue-gate-report.json` are accepted
for those respective gates. The mislabeled attempts are explicitly invalidated.

The new World integration case crosses three seeds with raw knocked states
0/1/3/6/-1. It verifies read-only preparation, exact native Health/Fatigue/wear,
base rather than current armor mastery, stale-RNG rejection without mutation,
atomic resources/condition/RNG publication and no action replay after binary
service restore. The existing unarmed identity corpus now expects the admitted
zero-Fatigue result and retains its unchanged-state assertion. This is service/
inventory integration; the assembled state snapshot is not a fresh-World armor
restoration course.

All1478 engine cases /121 suites pass normally and under ASan/UBSan, with exact
unfiltered inventories and zero failures/skips, in
`S4/unarmed-zero-fatigue-full-normal-01` and `-full-sanitized-01`. The focused
five-case runs also pass in `-normal-01` / `-sanitized-01`. The tested dirty-source
fingerprint is
`a76d13e90b0566659c2a951c2546bfab782f81872a58265b6b8e084c2b404184`;
only this evidence documentation and editable scenario/recipe files follow it.
ASan leak checks are disabled; UBSan halts. Existing unrelated compiler warnings
remain in the build logs. Components and Python implementation are unchanged.

Editable runtime inputs are
`scripts/data/oblivion_compat/oblivion_m15_unarmed_knocked_contact.json` and
`m15_unarmed_knocked_contact_fixture.json`; continuation reuses the existing
`oblivion_m15_native_firstperson_melee_idle_continuation.json` (only the diagnostic
name differs in the recorded copy). The fixture declares copied-source identity,
geometry, AI restraint and raw knocked-state1. Apply those fields using the native
save decoder/writer to a private copy, then place it in the scenario's private
`userdata/saves/M15Review/Quicksave.omwsave` slot. Supply `miss_fixture` from the
recipe along with `openmw`, `resources` and `oblivion_data`. Do not overwrite the
historical source. The new scenario passes the harness manifest validator.

The normal runtime course `S4/unarmed-knocked-runtime-normal-02` passes with
PIDs429742/429902: one keyboard punch hits named Valen Dreth through actual
animation/contact dispatch, spends attacker Fatigue140 -> 133, and changes victim
Health Damage by exactly-1.4199999570846558 with no victim Fatigue debit. Native
flesh/hand sound requests occur. All nine native actor-value arrays are checked;
only that declared Health channel changes after regeneration. Player inventory,
RNG, action counter2 -> 3, empty pending ownership and raw knocked-state1 survive
actual separate-process restart. Both phases save schema46 and exit peacefully.
The preparation-only normal-01 failure used `reference` instead of the decoder's
`key` field; no game launched. It remains retained.

The target's raw knocked byte is declared setup. Its standing rendered pose is
an outstanding reaction-integration limitation, not evidence of a real knockdown
or recovery. Neither this bare-target course nor the unit wear matrix establishes
armored gameplay/restart, equipped weapon blocking, power/creature/mastery or the
full S4 control/view/reaction matrix. S4 remains incomplete.

The corresponding ASan/UBSan runtime course
`S4/unarmed-knocked-runtime-sanitized-01` also passes both processes
(PIDs432160/434553). The same independent decoder assertions prove exact Health,
unchanged victim Fatigue, all nine native value arrays, inventory/RNG and no
replay. Its restarted capture was directly reviewed: named target, drawn
first-person hands and live HUD render. Both processes quit peacefully with no
ASan/UBSan diagnostics; leak checking is disabled. Instrumented binary SHA256:
`b81e166129f12da841f9fa92ae21d6f4a019cb28e20e9f5e5db4c9bfcf85cb08`.
Normal binary SHA256:
`8f9ca46dc89e9810b61a0acafa56045aff6dc3789dc29949b15087b663fd06c0`.
The per-course verification files retain input/output hashes, process epochs,
manifest hashes and the tested code fingerprint. All current run handles are
terminal. No physical knockdown/recovery or equipped block gate is closed.

## Equipped blocking and sequential victim wear — implementation checkpoint

Ordinary Player/NPC unarmed and weapon contacts now share native block preparation.
The active shield takes priority over the carried weapon; facing, raw posture6,
integer Paralysis, integer Block/Luck and current/base Fatigue select mitigation.
Novice Block pays its separate Fatigue writer before Health/contact Fatigue;
Novice/Apprentice gear receives the absorbed damage as wear. Shield wear also
uses the native armor-weight/base-skill mastery multiplier. The generic victim
condition vector is named `mVictimConditionChanges`, since it can include a
blocking weapon as well as armor. Armor-write telemetry counts actual ARMO items.

The stored body-armor wear follows original5FFB96 float stores on an accepted
equipped block: store its remaining portion, subtract that stored value, clamp
negative results, and suppress this wear at complete absorption. An unarmed
block preserves the original wear. This is not replaced by one algebraically
simplified multiplication. If the selected body-armor item is the same shield
already worn by blocking, prepare its second wear from the first stored
condition, then publish one final condition with the original instance guards.
Resources, both participants' conditions, RNG and action ownership still commit
through the existing atomic transaction. No schema change is needed.

Admission stays explicit for unfinished consequences. Expert/Master defensive
proc eligibility (Block with equipment, Hand to Hand without it), zero-condition
blocking items, break-after-wear, and the zero-absorption unarmed weapon-block
recoil branch reject without spending or changing live state. The new matrix
checks six supported Block boundaries0/24/25/49/50/74, rejected75/100, shield
priority over an equipped weapon, punches/weapon strikes, ordered double shield
wear, original-condition guards, stale-plan atomicity, zero-condition rejection,
and binary service restore without replay. This does not implement the gated
mastery or break transitions.

Native executable identity remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`S4/equipped-block-oracle-03` independently executes42 precision cases plus120
body-wear adjustment cases, both x87 words. It concatenates original5474A0,
547590,5F5BAB,5F3870/484850,5FF8F3 and5FFB96 with declared value/condition-reader/
writer boundaries; base mastery admission is supplied from the separately
reviewed rank rule. It is not a full contact or original-game probe. The corpus
and adjustment SHA256s are respectively
`40b2e355a48f180e9f1d2568e0490786716c095433ad42e31c6461e1ebf166d9`
and `ad46baf1f2cf147fc33ce40eaf528f40a78249a1738c99f24b77a4a51dceb326`.
The component golden inventory checks all60 distinct adjustment outputs, including
subnormal/large-adjacent float stores and complete absorption.

`S4/equipped-block-runtime-oracle-02` adds two exact original observations for
native NPC Rusty Iron Shortsword damage1.009374976158142 against Player Block5,
Luck50 and full Fatigue, with a blocking weapon at55.875/56. Expected block
fraction is0.02499999850988388, separate Fatigue cost20.024999618530273,
Health damage0.9841405749320984 and blocking condition55.84976577758789.
Corpus SHA256:
`048cfed36293fc5bcd5365bdb5fd74ccf36cc336494de1235fd7aef86bce8a6b`.
The -01 runtime oracle setup omitted `__file__` and failed before emulation;
its source is retained. The corrected -02 supplies it explicitly.

One trace interpretation is corrected: actor virtual+39C is **not** an animation
request. Character/Creature dispatch to890B90 (`ret12`); Player dispatches to668C30,
which uses the SKIL definition/use path. Thus the Block15 argument at5F5C05 is not
an animation-group ID. The native table beginningB102E0 puts BlockHit at28,
Recoil30 and Stagger31. These are dispatch/table observations; they do not prove
a runtime reaction animation. Reaction wiring remains open.

Final `S4/equipped-block-full-normal-02` and `-full-sanitized-02` pass all1479
engine cases/121 suites and2618 component cases/252 suites each, with exact
unfiltered inventories, zero failures and zero skips. Tested dirty-source
fingerprint:
`e1f9759a5b33063cbabef5061fbadbf1267249eea7c3ea3a058032657119c309`.
No C++ changes follow it; only this documentation and editable runtime inputs.
ASan leak checks are disabled and UBSan halts. Existing compiler warnings are
retained. The earlier normal -01 checks pass before the final admission guards;
the earlier sanitizer -01 build was explicitly cancelled and its process tree
confirmed quiescent before adding those guards. Its cancellation report remains
in place. Python implementation is unchanged.

Editable inputs are `oblivion_m15_equipped_block.json`,
`oblivion_m15_equipped_block_continuation.json` and `m15_equipped_block_fixture.json`
under `scripts/data/oblivion_compat`. Both manifests pass the harness validator.
The recipe declares a private copy of the existing stationary fixture's actual
schema28 output, unequips prior Player equipment, adds the parry weapon, resets
Player Health Damage and the opponent activation local, and restores the
opponent's full weapon. Supply the existing `stationary_fixture` directory plus
`openmw`, `resources` and `oblivion_data`. Original source files stay unchanged.
Normal b input holds native OneHandBlockIdle, ordinary Space activation invokes
the production stationary policy, and one native animation contact applies the
frozen damage/cost/condition outcome. Saving while held stores posture6; key
release and the fresh-process continuation return it to-1 without replay.

The fixed15-second settling attempt in `equipped-block-runtime-sanitized-02`
passes scenario actions but fails independent value comparison: Fatigue is still
regenerating under instrumentation. It is not accepted settled-state evidence.
Corrected -03 manifests add a read-only Player onFrame observer that requires a
real debit followed by actual Fatigue140 before saving. It changes no resource,
clock, input or outcome. Normal -02 passes, but the recovery-gated -03 courses
are the canonical recipe. The pre-admission sanitizer runtime -01 completed
only its first phase and establishes no restart result.

Audio is explicitly disabled in these block courses. A rendered held blocking
pose, numeric mitigation and condition persistence do not close block-hit/recoil
animation, block audio, the gated mastery/breakage consequences, shield gameplay,
third person/gamepad/FPS crosses, or complete windup/block/recovery restart
comparison. S4 remains incomplete.

Canonical recovery-gated runtime verification now passes in
`S4/equipped-block-runtime-normal-03` (PIDs445081/445333) and
`S4/equipped-block-runtime-sanitized-03` (PIDs445118/445542). Both prove one
production opponent contact with the exact oracle Health and both weapon
conditions, actual recovery to native Fatigue channels `[140,0,0,0]`, unchanged
remaining native value arrays and combat RNG, counter2 -> 3 with no pending
ownership, and held posture6 -> released posture-1 after real process restart.
The read-only recovery marker occurs after the hit in both logs. Script local
fighting0 -> 2 proves the activation/stop course; neither second process attacks.
The independent verifier compares all nine native actor-value arrays, all Player
inventory fields (only absent ownership defaults normalized), the opponent's
weapon, exact input/output hash linkage and process epochs. Both saves in each
course are schema46. All processes and check handles are terminal, with no
ASan/UBSan diagnostic. Leak checking remains disabled.

The instrumented held-pose capture was directly reviewed: named Valen Dreth,
both drawn weapons, Player's OneHandBlockIdle and the live HUD render. This is
held-pose evidence, not a captured block-hit/recoil animation. The normal earlier
held capture was also reviewed. Final binary SHA256s are
`db6b79b0474bd62fae5e52c74414c0e8e7ecb0ddc561e416130a6930904901d2`
(normal) and
`b3b1aacba95ef2b323a29468f7bd8be7c51b08a15e4bfc086c9c462ed249f8c3`
(instrumented). Per-course verification retains the tested code fingerprint,
manifest hashes, original/source/output save hashes and exact assertions.


## Equipped block impact audio — implementation checkpoint

Prepared ordinary weapon/unarmed contacts now carry the accepted block sound
selection through the same owned contact boundary. The controller starts audio
only after that contact commits. Weapon blocking requests independent attacker
and defender layers at the native world-position midpoint; positional playback
preserves both voices when they share the same SOUN. Actor-bound playback would
replace the first copy. Shield blocking selects its light/heavy material plus
the native NPC body/weapon layers. Rejected posture/cone/Paralysis blocks no
longer suppress an otherwise admitted unarmored ordinary hit's sounds merely
because the victim holds the blocking posture. Unsupported contact branches
retain their existing admission gates.

Independent original executable SHA256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`S4/block-sound-oracle-01` executes the complete6AFFF0 selector/return path for
242 attacker/defender/enchantment combinations under each x87 control word,
484 cases total. The6ADE50 boundary records requested EDIDs/flags and returns
null: original voice construction, pitch/randomization and output are not
emulated acceptance evidence. The checked-in `blocksound_expected.inc` retains
242 independent expected cases, including absent layers and signed enum limits.
The attacker-only enchanted blade/blunt selector is covered as a pure rule;
this does not admit enchanted runtime contacts. The original5FF934/5FFB05
callers distinguish weapon blocking from shield classification. Executing
4B4C70 on all256 flag bytes independently confirms the shield weight high bit.
The existing NPC hit selector covers the shield material/body/weapon ordering.
The32-combination World matrix now checks prepared shield/weapon sound layers
alongside its exact numeric, atomicity and replay assertions.

Final `S4/block-sound-full-normal-03` and `-full-sanitized-03` each pass all1479
engine tests/121 suites and2619 component tests/252 suites, unfiltered, with
exact inventory/XML agreement, zero failures and zero skips. Both retain the
same frozen dirty-source fingerprint atop97a45a2f00:
`319a75e087ab5cd850ab7f88a72584b1346c60fe637ab83e250ed4bdae058f99`.
ASan leak checking is disabled; UBSan halts. Only these documentation and
portable manifest updates follow the tested C++ source. Normal -01 and sanitizer
-01 were deliberately stopped when review found same-SOUN replacement. Normal
-02 retains the diagnosed `auto*`/`NotNullPtr` compiler error; sanitizer -02 was
stopped before fixing that shared source. Neither is passing final evidence.

`S4/block-sound-runtime-normal-01` and `-runtime-sanitized-01` pass the real held-b
block, Space activation, production stationary opponent contact, actual Fatigue
recovery, F5 save and distinct-process load/resave course with audio enabled.
Normal PIDs447932/448118 and instrumented451092/451443 are terminal. Independent
verification checks all nine native AV arrays, exact Health/condition results,
full Player inventory, opponent weapon, unchanged combat RNG, consumed action
ownership and posture6 -> -1 without replay. Both native block voices return
successful playback. OpenAL's wave backend captures actual finite, non-silent
mixer output; both held-pose screenshots were directly reviewed.

The unchanged stock-audio analysis method/0.75 correlation threshold finds one
block cluster before restart and none after in both builds. Normal matches
`wpn_block_blade_03.wav` at0.9984971353; instrumented matches
`wpn_block_blade_02.wav` at0.9950682284. The overlapping two native voices occupy
one temporal cluster. Winning SOUN `WPNBlockBlade` is00058F14, folder
`fx\wpn\weaponblock\blade\`, from the unchanged Oblivion master
SHA256`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.
Stock BSA SHA256:
`f11e92315666b7e6ee5f6936fa68300fb5234215c59d5e3836375f262fba05ec`.
Per-asset hashes, waveform hashes, comparison reports and rejected preflights
stay in ignored evidence. This proves stock PCM delivery, not hardware listening
or an original-game waveform/pitch comparison.

Runtime game binary SHA256s are
`511c78791a749dec35c3c3d870c85fca9c6c0baa2b7448240df9dadd818f2d77`
and`998084aee6a7ace4d904bb4939682eaed8112b037e9c875dce7893ab681e2707`.
The checked-in equipped-block/continuation manifests now enable sound and require
successful block playback/no replay. The local capture backend is declared only
in the retained runtime manifests; generated plugins/assets remain ignored.

S4 remains incomplete. This checkpoint does not close live shield or unarmed-only
block audio, block-hit/recoil animation, proc/breakage/drop consequences,
power/creature routes, or the remaining view/control/FPS and in-action restart
matrix. In particular, native5F3870 has an NPC drop branch as well as unequip
paths. Numeric break publication needs complete scene/inventory consequences
and equipment notifications deferred beyond active controller/actor iteration;
simply clearing an equipped slot would not deliver that transition safely.


## Deferred equipment notifications and lifetime guards — transaction groundwork

`PreparedUnequip` now exposes a token-owned notification entry point guarded by
both inventory revision and a weak storage lifetime. It rejects destruction,
clear/replacement and same-address inventory reuse before dereferencing either
owner or item. The existing publication still changes slots without quantities
or callbacks. Notification remains once-only, including observer failure.

World now stages an owned callback/list node before resource publication. Its
commit only splices the preallocated node; it allocates nothing and invokes no
observer. `World::update` drains the queue after the engine's mechanics/actor and
physics updates, before rendering. Each event is removed before invocation.
Reentrant drains leave newly queued work for a later boundary; a throwing event
cannot replay, and older undispatched events retain their order ahead of newly
queued work. Payload destruction occurs before the post-observer World guard.
The queue invalidates its identity before destroying pending captures, so their
destructors cannot publish into a tearing-down queue.

A clear or destruction stops dispatch and returns from `World::update` before
using stale backend/actor state. A detached but still-live original queue retains
its unfinished events for simulation rollback. Queue ownership moves with
`RuntimeFields`; staged preparations reject the incoming simulation and become
valid again only when their original World/queue/generation is restored. This
queue is transient notification infrastructure, not a new save-wire authority.
Borrowed actor/store payloads still require their own lifetime guard; the new
unequip token provides it for that operation.

Final `S4/deferred-equipment-full-normal-03` and `-full-sanitized-03` each pass all
1487 engine tests/121 suites, unfiltered, with exact inventory/XML agreement,
zero failures and zero skips. Frozen source atop17710f46a8:
`297c903888cc98761df27598a7d280335f7dcde52364b6a5dacede4659c420e4`.
Both before/after fingerprints agree. ASan leak checking is disabled and UBSan
halts. Only documentation follows the tested implementation. Components and
Python code did not change and were not rerun for this checkpoint.

Eight new engine tests cover deferred unequip publication, same-address owner
reconstruction, reentrant/throwing observers and ordering, clear during actual
`World::update`, World destruction, clear during capture destruction, queue
teardown during capture destruction, and simulation replacement during dispatch
with both normal return and exception. The existing simulation archive test also
checks staged and queued notifications through detach/exchange/restore. These
are engine transaction/lifetime tests, not normal-input break acceptance.

The retained -01 builds were deliberately stopped after review found that a
replaced-but-retained queue must keep its unfinished events. Normal -02 passes
1485 cases before the two final capture-destruction cases; sanitizer -02 was
stopped to correct those lifetime boundaries. Neither replaces the final -03
scope. All final build/test handles are terminal.

Independent original `S4/broken-equipment-type-audit-01` also resolves the native
NPC break predicate without inferring meaning from a function label. The
hash-verified executable is
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
An RTTI complete-object-locator walk finds primary weapon vtableA45354 and armor
vtableA441BC. Both virtual+78 slots resolve to4D7030. Executing that original
function for546 flag inputs confirms its low-byte result reads internal TESForm
flags at+08, bit0x400. Original5F3A47 tests that byte: true reaches5F3B0D's
unequip call, false reaches5F3A59's NPC drop preparation. Player bypasses the
predicate. The drop path uses the equipped scene node and invokes actor
virtual+2C8 plus4DC000; this is more than a slot change. The ignored reproduction
script/report retain the RTTI walk, cases and exact boundary. On-disk flag mapping,
full drop execution and original-game runtime acceptance remain separate work.

This checkpoint prepares the notification boundary needed by the complete
break/drop transaction. It admits no new melee break branch. Zero/broken-after-
wear contacts, their scene/inventory publication, interruptions, script effects
and normal-input/restart acceptance still require implementation. S4 remains
incomplete.

## Prepared physical contacts — transaction groundwork

Native physical contact now has separate preparation, validation and publication.
The existing immediate World/service route delegates to this API, so accepted
ordinary melee, blocked contact and projectile callers use the same publication
boundary. Preparation stages actor values/lifecycle, projections, sequential
condition writes, RNG, timed knockback and death events without publishing them.
Cancelling a preparation leaves the action and resources unchanged. A successful
commit consumes the contact once; repeated commit or validation fails.

Before publication, the token rechecks the service generation, participant values,
base overrides, lifecycle, action ownership, melee/bow state, animation clock,
draw state, position/rotation/scale, and equipped item identity/count/condition/
charges. Borrowed reference leases expire before custom-data replacement,
copy/move replacement or destruction; inventory leases reject replaced storage.
These checks precede dereferencing cached stats, inventory and item pointers.
Native ACHR/ACRE and matching generic native-reference bindings remain admitted
at this low-level service boundary. This does not enable ordinary creature melee.

Commit performs no allocation or observer callback. It swaps only the prepared
condition fields, preserving unrelated ownership metadata, and changes only the
victim's prepared knockback pulse. Concurrent unrelated actor actions, values and
pulses survive; changed participant state, RNG, relevant pulse or staged death
queue rejects before publication. Token-owned entry points safely reject clear,
service/World destruction and expired participant lifetimes. This is a service
transaction: a future fallible World/scene admission must also guard its active
World and simulation context before calling it. Retaining an old service is not
proof that it is the currently selected simulation.

Final `S4/prepared-contact-full-normal-07` and `-full-sanitized-06` pass all
**1497 engine tests/121 suites**, unfiltered, with exact inventory/XML agreement,
zero failures and zero skips. Both before/after source fingerprints agree:
`4d3bfbe89b8ad4b63326ac7b87547a5d8fd31147e63dcd787d6dace2a8b0bb19`,
atop `e0a3bb1859b07b83c19baed80eb5001869c187e5`. ASan leak checking is disabled;
UBSan halts. Ten new cases cover staged cancellation/once-only publication,
stale authority/geometry/projection owners, unrelated actors, service/World
teardown, reference leases, equipped conditions and ownership preservation,
stale RNG/death queues, lethal NPC/creature service contacts, independent versus
stale victim pulses, and both native-reference representations. Only documentation
follows the tested source; unchanged components/Python were not rerun.

`S4/prepared-contact-allocation-05` additionally traces nine selected prepared-
contact GTests in the normal Linux x86_64 binary. Exact ELF entries and original
stack return addresses bound 61 balanced outer scopes, with 59 nested entries;
token validation/commit, service validation and the executed outlined publication
body are observed. There are zero observed main-thread calls to malloc, calloc,
realloc, aligned_alloc or posix_memalign within these scopes. This is a bounded
trace, not coverage of other threads, allocator APIs or unselected paths.

Retained attempts remain distinct: early builds/preflights exposed private
CellRef access, an unsupported variant alternative, nonmovable projection
construction and a wrong command enum. Normal -04's five new snapshot-fixture
failures came from omitted registered actors; normal -05 then passed 1496 cases.
Sanitized -04 caught the new NPC-only test helper binding a reference to its
absent Player; the helper now passes `nullptr`. Normal -06/sanitized -05 were
stopped to remove new shadow warnings. Allocation probes -01 through -04 are
not passes: optimized function splitting, quoted-symbol lookup, an internal GDB
crash and a requirement for an unexecuted wrapper invalidated their reports.
The final trace uses actual ELF addresses, follows token entries and changes
breakpoints outside stop callbacks. These failures were not engine acceptance.

Independent bounded original caller inspection in
`S4/broken-drop-callers-01` retains the same executable hash as the preceding
predicate audit. RTTI resolves the Character/Creature actor virtual slot
`+0x2C8` to `0x5FC440` and PlayerCharacter's slot to `0x660CC0`. Original
post-drop `0x4DC000` associates the actor's ExtraDroppedItemList (extra type
`0x42`, constructor RTTI) and the dropped reference's ExtraItemDropper (type
`0x41`). TESObjectREFR virtual slot `+0x78` delegates to the base-record predicate;
slot `+0x40` forwards the `0x20000` mask to a change helper. That mask is not
established as a literal persistent-reference flag. This is bounded instruction/
RTTI evidence, not full drop execution or an original-game runtime pass. Scene
placement, inventory removal, drop identities/relations, active-World validation
and deferred notification still need one complete break/drop transaction. This
checkpoint admits no new break branch.

Final real-input regression courses `S4/prepared-contact-runtime-normal-02` and
`-sanitized-02` both pass. Held keyboard block intercepts one production
stationary-opponent attack after ordinary activation; F5 saves while held, and a
new process loads/resaves after release. Exact inspection preserves all nine
native actor-value arrays except the independently expected Player Health delta
`-0.9841405749320984`. Player blocking-weapon condition is
`55.84976577758789`, opponent weapon condition `55.70000076293945`; complete
Player inventory, NPC weapon, unchanged combat RNG, action next ID `2 -> 3`,
empty pending/owner lists and script phase `0 -> 2` match. Actual block Fatigue
recovers before saving; process action is `6` while saved blocking and `-1` after
fresh release. Distinct process epochs are `477711 -> 477908` normal and
`480004 -> 482406` instrumented. No contact, selection or impact voice replays.

Both courses start two successful native WPNBlockBlade voices and capture finite,
non-silent actual OpenAL wave output. The unchanged stock-PCM method and fixed
0.75 threshold find one block cluster before restart and none afterward:
`wpn_block_blade_02.wav` correlation `0.9982556661884925` normal,
`wpn_block_blade_01.wav` correlation `0.990941916884005` instrumented. Both
held-block images were directly reviewed. Game binary SHA256s are
`433705fd678b2305d3de06d5bc2be33e11bb26ab2621192eac40143089094500` and
`08e0fbb94e813e2d0db14bcd4abb4e723e56e0ab9eb3ecbbdfbc3727a45a867f`.
All five final reports carry the same frozen source fingerprint; the combined
report is `S4/prepared-contact-allocation-05/checkpoint-verification.json`.
Earlier runtime -01 courses remain intermediate evidence. Final build, test,
debugger and runtime handles are terminal. This is regression acceptance of the
existing equipped-block route, not block-hit/recoil animation, break/drop,
mastery, power/creature or the full view/control/FPS/restart matrix. S4 remains
incomplete.


## Active-World contact guards and compound scene publication

World now exposes a prepared contact that binds the service preparation to its
selected World generation, combat service, Player, physics owner, scene and God
Mode. A constant-size registry guard checks lifetime, selected registry identity,
revision and generated-reference boundary without copying the reference index.
Validation and commit allocate nothing and invoke no observer. The existing
immediate World entry point delegates through this guarded path. World invalidates
its borrowed lifetime marker before backend or payload teardown.

An outgoing simulation frozen before archive detach rejects contact publication.
A retained original combat service alone does not authorize an unselected
simulation or registry. After rollback restores the exact original owners and
unchanged participant state, its original token can commit once. Registry-only
retention is covered independently from combat-service retention. Clear, World
destruction, registry mutation, changed God Mode and physical-owner replacement
also reject before any resource publication.

Equipped-weapon removal tokens now have owner-safe validation, commit and notify
entry points. A weak storage lease is checked before the borrowed inventory or
item is touched, including destruction/reconstruction at the same address.
Deferred observers can destroy the inventory, return or throw; notification
cannot resume into that dead owner or replay. Final wear remains owned by the
contact transaction, and removal still changes only quantity/slot/caches.

The existing compound native weapon admission test now commits actual prepared
Health/Fatigue/condition changes alongside exact inventory removal and the World
serial, after actual renderer/physics admission. Seven outcomes cover successful
once-only publication, stale condition, stale serial, scene rejection, throwing
scene insertion, changed participant values and changed God Mode. Rejection
rolls back model/body admission before contact, inventory, cell or registry
publication. This is engine transaction coverage with test-owned model/body
assets, not a production stock drop factory or normal-input break acceptance.

Final `S4/world-contact-full-normal-02` and `-full-sanitized-02` each pass all
**1502 engine tests/121 suites**, unfiltered, with exact inventory/XML agreement,
zero failures and zero skips. Source before/after agrees in both:
`d6b697e5b49e9b15a1d4ae64c88332acdf9919ee085f281ffea4ef975f9f51cf`,
atop `650066f6d44ff986d8d3a528f4ae85b85e768553`. ASan leak checking is disabled;
UBSan halts. Five new tests cover World context/lifetime, independent registry
retention and safe weapon-removal owner teardown. Existing full simulation
archive and compound admission cases are extended. Unchanged components/Python
were not rerun. Only documentation/probe-manifest edits follow these C++ checks.

`S4/world-contact-allocation-01` passes seven selected World contact, archive,
compound admission and removal cases. Its exact-entry Linux x86_64 trace observes
61 balanced outer scopes and 81 nested entries, including World/removal token
validation and commit, service validation and executed publication. No calls to
malloc, calloc, realloc, aligned_alloc or posix_memalign are observed within
these main-thread scopes. This does not cover other allocator APIs, threads or
unselected paths. The test binary and frozen source hashes are retained.

Normal -01's two failures were fixture defects: a new victim remained in the
uncapturable draft cell, and the archive fixture's placeholder Player class
could not initialize native major skills. The victim now resides in the managed
cell; the archive case uses its valid native NPC's missed swing, retaining the
same World/Player ownership guards. Sanitizer -01 was stopped before these
shared fixture corrections. The initial normal runtime course uses the same
final game binary: subsequent corrections changed only tests. Its final state
verifier checks the current binary against both captured process hashes.


Original follow-up routing is independently verified for 192 selected contexts
in `S4/drop-followup-routing-05`, with actual entry/return, base getter and combat
query instructions, and declared opaque resource/equipment/scene boundaries.
The [probe contract](M15-ORIGINAL-DROP-FOLLOWUP-PROBES.json) and native provenance
record the replacement-equipment requests, their independence from a returned
reference, stack conventions and retained harness failures. This is additional
factory guidance, not original-game drop acceptance.

Read-only stock asset diagnosis identifies the next concrete factory dependency.
`S4/stock-melee-drop-body-01` extracts eight iron melee NIFs from Meshes BSA
SHA256 `d05bb62f933856105536beb06c26bcdf5fa37f685152a39ece587e7f26b4e99b`.
The existing authored-body loader accepts only the mace hull. The other seven
NIFs parse structurally but their body graphs reject unsupported shapes.
`S4/stock-drop-body-preflight-05` verifies body-root/child links and identifies capsule lists for swords, dagger,
claymore and warhammer, and capsule/box/transformed-box lists for both axes.
The bow's single hull is accepted; the arrow's separate unsupported graph is
outside this melee diagnosis. Earlier probe attempts retain an obsolete reader
setter and invalid JSON from printing a binary NIF hash as UTF-8; the corrected
report hex-encodes that hash. These are parser diagnostics, not physical admission.
Complete compound/box body support, stock model/body and carried-pose binding,
dropper/list metadata, replacement-equipment follow-ups, deferred notification
and real break/drop/restart courses remain required. No break rejection is
removed by the World guard checkpoint, and S4 remains incomplete.


Final runtime courses `S4/world-contact-runtime-normal-01` and
`-sanitized-02` each pass both ordinary-input and fresh-process phases. Exact
inspection preserves all nine native actor-value arrays except expected Player
Health `-0.9841405749320984`, with Player blocking-weapon condition
`55.84976577758789` and NPC weapon condition `55.70000076293945`. Complete Player
inventory, NPC weapon, recovered block Fatigue, unchanged RNG, action ID advance
`2 -> 3`, empty pending/owner lists, script phase `0 -> 2`, and held/released
process actions `6 -> -1` match. Process epochs are `485202 -> 485323` normal and
`490833 -> 493103` instrumented. No native contact or voice replays after restart.

Both courses start two successful WPNBlockBlade voices and capture finite,
non-silent OpenAL wave output. The unchanged stock-PCM method/fixed 0.75 threshold
finds one block cluster before restart and none after: blade sample03 at
`0.9979219634698733` normal, sample01 at `0.9973005777996221` instrumented.
Both held-block images were directly reviewed. Game SHA256s are
`e3d1de343fc3d0515844ce4872753d0c2334f6f9a77e3f0f9b01d78917009233` and
`b74b7858ad185e729aa1a67bf0de42396a85fb713429e364d9b637e5f11f2f89`.
The retained instrumented runtime -01 is not a pass: its launcher began hashing
before final linking settled, and correctly rejected the changed executable hash
after an otherwise completed first phase. Fresh -02 starts after the completed
build target and verifies unchanged before/after/epoch hashes in both processes.

All five final build/trace/runtime reports share the frozen implementation
fingerprint recorded above. The combined checkpoint report is
`S4/world-contact-allocation-01/checkpoint-verification.json`. All check/runtime
handles are terminal. This verifies the existing equipped-block route after the
World-wrapper change. It does not close stock break/drop, mastery/effects,
power/creature dispatch, reaction animation or the remaining S4 acceptance matrix.


## Compound stock melee body geometry

The authored-body loader and Bullet owner now retain nested `bhkListShape`,
`bhkConvexTransformShape` and `bhkBoxShape` geometry as owned trees. Lists preserve
child order, separate instances and filter metadata; transformed children retain
rigid XYZ bases and translation. Actual compound collision shapes preserve gaps
between children. Box half extents describe the core, with convex radius added
outside it. Native cached-property scaling recursively scales box extents and
transform translation, preserving convex radius and rotation. This adds no save
wire state and admits no new ordinary-contact branch.

Loading rejects foreign/cyclic/null child links, malformed dimensions/transforms,
empty lists and inconsistent filter counts. Loading, copying for native scaling,
and physics construction bound each body to 128 edges of depth and 4096 expanded
shape nodes. Physics rejects non-rigid or reflected XYZ bases before publication.
Finite fourth lanes are padding: the actual stock waraxe has a tiny M33 rather
than one. They must not produce projective division or an affine-W rejection.
Collection filter metadata is retained; the caller still supplies body collision
group/mask. This is not implementation or verification of native child filtering.

The [editable original probe contract](M15-ORIGINAL-COMPOUND-SHAPE-PROBES.json)
pins executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`S4/compound-shape-scale-oracle-02` passes 340 configurations of actual box and
transform scaling prefixes, with actual cached getters, declared clone boundaries
and both x87 precision settings. Its 170 distinct XYZ results are retained as
exact float-bit tests. `S4/box-shape-padding-oracle-01` executes the actual box
constructor/AABB/helper without callee stubs and passes 576 contexts. It confirms
core/radius semantics and padding invariance for identity/right-angle rotations.
The earlier 144 AABB contexts are included, not additional unique coverage; their
72 distinct results are retained with a 1e-6 Bullet comparison tolerance. These
probes do not execute full NIF cloning or establish original-game acceptance.

`S4/stock-compound-body-normal-04` and `-sanitized-01` each pass all eight actual
iron melee meshes from Meshes BSA SHA256
`d05bb62f933856105536beb06c26bcdf5fa37f685152a39ece587e7f26b4e99b`.
They verify loading, generic Bullet world admission, twenty free-fall steps
relative to initial center of mass, finite bounds and complete body teardown.
Asset, source, component-library and probe-binary hashes are recorded. This
improves the retained preimplementation baseline from one of eight to eight of
eight. Explicit group 1/mask -1 and length scale 7 are probe policy; renderer,
production drop input, native filtering and save/restart acceptance are not tested.

Final `S4/compound-shape-full-normal-03` and `-full-sanitized-03` each pass all
**2631 component tests/252 suites** and **1502 engine tests/121 suites**, unfiltered,
with exact inventory/XML agreement, zero failures/skips and identical before/after
source fingerprint `101c9943985b6de8ba904556358020fce5095848c0c690dc488701f975acc42b`
atop `3a7680636e76a8a54cff456da236403e59a1bb3c`. ASan leak checking is disabled;
UBSan halts. Twelve new tests cover ownership, invalid graphs, resource bounds,
scaling, child gaps, transformed bounds, padding and original expected outputs.
Only documentation and the editable probe contract follow these checks.

Retained failed/intermediate attempts matter: initial syntax probes caught OSG
vector overload and test trace errors. Sanitizer -01 exposed a null fixture using
an unresolved record handle; fixtures now use a posted null and loading checks
empty children before `getPtr()`. Normal -02 passed before the stock waraxe exposed
an incorrect affine-W restriction. Instrumented -02 was stopped before its engine
phase completed. Stock normal -01 lacked a Bullet header; -02's reporter lacked
its helper import path; corrected -03 records seven of eight passing and the
waraxe rejection. The native padding probe justified the final correction, and
both final full runs and stock probes pass. Failed evidence remains in place.

Stock model/body hash and carried-pose binding, dropper/list relation metadata,
replacement equipment, complete contact/removal/scene publication and deferred
notification still need production integration and real break/drop/restart
courses. The earlier equipped-block live course was not repeated for this shape
chunk. S4 remains incomplete.


## Carried model and authored-body source binding

`ESM4NpcAnimation` now captures the binary 16-byte file identity from the scene
**template actually instantiated**, alongside the live item, normalized model
path and attachment generation. `captureCarriedWeaponBody` loads an owned,
immutable NIF body definition only when its path/hash matches that attachment.
Non-NIF models, missing body graphs and mismatched scene/parser cache generations
cannot supply a body capture. Malformed supported body graphs remain fallible
preparation errors. Pose/body revalidation checks source identity and the existing
slot/count/model, scene topology, transform, generation and visibility guards.
Body capture does not compute native reference rotation, strip placement scale,
select collision policy, remove inventory or admit a drop branch.

An already captured immutable body still matches its old rendered attachment
when the VFS file changes. A new capture rejects the different parser bytes;
refreshing equipment against the old scene template still rejects them. Clearing
the template cache and refreshing produces a new matching body and invalidates
the old attachment generation. Geometry remains owned after parser cache and
animation teardown. Future transactions must validate World/inventory lifetimes
before borrowing the item or calling the still-live animation owner.

The existing actual NPC animation test now runs with both its owned OSGT model
and an explicit test-owned TES4 NIF (one named node, collision, rigid body and
sphere). It checks source hash tampering/absence, authored bind transform,
non-NIF rejection, empty body handle, changed file and independently stale caches,
refresh invalidation, immutable retained geometry and original quantity/condition/
charge. `S4/carried-body-nif-preflight-01` separately verifies that fixture through
the real parser/owned-body loader. These tests use real resource and animation
objects without a GL draw or stock asset; they are not input/drop acceptance.

Final `S4/carried-body-full-normal-02` and `-full-sanitized-02` each rebuild/link
`openmw`, `openmw-tests` and `esmtool`, then pass all **1502 engine tests/121 suites**,
unfiltered with exact inventory/XML agreement and zero failures/skips. Both
before/after source fingerprints agree:
`b6381248becb4196b0971c78c9a2994bdbd310629c58e52d1327ae6a65bbb92a`,
atop `5035159669ec265e8b232f9ce8f91be5af0a2067`. ASan leak checking is disabled;
UBSan halts. Component/Python/schema implementation did not change and those
suites were not repeated. Only documentation/probe-contract edits follow these
C++ checks.

`S4/carried-body-allocation-02` traces the same normal test binary SHA256
`f752c5ba13fd863a8f4bc83846a67967e38e7a527f5a723371d17c23a3a69fb9`.
One selected NPC animation test passes; exact ELF entries plus main load bias
observe **37 balanced outer scopes and six nested entries**. Pose validation is
hit 35 times and body validation eight times. No main-thread malloc, calloc,
realloc, aligned_alloc or posix_memalign call occurs inside those scopes.
Preparation may allocate. Other allocators, threads and paths are not covered.

The first full normal run passed before this trace exposed existing path-building
allocations in pose revalidation. Allocation -01 also selected GCC's `.cold`
pose symbol instead of the normal entry; its body entry still observed the real
string allocations. The instrumented -01 build was deliberately stopped before
editing. Preparation now owns the borrowed class record's normalized path;
validation compares it directly without constructing/correcting strings. Final
trace -02 selects both actual normal entries and passes. Syntax attempt -01 used
a stale compilation database missing these sources; -02 uses current CMake flags
and passes. These failed/intermediate attempts remain retained.

Original drop relation probes also advance the next factory dependency. The
[editable relation contract](M15-ORIGINAL-DROP-RELATION-PROBES.json) records 1440
existing-extra contexts and 384 construction contexts at actual `0x004DC000`,
including original list/dropper mutators, list insertion and selected constructors.
It establishes duplicate suppression, front insertion, dropper overwrite and
conditional `+0x40(0x20000)` change requests under explicit pointer/query/extra/
allocation boundaries. Raw reference `+8` bit `0x400` is tested only through the
selected null `0x0046B680` fallback; the lookup's role and other query branches
are not inferred from a harness label. Generic extra attachment, allocation
failure, persistence and original-game runtime acceptance remain untested.

The next production work remains native dropped-reference placement and scale,
dropper/list relation storage and persistence, replacement equipment, composition
with prepared contact/removal/key/scene publication, and real break/drop/restart
input courses. S4 is incomplete; no break rejection is removed here.

## Owned native drop relations and runtime-state version 47

Native `CellRef` instances now own `ExtraItemDropper` and ordered
`ExtraDroppedItemList` identities outside authored record fields and inventory
slots. Absent and present-empty lists remain distinct. Fallible preparation
rejects null/noncanonical identities, duplicate list entries and wrong reference
categories before mutation. Prepared relation-only publication swaps owned extras
without allocating or calling observers; caller lifetime/identity guards remain
required. This does not impose inverse consistency: the verified native helper
can overwrite a dropper without removing the reference from an earlier list.

Runtime-state version 47 appends explicit dropper/list presence and ordered keys
after each reference's existing ownership extras. C++ and Python codecs agree
on independently emitted bytes, canonical identities and exact presence/order.
Malformed presence/keys, resource bounds and lossy back-version writes reject.
Old payloads remain readable; restoring <=46 preserves live relation extras.
Whole-save admission and direct World apply share early target validation.
Dropper targets must resolve to actors and list targets to takeable placed
references, including references created by the same restore. Preparation and
publication use the existing staged reference bindings.

Two component cases, four World cases and three Python cases cover independent
wire bytes, all six presence combinations, copy/ownership/category rejection,
exact capture/restore, old-save behavior, invalid target rejection before clock
or registry publication, and repeated dynamic-reference restoration. Final full
normal report is `S4/drop-relations-full-normal-03`. The completed sanitizer
component run in `S4/drop-relations-full-sanitized-03` is independently rechecked
by its `components-reinspection.json`; the final sanitizer engine report is
`S4/drop-relations-engine-sanitized-04`. Both builds pass all **2633 component
tests/252 suites** and **1506 engine tests/121 suites**, unfiltered with exact inventory/XML agreement and zero
failures/skips. Before/after source fingerprints agree:
`a694145a4c38e195aa1c41b8b09019de7d325f9842c7f2aaf94dc11b42544002`,
atop `184aefd514e93b79f32ab7c76484c4daef26189f`. The normal full Python suite
passes 301 tests. ASan leak checking is disabled and UBSan halts. Only probe
contracts, declarative runtime sources and documentation follow the final source
checks.

`S4/drop-relations-allocation-01` selects the actual ELF entry for
`CellRef::swapNativeDropRelations` in normal `openmw-tests` SHA256
`c9ef2df0d838ddf76732d885d12f83feeed0db1b7e6abaafe63076c991ce5773`.
One selected World test passes with four balanced scopes (invalid category,
two successful swaps and invalid TES3 source). No main-thread malloc, calloc,
realloc, aligned_alloc or posix_memalign call occurs in those scopes. Fallible
preparation and other allocator/thread/path behavior are outside the trace.

The editable [persistence scenario](../../scripts/data/oblivion_compat/oblivion_m15_drop_relation_persistence.json)
and [seed recipe](../../scripts/data/oblivion_compat/m15_drop_relation_fixture.json)
are versioned under `scripts/data/oblivion_compat/`. They explicitly seed existing stock references:
one actor list has two non-sorted items, another has a present-empty list, and
the two placed items carry droppers. No plugin generation or new content records
are involved. The original schema-46 seed SHA256 is
`4f88acb728719dfb245f36fd67ccb9f84f8987bf99ac2ee8f11570eb592d6c17`;
the promoted/seeded version-47 save is
`798f1c48d404650099618c213a5f5bd793d19f647ea2be86b19d537ade42b66a`.
`S4/drop-relations-cpp-seed-inspection-01` additionally passes actual C++ ESM3
save reading, T4ST validation, exact canonical agreement with Python and complete
payload reencoding for all 34,728 references, nine native actor arrays and four
relation owners. This is structural/semantic codec inspection, not World or
original-game acceptance.

Final actual rendered/SDL courses `S4/drop-relations-runtime-normal-03` and
`-sanitized-01` each load the seed, save using F5 after the read-only five-frame
ready marker, quit, then load/resave in a distinct process epoch. Normal game SHA256
`0e835c0836dbd31be1900cae605f4b0e98d932eb1bd04cb8ec4e90047a14b970`
runs PIDs 521789/521896 (save ticks 6/5); instrumented game SHA256
`9a19a78bfc5bd1f04fab83bdfbb10f3a62fb65a497767bb10074fa4071e0bb84`
runs PIDs 524125/524373 (both save tick 5). Both courses terminate cleanly and
use distinct epochs/PIDs, verified save input/output hashes and unchanged pinned
images. Exact output-save hashes are retained in each verification report.
Live load/save dumps and binary saves agree exactly on all four relation owners;
all other references retain absent extras. All nine native actor arrays,
Player inventory, selected item metadata, RNG, action identities and dynamic
serial also remain exact. Screenshots were inspected. This verifies seeded
relation persistence through actual game processes; normal-input break/drop,
replacement equipment, pickup cleanup and audio acceptance remain open.

`S4/drop-relations-morrowind-01` separately passes actual Morrowind profile
load/resave with the final normal image, no GPRO/T4VR/T4ST top-level record and
no OMW4STATE payload. The 102-record save has 19 distinct TES3 save record tags.
The inspected screenshot shows terrain/sky and HUD; the course loads Balmora
but does not establish broader town/gameplay or audio acceptance. The existing
fixture's reviewed missing-resource findings remain explicitly bounded.

Retained attempts: full normal/sanitized -01 were deliberately stopped to remove
a new shadow warning. Both -02 component runs passed 2633 cases. Normal -02's
engine run passed 1505 and failed one negative test: direct World apply accepted
invalid relation targets even though outer admission rejected them. Sanitized
-02's engine build was stopped before correction. The shared validator now runs
before either entry point can publish changes. Final normal -03 and sanitized
engine -04 retain that negative coverage. The final -03 supervising helper disappeared during a
continuation while its detached CMake child remained live. No source was edited
or build restarted: the original build finished, the completed 2633-case component
XML/inventory was reinspected, and a detached -04 helper passed final engine
checks against the same source. Runtime normal -01 failed before loading because
the pinned executable lacked its adjacent defaults/config files; -02 fixed that harness
and passed an intermediate engine image. Final normal -03 repeats the course
with the corrected final image. Failed evidence is retained, not counted as a
pass.

The original relation contract now also records 2880 contexts using actual
reference/WEAP vtables, actual empty reference+0x10 list lookup and actual
reference/base predicate bodies. A separate 72-case probe executes the original
69-row signature initializer, WEAP header-load prefix and predicate: raw header
bit0x400 feeds the predicate, while prior runtime bit0x4000 is retained and bit8
is cleared by the WEAP prefix. File read, registration and membership boundaries
remain explicit. The first two header harnesses failed because their uninitialized
native signature table rejected WEAP; the corrected probe executes the actual
initializer rather than substituting table values or the decoder.

Native dropped placement/scale, replacement equipment, composition of relation
publication with prepared contact/removal/key/scene state, pickup cleanup and
real normal-input break/drop/restart courses remain production work. S4 remains
incomplete; no previously rejected break branch is admitted here.

The seed can be reproduced from the recipe's pinned local source without
modifying that source or generating a plugin:

```python
from pathlib import Path
import hashlib, json
from scripts.tes4_runtime_state import load_save, write_save
recipe = json.loads(Path("scripts/data/oblivion_compat/m15_drop_relation_fixture.json").read_text())
source = Path(recipe["source"])
assert hashlib.sha256(source.read_bytes()).hexdigest() == recipe["source_sha256"]
work = Path("build/oblivion-compat/m15/S4/reproduced-drop-relation-seed")
work.mkdir()
promoted, seeded = work / "promoted.omwsave", work / "seed.omwsave"
write_save(source, promoted, load_save(source))
state = load_save(promoted)
references = {ref["key"]: ref for ref in state["references"]}
for key, extras in recipe["expected"].items():
    references[key].update(extras)
write_save(promoted, seeded, state)
assert hashlib.sha256(seeded.read_bytes()).hexdigest() == recipe["seed_sha256"]
```

`S4/drop-relations-runtime-staging-01/recipe-reproduction.json` verifies that
procedure byte-for-byte. The scenario expects this seed at
`{output}/userdata/saves/M15DropRelations/Quicksave.omwsave` and variables
`openmw`, `source`, `resources`, `oblivion_data`, `epoch`. Each epoch uses a
fresh output directory/process; the second receives the first's saved output.
Pinned executable copies also need their adjacent `defaults.bin` and
`openmw.cfg`. Keep the read-only ready script, actual SDL F5, clean quit and
live/binary semantic inspection; a scenario success alone does not assert the
exact persisted relations.


## Native equipped-node drop placement arithmetic

`ESM4::droppedItemPlacement` implements the original NPC break caller's position
and Euler arguments as an immutable rule. The overload accepting an actual
`Nif::NiTransform` copies its rotationScale and translation and deliberately
excludes its separate scale. The selected original path accepts finite matrices
with embedded scale, shear, reflection and degeneracy; it does not normalize them.
Nonfinite matrix/position inputs are outside the supported fork domain and reject.
The native uniqueness flag is retained, including false at gimbal poles, where
the original caller still uses the deterministic returned angles.

The [original contract](M15-ORIGINAL-DROP-PLACEMENT-PROBES.json) now records
`S4/drop-placement-native-oracle-06`: **4992 contexts**, crossing 208 matrices,
two translations, six separate scales (including zero/negative) and both x87
control words. Actual original 005F3A93..005F3AF7, 00711440 and its math callees,
and pi/half-pi initializers execute. Only actor virtual +2C8 records the drop
arguments and returns null. Eighty curated matrices include signed zeros, signed
cardinal angles, combined/gimbal/adjacent-threshold cases and embedded matrix
scale/shear/degeneracy. Another 128 use declared seed 1504003. The 160 curated
control-word outputs are versioned; the larger corpus stays ignored.

`S4/drop-placement-native-oracle-06/cpp-comparison.json` compares all **416**
distinct matrix/control-word outputs with a separately compiled current production
C++ rule: every angle bit and native uniqueness flag agrees exactly. Separate
scale/translation crossings are independently checked by the original caller
probe. Three component tests additionally compare the 80 curated cases against
literal original bits, cover the native transform overload across five scales,
copy translations exactly and reject nonfinite supported inputs. Test expectations
come from the original image, never the production implementation.

Final `S4/drop-placement-full-normal-01` and `-full-sanitized-01` each pass all
**2636 component tests/253 suites**, unfiltered with exact inventory/XML agreement
and zero failures/skips. Both before/after source fingerprints agree:
`a96f6e65ff64ee5df87356ed39b331d3f3b2f18759aa63383502fee55b266153`,
atop `ec471945a7469e2d7a6afe0f33543b22fa5469d3`. ASan leaks are disabled and
UBSan halts. No engine, Python or runtime authority changed, so those checks were
not repeated for this arithmetic chunk. Only documentation/probe expectations
follow these final checks.

Preflight -01 retained one failed signed-zero comparison: unary negation differs
from the original gimbal `0 - angle` subtraction for zero. The corrected -02
comparison passes; -03 adds the native-transform overload and finite-matrix domain.
An early uncommitted proper-rotation validator was removed when source review
showed that native NIF matrices explicitly permit embedded scale. Expanded
original -03 through -06 probes confirm that distinction and the native AL flag.
No failed evidence is counted as an acceptance pass.

This is not a renderer capture/factory integration or a new gameplay branch.
`NifOsg::MatrixTransform` retains native separate scale and rotationScale precisely
because the final OSG matrix cannot recover them. The next capture must preserve
that provenance, including actor placement/morphology transforms, rather than
normalize/decompose the assembled matrix. Created-reference scale, replacement
equipment, complete source/contact/scene/body/relation publication and real
normal-input break/drop/restart courses remain open. S4 remains incomplete.


## Native carried-weapon drop anchor capture

`ESM4NpcAnimation::captureCarriedWeaponDrop` now prepares the actual carried
weapon body together with an independently captured skeleton `Weapon` anchor.
The original node-index constructor and Character/process getters establish
that index 3 is this anchor. It is not the carried model root: an inserted model
offset changes the carried body binding while leaving a freshly captured drop
position at the skeleton anchor. This is preparation for the drop factory;
no contact or inventory branch starts using it in this checkpoint.

`OblivionDroppedWeaponPose` owns the scene path and preserves each authored
NIF separate scale. It reads current rendered local matrices divided by their
known NIF scale; procedural controllers can leave cached `mRotationScale` stale.
Synthetic MatrixTransform/PAT wrappers retain their full linear map with separate
scale 1. Composition uses the shared original-compatible `Nif` helper, retaining
finite embedded matrix scale/shear/reflection and zero/negative separate scales.
The existing positive ragdoll API keeps its input/output validation around that
helper. A nearest-even store admits finite FLT_MAX rounding up to, but excluding,
the original overflow midpoint. Nonfinite inputs or intermediate results reject.

The caller must supply resolved native actor placement; capture verifies its
rendered root translation after float narrowing and linear map at relative
1e-6 tolerance (absolute floor 1). Subsequent validation compares stored root,
scene and native result bits exactly. A zero explicit root scale is supported;
an intermediate authored NIF separate scale of zero rejects because current
rendered orientation cannot be recovered. Unknown/projective/nonrelative
transforms, detached/hidden paths, overflow and stale bindings also reject.
This API does not establish native actor placement/morphology policy.

Validation first checks the retained carried item/model/definition binding, then
borrows the already populated node map. It never calls the lazy map builder.
Anchor and carried-body captures must share the same actor placement node.
The caller still needs World/lifetime guards before borrowing animation/item
owners. Move/copy retention and invalid source/path/definition/scale/root/map
cases are covered, including the actual NPC animation with an owned NIF body.

The [editable original contract](M15-ORIGINAL-DROP-TRANSFORM-PROBES.json) records:

- Complete original 0053D7A0 composition with actual matrix/vector helpers and no
  stubs: 688 contexts, including 48 signed near-FLT_MAX store cases. All inputs,
  callee-saved registers, return value and stack convention are checked. Separate
  normal and ASan/UBSan production comparisons match 664 finite outputs exactly
  and reject 24 nonfinite results. Wrong-bit, missing/duplicate-row and malformed
  input controls all reject. Sixteen curated original rows are versioned.
- Original 00478070 node-index construction, 004349A0 Character getter,
  0064B110 process getter and 00477EC0 index getter: eight missing/index/cache
  combinations. Named scene search, as-node conversion, refcounts, logging and
  BackWeapon/SideWeapon visibility are declared boundaries. No full scene search,
  shield, inventory or runtime route is asserted.
- Original 004D7AF0 reference matrix builder with its actual REFR vtable dispatch
  and WEAP base: 356 drop-angle/control-word cases, no stubs. The existing
  `Misc::Convert::makeOsgQuat` agrees within declared 2e-6 tolerance (maximum error
  1.084227760905776e-7). The earlier direct reconstruction baseline differs for all
  146 combined matrices; it incorrectly assumed the native angle extractor and
  reference builder must be inverses. Both results are retained. No generic
  reference renderer change is warranted by that baseline.

Final `S4/drop-native-capture-engine-normal-02` and `-engine-sanitized-02`
each pass all **1510 engine tests/122 suites**, exact inventory/XML agreement,
zero failures/skips. Their source fingerprints agree before/after:
`6353f9fd225a3a5a0890c2dd71c34cc63616fd18c91dca2061f58878ee356d97`,
atop `b2bf479fe94ef2abc90a8eed7c8827da85bbffef`. Normal and sanitized
`S4/drop-native-capture-full-*-01` component results each pass **2639 cases/254
suites**, fingerprint `626dd5558a2b96c165398c2bf839b7f87ad6bfe1ad7101f5eb288b74966efa79`.
The subsequent root correction changes only apps renderer/header/World test;
`drop-native-capture-review-01/reinspection.json` and saved per-source digests
verify unchanged component source and binary hashes. ASan leaks are disabled;
UBSan halts on errors. No Python codec or runtime authority changed in this chunk.
Only documentation and editable original expectations follow these checks.

`S4/drop-native-capture-allocation-01` runs five focused actual engine tests under
GDB against normal test binary SHA256
`d4ce7b67857e6ddcf14aa1cb4cb37dc5b542c97fd0338b83d45860a3fb59faeb`.
There are 53 balanced outer entries/returns and 42 nested entries; native
composition/anchor-validation/NPC-validation have 50/35/10 hits respectively.
No main-thread malloc/calloc/realloc/aligned_alloc/posix_memalign is observed.
Preparation and other threads/APIs are outside this bounded trace.

Retained failures include an initial fixture assertion on intentionally rejected
scene overflow, 12 finite original FLT_MAX results rejected by an overly strict
range check, and a regression demonstrating that a bone-rooted anchor could be
accepted as actor-rooted. The corrected store and explicit root pointer guard
pass the final checks. Sanitized full-01 was deliberately stopped at its engine
build before the root fix; its completed component result remains valid, and its
overall report remains failed. No failed or stopped run is counted as a full pass.

Factory scale, actor placement/morphology admission, compound source/contact/key/
scene/body/relation publication, replacement equipment and pickup cleanup remain
open. Actual normal-input break/drop and fresh-process restart acceptance have
not been added by this checkpoint. S4 remains incomplete.


## Resolved native NPC root placement

`ESM4::npcPlacementRoot` now constructs the original NPC root rotationScale
from yaw and resolved race weight/height, with absolute reference scale kept
separate. Actor pitch/roll do not enter that matrix. Missing race, authored
zero/negative morphology and negative-zero reference scale remain distinct.
Nonfinite consumed inputs/results and the original x87 range boundary reject.
`ESM4Npc::adjustScale` now applies the same absolute reference scale before
signed TES4 race morphology, so negative reference scale agrees with the native
root rather than reflecting the rendered NPC. Other record families retain their
existing rendering policy.

The store-resolved carried-drop overload reads initialized native NPC custom
data and winning records without lazily constructing a cache. It rejects cold
or wrong caches, stale traits/base/race/sex, mismatched placed base ID/key, wrong
store and non-TES4 NPCs before borrowing stale record fields. It recomputes the
root and uses the existing exact scene/body/anchor validation. The caller still
owns World/reference/animation lifetime guards; this resolver is not a lifetime
lease or a contact/drop publication. A new World case covers both race sex lanes,
cache/stale-record failures, negative scale and actual class scale adjustment.
The owned-NIF NPC case now exercises automatic root capture, native translation
under scale2, sign-equivalent reference scale and restoration/stale binding.

The [editable contract](M15-ORIGINAL-NPC-PLACEMENT-PROBES.json) and
[21 original expectations](M15-ORIGINAL-NPC-PLACEMENT-EXPECTED.json) preserve
independent evidence. `S4/drop-npc-placement-native-04` runs **12348 contexts**
twice in a native Linux i386 harness using hash-identified original PE functions,
not an emulator or production C++. Complete004D7AF0 and its actual base/sex/axis/
matrix callees execute. The actual GenerateNiNode004E43C8..004E43E9 prefix reads
004D7260, performs FABS and stores node+60; node virtual+50 is a declared stop
boundary. Explicit callee-saved canaries, builder return/EAX/stack and reference/
base/race input checks pass. The host CPU model, harness/source/corpus hashes
and build command are retained. This is original instruction execution, not a
full original-game session or whole GenerateNiNode acceptance.

The corpus includes the preceding2016 structured contexts, 4096 seeded float-bit
yaws and1024 seeded ordinary angles under both x87 words, plus stress/edge cases.
Seed1504005 and deliberately different unselected sex lanes are declared.
Separate current normal/ASan production comparisons match all **12316 finite
outputs bit for bit** and reject **32 nonfinite outputs**. Wrong-bit, missing/
duplicate-row and malformed-input controls reject in each profile. Native04 has
byte-identical expectations to native03 used by the comparison.

Hardware execution matters here: the emulated ordinary2016 cases agree, but40
large-angle contexts differ from hardware. Direct host libm/long-double attempts
also disagree for16 finite stress contexts. The implemented fixed uint128
remainder uses the documented x87 Pi constant before bounded sine/cosine and
native float stores. It avoids platform-specific assembly/long-double precision.
[Intel's SDM, Approximation of Pi](https://cdrdv2-public.intel.com/789574/253665-sdm-vol-1.pdf)
supports that reduction constant; original hardware execution supplies all golden
outputs. Getter-only signed scale is also insufficient:3252 corpus rows differ
from the actual FABS/store. Both failures remain recorded, not accepted passes.

Final `S4/drop-npc-placement-full-normal-02` and `-full-sanitized-02` each pass
**2641 component tests/255 suites** and **1511 engine tests/122 suites**,
unfiltered with exact inventories and zero failures/skips. Both before/after
source fingerprints are
`29c2a652efc5cf84633aabfaff74ac8c401effcc02ea2a6796ea715b0b4afe3b`,
atop `3b5c168cc30e0db38385471b6d6108f6b2b84cdf`. ASan leaks are disabled;
UBSan halts on errors. No Python codec/schema implementation changed.
Only documentation and original probe expectations follow these checks.

`S4/drop-npc-placement-allocation-01` additionally runs six actual renderer/World
cases under GDB. Native root arithmetic, winning-record resolver, native anchor
validation and store-resolved NPC validation have13/33/42/5 hits respectively.
There are74 balanced outer entries/returns and19 nested entries, with no observed
main-thread malloc/calloc/realloc/aligned_alloc/posix_memalign calls. The integer
reduction branch executes. Exact test ELF SHA256 is
`2059abc101018883919c046db8e28b2b3efdf2422d471a798628c3bb83d5e5c9`.
Preparation, other threads and other allocator APIs are outside that trace.

Actual SDL-input regression courses pass in both final game images, each with
held block, activation, one NPC attack, F5 save, then a distinct-process load and
resave after release. Normal PIDs545835/545966 and sanitizer PIDs548259/548473
are terminal. Their game SHA256 values are respectively
`cc1349851989ac0aec9ecd782b4d770d4614fa3c24666d720ef2b9680b779921`
and `0cd0b2e1c41caafeed9627b10d7fed5daab14f326fcc392ffe7f0189f8250e05`.
All nine actor-value arrays match exact expected state except the declared Player
health delta(-0.9841405749320984); weapon conditions are55.84976577758789 and
55.70000076293945. Inventory, RNG, action counter, empty pending owners and
released posture checks pass through schema46-to47 save/restart without replay.
Two positional WPNBlockBlade voices pass the fixed0.75 stock-PCM threshold
(normal correlation0.9981472655040019; sanitizer0.9983007993984149), with no
second-epoch replay cluster. Held/restarted captures were directly reviewed.
Evidence is `S4/drop-npc-placement-runtime-{normal,sanitized}-01`.
A copied verifier initially attributed these runs to an older source report.
The original reports remain retained; corrected verification binds the actual
pinned executable hashes and launch provenance to the final build fingerprint,
reruns every semantic assertion and confirms that only source attribution changed.
This course verifies existing block/contact/save behavior, not break/drop gameplay.

Further original factory dependencies are now bounded independently: the full
new-reference constructor initializes scale1 in32 no-stub contexts; complete
extra transfer runs1024 contexts across all64 subsets of owner/global/rank/
condition/charge/scale extras and four payload profiles. It preserves float bits,
but removes null owner/global, rank-1 and scale1 extras through native setters.
Critical sections and allocation/free remain explicit transfer boundaries.
The selected final WEAP ownership suffix passes96 cases with actual effective
owner/getters, typed ownership mutation and original CRT CLOT RTTI. Cell-owner/
actor-claim queries and change marking remain declared boundaries; the result
replaces an absent explicit owner with actor-base ownership only for an owned
cell with no actor CELL claim. It preserves other extras. These probes do not
implement or accept the complete factory.

The current generic renderer yaw quaternion still differs from original NPC
hardware reduction for76 of1780 finite no-morphology baseline contexts, all at
large noncanonical angles. Normal-input angles agree within the existing1e-6
root admission tolerance; mismatched rendered roots reject capture. The retained
`drop-npc-placement-review-01/remaining-renderer-yaw-baseline.json` is an open
renderer integration dependency, not an accepted large-angle drop route.
Source ExtraScale application also remains open: a first full SetScale probe
cannot complete its game-dependent setup and supplies no numeric expectations.
Do not infer all dropped-reference scales from the fresh constructor's1.

Retained build attempts include an initial fixture write through a const
position reference and deliberately stopped full-01 runs before the FABS fix.
Their reports remain failed/stopped. The first transfer fixture used a wrong
ExtraScale vtable; the actual typed setter identifies the corrected one.
No failed or stopped attempt substitutes for the final checks.

Compound source/contact/key/scene/body/relation publication, replacement equipment,
pickup cleanup and actual normal-input break/drop/restart acceptance remain open.
This checkpoint enables no new broken-item contact branch. S4 remains incomplete.


## TES4 NPC renderer yaw bridge

Scene insertion and both direct/inverse actor rotation updates now share
`MWWorld::makeActorNodeRotation`. For TES4 NPCs it uses the independently verified
native float yaw matrix, then converts its sine/cosine to a normalized OSG
quaternion. Other actor profiles retain their existing quaternion convention.
The OSG representation approximates the original float matrix within the existing
1e-6 root-admission tolerance; it does not replace the exact matrix retained for
native drop arithmetic. Native out-of-range/nonfinite yaw still cannot be
admitted by drop capture. No lazy NPC custom-data construction is needed for
scene rotation.

A new World case compares five literal original hardware matrices, including
four large-angle cases, and checks profile isolation and cold-cache behavior.
The actual owned-NIF carried-weapon test additionally exercises three large yaws:
the old quaternion rejects capture, the production Scene helper permits capture
and revalidation, and restoring yaw restores the earlier binding. This closes
the previously recorded large-angle renderer dependency for this NPC path.

`S4/drop-npc-yaw-normal-01` and `-sanitized-01` each pass all **1512 engine
tests/122 suites**, unfiltered, with exact inventories and zero failures/skips.
Before/after source state is `e96f19230e8aaae87aa9fbc7b053997793e541c8` plus fingerprint
`5e1b7528a1a147e0a91b5d90e6e8da1e40adbfb7de987bb7bc0321796810a34c`. The three implementation-file digests remain
unchanged after verification. ASan leaks are disabled; UBSan halts on errors.
Component rules and Python codecs are unchanged; their earlier checks are not
represented as rerun evidence for this renderer-only checkpoint.

Actual SDL-input block/contact/save/restart regressions pass in both final game images.

The normal course uses distinct terminal PIDs 552178/552384, game SHA256 `bb0871fb536fc88fcd5a96b03607e909726af3bf37c7d7c93bec9f610d80654c`, and stock-audio correlation 0.9987121739676377.

The sanitized course uses distinct terminal PIDs 554772/555040, game SHA256 `ebc23a3fbc8423295a1b4c2c1762447834a1164122bc3f8794668fcd49c4fdef`, and stock-audio correlation 0.9954370796202132.

Each course preserves the exact nine actor arrays, declared Health delta
-0.9841405749320984, weapon conditions 55.84976577758789/55.70000076293945,
inventory, RNG and action ledger through held-block save and released-input
fresh-process continuation without replay. Two positional block voices and the
stock-PCM cluster checks pass; no replay cluster appears after restart. Held and
restarted captures were directly reviewed. Evidence is
`S4/drop-npc-yaw-runtime-{normal,sanitized}-01`. These are ordinary block/contact
regressions, not new break/drop gameplay or large-yaw normal-input acceptance.

The [source-scale probe contract](M15-ORIGINAL-DROP-SCALE-PROBES.json) also bounds
the next factory dependency independently. `S4/drop-source-scale-native-02`
executes complete original 004DB520 with actual original sprintf/atof and CRT float
initializer 00982837 on native i386 hardware: 14,440 contexts repeat identically.
Seed 1504006 supplies 2,048 arbitrary finite float-bit patterns and 2,048 normal scale
values under both x87 words, plus 6,000 contexts adjacent to decimal half-cent
boundaries. The preceding 248 emulated contexts agree exactly. Boundary fixtures
supply Windows error/TLS/FLS services, the temporary buffer and change marking;
no formatter or parser is stubbed. Registers/stack/FPCW and reference/base
preservation checks pass. The original rounds text to two decimal places, parses
to binary32, then clamps to 0.01..10; 0.125 becomes 0.13, while binary32 0.005 formats
as 0.00 and clamps to 0.01. No production scale law is added in this checkpoint.

`S4/drop-source-scale-transfer-oracle-03` separately executes actual original
004902C7..004902FC in 24 contexts: null/empty source, condition-only source and
condition+scale source values. The original getters, complete SetScale, remove-true
extra transfer and destination ExtraScale removal execute together. Source extras
clear; condition remains; destination ExtraScale is absent. ESI/EDI/EBP and stack/
FPCW remain intact; EBX is the declared destination-list output when copied.
Allocation/free and critical sections are additional boundaries. These are
original instruction probes, not a complete factory or game session.

Earlier SetScale failures came from unresolved PE imports and uninitialized CRT
float-dispatch state. They remain retained. The corrected harness resolves the
declared OS boundaries, seeds a synthetic CRT thread with original default locale
pointers and executes the original float initializer. Development diagnostics
are not final acceptance reports.

Compound contact/source/key/scene/body/relation publication, replacement
equipment, pickup cleanup and actual normal-input break/drop/restart acceptance
remain open, along with the broader S4 melee/mastery/reaction/input matrix.
S4 remains incomplete.


## Native dropped-reference scale rule

`ESM4::droppedReferenceScale` implements the independently verified fresh-reference
scale dependency: absent ExtraScale selects 1; explicit finite scale rounds to
two decimal places using positive half-away ties and clamps to 0.01..10. Negative
and signed-zero inputs select the positive minimum. Nonfinite extras reject.
Actor/attachment scale is never an input. Binary64 multiplication/addition are
exact in the admitted rounding interval, avoiding host printf tie policy.

Thirty-two literal original hardware input/output pairs cover exact ties,
adjacent floats, signed zeros, denormals and extrema. Separate normal and
ASan/UBSan production comparisons match all **14,440 original contexts**; three
nonfinite extras reject and three absent-extra cases ignore their payload.
Wrong-bit, missing/duplicate output and four malformed-input controls reject.
Evidence: `S4/drop-reference-scale-comparison-01`. The independent corpus remains
`S4/drop-source-scale-native-02`; expectations are not generated by production.

`S4/drop-reference-scale-normal-01` and `-sanitized-01` each pass all **2642
component tests/255 suites**, unfiltered with exact inventories and no failures
or skips. Before/after state is `e084bcb2bc6e825f1c4819e49bc72132372761f7` plus
`295ad45ea8645d7679501f30921bcb9a26cfc4d7f869757da77a93ac00fea061`. ASan leak checks are disabled; UBSan halts.
Only arithmetic/header/component-test code changes in this chunk; no engine or
normal-input runtime evidence is claimed for the scale rule itself.

Loose reference admission still requires unit scale. Scaled physical properties,
root reconstruction, compound source/reference publication and actual break/drop
acceptance must consume this rule before the factory gate closes. Existing scale
save storage is `custom_state["scale"]`; no schema change is required here.
S4 remains incomplete.


## Resolved scale in loose-weapon scene and collision admission

`WorldModel::prepareLooseWeaponAdmission` now accepts resolved native reference
scales and rejects unrounded, out-of-range or nonfinite values before publication.
It retains scale on the renderer placement and owns a copy of the existing native
property-scaled body graph for collision admission. The independent Havok/world
length factor stays unchanged, so packed velocities retain their original units.
The source authored graph remains unchanged. Existing independently verified
mass/inertia/shape/bodyT property rules supply the scaling; no new native
property law is inferred from the integration tests.

The loose physical owner retains its admitted reference scale. Prepared collision
validation rejects changed scale, and rigid-root reconstruction rejects stale
scale. Reconstruction removes the scaled bind translation while keeping renderer
scale separate from rigid rotation. This avoids both an incorrect root offset
and multiplying the world-unit conversion by reference scale.

Three new engine cases cover 0.01, 0.13, 2 and 10 under ordinary/bodyT forms:
actual scene scale, independent rigid-root expectations, collision-sphere ray
intersections, native packed velocity units and physical-owner reconstruction
from captured pose/velocity data. Unrounded/nonfinite scales and changed prepared
scale reject without publication. This is in-process physical reconstruction,
not fresh-process save acceptance or contact-driven drop gameplay.

`S4/drop-scaled-admission-normal-03` and `-sanitized-03` each pass all **1515
engine tests/122 suites**, unfiltered with exact inventories, zero failures and
zero skips. Before/after state is `88a55a3f43b64035b7f67c8dfcb6d4e105f924c5` plus fingerprint
`967bdaabf1667c09dbfc53efecefccb6fb335ec93c2676aa64fbe0923c03ef1a`. All three implementation-file hashes match
the frozen review manifest. ASan leak detection is disabled; UBSan halts.
The component arithmetic is unchanged from the preceding 2642-test checkpoint;
no Python codec changed. No new normal-input runtime course is claimed here,
since the new scale path still awaits the production contact/drop factory.

Retained full-01 attempts contain a missing direct body-definition include; the
parallel sanitizer build was stopped before correction. Full-02 attempts were
stopped after a test-only duplicate SCOPED_TRACE name was diagnosed. The direct
include and separate macro lines are corrected. The affected test file compiled
before final full-03 runs. Failed/stopped attempts are not accepted evidence.

The [scale probe contract](M15-ORIGINAL-DROP-SCALE-PROBES.json) additionally
records 162 original extra-list comparator contexts, repeated on native i386
hardware with actual 0041E550/ExtraScale virtual comparison. Critical sections
and synthetic Windows TLS are explicit boundaries. All results agree with the
separate emulator. In 98 finite contexts, presence and float value participate:
missing scale differs from explicit1; signed zeros compare equal. The result is
AL, not full EAX. The other 64 contexts are nonfinite diagnostics. The first
native harness's missing-FS fault is retained; no game failure is inferred.
This is not the complete stacking or pickup caller.

Read-only source inspection identifies the next dependency: RuntimeInventoryItem
and World actor-inventory capture/staging omit scale; native stacking and
prepared-add matching omit it too. Inventory ExtraScale needs optional presence
and value authority, separately from the world reference's existing
custom_state["scale"]. Original stacking/pickup callers still need verification.
Compound source/contact/key/scene/body/relation publication, replacement
equipment, pickup cleanup and actual normal-input break/drop/restart acceptance
remain open, along with the broader S4 melee/mastery/reaction/input matrix.
S4 remains incomplete.


## Optional native inventory ExtraScale persistence and identity

Schema 48 appends an optional binary32 ExtraScale to every Player/reference
inventory item. Missing metadata differs from explicit 1; negative finite values,
signed zeros, denormals and extrema retain their bits. Legacy 1..47 decode absent;
a downgrade with present scale rejects instead of losing information. Canonical
JSON uses `extra_scale` with null or a finite value. C++ and Python codecs agree on a 730-byte independently assembled wire fixture with
Player -0 and NPC explicit 1. Presence flags outside 0/1 and nonfinite payloads
reject. The fixture SHA256 is
`fd764ef60752ef463ce7a97a58e4e063c268e3e75e2018df16948346be082360`.

Projected inventory CellRefs own this optional field separately from their
ordinary placement/mesh scale. The fork's NSCL save subrecord preserves it
through shared object-state serialization; native placed references cannot use the inventory setter. World
capture/staging and lazy NPC/creature inventory restoration preserve the field.
Capture rejects corrupt metadata rather than silently omitting an item. Native
stacking includes optional scale using numerical equality, including signed-zero
equality. Prepared addition, equipped removal, bow release, physical-contact
gear snapshots and condition requests guard exact presence/bits, so an instance
change invalidates a pending operation without publication. Condition wear
preserves the metadata. Morrowind stacking retains its existing profile policy.

The emulated original two-candidate merge-search block
5D27C6..5D281B/5D27EA selects the first equal extra list or takes the append branch in all 686 declared contexts.
Complete 41E550 and actual ExtraScale comparison run; expected selection comes
from the prior native hardware comparator corpus. Critical sections and synthetic
TLS are boundaries. The probe stops before count/publication changes and checks
input lists/links, stack, EBX/ESI and FPCW preservation. It does not establish the
complete outer function, all stacking fields, pickup conversion or gameplay.
Evidence: `S4/inventory-scale-merge-search-oracle-01`.

Final normal evidence: `S4/inventory-extra-scale-full-normal-03` passes 2684
component tests/255 suites, 1519 engine tests/122 suites and 303 Python tests.
C++ inventories are unfiltered/exact with zero failures/skips. Before/after:
`32ac62f3acf88b19dcf368e96db41f49996f526b` plus
`7cf6c184a16d00017f5fa445ecfa4de30ddc8a9183e39e1c8b5638d5523943f0`.

`S4/inventory-extra-scale-runtime-normal-01` passes all 8 commands with distinct
terminal PIDs 574344/574456, binary SHA
`93dd34f4c1c6ebb4b44cd44e57aadd519aac711fa5cb01f417431470d6c22f1b`.
Player weapon 0x090615 explicitly carries scale bits 3e051eb8; Valen Dreth's
0x01fc43 inventory explicitly carries 3f800000. Both save epochs retain bits;
all 9 actor arrays, exact Health delta -0.9841405749320984, Player/NPC weapon
conditions 55.84976577758789/55.70000076293945, RNG and action ownership match.
Stock block PCM correlation 0.9941326175376067 appears once; restart has no replay.
Both gameplay screenshots were directly reviewed. These are inventory metadata
persistence regressions during ordinary activation/block/contact; no visual
carried-scale, pickup, factory or actual break/drop acceptance is claimed.

`S4/inventory-extra-scale-morrowind-01` passes an actual Morrowind load/resave
with the same pinned normal game, 102 output records and no GPRO/T4VR/T4ST,
OMW4STATE or NSCL. Original input SHA
`c4b8bb69e43f5b80a760834e3dbd6dc2b9507ebe4d8979d2f1943e05c3b4f915`
is unchanged. Its screenshot was directly reviewed; this is profile isolation,
not broad Morrowind gameplay or audio acceptance.

Retained full-normal-01 failed an existing schema 47 fixture that inherited
CurrentVersion 48; it now explicitly chooses 47. Sanitized-01 was stopped before
that correction. Full-normal-02 passed 1518/1519 engine cases; the new roundtrip
fixture lacked native NPC race metadata. Sanitized-02 was stopped before the
fixture correction. A focused correction run then revealed only actor_ai 0->1
registration after restore, independently diagnosed via GDB JSON snapshots;
explicitly initialize outgoing resident NPC AI before taking the baseline.
The corrected test preserves complete byte-equality and literal scale assertions
and passes. These failed/stopped runs are not accepted evidence.


`S4/inventory-extra-scale-full-sanitized-03` also passes all **2684 component
and 1519 engine tests**, with exact unfiltered inventories and zero failures or
skips. Its before/after source matches the final normal report above. ASan leak
checking is disabled and both sanitizers halt on errors. The sanitizer game
build continuation is retained separately; this final helper supplies the
accepted source/binary/inventory checks.

`S4/inventory-extra-scale-runtime-sanitized-01` passes all 8 commands with
separate terminal PIDs 578973/579153, executable SHA
`21468df907733bb4383e2e0e5a3fd191e331bcb09317ee0b114cda240f34e1a7`.
Exact scale bits and the same contact/state/audio/restart invariants pass. Both sanitizer screenshots were directly reviewed. No ASan/UBSan diagnostic
appeared. This adds instrumented runtime regression coverage, not actual
break/drop or visual scale acceptance.

All 20 implementation-file hashes match the frozen review manifest. The remaining
production work includes compound contact/source/key/scene/body/relation
publication, replacement equipment, pickup conversion/cleanup and actual
normal-input break/drop/restart acceptance. Broader S4 power, creature, mastery,
reaction, effect and input/controller/FPS requirements remain open. S4 remains
incomplete.
