# M15S4 delivery audit

Status: **incomplete**. The initial audit inspected `4996a2eb1a3a666751358f7435a459907e6e6345`
on 2026-10-15. This is a working requirement map, not acceptance. The controlling
scope is [S4 in the implementation plan](M15-COMBAT-STEALTH-CRIME-IMPLEMENTATION-PLAN.md#s4---deliver-a-normal-input-melee-slice),
including its tests and gate. Historical pure-rule, isolated body and service
checkpoints in the milestone ledger do not establish complete live gameplay.

## Required implementation

| S4 item | Current source evidence | Remaining delivery work |
| --- | --- | --- |
| 1. Input through contact for Player, NPC and creature | `CharacterController::updateOblivionWeaponState`, `dispatchOblivionMeleeContact`, `advanceOblivionMeleePlayback`; `Actors::update` handles paused input. Ordinary and standing-power Player/NPC contacts reach `commitOblivionMeleeContact`; the ordinary API remains a restricted wrapper. | Spell-free, unarmed Creature ordinary strikes now reach the same contact owner through stationary AI and stock handtohand KFs. Spell-free Creature victims now accept bounded ordinary contacts and timed force; defensive/effect branches, armed sources and the complete input/view matrix remain open. Trace and verify every admitted actor route. |
| 2. Blade/blunt/unarmed/creature, normal/directional power, geometry | `oblivionmelee.cpp` acquires collision/LOS contacts and resolves ordinary and standing-power weapon/unarmed damage. Power owns three phases and schema49 persistence, independent of the ordinary queue phase. First-person keyboard unarmed/shortsword and restart courses pass; see the standing-power verification index. | Directional/mastery selection is explicitly unfinished in the controller; directional contacts remain rejected. Creature natural attackers have bounded integration and completed-strike restart coverage; Spell-free unblocked Creature victims have bounded resource/force integration; defensive/effect and remaining source domains are rejected. Complete those routes and the full geometry, weapon/view/input/FPS matrices. |
| 3. Block, mitigation, wear, fatigue and reactions | `beginBlocking`/`endBlocking` own posture; ordinary contacts prepare values, conditions, RNG and timed knockback. | Basic equipped block now prepares shield/weapon mitigation, separate Fatigue cost and sequential condition wear through the owned contact. Attacker/block spending now uses the native positive-current cap; direct hostile Fatigue damage stays separate. Expert/Master defensive procs, zero-absorption unarmed weapon-block reactions and zero/broken-after-wear items remain rejected. Complete proc/breakage/reaction publication and renderer/physics recovery once per window. |
| 4. Safe interruptions | Controller cancellation clears strike/input/posture before animation disable. Actors cancel Player and owned stationary intents on pause and control locks. Lifetime-guarded unequip notifications can now be staged for dispatch after controller/actor updates and retained across simulation rollback. | Verify all named transitions: weapon change/breakage, unequip, menu/control lock, stagger/knockdown, death, load, mount; release/held-input behavior and restarted ownership need live coverage. Existing guards alone are not proof of the entire matrix. |
| 5. Actual animation, HUD and sound | Native KF timing, swish, unarmored NPC/Creature hits, equipped-block sounds and committed Player target Health HUD are wired in the controller. Weapon-block stock PCM and fresh-process replay checks pass. | Armored ordinary hits, remaining unarmed-only/Creature sound branches and live shield audio remain open. Complete native reaction animation and related sound/HUD feedback across the required matrix. |
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


## Grouped native weapon-drop publication

A single melee contact can break both equipped weapons. Preparing each dropped
reference independently reserved the same World serial/local RefNum and separate
registry revisions, so sequential commits could not publish both atomically.
`prepareLooseWeaponAdmissions` now prepares one nonempty group of detached
native WEAP references, with one private pointer registry and distinct local
reference numbers. Original input references and public counters stay unchanged
on cancellation. Duplicate stable keys and malformed members reject before
publication. The existing single-reference API delegates to this path.

`prepareOblivionDynamicReferenceKeys` reserves a contiguous key range without
advancing the saved serial. One validated commit advances by the whole range.
The existing singular key accessor/preparation remain compatible. Empty ranges,
native-reference collisions and overflow reject; the last two usable keys advance to
the existing exhausted sentinel without wrapping. Competing reservations,
registry changes, clear, destruction and successful restore retire old handles.
The existing schema 48 serial field is sufficient; no save-schema change occurs.

All fallible scene admissions precede all physical admissions. Two models
prepared for a previously unseen cell share the first admitted cell root; a
subsequent token retains its placement/count snapshots while rebasing onto that
specific sibling. Published models revalidate actual position, rotation,
adjusted scale, node mask, parent/map ownership and reference snapshots. A later
scene callback changing the first published model therefore rejects the group.
Physical admission and the final source guards follow. One publication hook,
registry swap and the prepared cell-list splices publish the entire group.
Every failure rolls back its admitted models and bodies before source inventory,
contact, serial, registry or cell publication. Reverse rollback removes children
before the transaction's new cell root.

Seven new engine cases cover shared-cell publication with distinct stable/local
identities; cancellation, empty/duplicate/malformed groups; rejection or
insert-then-throw on a second cell; changed first reference/scene transform;
late rejection after both actual physical owners exist; key-range cancellation,
competing commit, save/restore and overflow; and a compound contact with two
actual NPC inventories. That compound test publishes both condition `0` writes,
two equipped removals, one exact Health/Fatigue contact and one two-key serial
advance, or preserves the pending contact, both inventories and public state.
A late source ExtraScale change and changed World god mode reject before source
publication. The external scale change is retained. Repeated commits do nothing.
Owned OSGT and one-body fixtures isolate external asset loading; this is a
transaction boundary test, not native factory construction or input-triggered
wear/break dispatch.

`S4/contact-drop-group-normal-01` passes all **1526 engine tests/122 suites**,
unfiltered with exact inventory/XML agreement and zero failures/skips. Its
before/after source is `ea822b168d072ac6a4b42bc604fed04796188c18` plus fingerprint
`5ae0ace71ae4e0ded97b4bfa951ca78f9e20a88e8637e0584dc29fe7b9c3c315`.
Component rules/codecs and Python sources are unchanged from the preceding
2684-component/303-Python checkpoint; they were not rerun for this engine change.

`S4/contact-drop-group-sanitized-02` also passes all **1526 engine tests/122
suites**, unfiltered with exact inventory/XML agreement, zero failures/skips and
no sanitizer diagnostic. Its before/after source matches the normal report above.
ASan leak checking is disabled; ASan/UBSan halt on errors. All eight implementation
file hashes match the frozen review manifest. No new normal-input runtime course
is claimed: the new group still needs production factory/contact dispatch.

`S4/contact-drop-group-allocation-01` traces the successful two-weapon hook
through the entire group commit return, including actual contact/two removals,
serial advance and registry/cell splices. One entry/return has zero allocations
from main-thread malloc/calloc/realloc/aligned_alloc/posix_memalign. Exact normal
test ELF SHA256:
`860161af52657ab4522a7cf1104dafcad95e39282fd600eefc81f27b7eb93dcc`.
The same detector catches one deliberate allocation in its positive control.
Prepublication validation/resource admission, other threads/APIs and native
factory/gameplay are excluded from this allocation claim.

The retained sanitized-01 build was stopped before adding actual published-node
transform validation and its late-mutation case. Implementation and test-object
preflights remain diagnosis, not final source evidence. Review also corrected
the new compound fixture to initialize its physics owner before preparing a
contact, because the contact intentionally guards the World physics identity.
All corrected normal tests retain their original transaction assertions.





The next native follow-up is now bounded by
`S4/drop-item-sound-followup-oracle-02`: **120 original x86 instruction contexts**
under Unicorn, with actual selected WEAP getter `005E96E0` and caller `005E99C0`.
Blade subtypes 0/1 choose `ITMWeaponBladeUp/Down`; blunt 2/3 choose
`ITMWeaponBluntUp/Down`; staff 4 and bow 5 choose their corresponding IDs. A null
OSGlobals sound-manager slot selects generic handling IDs. Null items return no
ID. The caller executes that actual getter, then requests the Player or NPC
emission function with its independently observed receiver/arguments, unless the
declared `578FE0` gate returns `0x3EF`. The gate semantics, audio emission bodies,
nonnull handle cleanup, live sound-record resolution and audible acceptance are
not established. Subtypes 6/255 are diagnostics. Register/stack/FPCW/input-byte
preservation and all 60 paired control-word results pass. The first attempt's
incorrect null-manager/no-ID expectation is retained as a falsified assumption.
The editable follow-up contract and native provenance retain exact hashes,
boundaries, sound IDs and request tuples. No production sound rule changes yet.

Native game formulas, drop placement/scale laws and replacement routing are
unchanged. Native factory construction from the bound carried NIF, ownership and
dropper/list transfer, retained Player/quest unequips, replacement equipment,
production contact/wear dispatch, pickup cleanup and actual normal-input
break/drop/restart acceptance still need integration. Broader S4 power,
creature, mastery, reaction, effect and input/controller/FPS requirements remain
open. S4 remains incomplete.

## Immediate pickup after native reference publication

`World::takeOblivionReference` previously required an entry in the cached runtime
snapshot even for a currently registered, live native reference. A reference
created after that snapshot was applied could not be taken until the cache was
refreshed. Pickup now captures and stages only the missing reference, reserves
cache capacity before resource preparation, and checks cache ownership, absence,
size and capacity before publication. The source debit, inventory addition and
new cache entry publish before observers. The appended entry has a statically
verified noexcept move; publication uses the reserved capacity. Existing cached
reference custom state is preserved.

The new tests cover both content and dynamic references created after an applied
cache, exact broken-item condition/charge, observer-visible publication,
recursive pickup suppression, throwing observers, preservation of unrelated
cached state, binary round-trip, and rejection after preparation followed by a
successful retry. They construct real registered cell references and use the
production interaction/inventory path. They do not exercise normal game input
or the still-unwired drop factory.

`S4/new-reference-pickup-normal-02` and
`S4/new-reference-pickup-sanitized-02` each pass all **1528 engine tests/122
suites**, unfiltered with exact inventory/XML agreement and zero failures/skips.
ASan leak checking is disabled; ASan/UBSan halt on errors. Both reports have
identical before/after source state, atop `07b3de5da4e0e0d12bbbef100a4b67054bbdb9ff`,
with tested source fingerprint
`46396a83e873888b2b995440d1735aee6fdbd31891d6941a5d1e79409f49e0b1`.
Component/Python implementation is unchanged and those suites were not rerun.

`S4/new-reference-pickup-baseline-03` links the corrected tests against the
previous committed `oblivioninteraction.cpp`, compiled with the configured normal
engine flags into a separate object/binary. The link map confirms that object's
pickup definition is used. Both tests fail specifically with `pickup source is
absent from the current native state`; the negative-control verifier requires
that message for each case. Scripts, source/object/test/binary hashes, link map,
logs and XML are retained. This is an engine regression control, not an original
Oblivion executable oracle.

The first normal run exposed fixture mistakes: the snapshot was staged without
being applied, and the cloned reference inherited count two. Those fixtures did
not test the intended cache state. The normal-01 log is retained, sanitized-01
was stopped before correction, and baseline-02 is explicitly invalidated.
Baseline-01's compile recipe lookup failed against stale compile_commands; the
accepted baseline uses the actual configured target flags. Production code did
not change after the first normal run; the corrected fixtures establish the
regression and pass in both final builds.

`S4/new-reference-pickup-review-01` freezes both implementation file hashes and
the patch before these report updates. No new gameplay/restart course is claimed.
Native carried-NIF factory construction, source/ownership/drop relations,
retained Player/quest unequips, replacement and sound follow-up, native pickup
ExtraScale/dropper/list cleanup, and real normal-input break/drop/pickup/restart
acceptance remain required. The wider S4 matrix above remains incomplete.

## Carried-NIF weapon resource factory

`prepareEquippedWeaponDrops` now constructs one or more native NPC weapon drops
from actual `ESM4NpcAnimation::captureCarriedWeaponDrop` bindings. It resolves
winning WEAP/CELL/NPC records, reserves a contiguous dynamic-key range, prepares
exact source removals and final condition writes, copies/normalizes native
ownership extras, applies the independently verified CELL-claim owner suffix,
and stages actor dropped lists/reference droppers. The rendered model and
physical definition come from the same identified carried NIF bytes. Inventory
ExtraScale is independently resolved to reference scale; actor/attachment scale
does not replace it. Fresh reference rotation uses the original extracted Euler
through the existing `makeOsgQuat`, not the carried anchor matrix. Initial rigid
bone targets scale bind translation once; body properties/bodyT offsets use the
native graph-scaling helper, and admission receives the unscaled original graph.

The factory uses one grouped scene/body/registry admission. Its publication hook
can compose with a prepared physical contact; source wear/debits, actor relation
swaps, serial advance and deferred removal-notification queues then commit
without observers. Source metadata, legacy health/remainder, winning records,
CELL ownership/factions, carried binding/placement and private destination
metadata are revalidated after fallible resource work. Lifetime tokens precede
borrowed World/inventory/animation/cell dereferences. Cancellation and rejection
leave source resources and serials unpublished. Borrowed reference views retain
the underlying prepared-admission ownership rules; they do not own a published
World's lifetime.

This reuses the accepted native placement/render/scale, transfer, owner-suffix
and relation evidence, including complete original reference rotation builder
`004D7AF0` (356 contexts), the final WEAP owner suffix (96), native relation
predicates (2880) and raw WEAP-header predicate (72). No new original-executable
or original-game acceptance probe is claimed. The relevant contracts remain
`M15-ORIGINAL-NPC-PLACEMENT-PROBES.json` and
`M15-ORIGINAL-DROP-RELATION-PROBES.json`; their declared boundaries still apply.
Legacy projected health is an engine adaptation: integer plus fractional health
without a native extra becomes an explicit placed condition rather than full
health. The caller still owns policy, wear calculation, retained-item unequips,
replacement and handling sound.

Nine new `NativeWeaponFactory*` tests use owned NIF/skeleton assets, real managed
cells, native NPC inventories, Objects, PhysicsSystem and registry ownership.
They cover exact condition/charge/scale, legacy fractional health, explicit
owner/rank/global and CELL claims, relation prepend/quest-header behavior,
observer-visible once-only deferred publication, two-drop success and late
rollback, prepared-contact success and stale-contact rejection, private metadata
changes, cancellation, erased winning records, animation destruction, World
clear, and preparations outliving all their owners. Contact tests supply
precomputed deltas to verify atomic publication; they are not normal-input
combat or native damage-formula acceptance.

`S4/carried-weapon-factory-normal-02` and
`S4/carried-weapon-factory-sanitized-02` each pass all **1537 engine tests/122
suites**, unfiltered with exact inventory/XML agreement, zero failures/skips and
no sanitizer diagnostic. ASan leak checking is disabled; ASan/UBSan halt on
errors. Both reports have identical before/after source state atop
`9a6d16660e777429dafea18b4b80419912a96a7d`, with tested source fingerprint
`1f47e3fd5e0ef090772c429ec916f5640954438ea052f7584cca20fb3fceb32d`.
Component/Python implementation is unchanged; those suites were not rerun.

`S4/carried-weapon-factory-allocation-01` traces the successful two-drop
publication hook through the outer factory commit return: two wear/source
writes, relation swaps, serial-range and deferred-queue publication, plus
registry/cell splices. One publication/return has zero allocations from the
main-thread malloc/calloc/realloc/aligned_alloc/posix_memalign entry points. The
same detector catches one deliberate allocation in its positive control.
Normal test ELF SHA256:
`7d243e2d0035e29558a6b7bd2788f8dd5d6e7740c88dcb274ef3496ff7b29145`.
Prepublication validation/resource work, other threads/APIs, production contact
dispatch and gameplay are excluded from that allocation claim.

Retained preflights found and corrected an ESM::Path comparison, an overly
specific injected-scene exception expectation, and the destination identity
guard: native `REFR.mId` is the local RefNum assigned by private registry
preparation, independently of its stable key. Registry revision increases per
inserted entry (two here), even though the group performs one registry swap.
The normal-01 and sanitized-01 helpers were intentionally stopped before fixing
legacy integer/remainder health preservation and adding its regression test.
These attempts and prototype logs remain diagnosis, not final acceptance.
`S4/carried-weapon-factory-review-02` freezes all four implementation hashes and
the complete patch before these documentation updates, with a concrete handoff.

The resource factory is not yet called by the production break/mastery path.
Complete scene admission still needs Lua ObjectLists/OnActive/autostart handling:
`Scene::addObject` normally calls `LuaManager::objectAddedToScene`, whereas this
factory currently admits Objects/Physics/WorldModel only. Multiple prepared
additions need one coherent queue/registry publication. Newly created dynamic
references also need immediate native script-state admission: the script
manager's missing-cache fallback currently relies on static metadata, while
pickup imports only on Take. Preserve unrelated cached state when fixing that
boundary. Initial physical/rendered alignment does not prove falling-object
synchronization or save/recreation; no World/render call site currently consumes
`looseObjectOwners`/`captureLooseObjectRootPose`. Audit and implement those paths
before runtime acceptance. Replacement selection/application, sound follow-up,
retained Player/quest unequips, pickup ExtraScale/dropper/list cleanup, and the
real normal-input break/drop/pickup/fresh-process course remain required. All
broader S4 requirements above remain open; S4 is incomplete.


## Prepared Lua scene admission for grouped weapon drops

Checkpoint **2026-10-16** closes the Lua admission gap identified in the preceding
factory checkpoint. `PreparedSceneAdditions` stages grouped ObjectLists set
nodes, unstarted LocalScripts containers, active-set nodes and capacity for
activation/autostart queues. The factory publishes these within its existing
callback-free reference transaction. Lua preparation neither registers private
references nor attaches scripts; publication executes no modules or handlers.
The normal Lua update starts scripts and delivers activation after registry/cell
publication. Ordinary scene admission retains its existing behavior.

List revisions, RefData/owner identities, registry guards, script bindings,
configuration identity, queue sizes/capacity and runtime selection reject stale
preparations. Intrusive owner tracking revokes unpublished Lua containers before
LuaState/tracker destruction, including competing preparations held past manager
teardown. Retaining, detaching and restoring the original runtime preserves the
selection contract; frozen/incoming selections cannot publish an old plan.

Seven new tests cover grouped membership without premature registry/vector
writes, duplicates and mutation, clear/retention/reference and owner death,
factory activation queue publication, queue mutation/retained selection, actual
two-NPC autostart and callbacks, cancellation and manager death. The real Lua
handler asserts both actors are in `nearby.actors` and saves exactly one
activation per actor across two updates. The fixture initializes the real Lua
manager with its internal libraries, real InputManager, journal, mechanics and
localization services, and a strict window-manager facade. These are headless
engine tests, not normal-input combat or gameplay acceptance.

`S4/drop-lua-admission-normal-03` and
`S4/drop-lua-admission-sanitized-04` each pass all **1544 engine tests/122 suites**,
unfiltered with exact inventory/XML agreement and zero failures/skips. No
sanitizer diagnostic occurred; leak checking is disabled and ASan/UBSan halt on
errors. Both reports preserve identical before/after source state atop
`1a2c5d52dcf1bffc50cc6bee949e9100e53425e4`, with source fingerprint
`9141672667cfd94d65d4e84e62ec2a02344d19651c3ec73b62ec952bb36e38dc`.
The focused sanitizer preflight also passes all 18 selected cases. Component
and Python implementation is unchanged; those suites were not rerun.

`S4/drop-lua-admission-baseline-01` retains the old factory plus the queue
regression only: its one test fails specifically because publication omits the
activation event. The copied baseline ELF is hash-pinned, and symbol inspection
confirms it lacks the new admission method. This is a negative control, not
acceptance. `S4/drop-lua-admission-review-03` freezes the final nine implementation
file hashes and complete patch before documentation updates.

`S4/drop-lua-admission-allocation-03` records one publication/return and zero
selected main-thread allocations in each of two scopes: (1) the actual two-drop
factory publication hook through outer commit return, including Lua membership,
events, source wear/debits, relations, serial, deferred notifications and
registry/cell splices; (2) prepared-list commit entry through outer Lua scene
commit return for two new script containers, including attachments, active-set,
autostart and activation queues. The latter excludes caller registry/cell
publication and later Lua module/handler execution. Both exclude preparation,
other threads/APIs and gameplay. The malloc/calloc/realloc/aligned_alloc/
posix_memalign detector catches one deliberate allocation in its control.
Final normal test ELF SHA256:
`54f21efd4e0dc8af495352f25237b7565f90cb511d9263df3bcda71202d16e03`.

Retained unsuccessful attempts document a compile-time NotNullPtr accessor
mistake, initially uninitialized headless Lua fixtures, missing internal
util/content modules, an incorrect Lua comparison with SelfObject rather than
`self.object`, and missing fixture localization/journal services exposed by
assertions. The journal fix also binds the eagerly required real mechanics
service. Stopped helper runs were terminal before editing; failed attempts and
older-source passes remain diagnosis and do not replace the final evidence.

The factory still needs production break/mastery dispatch. Newly created dynamic
references need immediate native script-state cache admission; Take currently
imports only on pickup. Loose-body frame synchronization and physical save/
recreation remain unwired. Retained Player/quest unequips, NPC replacement and
sound follow-up, pickup ExtraScale/dropper/list cleanup, and actual normal-input
break/drop/pickup/fresh-process courses remain required. The wider power attack,
creature, mastery, reaction/effect, controller and FPS matrix remains open.
**S4 is incomplete.**


## Native script state on fresh factory references

Checkpoint **2026-10-16** fixes the cache-dependent command path for newly
published dynamic references. `OblivionScriptManager::referenceState` previously
required static FormKeyIndex metadata and a compiled reference/base mapping for
any missing cache entry. Fresh factory drops have neither, so `SetDestroyed`
silently wrote no state and `GetDestroyed` continued returning zero.

For a missing dynamic entry, the manager now requires a registered live dynamic
Ptr, captures the existing native runtime-state projection and imports only the
matching reference into its cache. Already-cached state and the static fallback
retain their behavior. Private preparations and unavailable keys are rejected
before snapshot capture. This is lazy admission at the first cache-dependent
script call; the factory publication phase does not perform a snapshot or gain
allocation work. Snapshot work can fail through existing capture validation.
There is no new native arithmetic, command rule or save schema in this change.

The new factory integration test first establishes unrelated cached script
state, publishes an actual carried-NIF weapon drop, and immediately executes
`SetDestroyed`/`GetDestroyed` before pickup. It verifies the command's stored
result, preservation of the unrelated entry, native condition/charge, exact
binary save round-trip and independent subsequent updates. The one-test
`S4/drop-script-admission-baseline-01` run fails on the preceding production
implementation specifically because the dropped reference still reports zero.
Its negative-control report and copied baseline test ELF are retained.

`S4/drop-script-admission-normal-01` and
`S4/drop-script-admission-sanitized-01` each pass all **1545 engine tests/122
suites**, unfiltered with exact inventory/XML agreement and zero failures/skips.
No sanitizer diagnostic occurred; ASan leak checking is disabled and ASan/UBSan
halt on errors. Both preserve identical before/after source state atop
`1cf2569f146bcfd674881f90457660f439d28c05`, with source fingerprint
`ba38535d31527eef45adbe99126b37e3e0094b3280ac770d5ef0ad20a54ff64f`.
`S4/drop-script-admission-review-01` freezes both implementation file hashes and
the complete patch before documentation updates. Component/Python implementation
is unchanged; those suites were not rerun. The Lua/factory publication code is
unchanged from the prior allocation traces; allocation tracing was not repeated
for this command-cache change.

This closes command access to persistent state on a fresh drop. It does not
prove actual gameplay drop/pickup or fresh-process scene reconstruction. Loose
body frame synchronization and physical save/recreation, production break/mastery
dispatch, retained Player/quest unequips, NPC replacement/sound, pickup metadata
cleanup and all broader S4 gameplay/controller/FPS gates remain open.
**S4 is incomplete.**


## Original one-body Havok movement save layout

Checkpoint **2026-10-16** records a bounded original-instruction probe for the
next loose-body persistence integration. The hash-pinned original `4E0970` layout
function and `4E0A40` writer run through full returns with actual callbacks,
REFR/collision/body vtables, NiRTTI, primary lookup, activity and spatial/velocity
getters. Scene traversal supplies one collision object and the byte sink captures
writes; those two boundaries remain stubs. Actual original RTTI startup
initializers run before derived-type queries.

All **160 cases** match independent literal layout/unit expectations bit for
bit, across primary/nonprimary collision objects, null/zero/three nonzero island
activity states, ordinary/identity-corrected BodyT objects, four payload profiles
and both x87 words. The primary collision object's transform is omitted from
this payload; nonprimary bodies emit position plus quaternion, and active bodies
also emit linear/angular velocity. This does not establish where or when the
reference's stored pose is updated. An initial derived-type probe lacked the
original RTTI startup initialization and is retained as a failed setup attempt.

The editable [probe manifest](M15-ORIGINAL-LOOSE-WEAPON-SAVE-PROBES.json) records
functions, inputs, observed layout, exact scope and evidence paths. No new C++
rule or save implementation is introduced or validated by this checkpoint.
Nonidentity BodyT corrections, multiple-body/mixed layouts, actual scene
traversal and original-game save/load remain outside this probe. Engine
loose-body frame/render/logical synchronization and physical save/recreation
remain open, along with the other S4 gates. **S4 is incomplete.**


## Physical scene to reference pose arithmetic

Checkpoint **2026-10-16** resolves the arithmetic dependency for synchronizing a
loose object's scene root with its stored reference. The original `4E2160`
callback performs actual `4DC270` scene-extra reference lookup. Update-mask bit0
copies world translation; bit1 uses `711300` to store reference Euler. Each
selected update records modification masks4 and8. `711300` differs from the
fresh carried-node drop argument extractor `711440`: its pole lies in reference
Y. The existing fresh-drop extractor must not be used as the frame inverse.

`physicalDroppedItemPlacement` implements this separate immutable conversion,
reusing the verified native atan polynomial and preserving signed-zero stores.
It rejects nonfinite matrix/translation inputs. At a pole it supplies valid
angles with a false uniqueness flag, matching the original callback, which
stores those angles regardless of uniqueness. Twenty literal original matrices
cover axes, combined/random rotation and both sides of the Y poles; a second
test checks every input lane's nonfinite rejection.

`S4/drop-reference-frame-oracle-02` executes **3456 contexts** through the full
original callback return, with real REFR/TESObjectExtraData vtables and reference
lookup, actual pi initialization, TLS guards and math functions. Only
MarkAsModified storage is an event-recording stub. A null cell bypasses terrain
repair, and a configured engine gate bypasses cell migration. Masks0/1/2/3,
216 distinct matrices, two positions and both x87 words verify position copies,
mark sequences, unchanged scene inputs, stack cleanup and preserved registers.
The original-01 setup lacked a Windows TLS fixture and faulted in the real guard;
it remains retained diagnosis. Original-02 supplies TLS data without stubbing
the guard. Static dispatch inspection also identifies installation of `4E2160`
at global `BA7A88`, but no actual callback dispatcher/timing acceptance is claimed.

`S4/drop-reference-frame-compare-01` compares the production conversion with
all **864 combined-update original contexts**, bit for bit for rotation,
position and uniqueness, in both normal and ASan/UBSan standalone drivers.
`S4/drop-reference-frame-components-01` and
`S4/drop-reference-frame-sanitized-01` each pass all **2686 component tests/255
suites**, unfiltered with exact inventory/XML agreement and zero failures/skips.
No sanitizer diagnostic occurred; leak checking is disabled and ASan/UBSan halt
on errors. Both preserve identical before/after source state atop
`a849d2f56797756ca0fcfb5522c7f6cfff133427`, with source fingerprint
`1d4265d22d907da28bdf0d5791248a810ad9f85a46d52fd9e8f2b35d6acfdddb`.
`S4/drop-reference-frame-review-01` freezes the three implementation file hashes
and complete patch before documentation updates. Engine/Python wiring is
unchanged and those suites were not rerun for this pure arithmetic addition.
The [editable probe contract](M15-ORIGINAL-PHYSICAL-REFERENCE-POSE-PROBES.json)
records literal cases and exact boundaries.

World still has no loose-body frame consumer. This helper does not implement
body/renderer synchronization, logical cell migration, physical save/recreation,
production break/mastery dispatch or normal-input acceptance. Those and all
other broader S4 gates remain open. **S4 is incomplete.**


## Loose-weapon frame pose publication

Checkpoint **2026-10-16** wires `synchronizeLooseWeaponPoses` into World immediately
after physics actor movement. It accepts only the supplied World's current
physics owner in the Oblivion profile. Before publishing any captured pose, it
checks all loose owners' native WEAP identity, cell/registry membership and
current admitted resource/scale binding through rigid-root capture. It stages
rigid roots and the independently verified physical reference-Euler conversion.

Publication writes RefData position/rotation and the rendered placement node's
position/attitude, retaining reference scale on that node. It keeps the physical
rotation separately from its approximate Euler projection. Interior/same-cell
updates need no rendering-manager service or reference re-registration.
Exterior cell crossings use World's existing `moveObject(..., false)` path for
Ptr/render/physics/script ownership migration. Physics poses, velocities and
packed velocity authority are not rewritten by the frame consumer.

Two new engine integration cases use an actual carried-NIF factory drop with
scale2, real Objects/PhysicsSystem and managed cell/registry ownership. The first
sets initial native velocity and advances a real physics worker step, verifies
movement and gravity while scene/reference are still stale, then verifies their
publication, script `GetPos`, reference snapshot/binary round-trip and unchanged
physical pose/linear/angular/packed velocity state. The second applies an
independent +90 Z global rigid transform to the body, checks the known root
translation (-6,123,65), reference angles and rendered axis, preserves scale2,
and verifies repeated publication does not rewrite physics. A stale reference
scale rejects before reference/node publication and permits retry after restoring
the admitted scale.

`S4/drop-frame-world-normal-01` and `S4/drop-frame-world-sanitized-01` each pass
all **1547 engine tests/122 suites**, unfiltered with exact inventory/XML agreement
and zero failures/skips. No sanitizer diagnostic occurred; ASan leak checking is
disabled and ASan/UBSan halt on errors. Both preserve identical before/after
source state atop `34026b579e016b4b64851e2686819aef9fc64730`, with fingerprint
`2e08f32fbd98a8bd121eeec09662849559b0a23794ca9302acd835927566a699`.
`S4/drop-frame-world-review-01` freezes the four implementation file hashes and
complete patch before documentation updates. Component/Python code is unchanged;
those suites were not rerun for this wiring addition.

`S4/drop-frame-world-bypass-01` uses the same normal test ELF and returns the new
synchronization function at entry exactly once. Its single selected test fails
with exactly three stale-pose assertions: reference position, scene-node position
and `GetPos`, while physics advancement and unchanged-physics checks succeed.
This is a controlled function bypass, not a previous-source implementation build.
Normal test ELF SHA256:
`d1b233a514f184175df0d1d71741d11b9522a4722547899c8861536a064c73a9`.
The earlier compile preflight incorrectly used World's ray-casting interface as
a full physics owner; it was stopped before correcting the API to accept the
explicit owner. The second preflight passed pose/rotation/scale/physics checks
but its GetPos host call omitted the explicit reference target and queried X.
The corrected final test uses a reference target; explicit binary32 translation
conversion also removes new narrowing warnings. Failed/stopped attempts remain
retained diagnosis, not acceptance.

These tests call the real frame consumer directly; the compiled World call site
is installed, but no new normal-input gameplay or exterior crossing acceptance
is claimed. The binary round-trip covers the reference snapshot, not a physical
loose-body save/recreation contract. World physical capture still handles actors
only. Generic PhysicsSystem `updatePosition`, `updateRotation` and `updateScale`
also lack loose-body branches; wire script-driven transforms and live rescaling
before claiming complete related command semantics. The stale-scale guard does
not implement live rescaling. Physical retention/recreation, production break/
mastery policy, replacement/sound, pickup metadata cleanup and the complete
normal-input/restart/controller/FPS matrix remain required. **S4 is incomplete.**


## Loose-weapon position and rotation input

Checkpoint **2026-10-16** adds loose-owner branches to PhysicsSystem's actual
`updatePosition` and `updateRotation` dispatch. Position changes preserve the
current physical root rotation; rotation changes use the supplied quaternion
and current reference translation. The scheduler waits for workers, checks the
admitted native units/reference scale and rigid root, then publishes through
the existing native scene/bodyT offset conversion. Scaled bind translation is
applied once. Neither path replaces a body or changes its motion mode.

Forced publication stages the whole requested batch before mutation, preserves
forces, refreshes current/interpolation poses and broadphase bounds, and uses
an independently verified native velocity rule. For each linear/angular vector
separately, any XYZ absolute value strictly above binary32 `0x3a83126f`
(`.001f`) resets all four lanes to positive zero. Otherwise all four lanes,
including signed zeros and scratch W, survive unchanged. Small velocities are
not unnecessarily converted back through native/world units. The prior
keyframed publication path retains its velocity-preserving behavior.

The editable [forced-scene probe manifest](M15-ORIGINAL-FORCED-SCENE-INPUT-PROBES.json)
pins original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`S4/loose-forced-scene-oracle-03` executes full collision update `897510`,
flag0x40 through `89EE00`/`89EAE0`, actual bhkRigidBody/hkpSphereMotion setters,
real scene conversion and flag clearing in **1,800 contexts**. The fixture uses
real collision/body/sphere-motion vtables, an already active island and no
native World. Only Windows critical-section entry/exit `43F2E0`/`43F300` are
no-op boundaries. Thus original World broadphase/activation is not proved.
Ten signed values, threshold equality/adjacent floats, each linear/angular XYZ
lane, W99/-99 and x87 words027F/037F are covered. Complete return, stack,
callee-saved registers and unchanged scene input are checked.
`S4/loose-forced-scene-compare-01` compares all eight production C++ velocity
lanes against every recorded original result: **1,800 normal and 1,800
ASan/UBSan comparisons, zero bit mismatches**. Original attempt01 faulted at
an uninitialized Windows critical section; its failure is retained. Attempt02
introduced the explicit lock boundaries and passed128 initial contexts;
attempt03 extends that evidence. Those setup attempts are not acceptance.

Two component tests cover threshold/signed-zero/vector independence and
nonfinite lane rejection without input mutation. Six engine cases (two tests
at worker counts0/1/2) use real physics owners. They verify rejection before
mutation, unchanged body identity/forces, preserved tiny angular velocity and
scratch lanes, zeroed linear velocity, refreshed interpolation and actual ray
hits at the new broadphase location with misses at the old location. Actual
PhysicsSystem position/rotation dispatch is checked for ordinary/bodyT bodies,
reference scale2 and independent local bind/scene offsets, using a declared
`.0005f` matrix tolerance. Ownership and reference scale survive.

`S4/loose-command-transform-normal-01` and
`S4/loose-command-transform-sanitized-01` each pass all **2,688 component
tests/255 suites and 1,553 engine tests/122 suites**, unfiltered with exact
inventory/XML agreement and zero failures/skips. No sanitizer diagnostic
occurred; ASan leak checking is disabled and ASan/UBSan halt on errors.
Both preserve the same before/after source state atop
`363972bb4087d788e39a92ed01191d658c195f10`, fingerprint
`9aafa0fab1a6d89aba5f9b893fd673a66492d9003bf800a5917d5ddba7b47793`.
`S4/loose-command-transform-review-01` freezes all nine implementation/test
file hashes and complete patch before documentation updates, plus the two
verified result inventories. The earlier filtered preflight passes all six
engine cases; it is not counted as a full suite. Python is unchanged.

`S4/loose-command-transform-bypass-01` returns actual PhysicsSystem position
and rotation dispatch at entry twice each. Its one selected test fails with
20 stale-pose assertions across ordinary/bodyT owners, using exactly the full
normal run's test ELF SHA256
`3c3bd75156dda6015c687fc2e2ab3182aa14411a42eda9930e6df88f6964db73`.
This is a controlled function bypass, not a previous-source implementation
build. It proves the test detects the missing dispatch.

This supersedes the preceding checkpoint's missing `updatePosition` and
`updateRotation` branches. It does **not** close the entire script command
path: the native host's `SetAngle` still calls World with legacy inverse
rotation order, while original `509110` calls native builder `4D7AF0` matching
the separately verified direct order. Correct and test that command/scene
wiring next. `updateScale` still lacks a loose branch; raw authored properties
must survive preparation to avoid scaling already rounded properties. Physical
save/retention/recreation remains actor-only. Production break/mastery policy,
replacement/sound, pickup cleanup, actual normal-input gameplay and the full
controller/FPS/restart matrix remain required. **S4 is incomplete.**


## Native SetAngle rotation order

Checkpoint **2026-10-16** changes the native script host's `SetAngle` call to
explicit `RotationFlag_none`. World's default inverse order previously routed
native object commands through Scene's legacy X/Y/Z composition. Native
`509110` builds its matrix with `4D7AF0`, matching Scene's direct Z/Y/X order.
The actor-specific node rotation path is unchanged. This supersedes the
preceding checkpoint's known command-order gap, without changing the default
World order or the Morrowind command host.

The editable [runtime probe manifest](M15-NATIVE-SETANGLE-RUNTIME-PROBES.json)
records the six actual native command diagnostics, reference/base/cell identity,
input-save hash and all12 original matrix outputs. The commands address the
resident weapon `content:oblivion.esm:000f1f`, zero X/Y/Z then set X90/Y90/Z30.
`S4/setangle-order-oracle-01` executes full original `4D7AF0` through real REFR
vtable/+190 dispatch with WEAP base, no stubs, for each stored binary32 angle
state and x87 control words027F/037F. Complete return, EAX output pointer,
stack, callee-saved registers and unchanged reference are checked. Original
executable SHA256 is
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
This executes the matrix builder, not the original command argument parser.

`S4/setangle-runtime-baseline-03` uses the preceding normal engine binary.
Read-only GDB observes actual host -> World -> Scene -> PhysicsSystem calls,
stored reference angles, rendered placement-node quaternion and independent
Object rotation storage. The same comparison against original matrices fails
with maximum error **1.5000000504735647**, including combined-axis failures.
This is a real pre-change binary, not a controlled function bypass.

`S4/setangle-runtime-normal-01` and `S4/setangle-runtime-sanitized-02` execute
the corrected host in actual engines with renderer/physics and the exact saved
content dependencies. All six command returns succeed; scene attitudes and
independently observed Object rotation storage agree. The normal observer also
reads the actual PhysicsSystem parameter. Sanitizer debug information omits
both that parameter and Scene's caller local, so the instrumented observer
reads the rendered node at PhysicsSystem entry, then independently checks
Object's stored quaternion and the actual committed Bullet collision basis.
Both runtimes match the original matrices with maximum error
**3.731898345904483e-8**; the final committed collision basis differs by at most
**3.011408766795398e-8**, below the preregistered `2e-6` tolerance. No sanitizer
diagnostic occurs; ASan leak checking is disabled and ASan/UBSan halt on errors.
The observers perform no gameplay writes and deliberately kill the engine after
final collision commit. This is bounded command-path evidence, not ordinary
process-exit, save/restart, audio or normal-input melee acceptance. The resident
weapon uses ordinary Object physics; loose-body bodyT/scaled input dispatch is
covered separately by the six worker cases in `04e84ad0d8`.

Runtime normal executable SHA256:
`1455a65c4dc2e5bb2c47d27a14c35beacfbdc3e27ae875a4bcffcbacf2652409`.
Runtime sanitizer executable SHA256:
`3847c85dff96c357abfa649c4d933a3effb9b402ebe6cdc61b459ff11e33b0e0`.
Provenance files hash the binaries, input saves, command events, exact saved
script dependencies, configs and observer sources. Baseline attempt01 omitted
the saved script dependency; attempt02 supplied a placeholder rejected by the
saved content fingerprint. Both were stopped after confirmed load failures.
Sanitizer runtime01 stopped at the first command because GDB could not read an
optimized-out quaternion; attempt02 adds the explicitly reported fallback and
independent storage/collision checks. Retained failures are not acceptance.

`S4/setangle-order-normal-01` and `S4/setangle-order-sanitized-01` each pass all
**1,553 engine tests/122 suites**, unfiltered with exact inventory/XML agreement
and zero failures/skips. Both preserve identical before/after source state atop
`04e84ad0d8fdf95e6fa7b36e61ba6a5504701cca`, fingerprint
`efb11ba4ec28c3e7089a6d23cb69ab2b29e6521fa41e8d1716927398b86e3dd3`.
The single implementation-file hash and patch are frozen in
`S4/setangle-order-review-01` before documentation updates. Component/Python
code is unchanged and those suites were not rerun for this host-call correction.

Live scaling, physical loose-body retention/save/recreation, production
break/mastery dispatch, replacement/sound, pickup cleanup and the complete
normal-input/restart/controller/FPS matrix remain required. **S4 is incomplete.**


## Native SetScale command limits and live-body observation

Checkpoint **2026-10-16** adds immutable `scriptReferenceScale` and routes the
native script host's `SetScale` input through it before World mutation. Original
command `5093B0` clamps to binary32 **0.5** (`A3D65C`) and **2** (`A379B4`), then
calls reference setter `4DB520` for decimal formatting/parsing. The existing
fresh-drop helper supplies that verified rounding operation; its own wider
`[0.01,10]` source-ExtraScale rule remains unchanged. For example script input 0.125
stores 0.5, script input 0.625 stores 0.63 and script input 7 stores 2, while fresh-drop input 0.125
stores 0.13 and fresh-drop input 7 stores 7. Nonfinite typed command inputs explicitly
reject before World mutation; original nonfinite command behavior is excluded.

The editable [original script-scale manifest](M15-ORIGINAL-SCRIPT-SCALE-PROBES.json)
records 32 independent literal scale outputs, original constants, boundary stubs,
comparison and actual runtime reports. Original executable SHA256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`S4/live-scale-command-oracle-05` executes **488 full original-command contexts**:
122 finite binary32 inputs, both x87 control words027F/037F and ordinary/bodyT
bodies. Cases include signed zero, extreme finite floats, clamp-adjacent values,
exact half cents and a regular interior grid. Argument extraction `4FAE80`
supplies the chosen float at the real output pointer; RTTI `9832E6` supplies the
nonactor result. Actual `4DB520`, original CRT sprintf/atof and buffer getter,
NiNode vtable `A7E38C`, two complete `707370` updates and ordinary/bodyT dynamic
collision/motion dispatch execute. Windows thread/locale/error imports,
critical-section no-ops and change-record capture are declared boundaries.
The final probe removes the preliminary buffer-provider stub.

The ordinary/bodyT fixture has nonzero bodyT quaternion at+20 and translation
at+30, native sphere-motion type 2, no native World, an already active island
and an undereferenced poisoned shape sentinel at hkp+14. All **20KB of body,
hkp, motion, island and sentinel storage remain byte-for-byte unchanged**.
Reference/node scale updates correctly. Actual final REFR virtual+90 is
`4D6DB0` -> `46C640`, which marks the reference altered (temporary flags 0x4000
become 0x4002). It is not a body-rebuild request. This corrects the earlier
handoff assumption about that virtual slot.

`S4/live-scale-command-oracle-06` repeats all 488 contexts with collision flags 0x5,
actual global callback `BA7A88=4E2160`, actual TESObjectExtraData/REFR lookup
and the original TLS mutation guard. The collision target is the root NiNode;
null reference cell and engine config+34=1 skip terrain/cell migration. Body
storage still remains byte-identical; reference position/rotation follow the
physical target and modification masks 4/8 are captured. The scale results are
identical to probe 05. Full ALtrue return, stack, callee-saved registers, FPCW,
base and reference-outside-pose/scale/altered flags are checked. Original argument
parsing, attached native World broadphase/activation, hierarchy and actor/fixed/
keyframed motion branches are not proved by these fixtures.

`S4/script-scale-compare-01` compares production C++ against all 488 original
scale outputs in normal and ASan/UBSan executables: **zero bit mismatches**.
Two new component tests cover 32 literal native outputs, the distinction between
script/drop ranges and nonfinite rejection. Earlier bounded scene/command
probes remain retained development evidence. Command01 covers 22 plain-body
contexts;02 adds bodyT rotation but placed translation in an unused slot;
03 corrects translation+30;04 extends to 488;05 executes the real buffer getter;
06 enables the actual physical reference callback. Only05/06 support the final
claims above.

`S4/script-scale-runtime-baseline-01` pins the preceding normal engine binary
and runs six actual `SetScale`/`GetScale` pairs on resident weapon
`content:oblivion.esm:000f1f`. Inputs 0.125/0.625/7/-2/1.375/1 reach World,
reference storage, rendered node and query unchanged. The independent original
comparison fails exactly the first five cases. This is a true pre-change binary,
SHA256 `1455a65c4dc2e5bb2c47d27a14c35beacfbdc3e27ae875a4bcffcbacf2652409`.
`S4/script-scale-runtime-normal-01` and `-sanitized-01` repeat the same sequence
with the corrected host. All six stored reference and node scale values match
original binary32 outputs; all twelve command returns complete and `GetScale`
agrees. The read-only GDB observer captures actual World arguments where debug
information permits, native reference storage and all three rendered scale
lanes after World returns. The launcher deliberately SIGKILLs the engine after
all command returns, so this is not normal exit/save/restart acceptance.
Input-save SHA256 is
`798f1c48d404650099618c213a5f5bd793d19f647ea2be86b19d537ade42b66a`.
Each course pins binary, commands, private save, exact saved dependency scripts,
configs, observer and launcher hashes. Corrected normal engine SHA256:
`e2dd93b947cbb3825f25b6b957726b2a5551d4a64390842568e4eba05c43b12a`;
ASan/UBSan engine SHA256:
`69f3aa2b6dc7a3271335e105afc2b88208ec67af5e3de80bbfd49daf4144a107`.

`S4/script-scale-normal-01` and `S4/script-scale-sanitized-01` each pass all
**2,690 component tests/255 suites and 1,553 engine tests/122 suites**,
unfiltered with exact inventory/XML agreement and zero failures/skips. Both
preserve identical before/after source state atop
`9119c3db8713c9280f647d31b31d7323fbf99dde`, fingerprint
`5a6acdfa0636bbe53b969293b8b05ab0f7586cc12c4fb411b144f6ffd17138bc`.
No sanitizer diagnostic occurs; ASan leak checking is disabled and ASan/UBSan
halt on errors. Four implementation/test hashes and the complete patch are
frozen in `S4/script-scale-review-01` before documentation updates. Python is
unchanged and its suite was not rerun.

This delivers the command-input arithmetic and host wiring. PhysicsSystem still
ignores loose-owner scale updates, and subsequent loose pose capture rejects a
changed reference scale. Separate visible reference scale from admitted
physical scale using the verified body-preserving behavior; first resolve
parent/child NIF/reference callback behavior and subsequent forced transforms.
Ordinary Object collision resizing is also outside this command/node runtime
acceptance. Physical save/retention/recreation, production break/mastery policy,
replacement/sound, pickup cleanup and the full normal-input/restart/controller/
FPS matrix remain required. **S4 is incomplete.**


## Loose-weapon physical node bindings and native hierarchy evidence

This prerequisite retains mutable TES4 collision-target nodes through NIF
loading/optimization and binds each owned factory model to its actual hashed
NIF nodes. The passive ObjectAnimation binding preserves embedded controller
traversal. It provides per-node physical projection without moving the outer
reference placement or mutating the carried/cached model. Prepared and published
model validation now checks the binding's asset, record, bone and parent-path
identities, exact local matrices and separately cached binary32 NIF fields.
The publication guard performs no transform writes or heap allocations.
Unsupported transforms are rejected on physical ancestor paths; unrelated effect
branches remain admissible and untouched.

Independent original-executable hierarchy probes execute full SetScale with a
physical child beneath a separately scaled parent. Forty-eight contexts cover
three input scales, both x87 precision words, ordinary/bodyT dynamics and
collision flags 1/5/9/13. Bit8 controls child-local writeback; bit4 enables the
actual physics-to-reference callback. The final reference position equals the
last callback target selected by its change mask. In six contexts it differs
slightly from the final recomposed child world position, so publishing the final
node unconditionally is incorrect. All 20KB of admitted physical state remain
byte-identical. Existing production local projection matches all 13 binary32
local-transform lanes in all 48 cases in normal and ASan/UBSan comparisons.

These are instruction and factory/resource/controller integration checks, not
ordinary-input gameplay acceptance. The current World consumer still derives a
placement-root pose and does not implement child callback-mask semantics.
Visible scale versus admitted physical scale, native root normalization,
subsequent forced transforms and loose save/recreation remain open. Production
break/mastery, pickup/replacement/sound and the complete S4 normal-input matrix
remain open. S4 is incomplete.

The editable [hierarchy/binding manifest](M15-ORIGINAL-LOOSE-SCENE-HIERARCHY-PROBES.json)
records the original executable identity, fixture matrix, four curated original
outputs, exact production hashes and all verification layers. Evidence root:
`build/oblivion-compat/m15/S4`.

- `live-scale-hierarchy-oracle-03`: 48 original contexts, including original
  Euler and CRT initializers, actual callback masks and full return/invariant
  checks. Argument extraction, nonactor RTTI, change-record capture, Windows
  imports/TLS and critical sections are declared boundaries. No native World;
  island already active; null cell/config skips terrain/cell migration.
- `live-scale-hierarchy-compare-01`: normal and ASan/UBSan production projection
  comparisons, 48 contexts each, zero mismatched lanes. Adapter inputs are the
  original parent/physical-child poses; body conversion is excluded.
- `loose-scene-binding-normal-02` and `loose-scene-binding-sanitized-03`: all
  **2,694 component tests/255 suites and 1,556 engine tests/122 suites** each,
  exact inventory/XML agreement, no failures/skips or sanitizer diagnostics.
  ASan leak checking is disabled. Both preserve identical before/after source
  fingerprint `2bcaf7e37f4296ef132141d5efa9498504d67c598ca43b06023b787b92952a49` atop
  `6b59a22b7e78e87b2917550688cb6396707832c2`. The 11 implementation/test-file
  hashes and patch were frozen in `loose-scene-binding-review-03` before these
  documentation updates.
- `loose-scene-binding-allocation-01`: final normal test ELF, 29 exact guard
  entry/return pairs including both Boolean results, zero observed allocations.
  The two-drop publication-through-factory-return trace also observes zero.
  Both measure main-thread malloc/calloc/realloc/aligned_alloc/posix_memalign;
  other APIs/threads and preparation are excluded. The positive control detects
  one deliberate allocation. A controlled GDB return-true bypass (37 calls)
  makes the rollback test fail with actual unwanted publication/debits; the
  independent detached-model guard still rejects that fifth mutation.

Retained preflight01 failed to compile a mixed unsigned/enum initializer list;
explicit `std::array<unsigned,2>` fixed it. Preflights02–04 were stopped and
awaited before admission/controller refinements. Preflight05 used the fixture's
unchanged translation(1,2,3) as a supposed mutation, so its rollback assertion
failed; the corrected test mutates to(7,8,9). Sanitized01 passed all 2,693
components, then its engine build was stopped and awaited before that test fix.
Normal01 and sanitized02 subsequently passed all 2,693 components/1,556 engine
cases before the final effect-branch refinement. All failures/stops remain in
place. Earlier hierarchy probes01/02 are exploratory; review02's analysis
control preserves the rejected assumption that the stored reference must always
equal the final recomposed child position. Stronger probe03 finds six exceptions
and verifies the exact last mask-selected callback target instead.


## Native reference root replacement for factory drops

The owned drop factory now loads a separate reference-placement definition from
exactly the carried NIF bytes. It omits the authored single asset-root transform
when computing loose body bind matrices. Body/joint data and descendant local
transforms remain authored. The carried graph is unchanged. Renderer loading
retains a mutable root for TES4 assets containing collision targets, including
physical children, so optimization and cloning preserve a private per-instance
root. ObjectAnimation replaces that root with identity while the outer node
owns reference position, rotation and scale. It then creates its passive
physical-node binding and local publication checkpoint.

A new real-NIF factory course includes a quarter-turned physical child at(3,4,5)
under the authored root at(1,2,3). With reference scale2 it verifies child world
translation=reference+(6,8,10), preserved child rotation, matching Bullet body
position and unchanged cached root. Direct-root admission verifies exact root
identity and initial rendered target=reference placement. Physics round trips
use the declared1e-4 world-unit tolerance. The preceding implementation fails
the exact direct-root assertions with target(105,20,41) versus reference
(103,16,35).

Independent original instructions4E4244..4E426E execute the actual reference
position copies and full4D7AF0 rotation builder with actual nonactor REFR
virtual dispatch. All36 contexts cover three reference rotations, two positions,
three different authored roots and both x87 precision words. The first12 local
root floats depend only on the reference pose; child/reference storage is
unchanged and stack balance holds. No function hooks are used. Scale stays
unchanged in this particular prefix; the separately recorded4E43C8..4E43E9
prefix later overwrites it with the reference scale. This is not a full
GenerateNiNode/resource/physics-registration or normal-input gameplay course.

World still needs per-node frame projection and masked reference publication;
visible scale versus admitted physical scale, forced-transform continuation,
loose save/recreation, production break/mastery, pickup/replacement/sound and
all normal-input/restart/controller/FPS acceptance remain open. S4 is incomplete.

The editable [reference-root manifest](M15-ORIGINAL-REFERENCE-ROOT-PROBES.json)
contains the executable/hash identities, curated original root outputs,
implementation hashes, exact inventories, allocation scope and retained attempts.
Evidence root: `build/oblivion-compat/m15/S4`.

- `loose-reference-root-oracle-01`:36 original root-prefix contexts, no hooks.
  The original executable SHA256 is
  `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
- `loose-reference-root-normal-04` and `loose-reference-root-sanitized-03`:
  **1,557 engine tests/122 suites each**, unfiltered with exact inventory/XML
  agreement, no failures/skips and no sanitizer diagnostics.
- `loose-reference-root-normal-05` and `loose-reference-root-sanitized-04`:
  **2,696 component tests/255 suites each**, with the same complete verification.
  All four reports preserve identical before/after source fingerprint
  `09e861582aab284fe43c8705b48d86ccab3e61c9dcf18e4f5592515727d177ae` atop
  `9217ea18ca17d15cd925d26bdc0f62729d381e33`. The ten implementation/test-file
  hashes and patch were frozen in `loose-reference-root-review-04` before these
  documentation edits. ASan leak checking is disabled.
- `loose-reference-root-allocation-01`: final normal engine-test ELF, one
  two-drop publication-through-factory-return interval, zero observed main-thread
  malloc/calloc/realloc/aligned_alloc/posix_memalign calls. Preparation, other
  allocation APIs/threads and gameplay are excluded. The detector's prior
  positive control is retained in `loose-scene-binding-allocation-01`.

The same hash-identified original image also passes32 registration-handler
contexts in `loose-registration-oracle-03`: actual RTTI initializerA12200,
88A7D0 hierarchy visitor, collision lookup4A05E0, reference callback4DE010 and
88A870 handler. The first-context bit adds0xC to the first discovered collision;
subsequent collisions clear0x4 and retain existing0x8. A synthetic World getter
returns null matching each body World, so no native insertion/activation occurs;
BA7A8C=3 suppresses collision dispatch during node world composition and optional
reference-special/debug gates are zero. This establishes the selected handler
branches, not production caller context initialization or live registration.

Retained baseline01 fails the new exact renderer root/target assertions on the
preceding implementation. It also exposed an inappropriate exact physics
round-trip assertion, corrected to the declared1e-4 tolerance. Normal01 was
stopped and awaited before that test correction. Normal02 failed component test
compilation because `Nif::Matrix3` is not aggregate-initializable; explicit
fixture entries fixed it. Sanitized01 was stopped and awaited first. Normal03
passes all2,696 components and1,556/1,557 engine cases; the new child test
incorrectly converted an already-world-unit expected position again. The actual
body and renderer both have(109,24,45). Sanitized02 passed all2,696 components,
then was stopped and awaited before correcting that test-only expectation.
Fresh final component reports also establish complete before/after fingerprints.
Exploratory registration probes01/02 failed because their synthetic fixtures
omitted native RTTI initialization; failure records remain alongside the scripts.
Probe03 runs the actual initializer and passes all32 contexts.

## Native child collision relocation during reference registration

The live drop definition now follows the original nonactor reference attach
selection: retain a root collision; otherwise inspect literal first and then
optional second child slots. It does not search recursively or skip a null
first slot. A selected dynamic child collision becomes a root-targeted bodyT
wrapper. Root-local reference placement and visible child local transforms
remain unchanged. Fixed/keyframed selected children retain their original target;
this is definition selection, not new live fixed/keyframed mode support.

The new immutable offset adapter follows the original old-quaternion frame,
authored-matrix point products, x87 matrix-to-quaternion extraction and old*child
quaternion product. Plain wrappers use the identity old offset, discarding unused
plain offset metadata; bodyT wrappers preserve the native old offset frame.
The previous ordinary scene/body helper is not equivalent: it fails 150 of the
288 relocated comparisons. The new adapter matches every XYZW/XYZ output bit.

The reference definition retains the source child record/name as validation
metadata. Renderer bindings keep its parent path and local checkpoint without
treating it as the physical body. This prevents a changed source translation,
cached signed zero, name or attachment from passing model admission after the
physical target moves to the root. An unnamed reference root can bind through
its exact record/hash identity; ordinary unnamed actor targets still reject.
The carried definition and parsed/cache geometry remain authored.

Actual factory integration verifies the root-target/bodyT definition, preserved
quarter-turned child geometry, matching initial Bullet/body placement, and a
stepped falling/rotating body followed by existing World root publication without
physics feedback. Source-child admission faults roll back before source wear,
debits, serial, registry or physics publication. The real NIF fixture now includes
BSXFlags2, matching the Havok-enabled original registration course.

This corrects the earlier inference that a newly admitted selected-child weapon
necessarily retains a child collision target. The earlier hierarchy SetScale
probes explicitly omitted registration; their synthetic child/flag cases remain
valid, but do not establish post-registration target identity. The original
registration moves the selected dynamic child collision to the reference root.
Unselected/deep targets and other native admission branches still need their
own complete runtime semantics.

The complete original probe executes 4E1FD0..4E20E6 through real bhkWorld vtable
A95CF4/+94=88C740, the entire registration method, hierarchy visitor,
reference/generic registration handlers and actual collision reassignment.
It covers 576 contexts: root/child*plain/bodyT*two x87*four child rotations*three
positions*three old quaternions*two old translations. 288 contexts relocate.
Root BSXFlags2 and actual RTTI initializers are supplied. The World getter returns
its actual null native World, matching body World; there is no insertion or
activation. BA7A8C=3 suppresses collision scene dispatch during world composition;
optional reference-special/property-copy/debug gates are zero. Fresh wrapper /
Havok allocations and owned/null frees, Windows atomic reference counts,
critical sections and thread ID are declared boundaries. Constructors, math,
target reassignment and dispatch execute original instructions. The full method
return, callee saves, stack, FPCW and prefix stop are verified. Visible locals and
REFR storage remain unchanged; native motion changes are recorded, not assumed
absent. This is not a real native World, property-copy scale, full attachment,
actor/fixed/keyframed or normal-input gameplay acceptance course.

World still publishes poses unconditionally and the scheduler still ties visible
scale to admitted physical scale. Havok-enable/special reference/cell admission,
unselected/deep target behavior, forced-transform continuation, loose physical
retention/save/recreation, production break/mastery, pickup/replacement/sound
and the full normal-input/restart/controller/FPS S4 matrix remain required.
S4 is incomplete.


Evidence is under `build/oblivion-compat/m15/S4/`:

- `reference-registration-selection-oracle-01`: 16 literal selection contexts.
- `reference-root-relocation-oracle-10`: full combined reference prefix and
  registration, 576 contexts / 288 relocations, with declared boundaries above.
- `reference-collision-retarget-compare-01`: actual normal and sanitized
  component libraries, 288 seven-lane bit-exact comparisons each.
- `reference-collision-retarget-normal-01`: all 2,703 component tests / 256
  suites and 1,559 engine tests / 122 suites, exact inventory/XML agreement.
- `reference-collision-retarget-sanitized-01`: all 2,703 component tests.
  `reference-collision-retarget-sanitized-02`: all 1,559 engine tests. No
  failures/skips; ASan leak checks disabled, UBSan halts. Component coverage is
  reused after an engine-fixture-only BSXFlags change; review03 records that all
  component production/test file hashes are unchanged.
- `reference-collision-retarget-baseline-03`: prior committed ELF observes
  child record1/plain, contradicting the independent root0/bodyT result. Its
  owned input is amended with BSXFlags2 before VFS opens it; no post-load
  gameplay property writes. `reference-collision-retarget-runtime-02` inspects
  the final ELF and hashes its unchanged input before opening: byte-identical
  626-byte Havok-enabled fixture, root0/bodyT, source record1, exact seven offset
  lanes, two actual loader returns. This is engine integration, not normal-input
  gameplay acceptance.
- `reference-collision-retarget-allocation-01`: factory publication and exact
  local guard traces; scope and counts are in the editable manifest. The
  unchanged allocator detector's prior positive control remains in
  `loose-scene-binding-allocation-01/control-verification.json`.

Normal and final sanitized engine checks freeze source fingerprint
`a1e1260314b99c572e7470172000df4bd3d4f8fd591866869aa01c3647f89119`
on parent `035e50cd02f7b304b9fb8b8b0d66a5ace66d00f5`. Sanitized component
fingerprint is `a4ae309ad08c708418f7a64c0c99cbda4c15867341b24d8cb9bbb4a85fc93f4b`;
only the engine fixture changed afterward. Final normal engine ELF SHA256 is
`91bae2142db7306ad77f4c455511cdff5e4ca2c0ae2f21a032214d85647bc0a6`.
Final sanitized engine ELF SHA256 is
`c33dc1ae02a37437e98b938a6c21417eb3fdda9a8d115b2c2e2e092264e16f59`.
Documentation follows those frozen checks; implementation hashes are retained.

Failed attempts are retained. Preflight01 incorrectly used a default unresolved
NIF pointer as a null slot; preflight02 uses an explicit null and passes. Earlier
original-registration attempts expose missing Windows/allocator/free boundaries
and an invalid unchanged-motion assertion; successful probes record the actual
motion changes. Baseline01 hit a GDB attribute collision, baseline02 lacked
BSXFlags2, and baseline03 corrects that fixture. Earlier comparison attempts
retain include/range-loop/RPATH errors and the ordinary helper's 150 mismatches.
Runtime01 did not preserve input bytes before fixture cleanup; runtime02 adds
read-only pre-open hashing. No failed attempt is reclassified as acceptance.

[Editable provenance](M15-ORIGINAL-REFERENCE-COLLISION-RELOCATION-PROBES.json)
records hashes, scope, literal examples and remaining gates. Full S4 remains
active and incomplete.

## Registered loose-root visible scale and admitted physical state

Registered one-body root targets now permit a finite positive visible reference
scale that differs from their admitted physical scale. Both physical root capture
and subsequent forced root transforms retain the owned admitted bind/bodyT
translation, shape, mass and inertia. The renderer placement can change scale
without rebuilding physics or rejecting the next World frame. Non-root targets
retain their previous guard until per-node local/callback projection is integrated.
Invalid visible scales still reject before physical mutation.

The real-NIF factory test covers direct-root and selected-child relocation,
visible scales .63/1/1.38, unchanged child local matrices, the newly scaled child
world position, unchanged root-local identity and body/packed-velocity state.
It explicitly performs the visible reference/PAT half of Scene's scale update,
then actual PhysicsSystem::updateScale and normal World publication. It does not
call World::scaleObject or prove normal script input for a dropped owner.
Parameterized scheduler cases cover ordinary/bodyT targets and all 0/1/2 worker
configurations, preserving body/shape identity, mass, inertia, bounds, forces and
packed velocities, then forcing a rotated/translated root after scaling with the
admitted offset. Invalid scale and unimplemented child-target changes reject.

Original registration sets flags 0x4D, including pending forced-sync bit 0x40.
The first actual 707370 scene update executes that scene-to-body synchronization
and clears 0x40, leaving 0x0D. A settled physical pose fixture is installed and
pulled through a second original scene update before freezing physical storage.
The complete original SetScale command then executes192 contexts across root /
child registration sources, plain/bodyT wrappers, two old offsets/quaternions,
two child rotations, both x87 words and three sequential scales. Every admitted
wrapper, Havok/motion/island/shape byte stays unchanged across those scale
commands; visible child locals and root rotation/translation remain unchanged.
No reference callback fires in this settled course (zero change mask).

Actual original constructors, registration, math, collision retargeting, scene
updates, SetScale rounding and physics-to-reference callback dispatch execute.
Allocation/free, command argument extraction4FAE80, command nonactor RTTI
result9832E6, Windows import/TLS services, critical sections and change-record
capture are declared boundaries. Original CRT initialization, sprintf/atof and
scale formatting execute. Null native World and null cell/config
skip broadphase/activation/migration. This is not complete attachment, the
property-copy scale branch, ordinary input or gameplay acceptance.

The existing production inverse bodyT, scene matrix and root writeback helpers
match all 192 original root local R/T/scale outputs plus zero mask bit for bit in
normal and ASan/UBSan component-library diagnostics. The same registered/scaled
course continues through 64 forced requests; existing forward scene/bodyT
conversion matches every resulting native XYZ/XYZW bit in both builds. The
request fixture sets root R/T and bit 0x40; this does not prove SetPos/SetAngle
command dispatch. Components are unchanged
from b837f6b6f1; their full passing 2,703-test evidence is reused from the preceding
checkpoint rather than rerun. Current engine results are recorded separately.

Earlier registered-root-scale probes01/02 fail the incorrect assumption that
physics stays unchanged while registration's pending force flag is still set.
Probe03 records original motion writes through897510/89EE00 and proves that
pending branch; probe04 consumes it via the actual first scene update before
establishing settled scaling invariants. Baseline01 could not launch the retained
ELF because copyfile omitted execute permission; baseline02 restores permission
without changing bytes and observes the original scheduler scale rejection.
All attempts are retained.

A separate 28-context full-registration topology audit confirms the next
integration distinction: root/first/second selected targets receive flags 0x4D;
unselected deep/null-first cases retain child targets with flags 0x41. Those flags
are recorded from actual registration, not synthesized callback profiles. After
pending bit 0x40 is consumed, their masks differ 0x0D versus0x01; do not apply the
root callback behavior to every physical descendant. Visible cached-world versus
local representation and non-root reference publication remain required work.

An additional 192-context original course introduces a small physical X movement
5->5.0005 native lengths after settling and before scaling. Both real scene
traversals leave REFR pose unchanged with no callback, while the final cached
root world position differs from REFR. This directly disproves unconditional
reference publication even for a registered root. Current World publication
still needs that correction; this chunk delivers scale/admitted-physics
separation, not complete root callback semantics.

Remaining work includes native callback-mask/cached-world publication, non-root
physical targets, actual World/script scale/forced-transform continuation,
Havok-enable/cell/property-copy admission, loose physical save/recreation,
production break/mastery and the entire required normal-input/restart/controller/
FPS S4 course. This checkpoint does not deliver complete SetScale semantics or
M15S4. The full goal remains active and incomplete.


Evidence under `build/oblivion-compat/m15/S4/`:

- `registered-root-scale-oracle-04`: 192 original registered/settled scale cases.
- `registered-root-scale-forced-oracle-01`: the same scale cases and 64 forced
  continuations; `registered-root-scale-forced-compare-01` matches all seven
  native pose lanes in normal and sanitized libraries.
- `registered-root-scale-compare-01`: all 13 root local float lanes and zero
  change mask match in all 192 normal and sanitized comparisons.
- `reference-registration-topology-oracle-01`: 28 full-registration contexts
  across seven one-body topologies, two wrapper types and both x87 words.
- `registered-root-small-motion-oracle-01`: 192 original counterexamples to
  unconditional reference publication; the remaining World correction is open.
- `registered-root-scale-baseline-02`: read-only prior-ELF throw observation on
  the existing positive-scale factory case; exact inventory/XML agrees.
- `registered-root-scale-runtime-01`: final-ELF read-only inspection of 14 actual
  owned `sceneBind` calls/returns; admitted scale2, visible scales2/.63/1/1.38,
  root record0, identity bind throughout, exact one-case inventory/XML.
- `registered-root-scale-preflight-01`: 63 selected loose scheduler/factory
  engine cases; `registered-root-scale-normal-01` and
  `registered-root-scale-sanitized-01`: all 1,563 engine tests / 122 suites each.
  Exact inventory/XML agrees, with no failures/skips; ASan leak checks are off,
  ASan/UBSan halt on errors. Existing unrelated compiler warnings remain.

The engine checks preserve source fingerprint
`690a980d63b38678a135104e579345f600ec847b9949411f4ef947e72c36e657`
on parent `b837f6b6f1bafc2bcb9d386ea5a884949d761937` before documentation.
Normal engine ELF SHA256:
`b91fd395343d19278e1a8aaef90e1e61fe3f65575dc3f432dbb229c7883fe358`.
Sanitized engine ELF SHA256:
`59217f9fcfa5ee064f8e53f32096342e0a62f4f8607c1fd977aca76934860777`.
The review directory freezes implementation hashes and exact patch. Its component
reuse report verifies no component source/test changes relative to that parent;
prior full normal and sanitized 2,703-component-test checks retain their own
fingerprints and are not reported as new runs.

[Editable provenance](M15-ORIGINAL-REGISTERED-ROOT-SCALE-PROBES.json) records
original executable/corpus hashes, curated forced cases, actual-ELF observations,
engine checks, retained failures and remaining gates. Full S4 stays active and
incomplete.

## Registered root traversal and masked reference publication

Owned active registered root-targeted drops now retain a native local pose separately
from the scene's cached world output. One normal scene traversal first composes
root world from its previous local, then performs collision writeback. The
native .01 position/.001 rotation thresholds select the world/callback mask;
local rotation/translation can advance when that mask is zero. The next frame
can therefore display a changed world transform without changing REFR storage.
World now updates only the reference position/rotation fields selected by the
mask, and renders the separately projected cached world. It considers exterior
cell migration only when the position mask is set. Euler storage and visible
physical rotation remain independent.

The loose physics owner retains this local state across frames and owner
transfers, and destroys it with the body. Frame capture is read-only; World
validates all borrowed references and projected frames before committing locals
and before any cell migration observers. Invalid frame/scale data do not advance
locals. Forced root transforms establish the next local pose after physical
synchronization. Removal/readmission starts fresh, without reusing old locals.
The existing authored-bind path for unselected/non-root targets is unchanged;
its unconditional publication and per-node cached-world semantics still require
integration. Root metadata is not a universal callback profile.

The original course executes full reference registration and consumes its
pending first scene-to-body update, then establishes a settled motion fixture.
Eight subsequent requests per context each execute one full original 707370 scene
traversal, real collision/bodyT reverse math and actual 4E2160 REFR callback.
There are 512 cases across 64 sequences: root/child registration sources,
plain/bodyT wrappers, two old offsets/quaternions/child rotations and both x87
words. They cover all four callback masks: 320 zero, 64 position-only,24
rotation-only and 104 both. Every helper entry's previous root world equals its
previous local, proving the root composition order. Root local/world outputs and
independently masked reference fields are recorded; physical state and child
locals remain byte-identical per traversal.

This uses motion position/quaternion input fixtures, not actual Havok dynamics.
Native World/cell are null and configuration skips migration; allocation/free,
Windows imports/TLS/critical sections, command argument/nonactor RTTI suppliers
from retained initialization and change-record marks are declared boundaries.
Constructors, registration, scene traversal, writeback and reference callback
execute original instructions. The original desired-transform scale slot is
uninitialized and unused; comparisons canonicalize it to 1 for the typed adapter.
Full game-frame dispatch, actual native World broadphase and ordinary gameplay
are not established by this instruction course.

Existing production bone-pose writeback plus physical reference Euler conversion
matches every local/world float lane, callback mask and six masked REFR lanes in
all 512 cases in normal and ASan/UBSan libraries. No component sources or tests
change in this chunk, so prior full component regression coverage is reused.

New engine tests exercise actual owned factory drops with direct and relocated
roots. Small physical changes retain stored REFR fields while the renderer cache
lags the local by one traversal. Later position-only updates preserve deliberately
distinct reference angles; rotation-only updates preserve deliberately distinct
stored reference coordinates. Body pose and packed velocities remain unchanged
by publication. Parameterized scheduler tests across 0/1/2 workers verify that
capture/rejected commit do not advance state, successful commit does, forced
transforms reset it and removal/readmission discards it.

Preflight01 retains 67 passing cases and one outdated scale assertion failure:
it demanded copying subthreshold inverse-transform rounding residue into REFR.
The corrected assertion checks exact preservation of the initial stored pose;
no numerical tolerance is relaxed. All new frame/mask cases already passed.

Pre-change actual-ELF observation uses an owned NIF input whose linear damping is
changed 0->39.6 before VFS open. The existing real falling-body test performs a
.025s physics step and World publication. Physical root position changes from
(103,16,35) to approximately(103.003494,15.999998,34.996780), below the independently
verified native callback threshold in every lane, yet the preceding World copies
it to REFR. The existing old test passes while the independent observer confirms
the bug. No post-load gameplay fields are injected by the observer. Earlier
baseline01 changes max linear velocity but native loading fixes it at 250, so the
step remains large and is not a counterexample; baseline02 damping59 suppresses
motion and hits the test's positive-X precondition before World publication.
Both failed attempts remain retained.

Additional original probes delimit the next integration work. Full registration
and four subsequent traversals cover 64 unselected deep/null-first child cases:
flags 0x41 become 0x01 after the initial forced update, child cached world follows
physical R/T, while child/root locals, entire REFR and physical state stay
unchanged. A 1,024-frame authored-flags 0/1 cross-check confirms that inactive
roots (flags 0x0C after initial synchronization) skip node writeback/callbacks,
whereas active roots use 0x0D. The current definition still lacks authored
collision flags; preserving that metadata and dispatching inactive profiles is
required next. Root identity alone does not establish active callback semantics.
The active subset reproduces the preceding 512-case corpus exactly.

Remaining gates include authored collision flags/inactive root dispatch,
unselected/deep target per-node and callback semantics,
actual World/script scale/forced-transform continuation, complete native
Havok-enable/special-reference/cell/property-copy admission, loose physical
retention/save/recreation, production break/mastery and pickup/replacement/sound,
power/creature/reaction/effect/controller integration, and the entire normal-input,
FPS and windup/block/recovery restart S4 matrix. Full S4 remains active and
incomplete; these are bounded instruction and engine integration checks.


Evidence under `build/oblivion-compat/m15/S4/`:

- `registered-root-frame-oracle-01`: 512 original one-traversal frame cases.
  `registered-root-frame-compare-01`: every local/world/mask/reference lane
  matches bit for bit in both normal and sanitized component-library diagnostics.
- `registered-root-frame-baseline-03`: retained preceding engine ELF, owned
  pre-open NIF damping input and actual .025s physics step; independent observer
  verifies incorrect reference publication below native thresholds. Its one
  existing engine case passes with exact inventory/XML agreement.
- `registered-root-frame-runtime-01`: final-ELF read-only inspection of eight
  actual factory frame returns, mask sequence `[0,0,0,1]` twice, local/world
  position splits and unselected reference fields preserved. Its one selected
  engine case passes with exact inventory/XML agreement.
- `registered-root-frame-normal-01` and `registered-root-frame-sanitized-01`:
  all 1,568 engine tests / 122 suites each, exact inventory/XML, no failures or
  skips. ASan leak checks disabled; ASan/UBSan halt on errors. Existing unrelated
  compiler warnings remain. Component sources/tests are unchanged relative to
  the parent; prior full 2,703-case normal/sanitized passes retain their own
  fingerprints and are reused, not reported as new runs.
- `unselected-child-frame-oracle-01`: 64 original child-frame cases delimit
  still-open per-node projection/reference semantics.
- `registered-root-active-flags-oracle-02`: 1,024 original active/inactive frame
  cases. `subset-verification.json` proves its 512 active cases exactly reproduce
  the earlier corpus; 512 inactive cases skip helper/callback. The preliminary01
  has identical cases but an ambiguous inherited scope description;02 corrects
  that description and records a fresh source hash/run.

Current engine checks freeze source fingerprint
`11a8fa806522277f80b238e422f8b9b2c68a27c8eacd568e5d3cd0fe756d743d`
on parent `e667cf31dccfbee1c0c2bb94dd2c9e8086608a47`, before documentation.
Normal engine ELF SHA256:
`cbb76f9c15767fa84ecc7756b9a5b558fcf3116b51df8a31d8f1793aafe74bbb`.
Sanitized engine ELF SHA256:
`48c944a825a3d6352a46d293d95fefd0662957047b8c0550aa805aee95493358`.
Review01 preserves the failed preflight implementation/test snapshot; review02
freezes the final seven implementation/test hashes and exact patch. No sources
changed during authoritative checks.

[Editable provenance](M15-ORIGINAL-REGISTERED-ROOT-FRAME-PROBES.json) records
hashes, boundaries, literal sequence values, current checks, retained failures
and remaining gates. Full S4 remains active and incomplete.

## Authored collision flags and inactive registered roots

`RagdollBodyDefinition::mCollisionFlags` owns the authored uint16 collision-object
flags. Loader, reference relocation and property-scaled graph copies preserve
those flags independently of both Havok filter flags and authored blend data.
The default active value remains available for explicit synthetic body definitions;
NIF loads always copy the actual collision record value.

The immutable `ragdollReferenceRootFrame` dispatcher follows original 897510:
registration adds local/reference bits 0xC but does not enable authored bit 0. Once
the initial pending 0x40 scene-to-body synchronization has completed, inactive
roots skip physical pose derivation and callbacks. They retain supplied locals,
compose root world from those locals, and preserve visible scale independently
of physical admission. The inactive branch does not read or validate unused
physical input. Active roots retain the previously verified threshold/mask
semantics. The original 1,024-frame course includes 512 active and 512 inactive cases;
the active subset exactly reproduces the preceding 512-frame corpus.

The scheduler now checks owned authored flags before deriving a physical root
pose. This changes scene dispatch, not dynamics or collision shape/mass policy.
Real-NIF factory tests cover direct-root and selected-child relocation with
flags 0: bodies take an actual falling step while six subsequent World updates
retain the original stored/reference/visible root pose across scales 2/.63/1.38.
Body poses and packed velocities survive publication unchanged. Parameterized
0/1/2-worker tests also cover ordinary/bodyT sources, arbitrarily moved/rotated
physical poses and explicit forced-root requests. An explicit forced request
establishes the next local; subsequent ordinary frames remain inactive. This
verifies that pose API, not full World/script source-pose or command semantics.

Normal and sanitized production comparisons match every 26 local/world float
lanes, change mask and six reference fields across all 1,024 original cases.
Original active desired-transform scale is uninitialized/unused and canonicalized
to 1 for the typed adapter. Inactive original dispatch has no derived desired
input; comparison supplies an unused identity and a component test independently
checks malformed unused input. Native World/cell, allocation/import/TLS/critical
section and fixture-motion boundaries remain those documented in the preceding
root-frame provenance. These are not actual native dynamics or gameplay courses.

Evidence under `build/oblivion-compat/m15/S4/`:

- `authored-root-flags-normal-01` and `authored-root-flags-sanitized-01`: freshly
  rebuilt full 2,706 component tests / 257 suites and 1,572 engine tests / 122
  suites each, unfiltered, exact inventory/XML, no failures/skips. ASan leak
  checking is disabled; ASan/UBSan halt on errors. Existing unrelated compiler
  warnings remain. No new failed authoritative attempt occurs in this chunk.
- `authored-root-flags-compare-01`: 1,024 bit-exact dispatcher/reference cases
  each in actual normal and sanitized component libraries.
- `authored-root-flags-baseline-01`: preceding committed ELF runs the existing
  falling-body factory/real physics-step/World course. Only the owned input NIF's
  collision flags 1->0 are amended before VFS opens it. No post-load gameplay
  fields are injected. The old root frame reports position mask 1 and changes
  stored REFR, contradicting the independent inactive-node rule. The old engine
  case itself passes with exact inventory/XML agreement; independent observation
  establishes its wrong behavior rather than treating that pass as acceptance.
- `authored-root-flags-runtime-01`: final-ELF read-only inspection sees authored
  flags 0 and root record 0 for both plain and relocated-bodyT reference definitions.
  All six actual World frame returns have mask 0, retained local/world positions
  and unchanged REFR. Its one selected engine case passes with exact inventory/XML.
  Pre-open hashing confirms the 524-byte direct-root NIF is byte-identical to the
  baseline's amended input, SHA256
  `54b85cb823326708fa9851ed03948b7b00e38e46ce0709d654892e1f1e9e26b2`.

Both authoritative checks preserve source fingerprint
`3e0fd6ed58cf21748e77c252ec83a1b9cf488dd3b49dd69c8d240455e23a7080`
on parent `e6da0e09929120e2a74e7279c747c91f95c5ff4f`, before documentation.
Normal engine ELF SHA256:
`0e42f37df74839602448aa77497294a197ebb4eb3a201bc4ac7a0eef44cf1f3b`.
Sanitized engine ELF SHA256:
`9aa97e88b588d45e10540af5ccf43a83ea96e28b8d4253050d0877a61b5ed513`.
The review directory freezes nine implementation/test hashes and the exact patch.
[Editable provenance](M15-ORIGINAL-AUTHORED-ROOT-FLAGS-PROBES.json) links original
corpus hashes, current comparisons, actual-ELF observations and full checks.
Prior failed attempts remain in their original evidence directories/manifests.

The authored non-root path still uses the old inverse-bind/unconditional-reference
adapter. It needs separate node-local/cached-world projection, controller updates
and proper callback dispatch, including inactive children. Root-controller and
actual World/script scale/forced-transform continuation remain open, as do
complete Havok-enable/special-reference/cell/property-copy admission and source
fixed/keyframed/no-physics routes. Loose retention/save/recreation, production
break/mastery, pickup/replacement/sound, power/creature/reaction/effects and the
full normal-input/controller/FPS/windup/block/recovery restart S4 matrix remain
required. Full S4 is active and incomplete.

## NIF controller-local and traversal matrix separation

`NifOsg::MatrixTransform::TraversalMatrix` is an optional immutable local-to-parent
traversal pose and cached inverse. `getMatrix()`, the author inverse and NIF
rotation/scale fields remain controller/local state. World/inverse traversal and
bounds can therefore follow cached physics output without destroying authored
locals. This is the representation required by original unselected-child flags 1,
where local state stays authored while cached world follows physics. The current
World adapter is not wired to select or apply per-node frames yet.

Numeric preparation rejects nonfinite/singular matrices and nonfinite inverses
before changing a node. Prepared handles retain immutable shared values; setter,
clone and checkpoint restoration cannot mutate another node's value. Default or
cleared state delegates to the original OSG path, including relative/absolute
reference frames. AutoTransform honors an explicit traversal pose and resumes its
existing camera-facing path after clearing; this tests OSG behavior, not native
billboard gameplay semantics.

Actor local checkpoints now retain traversal handles along with their double
local matrix, float rotation cache and separate scale. Guard comparison includes
presence and exact projection bits, including signed zero, while equivalent
prepared values compare equal. Animation target capture still reads controller
locals and does not feed traversal projection back into physics input. Restoring
projection handles after existing hierarchy validation needs no new allocation;
this does not claim that the whole hierarchy-validating restore is allocation-free.

Seven new NIF tests cover default/clear behavior, relative and absolute traversal,
noncommuting parent transforms, world/inverse/bounds, independent author writes,
clones, rejected input, a real keyframe controller with missing channels and the
billboard path. A new actor checkpoint case covers author-target independence,
projection restoration, equivalent values and signed-zero rejection. The actual
factory rollback case expands from five to six mutations: changing traversal
alone now rejects before source wear/count, serial, registry or physics publication.

Evidence under `build/oblivion-compat/m15/S4/`:

- `nif-traversal-projection-normal-01` and
  `nif-traversal-projection-sanitized-01`: freshly rebuilt full 2,714 component
  tests / 258 suites and 1,572 engine tests / 122 suites each, unfiltered with
  exact inventory/XML agreement and no failures/skips. ASan leak checks disabled;
  ASan/UBSan halt on errors. Existing unrelated compiler warnings remain.
- `nif-traversal-projection-compare-01`: real production traversal API plus OSG
  world/inverse/bounds against all 64 supplied original unselected-child frames,
  both builds. Declared tolerances are `1e-10` for world/inverse matrix entries,
  exact author-matrix preservation and `2e-5` for bounds center. Observed world,
  author and bounds errors are zero; maximum inverse error is
  `3.552713678800501e-15`. Cached NIF rotation/scale fields also remain exact.
  These supplied frame comparisons do not execute live World/controller order
  or original Havok dynamics and do not establish normal-input gameplay.
- `nif-traversal-projection-allocation-01/guard-verification.json`: 35 exact
  guard calls, 29 accepts / six rejects, zero observed allocations in the
  selected five main-thread heap allocators. Projection construction and other
  APIs/threads are excluded. The unchanged detector's prior positive control
  remains in `loose-scene-binding-allocation-01/control-verification.json`.
- `nif-traversal-projection-allocation-02/setter-verification.json`: ten prepared
  handle setter calls, including projection checkpoint restoration, with zero
  observed allocations. Raw matrix preparation and existing hierarchy validation
  occur outside this fence. The void setter's raw AL bytes have no boolean meaning.
  Allocation01's setter log retains an early launch before script configuration
  completed; it produced no setter evidence. 02 waits for existing script/config
  and passes. The allocation01 guard trace independently passes.

Both full checks preserve source fingerprint
`5323678d01523d0f9100b9667af5b93869cc8cb04fe0313bde7dce786d5cc1e1`
on parent `4bff6493262cc003bf2bac9f30f7bfff4039b891`, before documentation.
Normal engine ELF SHA256:
`7516a7f8c66a3c8fc1f45c88084ef76c6b04b717ffe7526d9be875dde26a94f0`.
Sanitized engine ELF SHA256:
`a58a539961644062cce68330ba6f38e5715ddaeedf26af1f694f6c54741e4d58`.
The review directory freezes eight implementation/test hashes and the exact patch.
[Editable verification](M15-NIF-TRAVERSAL-PROJECTION-VERIFICATION.json) records
current reports, original corpus identity, comparison tolerances and boundaries.

Live loose per-node dispatch, controller/first-synchronization ordering and root
reference alias initialization remain open. Rendering consumers that bypass
traversal also require integration: Skeleton::Bone::update reads `getMatrix()`
directly, and RigGeometry's skin-root inverse skips a transform when that author
matrix is identity. This representation is not accepted for live skinned
projection until those paths are handled and tested. Actual World/script
continuation, complete admission modes, loose physical save/recreation, production
break/mastery, pickup/replacement/sound, power/creature/reactions/effects and the
entire normal-input/controller/FPS/restart S4 matrix remain required. Full S4 is
active and incomplete.

## NIF traversal poses in CPU skinning

Bone::update now uses virtual local-to-world composition when a NIF node has an
explicit traversal pose. Projected ancestors and unsimulated connectors therefore
contribute to the skinning palette while controller locals remain independent.
An explicit absolute projection resets the parent as OSG traversal does. Nodes
without a projection retain their prior bone behavior, including absolute and
billboard author locals; this does not change legacy camera-facing skinning rules.
The existing caller requirement to invalidate cached skeleton matrices after
same-frame pose changes remains in force.

RigGeometry's skin-root inverse no longer skips a projected node just because
its controller-local matrix is identity. It calls the transform's inverse
traversal method and cancels the drawing pose. Clearing a projection restores
ordinary local composition and inverse calculation.

Two bone cases cover rotated parent/connector/child projections, retained author
matrices, same-frame invalidation, stable bone identities, explicit absolute
projection and clearing. Two parameterized rig cases exercise both case-insensitive
named-root lookup and missing-root fallback. They run actual CPU update/cull
skinning and check vertices, normals, tangent direction/handedness and bounds with
a `2e-5` float tolerance. Three frames cover identity controller locals with a
projection, author writes under that same projection, and clearing to the new
locals. Source vertex/direction arrays stay unchanged. The cull capture stops
before graphics submission and needs no GL context.

Evidence under `build/oblivion-compat/m15/S4/`:

- `nif-skin-traversal-regression-01`: retained test build failure caused by an
  unsupported OSG array initializer. Tests were corrected using push_back.
- `nif-skin-traversal-regression-02`: before the production fix, the corrected
  four selected tests all fail and reproduce the palette/inverse bypasses. This
  filtered diagnostic failure is not a full validation run.
- `nif-skin-traversal-normal-01` and `nif-skin-traversal-sanitized-01`: fresh full
  2,718 component tests / 260 suites and 1,572 engine tests / 122 suites each,
  unfiltered, exact inventory/XML agreement and no failures/skips. ASan/UBSan
  halt on error; leak checks disabled. No compiler warnings were emitted by
  these incremental builds.

The [editable verification](M15-NIF-SKIN-TRAVERSAL-VERIFICATION.json) pins both
reports, implementation/test hashes, regression XML and tested source identity.
This closes the identified bone-palette/skin-root CPU consumer bypasses for
supplied traversal poses. It does not execute original-game dynamics, a display
or normal gameplay input. Live loose per-node projection, controller/first-sync
ordering, root-reference alias initialization and safe reference-callback
publication remain open. Actual World/script continuation, complete admission
modes, loose retention/save/recreation, production break/mastery, pickup/sound,
power/creature/reaction/effects and the entire normal-input/controller/FPS/restart
S4 matrix remain required. Full S4 is active and incomplete.

## Settled-node dispatch and original live precision

`ragdollBoneFrame` composes the current controller/local with the parent's cached
world before settled collision dispatch. It checks bit0 before reading physical
input. Inactive children retain locals and use ordinary composed world output;
active children preserve or write locals according to bit8, including local writes
below cached-world thresholds. A child change mask does not itself authorize a
reference callback. The registered-root helper delegates with registration bits
0xC while preserving inactive bit0. First pending0x40 synchronization is separate
and remains an admission/ordering gate.

An expanded original instruction probe executes actual registration and eight
subsequent 707370 traversals over four unselected deep/null-first topologies,
plain/bodyT bodies, flags0/1/8/9 and three x87 modes. It supplies motion fields and
changes parent/root author transforms at frame4. The 768 frames preserve root
local, entire REFR and physical storage within each traversal, with no reference
callback. This exposes why the existing inverse-bind root adapter cannot represent
unselected-child behavior. It does not replace that live adapter in this chunk.

The initial 512-frame comparison exposed three extended-precision rotation-lane
differences: identical inputs produce different bits under027F versus037F. A
private original-game Proton prefix then provides a stronger check. Normal-input
Continue/F9 loads the isolated save; read-only debugger sampling records 64 entries
each at 707370, 7100A0 and 8978D0, all with **FPCW 007F (24-bit significands)**. Three
manager times occur in the scene-entry sample; the arithmetic/writeback samples
are short observations, not a universal precision guarantee. The game exits via
its UI with wrapper status0. The started Steam client also shuts down normally.
Source/private quicksaves retain SHA256
`67d927cad4a370131882da58b04a25a330c5defe288b9b6ca21f7768815c4d66`;
source config/plugin inputs remain unchanged and resolve separately from private
copies. First debugger attachment stops in Wine save_context on SIGSEGV without
samples; the retained retry passes that signal to Wine and captures all entries.

`NativeTransformPrecision` now selects Single or Double explicitly. Single rounds
every arithmetic instruction to 24 significand bits while retaining a wider
exponent until each native binary32 memory store. An ordinary float cast after
every operation would incorrectly overflow cancelling products or erase two tiny
products whose sum is representable. Exact original nearest-even ties, wide
cancellation and tiny-product examples are in the unit tests. Store validation
shares the reviewed overflow-midpoint guard. Unknown precision values reject.
The new settled-node API defaults to observed live Single. Existing composition
and low-level writeback API defaults remain Double; this is not a global migration
of actor/drop/bodyT/physical-target arithmetic from their earlier contracts.

Evidence under `build/oblivion-compat/m15/S4/`:

- `original-node-precision-01`: 192 live original register samples at three
  entry points, pristine saves, normal exit and independent source-prefix audit.
- `unselected-child-live-flags-oracle-01`: 768 original frame contexts / 96
  sequences, including007F/027F/037F. Callee saves, stack, control word and input/
  physical/reference invariants are checked. Null native World and disabled
  property-copy/special gates remain declared boundaries, with retained heap/
  Windows/TLS/critical-section suppliers. No actual Havok stepping.
- `unselected-child-flags-compare-03`: both builds match all 512 supplied node
  frames for explicit007F/027F, across 13 recomposed-world, 13 resulting-local,
  13 resulting-cache float lanes and the mask. The retained preceding double-only
  binary mismatches 32 live007F frames. Three 037F differences remain failing
  diagnostic cases; no tolerance is substituted for their exact comparison.
- `native-single-compose-oracle-01` and `native-single-compose-compare-01`:
  actual 53D7A0/7100A0/7101F0 with007F, no stubs; both production builds match
  335 finite outputs exactly and reject 12 nonfinite outputs across 347 cases.
- `settled-node-live-precision-normal-01` and
  `settled-node-live-precision-sanitized-01`: fresh unfiltered 2,725 component
  tests / 261 suites and 1,572 engine tests / 122 suites each; exact inventory/XML
  agreement, no failures/skips. ASan/UBSan halt on error; leak checks disabled.

Earlier retained diagnostics include compare01's 259 mismatches (256 missing
inactive dispatch plus three 64-bit differences), compare02's double-only canonical
pass/extended failures and the pre-live source's passing full normal/sanitized
checks. Compare03's baseline raw report mistakenly hashes later source files;
`baseline-provenance.json` preserves the older binary's actual compare02 hashes
and original input file. The [editable verification record](M15-SETTLED-NODE-LIVE-PRECISION-VERIFICATION.json)
uses that corrected provenance without replacing the raw report.

Live loose per-node renderer publication, safe owner lifetime, controller/first-sync
ordering, root-reference alias initialization and safe reference callbacks remain
open. Other live precision contexts, existing actor/drop composition and physical
desired/bodyT conversion callers require review and selective migration. Actual
World/script continuation, full admission modes, loose retention/save/recreation,
production break/mastery/drop/pickup/sound/power/creature/reactions/effects and the
entire normal-input/controller/FPS/restart S4 matrix remain required. Full S4 is
active and incomplete.

## Ordinary physical scene precision in loose-root capture

Reverse bodyT origin projection, ordinary native quaternion-to-matrix conversion
and native-to-world position stores now accept explicit Single/Double precision.
LooseObject::rootPose selects Single in the actual scheduler path, matching the
preceding checkpoint's observed live FPCW007F context. Public API defaults remain
Double; other actor/blend/forward/relocation/COM contexts still need independent
review and selective migration. No thread's floating-point control word changes.

The bodyT vector rotation keeps its original SSE float products, reductions and
cross terms. Only its x87 coefficient construction uses the selected intermediate
precision. Ordinary matrix construction similarly retains float product stores
but rounds each subsequent x87 instruction before final matrix stores. Position
conversion uses the existing finite overflow-midpoint guard. Unknown precision
values reject at each public boundary, including an ordinary body with unused
bodyT metadata. Quaternions are not renormalized by these adapters.

The first expanded original route probe supplies eight physical quaternions,
including arbitrary noncommuting axes and 180-degree rotations. It captures
actual native reverse origin/quaternion outputs and the desired world matrix
before node writeback. The preceding C++ binary mismatches 16/192 initial cases,
all in matrix diagonal lanes. A stronger second probe varies two local offsets,
two local quaternions and near-zero physical origins: the same preceding binary
mismatches 120/768 cases, all FPCW007F, including native bodyT origin fields that
rounding at larger origins had hidden. No tolerance is substituted for exact bits.

Both final production comparisons match all 768 cases across 19 binary32 lanes:
three native scene positions, four quaternion components and 12 desired world
rotation/position entries. Single is selected for 007F and Double for 027F/037F;
the latter outputs coincide in this corpus. This is not a general 64-bit x87
implementation. The prior three 037F node-composition diagnostic differences
remain open.

Three new component cases retain native near-zero/noncommuting bodyT examples,
ordinary diagonal/position stores and invalid precision rejection. The real
scheduler regression runs with worker counts 0/1/2, restores a supplied physical
quaternion, verifies its Bullet readback bits and checks original matrix bits
through captureLooseObjectRootPose and captureLooseObjectRootFrame. Queries
preserve the physical pose and packed velocities. This fixture tests scheduler
pose queries, not a normal-input game action or rendered gameplay result.

Evidence under `build/oblivion-compat/m15/S4/`:

- `physical-scene-live-precision-oracle-01`: 192 initial original contexts.
- `physical-scene-live-precision-oracle-02`: 768 contexts / 96 sequences, four
  unselected child topologies, plain/bodyT, active flags 1, three x87 controls,
  supplied physical positions/rotations and parent-transform changes. Actual
  registration, pending first sync and complete physical-to-scene math execute.
  Conversion functions are observed, not replaced. Stack/callee saves/control
  word and physical/root-local/reference preservation are checked. Declared heap/
  Windows/TLS/critical-section suppliers, null native World and disabled property-
  copy/special gates remain boundaries. The inherited inactive sentence in the
  raw scope text is not an inactive-case claim; all these cases use active 1.
- `physical-scene-live-precision-compare-01`: retained baseline harness build
  failure from range-for over osg::Vec3f. Fresh02 uses indexed access and records
  the initial 16 mismatches.03 records the stronger 120-case baseline failure.
- `physical-scene-live-precision-compare-04`: both final builds match 768/768
  cases exactly, retaining hashes of the unchanged original corpus and inputs.
- `physical-scene-live-precision-normal-01` and
  `physical-scene-live-precision-sanitized-01`: fresh unfiltered 2,728 component
  tests / 261 suites and 1,575 engine tests / 122 suites each, exact inventory/XML
  agreement and no failures/skips. ASan/UBSan halt on error; leak checks disabled.
  Existing unrelated engine-test compiler warnings remain.

The [editable verification](M15-PHYSICAL-SCENE-LIVE-PRECISION-VERIFICATION.json)
pins the tested source identity, implementation hashes, reports and prior live
precision basis. No new original-game process is launched in this checkpoint;
normal-input/register observations are the preceding 192-sample original run.
Current native route inputs are supplied physical poses, not Havok dynamics.

A next-step independent `node-controller-order-oracle-01` also passes 768 frames
with instrumented controller virtual Update suppliers on root/target/leaf. Actual
47C930 dispatch, composition, collision and child traversal execute. All 768 leaf
controller entries observe the target's current post-projection cache; 672 later
frame target-controller entries retain their previous cached global even though
the parent has already updated. The future OSG integration must preserve both
observations; ordinary lazy world recomposition alone is insufficient. These
suppliers do not establish real interpolation, controller clocks or Roll math.

Live per-node World/renderer dispatch, controller/first-sync ordering, root-reference
alias initialization, safe owner lifetime and reference callbacks remain open;
the inverse-bind child adapter is unchanged. Actual World/script continuation,
complete admission modes, loose physical save/recreation, production break/mastery/
drop/pickup/sound/power/creature/reactions/effects and the entire normal-input/
controller/FPS/restart S4 matrix remain required. Full S4 is active and incomplete.


## Manual ordinary-NIF controller and projection traversal

`SceneUtil::NifSceneTraversal` owns a manual settled traversal for a private
ordinary NIF hierarchy. Each node retains its native binary32 world cache
independently of the author/controller local and immutable relative OSG drawing
projection. After a parent projects, its nearest NIF children are immediately
rebased to their previous globals. Own controllers then see the previous own
cache together with the updated parent; children see the parent's post-collision
cache. A diagnostic ordinary OSG traversal instead observes25 where the retained
native fixture requires15 after parent10->20 with child local5.

Existing StateSet/property and node callback chains run once with child traversal
suppressed, including work after `traverse()`, before own projection and explicit
child traversal. FrameStamp is copied, and traversal number and external NodePath
are forwarded. Reference-root LookAt excludes synthetic OSG placement as a native
parent. Real Keyframe missing channels preserve author rotation/scale; real Roll
post-traverse work finishes before projection. These test existing controller
consumers, not independent native interpolation, Roll timing or LookAt formulas.

Inactive bit0 skips physical input; active bits1/9 retain or update author locals
according to bit8. Pending0x40 rejects. Optional per-node publication runs after
projection and child rebasing, before children. A child mask does not authorize
a REFR callback. Precision is explicit Single/Double. A consistent separate NIF
rotation cache survives scaled drawing storage without an inverse round trip;
procedural raw matrix writes still reconstruct rotation. The native nonunit-scale
fixture first falsified that earlier reconstruction-only reader.

Constructor validation prepares all projections before writing any. The binding
checks private hierarchy identities and projection ownership. Root, current node,
callback and external-ancestor leases prevent retirement while borrowed. Guards
follow collision/physical suppliers, and final placement validation checks both
ancestor identities and matrix. A new test caught an identity ancestor detaching
without changing the world matrix. Exceptions do not roll back controller writes
or completed nodes. Reentrant calls reject, and explicit later passes are allowed.
Skeleton palettes invalidate even when traversal numbers repeat; cached Bone
identity survives world-only physical publication while author locals stay put.

The domain is relative ordinary `NifOsg::MatrixTransform` nodes through transparent
Group/Geode, with an external Skeleton permitted. Invertible drawing transforms
are required. Nested Skeleton, billboard, Switch/LOD, absolute transforms, shared
or ambiguous hierarchy/placement and zero-scale paths need separate policies.
The root author local must already contain native global placement. Placement
can change between passes; an owning scheduler must prevent a second ordinary
OSG update. These requirements are not an assertion that current World satisfies
them. Current root-only/inverse-bind child publication remains unchanged.

Evidence under `build/oblivion-compat/m15/S4/`:

- `nif-scene-traversal-baseline-01`: retained existing OSG25-vs15 diagnostic.
- `nif-scene-traversal-preflight-01` through08: retained build/test attempts.
  01 catches ambiguous matrix types;03 catches native rotation-bit loss;05
  catches the StateSet test's wrong callback type;07 catches detached identity
  ancestry. Final08 passes all18 focused cases. Earlier02/04/06 pass their then
  smaller inventories.
- `nif-scene-traversal-compare-01`: driver compile failure from wrong mask field.
  02 passes the initial traversal;03 rebuilds both final binaries and compares
  the unchanged `node-controller-order-oracle-01` corpus. Each matches512
  supported-precision frames and8128 fields exactly across four topologies,
  plain/bodyT supplied world inputs, flags0/1/8/9 and eight retained-state frames.
  Own local-before/after, previous cache, current parent, final target local/world,
  mask, read count and controller order are compared. Native frame0 leaf previous
  cache is excluded because that fixture inserted it after first sync with zero
  world storage; later leaf observations are compared. A native null child slot
  becomes an empty OSG Group. All256 extended-precision diagnostic frames also
  match here, without closing prior037F differences from another corpus. OSG
  matrix tolerance1e-9 was declared before execution; max error5.68434e-14.
- `nif-scene-traversal-full-normal-01` and `full-sanitized-01`: intentionally
  interrupted owned builds before adding ancestor/supplier lifetime guards.
  Helpers were terminal before source edits. These are not full passing runs.
- `nif-scene-traversal-full-normal-02` and `full-sanitized-02`: unfiltered2746
  component tests/262 suites and1575 engine tests/122 suites in each build,
  exact inventory/XML agreement, no failures or skips. ASan/UBSan halt on error;
  leak detection disabled. No new compiler warnings appear in these build logs.
- `reference-root-initialization-oracle-01`: next-step72 original instruction
  contexts. Actual GenerateNiNode004E4244..004E426E stores reference T, executes
  complete4D7AF0 and copies nine rotation lanes into the asset root. Later
  004E43C8..004E43E9 executes4D7260, FABS and root scale store. Three angles,
  two positions, four signed scales and three x87 controls preserve child,
  reference/base bytes, stack,EBX,EBP and control word. Stops before4D6F20 and
  node virtual+50; intervening registration, first synchronization and complete
  GenerateNiNode are not executed. This is preparation for live root binding.

The [editable verification](M15-NIF-SCENE-TRAVERSAL-VERIFICATION.json) pins the
source fingerprint, code and evidence hashes. The controller corpus executes
native47C930/node/collision dispatch with declared controller virtual Update
suppliers; interpolation/clocks/math and Havok dynamics remain boundaries. No
new original-game process is launched by this checkpoint.

Next integrate correct global root initialization with renderer/physics ownership,
first-sync/controller order and an independent lifetime token. World publication
must preserve native REFR-before-children semantics and safe cell migration while
avoiding a duplicate viewer callback pass. Real controller clocks, unsupported
node/admission/property-copy modes, forced World/script continuation, other live
precision callers, loose retention/save/recreation, production break/mastery/drop/
pickup/sound/power/creature/reactions/effects and the full normal-input/FPS/fresh-
process acceptance matrix remain open. Full M15S4 is active and incomplete.


## Loose-model registration and physical owner link

`MWRender::LooseObjectSceneBinding` links one prepared native single-body model
with its exact ObjectAnimation, placement and renderer registration. It captures
source hash, physical/target records, collision flags and reference-root identity;
independently scaled physical properties need not equal their source values.
Opaque handles cannot be copied. Weak observer pointers do not retain an animation
or reference. Published handles can acquire a strong animation lease after
checking the current model/link maps, live base placement and NIF pose identity.
A distinct weak renderer-owner identity protects the borrowed Objects pointer.
ActorRagdollLocalPose's retained checkpoint authority is not used as a lifetime
token.

Objects owns the live registration. Preparation allocates its detached map node;
publication inserts that node and marks the link published. Cancellation,
admission rollback, ordinary removal, prepared item removal, cell removal and
Objects destruction retire it. Keeping a shared handle or animation alive through
the cleanup queue does not revive the registration. Prepared handles cannot lease
a live animation. Retirement remains observable even with old local checkpoints
and placement nodes retained.

WorldModel passes the prepared renderer link directly into PhysicsSystem and its
scheduler proposal. Physics retains a weak handle plus an explicit indication
that a link was supplied. Preparation validates source/target identity. Physical
commit also requires the exact model to be published, before collision routing
or body publication; rejection preserves the proposal for retry. A body originally
prepared without a scene link returns null. A previously linked body rejects an
expired/retired/replaced link instead of falling back to the unlinked mode.

`World::synchronizeLooseWeaponPoses` now obtains each link from physics, validates
the model before root capture/publication and retains its animation lease through
the pass. It rechecks registration before each reference publication. This works
with the actual separately owned Objects in headless factory fixtures, without
assuming World.getAnimation can resolve them through RenderingManager. This is
a model-ownership guard around the existing root-only/inverse-bind adapter. It
does not implement the new manual per-node traversal, global root aliases or
complete World/reference safety across external callbacks and cell migration.

Five new real native-World/owned-NIF/Bullet factory tests verify:

- The exact published private animation lease, successful transaction-plan
  destruction, source/record/flag/root identity mismatch rejection and changed
  physical-node identity rejection.
- Ordinary removal, prepared removal, cell removal and renderer destruction with
  a shared handle, animation lease and local checkpoint retained. Restoring the
  old base placement cannot resurrect the registration. Dropping the last handle
  exercises weak expiry too. Rejected World synchronization preserves reference
  position, physical pose and linear/angular velocity.
- Never-published model cancellation and published-model rollback invalidate a
  detached physical proposal. The latter retains an animation lease. Wrong source
  identity rejects while the prepared link is otherwise valid.
- A new model on the same reference has a separate link and animation. The
  original physical owner never follows it.
- Physical commit rejects a still-prepared model without consuming its proposal
  or publishing resources, then retries successfully after scene publication.
  Subsequent model rollback retires that published physical link.

The existing unlinked admission case also checks the null-link result explicitly.
These are real resource/World integration fixtures, not a rendered normal-input
break/drop action or fresh-process save/recreation course.

Evidence under `build/oblivion-compat/m15/S4/`:

- `loose-scene-owner-link-preflight-01`: production units compile; owned CMake
  group intentionally stopped before the large test unit completed so lifecycle
  tests and exact registration checks could be added. No passing test claim.
- `loose-scene-owner-link-preflight-02`:49 selected factory/admission cases pass.
  The compiler catches a new dangling-else warning in one test; braces are fixed
  afterward and rollback/preservation checks expanded.
- `loose-scene-owner-link-sanitized-01`: intentionally stopped for that correction.
- `loose-scene-owner-link-normal-01`: all1579 then-current engine cases pass.
  Review afterward finds physical commit must require published scene dependency,
  beyond preparation validity. The guard and fifth successful-retry case are added;
  this earlier passing inventory does not establish that gate.
- `loose-scene-owner-link-sanitized-02`: owned build stopped for the dependency
  guard/noncopyable handle change; helper terminal before edits. Not a pass.
- `loose-scene-owner-link-normal-02` and `loose-scene-owner-link-sanitized-03`:
  final unfiltered1580 engine tests/122 suites pass in each build with exact
  inventory/XML agreement and no failures/skips. ASan/UBSan halt on error;
  leak detection disabled. Existing unrelated test compiler warnings remain.

The [editable verification](M15-LOOSE-SCENE-OWNER-LINK-VERIFICATION.json) pins code,
reports and the tested source fingerprint. Component sources/tests are unchanged;
reuse the preceding53d432d5da checkpoint's2746 component tests/262 suites in both
builds, without representing them as a new component run. No new original-game
process or normal-input runtime course is launched by this checkpoint.

Next expose the actual physical target world before inverse bind and connect
native global root initialization/initial capture with NifSceneTraversal. Complete
original first-sync/controller clock/order evidence, one authoritative update pass
and ordinary-viewer suppression. Reference publication must retain native order
before children while safely rebasing placement across cell migration. Reconcile
physics-retained root locals with renderer controller state. Other admission/node/
property-copy modes, forced World/script continuation, loose physical save/fresh
recreation, production break/mastery/quest-retained/group-NPC drops/pickup/
replacement/sound/power/creature/reactions/effects and the full normal-input/FPS/
fresh-process S4 acceptance matrix remain open. Full M15S4 is active and incomplete.


## Physical target world source before inverse bind

PhysicsSystem and its scheduler expose `captureLooseObjectSceneTarget` metadata
and `captureLooseObjectTargetPose` for one admitted body. The result is a rigid
NiTransform in NIF drawing/world units before authored inverse bind. Metadata
contains physical/target records, reference-root identity and collision flags,
plus a private weak identity belonging to the exact admitted LooseObject. A
replacement body on the same reference cannot inherit a previous descriptor.
Genuine owner transfer retains that body identity. Tampered/default descriptors
reject, and renderer-linked owners also validate their current published model.
Neither metadata nor a descriptor retains the borrowed reference or scheduler.

Pending0x40 rejects before inactive dispatch. Inactive flags0/8 return no pose
without consuming physical input or unit conversion. Active input requires the
native length scale. The active path extracts the real physical body, reverses
bodyT, builds the ordinary quaternion matrix and stores world positions using
existing observed Single precision. The live legacy root reader now consumes
this same target conversion, then applies its retained inverse bind. World-to-
native position conversion is unchanged; this is not a global precision migration.

The distinction matters for an unselected physical child: in a supplied
noncommuting bind/root fixture, its target position is115,33,50 while the legacy
root estimate is100,40,80. A changed visible reference scale leaves that admitted
physical target unchanged. A nonrigid unused author bind does not prevent reading
the physical target, though the legacy inverse-bind reader still rejects it.
These source-query semantics do not admit such a hierarchy to the manual scene
traversal or implement native property/admission policies.

Five parameterized scheduler/facade patterns run with worker counts0/1/2. They
cover ordinary/bodyT distinction, descriptor fields/default/replacement/owner
transfer, inactive and pending flags with poisoned physical origins, active wrong
units and preserved physical/packed state. Three original zero-origin007F vectors
check all12 rotation/translation bits after verifying Bullet input quaternion
bits. Existing real native factory cases exercise the new facade on the exact
published model and reject target queries after model retirement while the body
still exists. Collision flags are the admitted definition snapshot; a mutable
native collision-mode state machine/first-sync consumer is still required.

The independent `physical-target-zero-origin-oracle-01` executes768 contexts
through the inherited full original registration/first pending synchronization
and physical-to-scene route. Four unselected topologies, ordinary/bodyT, two local
offsets/two local quaternions, eight physical quaternions and three x87 controls
remain. Physical positions are supplied as zero. Actual origin/quaternion/matrix
conversion functions execute, with declared heap/Windows/TLS/critical-section
boundaries, null native World and disabled property-copy/special gates. Physical,
root-local and reference byte preservation, stack/callee saves and control word
are checked. No native Havok stepping or new original-game process is involved.

Zero input avoids a fixture ambiguity: some prior arbitrary native float positions
have no exact preimage through the engine's float world-unit conversion. Expected
values are still original instruction outputs, never generated by production C++.
The actual scheduler replay supplies the original mathematical unit quaternions
as Double Bullet inputs and independently requires their float readback to match
all native input quaternion bits. Both normal and sanitized replays compare all
256 original007F contexts at worker counts0/1/2:768 worker contexts and9216 exact
native R/T checks per build, with zero mismatches. API rigidscale1 and physical/
packed-velocity preservation are separate assertions. The unused/uninitialized
native temporary scale lane is not compared. The original topology labels are
corpus contexts; the replay does not traverse an engine node hierarchy. Other
x87 controls are not replayed against the fixed liveSingle API.

Evidence under `build/oblivion-compat/m15/S4/`:

- `loose-scene-target-preflight-01`: retained test compile failure from a metadata
  variable colliding with an existing node visitor.02 corrects the name and passes
  all168 selected scheduler/factory cases.
- `physical-target-zero-origin-oracle-01`:768 original contexts, source/corpus/
  executable hashes retained. Its three selected vectors are in the engine tests.
- `loose-scene-target-compare-01`: standalone driver lacks MyGUI includes.02
  imports actual CMake includes/definitions but lacks the concrete Static record
  header; the premature comparison finds no binary.03 fixes the header, builds
  both modes and passes the complete live-precision replay above.
- `loose-scene-target-normal-01` and `loose-scene-target-sanitized-01`: unfiltered
 1595 engine tests/122 suites each, exact inventory/XML agreement, no failures or
  skips. ASan/UBSan halt on error; leak detection disabled. Existing unrelated
  test compiler warnings remain. Component source/tests are unchanged; reuse
  preceding53d432d5da component coverage2746/262 in both builds, not a fresh run.

The [editable verification](M15-LOOSE-SCENE-TARGET-VERIFICATION.json) pins the tested
source fingerprint, code, native corpus and report hashes. Next connect native
global reference-root initialization and initial physical capture to the manual
NifSceneTraversal, with independently verified first-sync/controller clocks and
one update owner. Native reference callbacks must precede children while safely
rebasing placement through cell migration and retaining World/reference lifetime
guards. The current World root-only/inverse-bind adapter remains unchanged.

Forced World/script continuation, all admission/property-copy/node policies,
other precision contexts, loose physical retention/save/fresh recreation,
production break/mastery/quest-retained/group-NPC drop/pickup/replacement/sound/
power/creature/reactions/effects and the complete normal-input/FPS/fresh-process
S4 matrix remain open. Full M15S4 is active and incomplete.


## 2026-10-16 — explicit global-reference pose capture checkpoint

`ActorRagdollPoseBinding::RootSpace::ReferenceWorld` now captures an initialized
native global root without applying synthetic OSG placement R/T again. It requires
a declared single physical hierarchy root, composes descendants in observed Single
precision and uses the corresponding root/child dynamic snapshot writeback. The
existing scale admission remains. Default ObjectLocal behavior is unchanged.
A shared `MatrixTransform::getControllerTransform` preserves separate native
rotation bits when the scaled drawing matrix agrees and handles raw procedural
controller matrices independently of traversal projection handles. The manual
scene visitor uses the shared reader. Four new tests cover capture, writeback,
checkpoint restore, native cache precision/projection independence and rejection.

Fresh normal and ASan/UBSan checks pass all2750 component tests/262 suites and
1595 engine tests/122 suites per build, with exact inventory/XML agreement and no
failures or skips. Leak detection is disabled; existing unrelated compiler warnings
remain. The original72-case root-initialization/composition probe supplies24 live
007F root inputs to each actual pose-binding replay:576 exact root/child R/T fields
per build, zero differences. This consumes recorded Euler-builder output as input;
it does not implement Euler construction. A separate native Linux i386 harness runs
the complete ordinary nonactor4D7AF0 builder plus the actual scale getter/FABS/store
prefix:3912 hardware contexts repeat identically, and all72 common emulated root
R/scale outputs match hardware. Those are pinned expectations, not a production
Euler-builder comparison or whole GenerateNiNode/gameplay acceptance. The unchanged manual
traversal replay passes512 supported frames/8128 field checks in each build after
the reader move, with drawing error5.68434e-14 within1e-9.

A separate384-context original probe combines actual initialization segments,
registration and first707370 update. Pending40 clears even with inactive bit0;
first sync force-dispatches scene-to-body. Controller lists are empty. Synthetic
heap/Windows/TLS/critical boundaries, null native World and disabled property-copy/
special gates remain. The middle GenerateNiNode path and virtual50, controller
timing/interpolation, native stepping and actual gameplay are unproven. A retained
forward-adapter diagnostic finds32/128 live007F cases differ in64 quaternion fields;
027F/037F each match128 cases. First sync must resolve this precision gap.

[Editable verification](M15-REFERENCE-WORLD-POSE-VERIFICATION.json) pins sources,
reports, corpora and comparisons under `build/oblivion-compat/m15/S4/` in the
`reference-world-pose-*`, `reference-world-traversal-compare-01`,
`reference-root-world-capture-oracle-01`, `reference-root-first-sync-oracle-01`
and `reference-first-sync-target-baseline-01` directories.

This is an opt-in adapter checkpoint without a production ReferenceWorld caller.
ObjectAnimation still initializes its legacy local root; World still uses the
root-only/inverse-bind adapter. Correct native global initialization and initial
capture, mutable registered/pending flags and first sync, one update owner with
verified controller clocks, safe native REFR-before-child/cell migration callbacks,
all property/admission/node policies, loose save/fresh recreation and the full
production melee/drop/pickup/reaction/effect and normal-input/FPS/fresh-process
S4 gates remain open. Full M15S4 is active and incomplete.


## 2026-10-16 — forward scene/body live precision checkpoint

`ragdollNativeSceneTargetPose` and `ragdollNativeSceneBodyTargetPose` now accept
explicit arithmetic precision. Double remains the default. Single retains the
observed live 007F rounding of original 7150F0 trace/branch and offdiagonal
arithmetic, bodyT offset coefficients and final quaternion W. The existing SSE
normalization and reductions remain. Unknown modes reject before conversion.
Eight native golden vectors cover the trace and three axis branches, ordinary/
bodyT and descendant scenes; a second test preserves rigid admission in both modes.

A broader original probe runs 1,536 initialization/registration/first-update
contexts. The retained default-only baseline differs in 192 of 512 live cases
and 356 float fields. Both current normal/SAN replays match all 512 original 007F
cases using Single and all 512 original 027F cases using Double: 7,168 exact
output float fields per build. The 512 original 037F contexts also match Double
diagnostically. Inputs are the recorded native node-world R/T and final body-local
offset/quaternion; the replay does not implement initialization/registration or
first-sync dispatch. Empty native controllers, synthetic heap/Windows/TLS/critical
boundaries, null native World and disabled special/property-copy gates remain.
Middle GenerateNiNode/virtual50, full attachment, controller clocks and gameplay
acceptance are unproven.

Fresh unfiltered normal and ASan/UBSan component runs each pass 2,752 tests from
262 suites, with exact inventories and no failures/skips. Leak checks are disabled.
No fresh engine or normal-input runtime course runs for this component change;
parent 42e3f4f912 has full normal/SAN component and engine coverage at its own source.
[Editable verification](M15-FORWARD-SCENE-PRECISION-VERIFICATION.json) pins sources,
checks, the broader native corpus, retained baseline and comparisons under
`scene-target-forward-precision-*`, `reference-root-first-sync-oracle-02` and
`reference-first-sync-target-baseline-02` in the S4 evidence directory.

A separate original velocity follow-up passes 24 sample contexts: pending sync
resets substantial XYZ velocity and all four packed lanes, while zero/tiny XYZ
preserves the fourth lane. It covers ordinary/bodyT, active/inactive authored
flags, two angles and live 007F at scale 1. `reference-first-sync-velocity-oracle-03`
corrects 01 metadata and adds explicit eight-lane assertions; 02 retains a probe
construction failure. This does not establish threshold boundaries. No engine
pending-sync state machine or runtime precision caller is added here.

Next implement correct native global-root initialization/initial capture and
first-sync velocity/flag ordering, establish one production traversal owner and
controller clock, and preserve safe native REFR-before-child/cell migration
publication. The remaining admission/property/node policies, forced World/script
continuation, loose save/fresh recreation, production melee/drop/effect wiring and
normal-input/FPS/fresh-process matrix remain open. Full M15S4 is active and incomplete.


## 2026-10-16 — native nonactor reference-root arithmetic checkpoint

`ESM4::referencePlacementRoot` implements the ordinary nonactor original 4D7AF0
X/Y/Z axis matrices and successive products, including the initial identity
multiplication. It keeps translation and absolute reference scale separate.
Single is the observed live default; Double supports the recorded 027F path.
Unknown precision, nonfinite consumed fields and |angle| >= 2^63 reject.
The existing NPC sine/cosine implementation moves unchanged into one internal
shared header, retaining the original 66-bit pi reduction for large angles.
Two tests pin ten original hardware vectors and validate consumed fields/range.

Original native i386 hardware probe `reference-root-placement-native-02` executes
4,152 contexts twice with identical outputs: 72 common emulated contexts, 1,024
seeded normal XYZ triplets, 256 raw float-bit triplets and 240 signed-zero,
subnormal, finite trig-range and scale edge contexts, under three control words.
Actual complete 4D7AF0 and scale getter/FABS/store execute; root virtual50 remains
a declared boundary. Both normal and sanitized production replays match all
1,384 live 007F and 1,384 original 027F contexts: 27,680 exact original R9/scale
fields plus 8,304 literal input-translation assertions per build. The 1,384
extended 037F contexts also match Double diagnostically. This does not implement
whole GenerateNiNode, registration, first sync or controller/physics scheduling.

The shared-helper NPC regression independently replays all 12,348 original
hardware contexts in both builds: 12,316 finite R9/scale results match exactly,
32 unsupported nonfinite outputs reject, and wrong-bit/missing/duplicate-row/
malformed controls reject. Fresh full normal and ASan/UBSan component checks
pass all 2,754 tests from 263 suites, with exact inventories and no failures or
skips. Leak detection is disabled. No fresh engine or runtime course is claimed.

[Editable verification](M15-REFERENCE-ROOT-PLACEMENT-VERIFICATION.json) pins the
source fingerprint, code, native and comparison hashes. Evidence is under
`reference-root-placement-{preflight,normal,sanitized}-01`,
`reference-root-placement-native-02`, `reference-root-placement-compare-02` and
`reference-root-placement-npc-regression-01` in the ignored S4 evidence directory.
The preceding 3,912-context native/compare01 evidence remains retained.

There is no production caller of the new nonactor builder in this checkpoint.
Next initialize the private native global root and compensating drawing matrices
before prepared checkpoint capture, then integrate initial physical capture and
pending velocity/flag synchronization with one traversal owner. ObjectAnimation
still uses its legacy local root and World its root-only adapter. Native callback
ordering, placement/cell lifetime handling, all property/admission/node policies,
loose save/fresh recreation and the full production melee/drop/effect and
normal-input/FPS/fresh-process S4 gates remain open. M15S4 is active and incomplete.


## 2026-10-16 — manual NIF pending-first-sync phase checkpoint

`NifSceneTraversal::Callbacks::mFirstSynchronization` now handles pending 0x40
explicitly, even with inactive bit0. Own property/node controllers finish first;
the animated native world and drawing matrices publish before the handler, with
immediate children still at their preceding caches. The physical owner must reset/
synchronize motion and clear only 0x40 atomically on success. Metadata is reread
and checked before mProjected and child callbacks. That first pass performs no
ordinary physical-to-scene read or bit8 author write. Missing handlers still
reject. Exceptions leave completed controllers/current cache published; they do
not imply rollback of external physical work. Four tests cover the six original
flag pairs, phase/cache ordering, active/inactive behavior, retry, bad metadata
transitions and raw procedural-cache preservation under bit8.

Actual OSG traversal plus forward-conversion replays match all 512 original 007F
and 512 original 027F contexts per build: 33,792 exact original float fields and
1,024 exact collision flags. Five separate engine invariants per context verify
one force handler, no ordinary physical read, zero physical change mask, current
cache visibility and unchanged author matrices. The 512 extended 037F contexts
also match Double diagnostically. Native initialized root and registered body-local
offset are fixture inputs. The callback computes a target and clears fixture flags;
it does not create/reset/step a Bullet body or implement scheduler mutable flags.
An empty OSG Group substitutes the literal native null slot. Native controllers
are empty; clock/interpolation, full GenerateNiNode and runtime acceptance remain
unproven. Both settled-traversal replays retain 512 supported frames/8,128 original
field checks, with drawing error 5.68434e-14 within 1e-9.

Fresh normal and ASan/UBSan full component runs pass all 2,758 tests/264 suites,
with exact inventories and no failures/skips; leak checks are disabled. Preflight01
passes 22 selected cases but retains an unbraced-GTest compiler warning, fixed by
braces before full checks. No fresh engine or normal-input runtime course runs for
this component-only checkpoint. [Editable verification](M15-NIF-FIRST-SYNCHRONIZATION-VERIFICATION.json)
pins sources and evidence under `nif-scene-first-sync-{preflight,normal,sanitized}-01`,
`nif-scene-first-sync-compare-01` and `nif-scene-first-sync-settled-regression-01`.

Next implement the scheduler's actual first-sync velocity/reset/pose/flag operation,
then connect native global root initialization and initial capture/draw setup with
one production traversal owner and verified controller clocks. Correct native
REFR-before-child/cell migration/lifetime handling remains required. Existing World
root-only and ObjectAnimation local-root adapters remain unchanged. All remaining
admission/property/node, loose save/fresh recreation, production melee/drop/effect
and normal-input/FPS/fresh-process gates remain open. M15S4 is active and incomplete.


## 2026-10-16 — guarded loose-physics first synchronization checkpoint

An explicit `PendingReference` admission mode now derives pending40 and selected
root0xC flags separately from immutable authored definitions. Default supplied-pose
admission retains its behavior. Scheduler/facade `synchronizeLooseObjectFirstScenePose`
waits for workers and validates exact body epoch, node/root/flags and a published
renderer link when supplied under the exclusive collision lock. It consumes world
R/T independently of separate Ni scale, invokes the actual staged forced-scene
operation with Single precision, then clears only40 and returns fresh metadata.
Old pending descriptors reject after success. Invalid inputs preserve motion and
flags; no REFR/node/local-cache writes occur. The physical operation reuses the
independently verified strict89EE00 threshold, preserving forces while refreshing
pose/interpolation/AABBs. Five worker-parameterized patterns add15engine executions.

Native i386 hardware revealed another live precision requirement: original4529E0
rounds FMUL before subnormal FSTP. The previous Double-only converter differs in
1,193 of5,158live cases/1,330fields. Explicit position precision now follows the
scene target's precision, retaining Double by default. Full native hardware02
executes15,474contexts twice identically. Both C++builds match5,158live007F and
5,158original027F contexts:30,948exact float outputs per build. The5,158extended
037F contexts also match Double diagnostically. Inputs include selected sensitive
vectors, seeded tiny/general finite triples and explicit edges. Two component tests
and an actual scheduler test pin eight literal hardware sensitivities.

Direct scheduler and combined root-builder/NIF-traversal/scheduler replays each
pass536original007F cases at workers0/1/2, or1,608contexts per build. Exact native
position/packed-velocity fields total17,688, with3,216before/afterflags. Combined
replay adds41,808exact root/target cache floats. Bullet's normalized representation
of the independently supplied native target quaternion agrees within1e-12, with
maximum observed error0; this is not native quaternion/basis bit identity or Havok
stepping equivalence. The combined replay reconstructs rootR9 with production Euler
code from literal angles/T/scale, computes actual OSG hierarchy worlds and invokes
the actual scheduler handler, refreshing its descriptor for flag reinspection.
Registered body-local offsets remain native fixtures. Physics is renderer-unlinked;
empty OSG Group substitutes a native null slot. No clocks, World callbacks, native
registration/property-copy replay or normal-input acceptance is established.

Final normal and ASan/UBSan checks each pass all2,760components/264suites and
1,610engine/122suites, with exact inventories and no failures/skips. Leak detection
is disabled; two pre-existing World-test compiler warnings remain. Preflight01's
owned build was stopped and recorded terminal before correcting the hardware-found
position gap; earlier sanitized component01 is not final-source coverage.
[Editable verification](M15-LOOSE-FIRST-SYNCHRONIZATION-VERIFICATION.json) pins code,
source fingerprint, checks and native/comparison hashes. Evidence is under the
`loose-first-sync-*` and `world-to-native-position-*` S4 directories. No new game
runtime course is claimed at this checkpoint.

Caller still schedules first sync before stepping. No production caller selects
the new admission mode or connects the manual callback yet. Next wire global root/
ReferenceWorld capture/drawing setup before prepared checkpoint capture, establish
one update owner and verified first-update/physics/controller clock order, suppress
duplicate viewer traversal, and support safe REFR-before-child reanchoring/cell
migration/lifetime guards. Existing ObjectAnimation and World retain their legacy
local-root/root-only adapters. Pending/save/fresh recreation, forced World/script
continuation, all property/admission/node policies and the remaining production
melee/drop/effect and normal-input/FPS/fresh-process gates remain open. Full M15S4
is active and incomplete.

## 2026-10-16 — native generation and registration cache ordering

The actual original Generate path exposes an initialization requirement before
production ownership can be wired. After reference R/T copy and recursive pending40
mark, complete4E3490 runs the ordinary nonactor update at time0/controllersFALSE.
This first collision force consumes authored root scale and clears40. The later
reference-scale store and root virtual+50 update bounds without refreshing native
world caches. Registration re-marks40 and refreshes only a selected root cache to
final scale; child caches retain authored scale until their own updates. Plain
direct-child collision replacement into root bodyT additionally resets physical
pose to zero/identity. Earlier assumptions that registration always preserved that
pose failed and are retained in oracle03. Oracle04 asserts the exception and passes
all1,536contexts over four topologies, plain/bodyT, four flags, eight angles, two
final scales and three precision modes. Asset/model/property branches, special
reference flag0x100000, nonempty controllers, clocks and real Havok remain excluded.

`NifSceneTraversal::refreshReferenceRootCache` represents the selected-root cache
refresh without executing controllers, physics or REFR writes. Construction at
authored scale, final author-scale assignment and optional root-only refresh retain
native old-self/current-parent controller observations and compensate child drawing
matrices. Two tests cover mixed caches, callback counts and existing safety guards.
Full normal and ASan/UBSan component runs each pass2,762tests/265suites without
failures/skips; leak detection is disabled. No fresh engine test is claimed here.

Both replay builds match59exact fields in each of1,024supported registration cases;
512extended-precision cases also match diagnostically. Drawing maximum error is
7.10543e-15 within1e-9. The existing settled traversal corpus also passes512supported
frames/8,128field checks per build, with256extended diagnostic frames and drawing
maximum5.68434e-14. Native selection/R/T and registration snapshots are inputs;
the replay uses nonmutating observation callbacks and does not execute native
registration, bodyT replacement, controller interpolation or a game frame.
[Editable verification](M15-NIF-REGISTRATION-CACHE-VERIFICATION.json) pins source,
hashes, retained failures and evidence under `reference-generation-order-oracle-*`
and `nif-registration-cache-*`.

Next connect native global root preparation with the newly verified authored-scale
early capture and selected-root cache refresh, including the replacement-pose
exception. Production update ownership, safe REFR-before-child migration/lifetimes,
controller clocks/physics order, remaining node/property/admission policies,
pending save/fresh recreation and melee/drop/effect/input/runtime gates remain
open. ObjectAnimation and World still use their legacy adapters. M15S4 remains
active and incomplete.

## 2026-10-16 — explicit root reference update and rendering migration

Manual NIF traversal now offers root-only `mReferenceUpdate` after final own
projection/first synchronization and `mProjected`, before child controllers. The
owner receives final writeback/mask and settled flags and decides REFR publication
eligibility. Only this callback may change rendering ancestors during a pass.
Traversal validates and leases the new invertible unique path, compensates root
drawing against unchanged native caches, rebuilds the visitor's borrowed path and
invalidates old/new Skeleton palettes before releasing old ancestors. Children
retain their old native/drawing world until their own update. This utility does
not itself retain a REFR or implement World movement/cell publication.

Five tests cover ancestor release during migration, callback order/current paths,
old child caches, same-traversal skinning invalidation, unauthorized placement
mutation rejection, singular placement, callback exceptions/retry, hierarchy
mutation, reentrancy and pending/settled physical flags/masks before children.
Preflight01 failed compilation from a nonexistent test accessor; it was terminal
before correction and remains retained. Final preflight02 passes29tests. Full
normal and ASan/UBSan runs each pass2,767components/265suites, no failures/skips;
leak detection is disabled. No fresh engine check is claimed at this checkpoint.

Both builds replay512supported original precision frames/8,128field checks with
no mismatches, plus256matching extended diagnostic frames. Every root publication
moves to a fresh rotated/scaled/translated synthetic rendering parent; every
controller checks its current ancestor path. Maximum drawing error3.55271e-13 is
within1e-9. This compares preserved native caches against an independent existing
corpus; it is not original World cell-migration or controller-clock evidence.
[Editable verification](M15-NIF-REFERENCE-REANCHOR-VERIFICATION.json) pins source,
checks, retained failure and `nif-reference-reanchor-*` evidence.

Production ObjectAnimation/World adapters remain unchanged. Next connect verified
native generation/capture and selected-root registration ordering, then one actual
manual traversal owner with native controller clocks/physics order and safe live
REFR migration. Remaining property/admission/node, pending save/fresh recreation,
melee/drop/effect and normal-input/FPS/fresh-process gates remain open. M15S4 is
active and incomplete.

## 2026-10-16 — live original dropped-reference clock and selective dispatch

An isolated original-game session now identifies the ordinary dropped weapon's
actual update context. Original executable SHA remains a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6.
The copied pristine save and INI/plugins were verified and restored exactly;
normal Quit/Exit Game completed, the wrapper returned0, and task-started game,
Steam, Xvfb and GDB processes are gone. Native game exit status is not separately
captured. Declared console setup added/dropped stock WEAP090615; this does not
count as normal-input item-drop or gameplay acceptance. Two Steam startup failures
and interrupted/misfiltered debugger attempts remain retained.

All256scene-wrapper samples use007F. Actor animation time, world-branch B33A30
and projectile frame delta B33E9C are different clocks. Direct original REF-extra
identity (getter4B24E0, RTTI B35ACC) associates the dropped model with WEAP090615
and dynamicFF000800, independently of its display name. Two early full-node
entries have time0/controllersFALSE and no collision on the root yet; their
sampler was manually stopped, so no full initial-generation trace is claimed.

The settled root sample has128events:94virtual+64/4A0D50 entries,12base70A190
entries,11world-transform70C120 and11collision897510 entries. Capture stops
partway through the last selected pass. All106outer/base time arguments exactly
match B33A30; all128events have selector0 and collisionflags0xD. There are82outer
returns before any base/transform/collision entry, followed by11complete observed
sequences and one partial sequence. This establishes conditional dispatch; the
specific visibility/model/state predicate was not independently isolated.

Static instructions confirm the integration requirement: selected70A190 uses
nodeflagbit3 for controller work, bit2 for world projection and child bits1/4 for
virtual+64/+68 selection. Outer4A0D50 can skip that base pass. Clock442546 stores
B33A30 after adding a float input;4425B7 passes it to a world branch, while443315
resets it on another path.8984A0 is invoked before that clock addition; its exact
physics role and full frame order remain unverified. An unconditional ordinary
full-downward traversal on every frame would miss these policies.

[Editable observations and probe manifest](M15-ORIGINAL-REFERENCE-UPDATE-CLOCK-OBSERVATIONS.json)
pin hashes, arguments, source scripts, retained failures and cleanup under
`original-reference-clock-01` and `reference-update-clock-audit-01`. Debugger
pauses affect frame deltas; no interpolation, FPS, contact, Havok equivalence or
fresh-process gameplay acceptance is claimed. No C++ change occurred at this
evidence checkpoint. Next verify native selected-node/visibility policies and
clock lifecycle/scheduling, then connect generation/capture and one production
owner. Legacy adapters and all remaining M15S4 delivery gates remain open.

## 2026-10-16 — ordinary selected and rigid NIF traversal policies

Manual NIF traversal now supports Selected/Rigid modes with explicit live node
flags; Full remains the default. Root entry is explicit. Child bit1 admits the
subtree, bit4 selects rigid dispatch, and a rigid pass propagates that mode to
entered descendants. Original47C930 revealed a key distinction: node controllers
always execute on an entered selected node, while bit3 gates only property work.
Projection bit2 is reinspected after controllers. Property callbacks may replace
the own controller list, which is then read fresh with callback leases retained.
Rendering scaffolding inherits native property policy, including OSG drawable
callback forms/order. Skipping projection leaves pending physics flags/cache and
REFR publication untouched. Enabled passes retain first-sync/settled writeback,
child-cache compensation and explicit reference migration before children.

An independent original-instruction matrix executes7,680contexts over all32root
and32child low flags, two entry modes, three precision modes and extra controller
flag mutations. Actual ordinary selected/rigid/controller-dispatch/composition/
child/bounds instructions execute. Property/own controller suppliers are declared
instrumentation; they record cached contexts and write chosen author translation.
Both C++builds match5,120supported contexts/1,575,680exact fields and2,560extended
diagnostic contexts. Drawing error is0within1e-9. This compares modes, callback
order/contexts, author/world caches and flags, not native model bounds or culling.

Six tests cover gates, rigid propagation/sibling restoration, current controller
list and mutable flags, renderer scaffolding, malformed policy and pending physics
retry/settled reads. Final preflight02 passes35tests. Full normal and ASan/UBSan
runs each pass2,773components/265suites with no failures/skips; leak detection is
disabled. Full traversal with synthetic parent migration also still matches512
supported frames/8,128field checks per build plus256extended diagnostics; drawing
maximum3.55271e-13within1e-9. No fresh engine check is claimed for this component
checkpoint. [Editable verification](M15-NIF-SELECTED-TRAVERSAL-VERIFICATION.json)
pins code, native probe manifest, fingerprints and `nif-selected-traversal-*` /
`node-selected-traversal-oracle-01` evidence.

Follow-up static inspection shows8984A0 only sets global time parameters; its
position before clock accumulation does not establish simulation stepping order.
The actual producer must still derive live node flags, resolve model-specific
outer dispatch/culling, initialize/capture in native generation order, own the
per-world clock lifecycle and schedule one traversal with real World reference
lifetimes/migration and physics order. Legacy ObjectAnimation/World adapters and
remaining property/admission/save/recreation/melee/drop/effect/input/runtime gates
are unchanged. M15S4 remains active and incomplete.

## 2026-10-16 — preserve physical NIF helper records through production loading

The production loader now scans complete Oblivion20.0.0.4/20.0.0.5 hierarchies
before admitting physical roots. Every AV record in a hierarchy with bhk collision
retains its transform and dynamic optimization protection, including static
helpers and nonphysical sibling branches. Flattening those records would remove
identities and author locals needed for native controller/cache observations.
Immutable source record-type and full parsed flags metadata accompany each node;
the owner must derive separate live update flags. This does not add unsupported
billboard/switch/other model policies or a production traversal caller.

Two tests exercise both versions, production-style optimizer permissions, actual
CopyOp clones and actual manual traversal. All four root/helper/physical/sibling
records retain identities/locals/type/flags. One clone's cached physical target
moves106.75to206.75 while source/second clone remain6.75 without projection
handles. Nonphysical Oblivion and existing Morrowind controls retain static
identity-root optimization. Preflight01's enum initializer compile failure and
preflight02's incorrect assumption of private OSG userdata ValueObjects are
retained. Authored metadata is read-only and may be shared; live flags are owned
separately. Final preflight03 passes42tests.

Fresh normal and ASan/UBSan builds each pass all2,775components/265suites and
1,610engine/122suites with complete inventories and no failures/skips. Leak
detection is disabled. [Editable verification](M15-NIF-PHYSICAL-HIERARCHY-VERIFICATION.json)
pins source, fingerprint, binary hashes and `nif-physical-hierarchy-*` evidence.
This is loader/optimizer/clone/traversal integration, not a new runtime course.

The preservation applies to all physical Oblivion models and prevents static
flattening/removal there; its rendering cost remains part of real-content/FPS
acceptance. Legacy adapters are unchanged. Native generation/capture/replacement,
live flags, outer model/culling policies, clock lifecycle/physics order, one update
owner, real reference/cell lifetimes/migration, pending save/fresh recreation and
remaining melee/drop/effect/input/runtime gates remain open. M15S4 remains active
and incomplete.

## 2026-10-16 — clone physical properties before reference collision relocation

The production reference loader/scaler now retains the original selected child
R/T, wrapper kind, body record and bodyT offset/quaternion as owned values.
Property copies scale the original bodyT translation before recomputing root
relocation with unchanged Ni child locals. Previously the combined relocated
offset was scaled, incorrectly scaling the child translation too. Plain wrappers
contribute an identity old offset. Repeated/independent copies preserve these
inputs; rejected identities and nonfinite used fields leave the source unchanged.
Reference relocation now explicitly uses nativeSingle arithmetic, including
unnormalized matrix extraction and old-offset quaternion frames. Existing
DefaultDouble callers retain their behavior.

Tracing the actual WEAP factory resolves an earlier initialization ambiguity:
primary vtableA45354/+114=4B2320 dispatches +EC=4B3750 before Generate copies REFR
R/T. The factory footer4B3BEC..4B3C37 resets the root to I/T0/scale1 and clears
hidden bit0. Clone properties receive the resolved reference scale; matching
already-scaled cached templates instead use clone factor1. Ordinary node copying
preserves all13 local words. The earlier generation-order04 authored-root-scale
1.75 fixture remains a scoped low-level fixture, not actual factory initialization.
Future early-cache capture must start with the normalized unit root. Existing
ObjectAnimation root neutralization is consistent with that footer; native global
R/T/cache ownership is still unwired.

Two independent original-instruction corpora execute1,920contexts. They combine
the actual factory footer, bodyT copy-member scale prefix/cached getter, reference
R/T copy, empty-controller early pass, final scale, full registration and forced
later update. The prefix's resulting cached translation is explicitly supplied
to the output wrapper; full model/heap/shape/constraint cloning is excluded.
Both C++builds match640supported single-precision contexts/4,480exact position and
quaternion fields. The1,280extended-precision diagnostic contexts have512differing
cases/1,136fields, consistent with explicitSingle production selection. The
preserved DefaultDouble helper also matches its prior288contexts/2,016fields per
build. WorldA95CF4 and WorldMA97614 both dispatch +94 to88C740. These are scoped
ordinary-node/sphere fixtures with declared boundaries and no real native World
insertion or physics/gameplay acceptance.

Three component regressions cover literal native bits, parsed-file destruction,
repeated copies and rejection without source writes. A new engine admission test
supplies native literal body poses and checks recovery of the reference root.
Two actual NIF factory/physics-step tests had assumed collision centers coincide
with scaled child geometry. Their failed run is retained; corrected assertions
check the native collision offset separately while preserving geometry, root,
motion and velocity checks. The redundant sanitizer build was stopped and its
helper reached terminal before edits. Final normal and ASan/UBSan runs each pass
all2,778components/265suites and1,611engine/122suites, with complete inventories and
no failures/skips; leak detection is disabled. Native setup/ABI failures, numerical
red baselines and precision failures remain in their original directories.
[Editable verification](M15-REFERENCE-CLONE-SCALING-VERIFICATION.json) pins source,
corpora, hashes, corrected context counts, reports and limits.

No normal-input runtime course is added here. Initial physical capture and plain
replacement reset, live flags/model/culling policy, clock lifecycle/physics order,
one traversal owner, real reference/cell migration, save/fresh recreation and
remaining melee/drop/effect/input/runtime gates remain open. M15S4 remains active
and incomplete.

## 2026-10-16 — prepare private ordinary selective update flags

The manual traversal now prepares live selective flags from an explicit instance
policy for ordinary NiNode/NiAVObject/NiGeometry. Transform demand propagates down;
update demand propagates up; property-controller demand stays local. Controller
presence includes inactive controllers. Node bit4 describes descendant rigidity
before its own transform controller clears the rigidity returned to its parent.
A skinned geometry forces update/projection and returns nonrigid while retaining
the base leaf bit4 store. Other input flags remain intact. Prepared flags are
private; source userdata and controller/cache/drawing/physical state do not change.
Resolver/type failures, placement/hierarchy mutation and reentrancy preserve the
previous flag snapshot. Selected/Rigid traversal consumes prepared flags when no
explicit live flag callback is supplied; existing callbacks retain precedence.

An independent no-hook original-instruction corpus executes800contexts over
2,848nodes, including inactive/later linked transform controllers, property and
null entries, sparse child slots, skin, inherited demand and arbitrary prior flags.
Both C++builds match all4,448flag/return-value fields. Three new tests cover actual
traversal consumption, source preservation, atomic rejection/retry, recomputation
and override precedence. Preflight passes35selected cases. Full normal and
ASan/UBSan runs each pass2,781components/266suites, complete inventories, no
failures/skips; leak detection is disabled. No fresh engine check is claimed.
[Editable verification](M15-NIF-SELECTIVE-FLAGS-VERIFICATION.json) pins source,
corpora, hashes, reports and scoped native inputs under `selected-flags-*`.

The final-scale virtual+50 call is a bounds rebuild, not this flag computation;
selective flags use virtual+5C. The actual production resolver, inherited root
input and recomputation event/caller chain remain to be connected and verified.
No arbitrary OSG callback or authored flag is inferred to be a live native policy.
Model-specific overrides/culling, initial physical capture/replacement, clock and
physics scheduling, one production owner, real reference/cell migration, save/
fresh recreation and the remaining M15S4 melee/drop/effect/input/runtime gates
remain open. M15S4 remains active and incomplete.

## 2026-10-16 — preserve load flags and correct generated-reference assumptions

Physical TES4 hierarchies now retain an immutable low-WORD loaded flag snapshot,
separate from authored flags and private live recomputation. The original modern
reader clears bit4 and sets bits1..3. A no-hook original-instruction oracle covers
all65,536words; actual production loaders match131,072cases in each normal and
sanitized build over both supported versions. Clone/source preservation and the
load14-versus-empty-controller-recompute16 distinction are covered. Full normal
and ASan/UBSan runs each pass2,782components/266suites and1,611engine/122suites,
complete inventories, no failures/skips; leak detection is disabled.

Two isolated original-game probes correct assumptions in earlier entries. The
stock weapon's generated root is a synthetic BSFadeNode above the file root.
Factory normalization applies to that wrapper; all13asset-root local words survive
cloning and generation. Scb is removed before registration, leaving its null slot.
Plain collision moves from the asset root to the wrapper and becomes bodyT.
The native Hk entity/motion and physical XYZ survive real-World registration;
quaternionZ changes one stored float step. Thus earlier null-World/sphere reset
observations cannot justify unconditional physical zero/identity replacement.
The earlier statement that production raw-root neutralization agrees with the
factory footer is superseded: production must represent the separate wrapper.

The weapon root retains flags000E throughout generation and32outer updates;
collision flags become000D after registration. No selective recomputation occurs
in that interval. Thirteen save-load calls from6C9B54 instead use inherited=true.
A separate one-context original trace confirms that the early ordinary-reference
Full pass uses time0/controllers=false, bypassing controller evaluation while
still projecting scene/physics. These findings constrain the future owner;
neither authored flags nor an empty-list recomputation models generation.

[Editable evidence](M15-ORIGINAL-REFERENCE-GENERATION-OBSERVATIONS.json) pins
reports, hashes, archived-source inspection, corrected assumptions, retained
setup/test failures and probe cleanup. Original probes use declared console
add/drop setup; they are not normal-input acceptance. Private inputs and pristine
saves were restored, and owned processes exited. Production wrapper preparation,
real-World replacement policy, controller-disabled initialization, clocks/physics,
one traversal owner, lifetimes and remaining gameplay/restart gates remain open.
M15S4 remains active and incomplete.

## 2026-10-16 — support the controllerless early Full pass

The explicit traversal now supports native Full(time,false) through
`updateWithoutControllers`. It projects every ordinary node and retains physical,
pending-synchronization and reference publication behavior while skipping own,
property, scaffolding and drawable update callbacks. Selective flags are not
queried. Selected/Rigid retain their distinct native signatures and behavior.
The future generation owner supplies time0; this API does not install an owner.

An independent original-instruction corpus covers192contexts/576ordinary nodes
with sparse slots, different rotations/translations/scales and arbitrary flags.
Nonempty own/property lists point to unmapped memory; none is accessed, and no
47C930 call occurs. No functions are replaced. Both C++builds match all7,488world
cache fields exactly. Two regressions cover original literal early world caches,
callback suppression across drawing scaffolding, all-node projection, unchanged
locals, ordinary callback resumption and failed/retried pending synchronization.
Full normal and ASan/UBSan component runs each pass2,784tests/266suites, complete
inventories, no failures/skips; leak detection disabled. No fresh engine/runtime
course is claimed for this traversal API checkpoint.
[Editable verification](M15-CONTROLLERLESS-FULL-VERIFICATION.json) pins evidence.
Production wrapper preparation, physical initialization/replacement, clocks and
physics scheduling, one owner and remaining M15S4 runtime/restart gates remain
open. M15S4 remains active and incomplete.

## 2026-10-16 — retain ordinary transformed asset roots beneath reference wrappers

Reference definition loading now distinguishes an ordinary retained file root
from its generated reference wrapper. Own-controller presence or a numerically
nonidentity rotation retains the file root, including its R/T/scale. Translation
and scale alone do not determine retention; negative zero compares equal to zero.
The synthetic reference identity is disjoint from source records and equals their
count. Registration selection uses this prepared hierarchy: a retained root is
the literal first child, and its deeper descendants are not searched. Collision
on that retained root relocates to the synthetic target with the existing native
bodyT offset rule. The folded identity-root path keeps its previous alias mapping.

Detached ObjectAnimation preparation creates a private unit wrapper above a
retained root, preserving its locals, callbacks and source metadata. Cached and
carried models remain unchanged. The loose scene lease checks the retained-root
mapping. A new factory engine regression publishes an actual rotated-root NIF,
checks private/cached identity, steps physics and synchronizes the reference while
preserving the source local matrix. Two new component regressions and a corrected
rotated-root regression cover the policy, source preservation, native relocation
bits and absence of a deep collision search.

An independent312case original constructor branch corpus matches both production
loaders. Each build also parses the archived stock short sword and exactly matches
the13asset-root local fields and7registered bodyT offset/quaternion fields from
the independent original real-World capture. Its tiny nonzero rotation is enough
to retain the source root. Full normal and ASan/UBSan runs each pass2,786components/
266suites and1,612engine/122suites, complete inventories, no failures/skips; leak
detection disabled. Build/test fixture setup failures remain retained.
[Editable verification](M15-REFERENCE-ASSET-ROOT-VERIFICATION.json) pins code,
source fingerprints, native/captured evidence and all reports.

This checkpoint handles ordinary exact NiNode retention. Full BSFadeNode
construction/names/culling, special roots, folded-root flag/visibility transfer,
scabbard removal and native initial physical/clock/update ownership remain open.
A subsequent309case original full scabbard-removal corpus records162removals and
147absent results, including interior null child slots; it prepares later work
and is not a production comparison. The factory test is engine integration,
not normal-input/fresh-process gameplay acceptance. Remaining M15S4 migration,
save/recreation, melee/drop/effect/input/runtime gates remain open; M15S4 remains
active and incomplete.

## 2026-10-16 — detach native weapon scabbards before collision registration

The production carried-weapon drop path now requests a weapon-specific reference
definition. Its immutable source selector follows original480770: complete Scb
names match case-insensitively; a first FadeNode-space prefix commits descent,
including termination on a non-node cast, without retrying siblings. It uses
C-string semantics and preserves literal null slots. Source records remain
unchanged. The removed branch is absent from the prepared bind hierarchy and
its source slot is null for collision selection, instead of compressing later
children into an earlier native slot. Generic reference/actor loading is unchanged.

Detached model preparation removes the private Scb subtree before binding;
cached/carried models keep theirs. The loose scene lease includes this preparation
identity. Four component cases cover exact/extended/case/NUL names, limited descent,
malformed inputs/cycles, immutable sources/copies, generic-vs-weapon policy and
literal-slot collision selection. A factory engine case covers folded and retained
roots, subtree absence/cached preservation, lease checks, physics stepping and
reference synchronization. A removed branch containing physical bodies rejects
whole admission before publication; that content remains outside current support.

Both C++selectors match all317original full-function cases. Original detach,
parent/slot/refcount observations are retained, with only checked Windows
Interlocked boundaries and no original game function replacements; the C++source
selector itself is read-only. Both builds also match the archived stock sword's
selected Scb9 and20previously captured original transform/offset fields. A fresh
hash-checked sweep passes all8archived iron melee assets through actual parsing,
ordinary/weapon definitions and scale2copies;3select scabbards. This is archived
content evidence, not complete winning installed records/assets or gameplay.

Final normal and ASan/UBSan runs each pass2,790components/266suites and1,613engine/
122suites, exact inventories and no failures/skips; leak detection disabled.
Initial preflight components59passed; its engine build and the first sanitizer
build were deliberately stopped to fix a new assertion-macro brace warning.
Both helpers reached terminal before edits, and interrupted attempts remain.
[Editable verification](M15-WEAPON-SCABBARD-VERIFICATION.json) pins source,
reports, hashes, counts and limitations.

A separate one-context original experiment constructs the complete sphere motion
state before early generation. The supplied position/quaternion are verified,
but plain registration still resets in that partial-entity/null-World fixture.
Motion-state initialization alone does not explain its difference from the live
real-World preservation observation. No universal reset rule is implemented.
Native initialization/replacement, clocks/physics and one update owner remain
open alongside creature/power contacts, mastery/effects/reactions and the remaining
S4 control/view/FPS/normal-input/save/fresh-process gates. M15S4 remains active
and incomplete. The next priority is completing those live contact branches,
without treating additional isolated rule coverage as delivery.

## Standing-power contact and restart checkpoint

[The evidence index](M15-STANDING-POWER-VERIFICATION.json) records the exact
implementation hashes, independently executed original instructions, normal and
instrumented comparisons, test inventories and accepted runtime epochs. The
three-stage power phase is separate from the four-stage ordinary queue phase.
Schema49 persists it in C++ and Python; older saves keep an absent power phase
until timing adopts the new path, preserving contact consumption and action IDs.

Standing-power contacts now apply raw current integer skill mastery and native
multiplication order. Hand Health/Fatigue scale after the hand calculation;
weapon subtotal scales before integer AttackBonus. Native timing uses three raw
Start/Hit/End keys, strict comparisons, one phase transition per update and the
existing authored blend/sequence manager bridge. Directional effects remain gated.

Both builds pass2793 component tests and1615 engine tests; Python passes304.
Component results belong to the earlier enclosing failed/stopped runs: only
engine test expectations changed afterward, and final engine checks pass.
Normal and ASan/UBSan comparisons match1134 lexical cases,9720 power frame
updates and44 cross-language wire states per build. Sanitizer leaks are disabled.
Existing unrelated compiler warnings remain in the retained logs.

Normal and instrumented three-process courses save an actual keyboard-started
power before contact, restore the pending ID once, and reload completed state
without another hit. Target Health Damage is exactly -1.4199999570846558 at
windup and -4.96999979019165 after the resumed power. A separate normal course
saves consumed phase1/action3 recovery and finishes after fresh load without a
second contact. Rusty-shortsword input matches the original sequential damage
and condition oracle: total Health Damage -2.9005613327026367 and final condition
55.275001525878906, unchanged through a fresh process. Item/position/restraint
staging is declared; damage and contact come from production input/playback.

These courses use first-person configured keyboard input and disabled sound.
They check exact contact debits, while ongoing regeneration makes saved Fatigue
comparisons bounded. The source manifests and case inputs are under
`scripts/data/oblivion_compat/*standing_power*`. They are editable test sources;
generated saves, screenshots and original-game corpora stay ignored.

The original expenditure caller audit additionally confirmed a defect at that checkpoint:
5E07D0 limits spending to positive current Fatigue. The then-current melee/block
spending adapter still needed this cap and its eligibility policy; direct hostile
Fatigue damage must retain its separate ability to become negative. The2112-case
original caller corpus is retained in
`S4/native-attack-expenditure-caller-oracle-01`, but no production cap fix is
claimed here. The following expenditure checkpoint implements that bounded change. Full M15S4 remains incomplete.

## Native Fatigue expenditure — integration checkpoint

The prepared contact now uses the native expenditure request cap for attacker
and block spending. It rejects nonnegative requests, spends nothing at current
Fatigue <=0, and limits a negative request to positive current Fatigue before
modifier storage rounds. Direct hostile Fatigue damage retains its separate
negative-resource behavior. NPC/Player eligibility is admitted; Creature live
eligibility transitions and contact remain gated.

[M15-FATIGUE-EXPENDITURE-VERIFICATION.json](M15-FATIGUE-EXPENDITURE-VERIFICATION.json)
indexes the source hashes, independent native corpora, exact selections/counts,
runtime epochs, staged-input recipes and retained failures. Both normal and
ASan/UBSan component results pass2794 tests; their enclosing initial engine
attempts failed or were stopped for test corrections. Corrected normal and
instrumented engine runs each pass1615 tests, and normal Python passes307.
The2508-case original-instruction debit comparison passes exact float bits in
both C++ configurations. A separate1056-power-observation caller probe places
the contact boundary before spending on its selected original branch; it does
not execute the full contact body or prove all input/frame scheduling.

The previous executable reproduces the regression from an actual keyboard-
started pending power: current Fatigue2 becomes -33 and saved Damage becomes
-173. With identical root input and the same script-free no-regeneration GMST
plugin, normal and instrumented builds save Fatigue0/Damage-140. Target Health
Damage is exactly -4.452499866485596 and Fatigue Damage -4.016249656677246,
matching original hand arithmetic at the supplied precontact Fatigue2 followed
by the separately verified spending wrapper. Each course also starts one real
configured-keyboard ordinary miss at zero Fatigue and reloads completed state
without replay. Source/output input hashes, complete C++/Python payload identity,
prior inventories and unrelated AV/lifecycle fields are checked.

The zero-Fatigue miss inputs differ as declared: normal relocates Player outside
reach; the corrected instrumented variant preserves floor position, turns away,
and waits for a read-only command at3 simulated seconds before pressing attack.
The initial intended zero-Fatigue acquired hit missed as target position changed;
its failed evidence remains. A stale selection prohibition rejected a subsequent
normal manifest; an airborne instrumented input selected power instead of
ordinary and was stopped. The instrumented first epoch's initial process
observer selected the wrong executable; a corrected observer independently
captured the same live process, while the original failure remains recorded.
Failed scenarios are not counted as gameplay acceptance. The first instrumented
gameplay scenario passed with the observer recovery recorded separately.

The fixture generator's structural/parser checks and real TES4 engine acceptance
are separate from these contact/resource assertions. Only the two declared
regeneration GMSTs are authored by the generated plugin. No actor damage or
contact outcome is injected. Captures of the actual power hit show upright
Valen Dreth, drawn fists and an empty Fatigue HUD bar; post-save captures do not
establish an exact animation phase. Sound is disabled in these courses. ASan
leak checks are disabled. No zero-Fatigue acquired hit, Creature/directional
mastery, other device/view/FPS, full original scheduling or complete M15S4 gate
is claimed; the delivery audit remains incomplete.

## Direct Creature Fatigue eligibility — integration checkpoint

Physical contacts now share the native modifier request gate with command
wrappers. A false Creature+278 policy suppresses direct negative Fatigue as well
as expenditure. Eligible NPC/Player direct damage retains its ability to go
negative; the expenditure cap remains separate. This corrects the Creature
expectation retained in the preceding expenditure checkpoint.

[M15-CREATURE-FATIGUE-ELIGIBILITY-VERIFICATION.json](M15-CREATURE-FATIGUE-ELIGIBILITY-VERIFICATION.json)
records 1,280 original-wrapper observations, 1,088 observable direct-gate
comparisons, and 6,272 unchanged command-wrapper comparisons. Normal and
instrumented runs each pass 2,795 component and 1,616 engine tests. Player/NPC
runtime regressions retain Player Fatigue 0 and target Health/Fatigue Damage
-4.452499866485596/-4.016249656677246 with complete C++/Python payload identity.
Actual-Ptr Creature/Player tests cover negative/positive/zero requests, god mode,
atomic publication, native facade views, saved restoration and once-only action
consumption. Original final mutators/callbacks are boundary stubs in the new
wrapper probe; C++ service storage assertions are a separate layer.

These corrections do not admit live Creature attacks. Creature normal-contact
and playback guards, native eligibility flag transitions, damage/sound/reaction
integration and actual Creature normal-input/restart acceptance remain open.
The runtime regressions use the prior declared low-Fatigue/no-regeneration
fixture, first-person keyboard-originated pending power and disabled sound;
other view/input/FPS and full S4 gates remain incomplete.

Normal verifies one completed-contact process. The instrumented save captured
consumed power phase1/action3 recovery. Its first verifier incorrectly assumed
completed playback and failed; that script/result remain. A separate strict
recovery verifier checks the exact saved phase, then a fresh engine process
finishes without another hit/debit, preserving all compared resources and
inventories. Both complete payloads re-encode exactly. This is a bounded recovery
continuation, with the failed completed-state assumption recorded separately.

## Resident Creature model-derived Fatigue eligibility

The blanket false policy was only a construction default. A private original-game
probe observes stock Dog and Skeleton1 actor byte104 changing from 0 to 1 in the
model callback, independently of base creature kind. Original625850 searches
the root plus three first children for `bhkNiCollisionObject` ancestry. The
paused original Dog console case observes Fatigue 160 ->155. The pristine save
hash was preserved; Continue's precise loaded-save identity was not captured.
Input bytes/modes/timestamps and private-prefix process cleanup are verified.

The renderer now derives a transient reference flag from the retained base NIF.
A shared service accessor supplies resident commands, resource/frame updates and
physical contact eligibility, preserving the separate expenditure cap and direct
damage semantics. The stock Dog collision sits at the final admitted depth 3;
the actual loaded model and original live flag agree. See
[the verification index](M15-CREATURE-MODEL-FATIGUE-VERIFICATION.json) for the
3,072-case instruction oracle, 64-mask C++ matrix, expanded actual-Ptr tests,
source hashes and declared boundaries.

Shivering Isles supplies a second Dog skeleton with different bytes. Both
installed candidates qualify at depth3. The corrected archive audit records the
overlap and the failed uniqueness assumption; no specific runtime archive-choice
claim is needed for this invariant result.

Normal checks pass 2,798 component /1,616 engine /310 Python tests. Sanitizers
pass 742 `ESM4*` component cases and all 1,616 engine tests, with leak checks off.
Normal and instrumented two-process command courses both preserve Health 20 and
observe Fatigue 160 ->155 ->150. Script modifiers are -5/-10 and Damage remains 0;
the original console's channel is Damage. Complete C++/Python binary payloads
agree, with all unrelated native AVs, lifecycle/action ownership and saved
inventories preserved. New source recipes retain the executed manifests and
reproduce an unchanged master CELL plus one placed, unchanged stock Dog base.
Structural readback, compiler diagnostics, semantic inspection and engine runtime
acceptance are separate evidence layers; this is no editor/MCP validation claim.

The first optional-pointer debug assertion, canceled partial normal build and
sanitizer preload-order startup failure remain recorded. Corrected checks and
ASan-first runtime manifests pass. The room captures do not establish visible
Dog combat, audio or normal-input Creature contact. Creature controller/contact/
AI adapters, sound/reaction integration, nonresident flag retention without model
reconstruction and the other controlling S4 gates remain open. Status remains
**incomplete**.


## Creature natural melee — bounded implementation checkpoint

Spell-free, unarmed Creatures now choose an ordinary attack through the explicit
stationary CSTY policy, play their stock handtohand group, acquire the actual
physics/LOS target at RNAM reach, and commit native damage and capped Fatigue
through the existing contact transaction. Creature AI identity survives typed
save validation. Ordinary five-phase and standing-power three-phase playback
share the existing owner; natural damage does not receive the NPC power or
AttackBonus multiplier. The form's damage word is resolved afresh for each
query, including current Player-level scaling; a shared construction cache is
not authoritative. Native source/probe inputs are versioned in
`m15_creature_natural_contact_native_probe.json`.

The independent instruction experiment passes 5,120 original-executable cases,
including default/installed Fatigue settings, PCLevelOffset, both conversion
branches and both x87 precision words. Its dynamic cast, Player base getter and
Fatigue ratio are declared boundaries; post-selection mitigation/effects are
outside that probe. It does not prove original-game whole-hit acceptance.

Normal and ASan/UBSan checks each pass all 1,617 engine tests in 122 suites;
normal Python passes 313. Leak checking is disabled. Final C++ checks use
`creature-natural-melee-main-02` / `creature-natural-melee-sanitized-03` and
fingerprint
`29a4879a954f8c839c693a4f80f47218c44f504833633879b26a34373566669e`.
Final Python evidence is `creature-natural-melee-python-02`, including the
executed manifest copies. The AV42 test-data
correction was rebuilt and rerun in both final full engine suites.
No component implementation changed in this checkpoint. The integration case
covers negative/excess Fatigue, both loaded-model eligibility flags, independent
Health/Fatigue, ignored NPC bonus/power multipliers, fresh Player-level reads,
installed settings, AI restore and explicit unsupported spell/custom-sound paths.

The editable fixture clones stock Dog stats/reach/assets/sounds/inventory,
removes its script/packages/**SPLO**, and supplies one deterministic CSTY and
ACRE in the unchanged master CELL. Stock Dog SPLO181D13 is an LVSP, not an
absent SPEL. Removing it explicitly isolates physical contact; this is not an
unchanged stock Dog gameplay claim. Structural readback and semantic byte
preservation, clean fork compiler diagnostics, and actual runtime acceptance
are reported separately. No TES4 editor/MCP world-validation claim is made.

The normal two-process runtime course records Health 80->72->65, Creature
Fatigue 160->153->146 and Player Fatigue 140 unchanged. One contact/action/sound
pair occurs per epoch. F5 saves and fresh-process reconstruction preserve all
unrelated native AVs, bases/life, combat RNG and inventories; the entire binary
payload re-encodes byte-exactly in Python. This is completed-strike restart
coverage. Rendered PCM is nonzero and both hit-sound requests acknowledge
playback; it is not isolated-waveform or hardware-listening acceptance.
Instrumented runtime results and loaded ASan/UBSan/MyGUI library identities
are recorded in the verification index. The first continuation attempt exceeded
the SDL focus acknowledgement deadline of 10 seconds; a fresh process reuses
the pristine accepted first save with a declared 60-second receipt deadline.
A successful continuation initially captured maps before libubsan loaded; the
final retry adds a library-readiness barrier and preserves that incomplete
snapshot rather than weakening the library-identity check.

Retained attempts include a same-line GTest trace compilation defect, a mistaken
windup contact-pending assertion, unsupported offscreen mouse action, an expected
Health 70 derived from fallback rather than installed FatigueBase 1, and a stopped
sanitizer build missing explicit options. Corrected tests/runtime retain these
failures. Early Xvfb captures precede camera initialization/deferred control
application. SDL/GDB diagnostics locate the normal previous-frame Lua bridge;
no camera implementation change was needed. The corrected visual course waits
for simulation-time barriers before camera setup and engagement. Normal
`creature-natural-melee-visual-normal-12` shows the named Dog in distinct idle,
windup and contact poses, records Health 80->72/Fatigue 160->153, and saves pitch
1.171875. First-person torso occlusion and original pose/geometry parity remain
open. Its 30 FPS cap is a configuration input: observed native frame increments
are .2 seconds, not a proved 30-FPS matrix. Attempts 01-09/11 are retained
visual/diagnostic failures; 10 proves only post-contact visibility. No accepted
visual claim relies on screenshot entropy alone.

Creature victims/materials, armed sources, spell/contact effects, custom Weapon9
sound selection, nonpositive contact consequences, Rat power-only fallback,
directional/mastery effects and the full interruption/view/input/FPS/restart
matrix remain open. **S4 is incomplete.**


## Creature victim palette and force dispatch — rule foundation

The generalized pure hit palette now accepts the victim's raw Creature Actor
byte 104, selecting Bone only for2 and Fur otherwise. It preserves NPC Flesh,
armor/shield precedence, body suppression, ordered layers and weapon variants.
This byte must not be replaced by the TESCreature base creatureType. The prior
original loaded Skeleton observation is Actor 1/base 2; it selects Fur here.

Full original dispatcher-prefix execution passes 5,120 cases; a standalone driver
compiling production physicalcombat.cpp matches every ordered request. Two
failed stack-flag interpretations are retained; the corrected probe confirms
the existing NPC behavior. Normal components pass 2,799; ASan/UBSan ESM4* selection
passes 743 with leak checks off. Exact inventories and tested fingerprint
`f568b530d97b275ca4e6e650d915b2dcafe9c4ad2f1f9e838b6ab90b987f7b43` are recorded in
[M15-CREATURE-TARGET-SOUND-VERIFICATION.json](M15-CREATURE-TARGET-SOUND-VERIFICATION.json).

A 768-case original caller experiment also proves matching NPC/Creature force
arguments through their actual distinct AV wrappers, with declared AV/Fatigue
and Havok availability/application boundaries. It audits shared incoming/Health
callback identities; it does not execute a complete Creature reaction or apply
Havok forces. Incoming Creature adapters still reject their victim domain.
No new engine, visible/audio, saved impulse or normal-input acceptance is claimed.
These remain required before the Creature-victim S4 route is delivered.

## Creature victims through owned physical contact

Spell-free Creature victims now join ordinary unarmed and unenchanted weapon
contact preparation. Their native Health/Fatigue values, loaded-model Fatigue
gate and timed knockback publish through the same owned action as the attacker
cost. Unarmored contacts select Fur from canonical Actor byte 104 state; base
Creature type does not select Bone. Active Creature blocking and spell-bearing
contacts remain rejected pending their complete native consequences.

The new World case checks both model gates, exact independent original damage
and stored acceleration, read-only preparation, atomic modifiers, duplicate
rejection and byte-exact service restore of active force. Weapon preparation is
covered there; actual runtime below uses hands. Original instruction evidence
adds 7,680 hand-contact observations and 960/1,152 force/storage observations,
including actual 8907A0. Parent AV/Fatigue values and controller availability are
supplied boundaries; these probes do not execute whole-game reaction/physics.

Both full engine suites pass 1,618 tests in 122 suites; Python passes 313 tests.
Inventories match with zero failures/skips. ASan leak checks are disabled and
UBSan halts. Shared tested source fingerprint: `03ed798d50e3a69754dcd7b3a1f0fb127659419b428ab0e87fdb995db0456972`.
See [the verification index](M15-CREATURE-VICTIM-CONTACT-VERIFICATION.json).

Normal and instrumented keyboard courses each commit exactly one Player hit,
F5-save, then load that output in a distinct process and commit exactly the next
hit. Player Fatigue 140->133->126; Dog Health 20->18.579999923706055->17.170499801635742;
Dog Fatigue 153->151.28999328613281->149.58525085449219; Player Health 72 unchanged.
The seed is an unmodified prior accepted camera-course save whose Dog had
already spent 7 Fatigue. Complete payloads re-encode byte-exactly; unrelated
native values, bases/life, inventories and combat RNG remain unchanged. Fur and
Hand sound-manager requests succeed and captured PCM is nonzero; no isolated
waveform or hardware listening equivalence is asserted.

The first saves retain active knockback. Sanitizer first save additionally
retains committed action 2 in recovery at 0.5999994277954102; the new process
finishes it without replay before action 3. The later save has expired force.
This is bounded state/continuation evidence, not uninterrupted/restarted
trajectory equality or the complete recovery matrix. Captures retain known
first-person torso occlusion and do not show an enemy Health bar.

The initial normal continuation failed rounded log expectations 18.58/151.29;
actual full-precision queries were correct. Its failed report remains intact;
fresh retry01 uses exact expectations. Initial seed-description metadata also
incorrectly assumed fresh 160 Fatigue; scalar audit corrected the description
without changing payload. Remaining blocking/effects, recovery/knockdown,
mastery/directional, HUD and complete input/view/FPS/restart gates remain open.
**S4 remains incomplete.**

## Live melee target Health feedback

Successful Player native contacts now select their committed victim in the HUD.
The native profile uses fEnemyHealthBarTimer instead of the shared TES3
fNPCHealthBarTime/Fade dependencies. An independent original initializer
9E9E30..9E9E49 captures its name, storage B37120 and default 2.5 at the declared
constructor boundary 41BAE0, in both x87 modes; the installed master has no
override. The shared HUD displays full opacity for that lifetime. Original
layout, alpha curve and countdown parity are not claimed.

Actual normal and ASan/UBSan keyboard courses capture no enemy bar before a
hit, 56 yellow pixels after action 2 and 52 after action 3, then no bar after
simulation barrier 10. The 61-pixel client maps expected 92%/85% native target
Health to those fills. Dog Health 20->18.579999923706055->17.170499801635742,
Fatigue 153->151.28999328613281->149.58525085449219 and Player Fatigue 140->133->126
match independent damage inputs. Each contact owns one delta; native actor
values, bases/life, RNG and inventories otherwise remain unchanged, and the
complete F5 payload re-encodes byte-exactly.

Both builds pass 29 selected engine cases with exact inventories and no skips
or failures. Selection: `*Melee*:*Blocking*:*PhysicalContact*:*CreatureVictim*`.
Python passes 313 tests. Source fingerprint: `7f34794891533fa68e443e8e8684d830b26c20b8b3d9d1e0771fcc496e5b609d`.
Full 1,618-test engine coverage belongs to the preceding Creature-victim chunk;
the HUD change reruns the relevant selection. ASan leak checks are disabled.
The initial normal attempt caught the missing TES3 setting after the hit had
committed; it is retained as failed. The concurrent initial sanitizer attempt
was also stopped and is not accepted. Corrected 02 runs have no frame error.

See [the verification index](M15-MELEE-ENEMY-HUD-VERIFICATION.json), executed
manifests and native initializer protocol. First-person torso occlusion and
Creature geometry artifacts remain visible. Mastery, recovery/knockdown and
the full input/view/FPS/interrupt/restart gates remain open. **S4 is incomplete.**

## Stock blade contact with Creature and worn-item restart

The admitted spell-free Creature victim route now has actual stock weapon
coverage. The isolated setup adds and equips Iron Shortsword 000C0D through
native commands, then keyboard input readies and attacks. F5 saves the output;
a new process resumes with the same readied, equipped, worn item and attacks
once more. No direct damage or outcome commands are used.

An independent original probe runs full 484F80 damage query, 547240 wear and
5F3870/484850 condition publication for both x87 modes, four hit observations.
Source AV/Fatigue, weapon component/condition and RTTI/reference/publication
boundaries remain supplied; whole hit, spending and reactions are outside that
probe. Stock source damage 7/max condition 98/weight 8 and installed wear .06 give
Health damage 1.1637499332427979 then 1.1289068460464478, with condition
98->97.58000183105469->97.16000366210938. The second input condition comes from
the first original wear result; source Fatigue 140/132.1999969482422 is declared.

Normal and actual ASan/UBSan engine courses match both hits exactly. Player
Fatigue 140->132.1999969482422->124.39999389648438, Dog Health
20->18.83625030517578->17.70734405517578; Dog Fatigue 153 and Player Health 72
remain unchanged. Each process commits exactly the next action 2/3, alternates
the native onehand left/right KF and plays Fur/BladeFlesh layers once. Full
payloads re-encode byte-exactly; unrelated native values, bases/life, inventories
and RNG remain unchanged. The single new weapon stays equipped; only its exact
native condition changes after restart. PCM is nonzero, without a claim of
isolated waveform or hardware hearing equivalence.

Final manifests pass 313 Python tests, fingerprint `ca8ad827804690c7098b988860c17756690cf7983a79e14fbee6e77d607e221a`.
C++ is unchanged from 1361cef542; actual game hashes equal the accepted HUD
courses. The preceding full 1,618-test suites and 29-case HUD selections are
reused, not described as new runs. ASan leak checks are disabled. See
[the verification index](M15-CREATURE-VICTIM-WEAPON-VERIFICATION.json).
First-person torso occlusion and Creature geometry artifacts remain; this
ordinary unenchanted blade course does not close power/mastery/effects,
recovery/knockdown or the full input/view/FPS/interrupt/restart matrices.
**S4 remains incomplete.**

## Live weapon precision and stock blunt checkpoint

An actual original ordinary Mace query established x87 control word 007F; the
previous wider-intermediate weapon helper was one ULP wrong for its captured
inputs. The corrected helper matches 4103 full original arithmetic executions,
39 frozen arithmetic rows and 576 original entry profiles on both Player/NPC
bindings. An actual normal-input fork return has the same arguments and bits
as the original live query. Full engine inventories pass 1618 cases / 122 suites
normally and under ASan/UBSan; full components pass 2804 cases / 268 suites,
sanitized ESM4 components 748/56, and Python 313 tests. The first engine attempt's
one stale rusty-weapon assertion and the cancelled sanitizer build are retained;
corrected engine02 runs pass without weakened tolerances. Leak checks are off.

Stock unenchanted Iron Mace now has an authored distinct-skill course:
Blunt 20 versus retained Blade 10, three contacts across two real fresh-process
loads, normally and under ASan/UBSan. Exact Player Fatigue is 140  -> 131.5  -> 123
 -> 114.5; Creature Health 20  -> 17.625  -> 15.327033042907715  -> 13.105790138244629;
condition 140  -> 139.39999389648438  -> 138.79998779296875  -> 138.19998168945312.
The canonical skill override and equipped worn item persist. Native arrays,
unrelated bases/life/inventories and current combat RNG are checked, with one
owned action per process and no replay. Real Fur/BluntFlesh sound requests are
acknowledged and captured PCM is nonzero. This is bounded contact acceptance.

Current pipeline RNG preservation does **not** establish native reaction RNG
parity: damage-knockdown/stagger and mastery consumption/consequences still
need the original caller gates, atomic publication and real recovery. The
weapon-only precision change does not close those branches, other physical
math, directional powers, effects, interruption/input/FPS or named restart
matrices. Existing Dog face holes and lighting differences remain outside the
accepted first-person hand/weapon visibility checks. Full S4 stays incomplete.
See [the precision verification index](M15-WEAPON-LIVE-PRECISION-VERIFICATION.json)
for provenance, exact test selections, editable scenarios and runtime limits.

## Final weapon integer-addition follow-up

The final AV42 addition now preserves native FILD/FADD rounding as well as the
547070 weapon product. Both Player/NPC World queries reproduce the independently
established large-integer/tiny-damage corner before the fix and pass after it.
All 1152 original comparisons, 140 frozen rows, the six new owned-item queries and
previous 1152 entry queries agree. Full checks pass 2805 components, 749 sanitized
ESM4 components, 1618 engine cases in each configuration and 313 Python tests.

One new normal and one new sanitized third Mace contact load the previous
accepted second saves and verify exact continuity through F5. Historical first
two epochs are source lineage only. A read-only normal probe confirms the new
helper is reached by production animation/contact dispatch and returns the
expected native bits. Huge-bonus lethal contacts are not established by this
course. See [the follow-up index](M15-WEAPON-ATTACK-BONUS-VERIFICATION.json).
Reaction/mastery RNG and consequences, remaining physical arithmetic,
directional/effect/interruption/input/view/FPS and named restart gates stay open;
this is another bounded implementation checkpoint, not full S4 delivery.

## Live unarmed precision checkpoint — 2026-10-17

Read-only observations of normal mouse contacts in the isolated original game
confirm control word 007F through 5FF3CA -> 547280 -> 5FF41E. The fresh second
contact reads Hand 10/Luck 50/Strength 40, current Fatigue 133/base 140 and standing
Dog knocked state 0. Native Health/Fatigue output bits are 1068788350/1071265087,
one ULP below the prior wider-intermediate CPP results. Both actual output
pointers, getters, live GMST values and code identities are checked. Setup and
sampler failures remain recorded; the source/clone pristine save hashes remain
unchanged and exact input files are restored after normal game exit.

The hand helper now preserves per-instruction PC24 rounding and explicit float
stores, including the original subtraction order in fatigue. It agrees with all
4222 independently generated original arithmetic profiles; the old source differs
on 288. There are 190 frozen native rows plus the actual second-contact regression.
NPC and actual Player/Creature World queries check Fatigue 133 and all 256 signed
victim states without query mutations. All 2806 components, 750 sanitized ESM4
components, 1618 engine cases in each configuration and 313 Python tests pass,
without failures or skips. Fingerprint: `1f2199ce03f7497a10ecf6a8e311027f491f1c62c50e055464cc45fc1974c44a`. Leak checks are disabled.

Normal and sanitized engines each commit two ordinary unarmed contacts across
F5/quit/fresh-process load. Exact Player Fatigue 140 -> 133 -> 126, Dog Health 20
-> 18.579999923706055 -> 17.170499801635742 and Dog Fatigue 153 -> 151.28999328613281
-> 149.58525085449219 match original damage inputs. Full saved payloads re-encode
byte-exactly; unrelated native arrays, bases/life, inventories and current RNG
are preserved. Each process owns one contact and sound pair with nonzero PCM.
A read-only fork probe observes the production controller/contact call returning
the exact same second-contact bits as the live original. Saved resource rounding
alone could hide this discrepancy; the raw return check does not.

Both builds save completed attack strikes while the first timed knockback pulse
remains active. The inherited historical sanitizer residual-strike verifier fails
and is retained; the current verifier strictly checks idle strikes/action -1.
This is completed-strike continuation, not uninterrupted/restarted recovery
trajectory equivalence. Current screenshots show fists without torso occlusion;
Dog face geometry holes and lighting differences remain. Native reaction/mastery
RNG and consequences, remaining physical math, directional/effect/interruption/
input/view/FPS and named restart gates stay open. **Full S4 is incomplete.**
See [the verification index](M15-UNARMED-LIVE-PRECISION-VERIFICATION.json).

## Native reaction query and rendered-source checkpoint — 2026-10-17

The canonical World authority now exposes GetKnockedState to actual M7 commands
and CTDA 107 conditions. The full original command/evaluator corpus covers 30720
profiles: only MiddleHigh/High raw states 3/4 return 1; every other signed byte,
Low/MiddleLow or missing process returns 0. This translation differs from the raw
state used to suppress hand Fatigue damage. The null original evaluator stores 0;
the null command wrapper preserves its prior result, whose higher-level
initialization is outside that instruction probe. World tests cover all signed
bytes for actual Player/NPC/Creature/Low bindings, both condition comparisons,
unchanged actor arrays and rejection of unknown legacy Active state.

A read-only renderer source capture binds immutable NIF body geometry to the
visible scene's asset hash, records, bones and hierarchy. Owner lifetime, actor
rebinding, model reload, parser/scene cache changes and sequence-selected reaction
root changes are checked. This prepares geometry; no production contact currently
uses it to admit bodies or recover from knockdown.

Full checks pass 2806 component cases, 750 sanitized ESM4 cases,
1620 engine cases normally and under ASan/UBSan, and 313 Python tests,
with exact C++ inventory agreement and no failures/skips. Leak checking is off.
Tested source fingerprint: `8bdd8a2a8eaa4f06c6ad44aed809b3bbfa4bac2b0ce6f407ebd76acf867e0307`.

Normal and sanitized engines each perform two ordinary unarmed contacts across
F5/quit/fresh-process load. Standing Player/Creature queries return 0 before and
after each contact; positive states are established by instructions and World
tests, not this course. Exact Player Fatigue 140 -> 133 -> 126, Dog Health 20
-> 18.579999923706055 -> 17.170499801635742 and Fatigue 153 -> 151.28999328613281
-> 149.58525085449219 persist. Both saves follow simulation time 6 and have no
active strike or timed knockback. Source/output identities, complete native
arrays, inventories, one action per process, no replay, sound acknowledgements
and nonzero captured PCM are checked. All four captures were reviewed; fists
render without torso occlusion, while Dog face holes/lighting differences remain.

The independent 2560-profile post-damage caller audit also establishes outer
Expert-rank/positive-Speed gates, actual animation groups, the shared mastery
draw and paralysis clearing the knockdown flag before spell lookup. The earlier
600218 prefix forces its flag at post-contact integer Fatigue <= 0 when incoming Fatigue is
positive; the old corpus's negative-only prose was wrong at zero. Frame reaction
initialization remains a separate float Fatigue < 0 rule. These audits do
not implement consequences. Native reaction/mastery RNG parity, automatic body
admission, physical recovery, directional/mastery/effect integration and the
remaining input/view/FPS/restart matrix remain open. **Full S4 is incomplete.**
See [the verification index](M15-NATIVE-REACTION-QUERY-VERIFICATION.json) and
[editable probe protocol](../../scripts/data/oblivion_compat/m15_native_reaction_sources_and_queries_probe.json).

## Rendered body admission — implementation checkpoint

Strict World admission now joins the actual rendered NIF, actor/base/lifecycle
authority and saved physical cache. Failed asset restoration rolls back the new
renderer/body/capsule ownership and preserves the previous cache. Physics also
provides detached body preparation: private graphs stay out of collision queries,
can stage native motion modes, and publish once with explicit capsule policy.
Stale, foreign, consumed and competing candidates reject; removing a committed
body cannot revive an older candidate. The integration case exercises zero, one
and two physics workers, real ray queries, retained/suspended capsules, saved
asset restoration, rollback and unchanged actor values/lifecycle.

All 1621 engine cases /122 suites pass normally and under ASan/UBSan, with exact
inventory agreement and zero failures/skips. Leak detection is disabled. Tested
source fingerprint: `9c862ee9597b602f0df18ab35c87e0628f89060fd6300a5c0fbe95892d65ba30`.
Only evidence/protocol files and a release-API comment clarification follow.
Components and Python implementations are unchanged and were not rerun.

An isolated original-game normal mouse punch independently records raw reaction
states 0 -> 4 -> 3 -> 5 -> 6 -> 0 across 95 sampled frames. The actual Dog GetUp
IDLE00091FB8 uses slot5 and conditions IsLeftUp ==1 / GetKnockedState >0. A
separate read-only snapshot matches the body-controller input time to ActorAnimData
+94, the per-actor animation clock already persisted by this fork. Original
inputs/autosave were restored and pristine save/executable hashes stayed unchanged.
Debugger timing is not an FPS gate; only the recovered standing capture was reviewed.

9072 original-instruction IsLeftUp profiles establish the scoped Spine lookup and
strict world R22 >0 predicate, with no process/knocked-state gate. Missing Spine
returns1; Horse type4 selects Bip02, other actors prefer Bip01 with Bip02 fallback.
The first wrong-sign interpretation is retained as a failed attempt. This query
still needs its actual renderer-backed World/M7/CTDA adapter.

These APIs have no automatic contact/frame/GetUp caller yet. Explicit physical
discard is not established native recovery policy. Neither test suites nor the
original observation establish fork live knockout/recovery or recovery restart
equivalence. Full S4 remains incomplete. See the
[verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json) and
[editable probe protocol](../../scripts/data/oblivion_compat/m15_physical_admission_and_recovery_probe.json).

The named-pose follow-up reads current NIF controller/physical locals using exact
unique names, separate scale and verified Single composition. It ignores drawing
traversal projections and rejects ambiguous or unsupported scene paths. All2808
normal component cases and the32 selected ActorRagdollPoseTest cases under
ASan/UBSan pass (leaks disabled). The Animation wrapper passes syntax only; no
new linked engine/runtime acceptance is claimed. Fingerprint:
`36e0fcc890898f574fb252d8fe9614c2e1ae9adbc36e16076ddcfb5a9a4e0925`.
Canonical actor root/placement resolution, World/M7/CTDA285 and automatic recovery
integration still remain open. Follow-up details are in the same verification index.

## World-backed IsLeftUp and native Creature scale

World, M7 and both CTDA evaluators now read the current rendered native Spine
world R22 for IsLeftUp285. Horse type4 selects Bip02; other actors prefer Bip01
with Bip02 fallback. Missing Spine returns1, without a process/knocked-state gate
or resource-authority requirement. Cold actor queries preserve actor values,
lifecycle and custom data. The owned-NIF test applies a real renderer physical
pose, verifies the query and restores the animated target. Player first-person
still needs a retained canonical full actor root; the current guard rejects the
partial view skeleton instead of using it as a full actor.

Native Creature scale multiplies reference scale by BNAM, stores binary32 and
then takes the absolute value. The renderer now uses that stored product.
1615 independent original i386 profiles repeat identically;1523 finite outputs
match the production helper exactly and92 nonfinite results reject normally and
under sanitizers.109 frozen rows cover the helper, including live Single products.
The missing-BNAM fixture failure is retained.

All 1623 engine cases /122 suites pass normally and under ASan/UBSan on the final
query-classification source. The 2809 normal component and 751 sanitized ESM4
cases pass on the preceding source; only World classification and its integration
test change afterward, covered by full engine tests. All 313 Python tests pass
after the subsequent audit-label addition.
Leak detection is disabled. Source fingerprints, hashes and retained failures are
in the [verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json).

Both current engines complete two SDL scancode contacts across F5/quit/fresh
process load. The unloaded NPC06A837 returns1 before/after each contact without
creating resource/lifecycle authority or inventory state. The rendered Creature
queries complete alongside exact native Health/Fatigue, ownership, no replay,
unchanged unrelated state/RNG/inventories, hit sounds and nonzero PCM. All four
captures are reviewed; fists render clearly, while Dog face mesh/lighting defects
remain. This is a standing-target query/contact course, with no native standing
bone/pixel/FPS parity claim. Actual body admission on contact, body frame driving,
GetUp selection/playback, recovery restart and the rest of S4 remain incomplete.


Prepared renderer admission now retains the selected animation callbacks until
geometry and detached physics preparation finish. Publication validates the
actor and cell, local checkpoint, callback chains and child slots, then detaches
callbacks without rebuilding active animation groups. Invalid body inertia
rejects before renderer ownership; tests cover all three physics worker counts.
Stale, competing, consumed, foreign and expired renderer preparations reject.
Both full engine inventories pass1625 cases/122 suites on the same final source
fingerprint, normally and under ASan/UBSan with leak detection disabled. The
obsolete rebuild-count failure and intentionally stopped sanitizer build remain
in the [verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json).

Independent original instruction probes add237 model-path-key cases,3584 KF
predicate cases,2048 recursive-tree cases,1280 outer selector/model-bank cases
and40 root-registration cases. GetUp uses an ordered model bank with ASCII
case-insensitive equality and exact separators. KF suffix eligibility ignores
ANAM's low group bits and the selected leaf's skeleton-directory prefix. An
optional no-file selection blocks later roots, then the outer selector returns
null. Unrelated live roots without predecessors insert at the front, reversing
load order. Boundary stubs and corpus hashes are recorded in the index.
These results guide the actual recovery implementation; they do not implement
GetUp selection or prove normal-input recovery. Contact/resource/RNG/body/cache
atomic publication, per-frame driving, recovery playback/restart and full S4
acceptance remain open.


A second independent original-game fatigue recovery course completes the native
raw0/4/3/5/6/0 sequence with 97 sampled frames/96 controller frames. It verifies
actual process+118 controller/proxy identities and changing reference placement
during incapacitation. The full actor root follows that live reference through
raw4/3/5/6; late GetUp restores the reaction root's animated local offset. A fork
implementation must include reference/root synchronization rather than leave
placement fixed while bodies move. The new course selects native GetUpRight
IDLE91FB7; the prior course selected GetUpLift91FB8, confirming that one fixed ID
is not a valid universal expectation. Retained physics pointers do not prove a
capsule suspension policy. The raw3 capture is not clearly prone and does not
close a visual gate. Exact inputs, hashes, cleanup and limits are in the
[verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json). No additional fork
normal-input reaction gate is closed; M15S4 remains incomplete.


The compound native physical-frame API now stages blend and generated velocity
controllers together, uses generated force candidates before body drives, and
publishes controller clocks once. Scene rejection preserves body state,
activation, controllers, target metadata and the shared clock. Scheduler tests
exercise stale handles and rejected scenes with zero, one and two workers.
The tested source passes all 2811 component cases/269 suites and 1625 engine
cases/122 suites normally; all 1625 engine cases and 145 focused ragdoll cases
also pass under ASan/UBSan with leak detection disabled. Counts, source and
artifact hashes, selections and the retained test-compilation failure are in
the [verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json).
This is a frame API checkpoint. Automatic reaction admission/driving, native
reference/root motion and proxy policy, prepared frame timing, GetUp playback
and recovery save/restart still require integration and actual runtime proof.
