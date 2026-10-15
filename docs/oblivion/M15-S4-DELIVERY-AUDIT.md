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
