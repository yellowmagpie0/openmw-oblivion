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
| 4. Safe interruptions | Controller cancellation clears strike/input/posture before animation disable. Actors cancel Player and owned stationary intents on pause and control locks. | Verify all named transitions: weapon change/breakage, unequip, menu/control lock, stagger/knockdown, death, load, mount; release/held-input behavior and restarted ownership need live coverage. Existing guards alone are not proof of the entire matrix. |
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
