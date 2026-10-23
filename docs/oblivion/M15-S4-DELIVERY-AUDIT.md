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
| 3. Block, mitigation, wear, fatigue and reactions | `beginBlocking`/`endBlocking` own posture; ordinary contacts prepare values, conditions, RNG and timed knockback. | Basic equipped block prepares shield/weapon mitigation, separate Fatigue cost and sequential condition wear. Attacking Player/quest unequips and ordinary NPC world drops now publish with contact resources. Attacker/block spending now uses the native positive-current cap; direct hostile Fatigue damage stays separate. Drawn defensive weapon wear-to-zero now atomically withdraws posture and unequips retained weapons or publishes an owned NPC drop. Expert Counter and bounded Master Counter-first/immediate NPC disarm now have World integration; advanced unarmed zero-absorption defense now joins that route. Lower Hand to Hand general-hit reactions and complete Player/Creature/spare policies remain open. Initially broken shields and drawn weapons now follow the native positive-wear/mastery consequences; zero-wear mastery controls retain equipment. Later selected-shield wear-to-zero now prepares retention/drop through the shared transaction; its normal-input reachability remains open. Ordinary blocking-shield breaks now atomically unequip retained shields or publish an owned NPC shield drop with posture withdrawal. Body armor reaching zero remains equipped; selected-shield further wear now uses the attacker posture/base-Block gate. Complete proc/breakage/reaction publication and renderer/physics recovery once per window. |
| 4. Safe interruptions | Controller cancellation clears strike/input/posture before animation disable. Actors cancel Player and owned stationary intents on pause and control locks. Lifetime-guarded unequip notifications can now be staged for dispatch after controller/actor updates and retained across simulation rollback. | Native loose-body/render state now restores for the bounded ordinary NPC weapon-break course; complete remaining equipment and interruption transitions. Verify all named transitions: weapon change/breakage, unequip, menu/control lock, stagger/knockdown, death, load, mount; release/held-input behavior and restarted ownership need live coverage. Existing guards alone are not proof of the entire matrix. |
| 5. Actual animation, HUD and sound | Native KF timing, swish, unarmored NPC/Creature hits, equipped-block sounds and committed Player target Health HUD are wired in the controller. Weapon-block stock PCM and fresh-process replay checks pass. | The bounded selected Heavy-armor/blade PCM and fresh-reload course now passes. Remaining material/weapon crosses, unarmed-only/Creature sound branches and live shield audio remain open. Complete native reaction animation and related sound/HUD feedback across the required matrix. |
| 6. Command/query semantics | `OblivionCombatService` and World/script adapters share native values, process action and engagement. S3 accepts authority/persistence infrastructure. | Audit every melee-related command/query against the completed contact, mastery and reaction paths. S3 authority acceptance does not prove those later semantics. |
| 7. Minimal production stationary opponent | `updateOblivionStationaryMeleeAi` sets attack intent after production reach/LOS checks; authored deterministic CSTY fixture and prior runtime matrices exist. | Revalidate alongside completed Player blocking and recovery. It must continue through controller/contact, without injected damage or outcomes. Full S7 movement/style policy remains outside this item. |
| 8. Mastery, dropped items and narrow effects | S2 pure rules and prepared inventory/resource primitives exist. | Ordinary attacking-NPC weapon breaks now have physical/render restart evidence. Retained Player defensive weapon breaks now have normal-input and fresh-restart evidence; ordinary NPC defensive drops have World/render/physics integration coverage. Normal-input ordinary NPC defensive drops now have physical/render restart evidence. Body-armor wear to zero now has bounded ordinary-NPC normal-input and fresh-process evidence. Ordinary blocking-shield breaks now have bounded normal-input and physical restart evidence. Pickup now atomically removes its reference from the dropper list and has normal-input/completed-pickup fresh-process evidence in both build profiles. Later selected-shield break publication now has bounded original-instruction and World/render/physics integration evidence; its normal-input reachability and mastery drop/effect consequences remain open. Wire every melee/block/armor mastery branch to live contacts, dropped-item identities and required timed effects. Enchanted weapon contact currently returns nullopt; resolve the narrowly required combat hook rather than declaring all effects deferred to M16. |

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

The [block reaction instruction verification](M15-BLOCK-REACTION-INSTRUCTION-VERIFICATION.json)
adds 10,392 independently executed original-instruction cases at revision
`ba2c82618dc01f5dcd622b1961c1a52f1e434106`. These establish expectations for
the remaining live reaction integration; they do not close its runtime gate.
Native BlockHit, BlockAttack, Recoil and Stagger are separate groups. Accepted
BlockHit requests action6; accepted BlockAttack requests action2; Recoil
requests action7. A requested BlockAttack may resolve to BlockHit instead.
The native setter really writes the selected action and sequence pointer in
the bounded NPC High-process probe, and replacing blocking action6 with
Recoil resets animation channel1. The Player first-person notification remains
an inspected path, not a dynamically verified renderer handoff.

Zero absorption still calls the native block handler. With the recorded cost
fixtures, an empty-handed defender below Apprentice Block receives a Fatigue
debit20 even when absorption and raw damage are zero. BaseBlock25+ skips that
debit. The separate counterattack chance uses Expert HandToHand for empty
hands, or Expert Block plus a shield when equipment is present; its inclusive
threshold accepts draw0 even at chance0. These are distinct from the earlier
low-Hand zero-absorption recoil roll. Health/Fatigue application precedes the
later contact-tail reaction routing in the inspected caller. Keep the existing
ReactionOnly and Expert/Master guards until the complete action, input,
resource/RNG, audio and saved-sequence consequences are implemented together.

The [reaction-slot timing checkpoint](M15-REACTION-SLOT-VERIFICATION.json)
implements native subtype0/1 Start/End decoding and the active clamp slot's
completion decision. Start is strict; End is **inclusive**, unlike ordinary
attack phase advancement. Both decisions can occur in one update. Execution
invalidated an initial static reading that treated End as strict; the retained
failed harnesses are not accepted evidence. The production comparison matches
384 independent slot/key observations, and the component regression tests keep
44 immutable active-slot expectations from the original instruction corpus.

Both component profiles pass at tested source fingerprint
`21c5a7c68e632239df4cd227f243d0f2d0a3e6c3cd1fddeb001bb05c209d085c`:
2,941 normal tests and all 794 selected `ESM4*` tests under ASan/UBSan, with
exact inventory agreement and no failures/skips. Leak checking is disabled.
This checkpoint does not rebuild or exercise the engine application.

The accompanying original-instruction probes establish the remaining recovery
contract. Reaching End requests slot deactivation through the blend path; it
does not itself mean sequence state0. Positive retirement duration enters
state3 and anchors its interval on the first following sequence update, then
becomes inactive at end equality. Recoil/Stagger process action7/8 remains
through active and retiring states and resets when the sequence is missing or
inactive. Expired action6 can instead retain the channel1 BlockIdle sequence.
Retirement interprets a nonzero Blend byte as **signed**, unlike the existing
unsigned activation duration. Live playback ownership, the complete retirement
implementation and its saved-state/input/controller integration remain open.

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
simplified multiplication. If the selected armor item is the same shield
already worn by blocking, the later caller additionally requires the attacker's
action6 and base Block below Journeyman before a second wear. When admitted,
prepare it from the first stored condition and publish one final condition with
the original instance guards. The shield-selected-wear follow-up below corrects
the earlier unconditional admission; the old concatenated oracle omitted this
caller gate.
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


A further original normal-input course adds 94 reaction samples and 93 selected
controller samples through raw 0/4/3/5/6/0. Hardware watchpoints identify the
reference position setter and observe no writes to the proxy object's world
link or packed filter during the declared contact-to-recovery interval. The
same non-null `ahkWorld`, filter `01720014` and sampled broadphase handle
`000001CD` remain throughout. These observations narrow the native integration
requirements; they do not establish all collision responses or movement policy.
The actual root-gain getter returns 1, 1, 0.3359994888305664 and 0 during raw 4.
Failed contact/setup attempts, exact fixture/camera inputs and clean restoration
are retained in the [verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json).
Neither partially occluded capture closes a visual gate. This is independent
original evidence; fork reaction and recovery acceptance remain open.


Native body admission now explicitly selects object-local renderer Single
precision. The binding reads current native controller transforms, composes
placement outside the authored root, and projects physical root writes back
relative to that placement. Prepared admission and animated target sampling
retain frozen original `707370` rotation/position bits. The new mode also
ignores traversal-only projections and preserves local checkpoints. Legacy
object-local and global reference-root contracts remain distinct.
All 2813 component cases/269 suites and 1626 engine cases/122 suites pass
normally. All 1626 engine cases and 34 focused pose-binding cases also pass
under ASan/UBSan with leak detection disabled. Hashes and selections are in the
[verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json).
This covers renderer composition/writeback. The scene-to-body admission adapter
still uses its default Double conversion. Automatic contact/reaction-frame,
GetUp playback, recovery restart and full normal-input acceptance remain open.


Strict native physical admission now restores saved body/controller state into
the detached graph and prepares cache DTO/map storage before renderer or physics
ownership is published. Invalid saved assets reject while the selected
renderer callbacks and movement capsule retain ownership. Resource preparations
also guard the reference lifetime/placement, renderer, cell, scale, capsule and
competing handoffs. The cache preparation checks expected state and complete
actor values/life, then publishes by swap or prepared map-node insertion.
Older cache preparations stay invalid after release/recreation, equal batch
publication, clear, restore or owner move. The immediate cache-removal API
retains its allocation-free implementation.
Both full engine inventories pass 1628 cases/122 suites, normally and under
ASan/UBSan with leak detection disabled. Tests include rejected saved assets
with a throwing renderer rebuild hook and detached restore/capture/discard with
zero, one and two physics workers, plus stale/foreign/expired/competing cache
handles. The initial builds stopped to correct the immediate-removal allocation
regression and the later placement-test compilation failure remain in the
[verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json).
This resolves the earlier strict-admission snapshot/cache work that occurred
after resource publication. Physics registration still has its own fallible
routing/world-registration rollback. First-frame visible projection, compound
contact/RNG/reaction publication, native proxy/reference movement, GetUp
playback and full recovery/input/view/FPS/restart acceptance remain open.


An original normal-input course now follows all 26 collision bodies over 94
reaction samples and 93 controller samples through raw0/4/3/5/6/0. Collision
wrappers, body wrappers and underlying body objects keep their identities.
At the first sample, after contact but before Down, all bodies have motion6
and null world links. During incapacitation they have motion2/4 and non-null
`ahkWorld` links. When GetUp hierarchy gain reaches1 in raw6, all return to
motion6 and null world links before animation completion. Their motion objects
change identity; their body objects survive. The character proxy retains its
world link/filter with zero hardware-watch writes over the observed interval.
The wrapper motion setter's world calls are independently resolved as getters,
so they must not be implemented as removals. General per-body membership and
constraint policy still need tracing. A pre-contact baseline is also needed
before claiming these objects existed in untouched standing state.
The [editable protocol](M15-ORIGINAL-BODY-LIFECYCLE-PROBES.json) records hashes,
transitions, exact setup, cleanup and limits. The occluded captures close no
visual gate. This is original-game evidence; automatic fork reaction admission,
frame driving, GetUp playback and full M15S4 runtime acceptance remain open.


The native per-body admission branch is now independently executed in 2400
profiles across both x87 precision modes. Full8A48C0, actual extra-map lookup,
NiRTTI ancestry and generic world dispatch execute. Both blend gains at least1,
signed counter+24<=0 and globalBA7909=0 select deferred admission/removal while
retaining the desired world owner. Other cases retain or change registration
through the generic dispatcher. Motion type alone does not determine this
policy. Lower world operations and constraint followup are explicit boundaries;
counter/override production lifetimes and full constraint policy remain open.
The [protocol](M15-ORIGINAL-BODY-LIFECYCLE-PROBES.json) records exact profiles,
hashes, boundary calls, return/stack/register assertions and limits.


Native actor admission now carries live Single arithmetic through the initial
scene-to-body graph adapter, matching its prepared native renderer. The adapter
accepts explicit precision and keeps Double as the existing caller default.
Eight frozen original first-sync ordinary/bodyT cases cover graph origin/basis,
a real Single/Double difference, preservation of the default and invalid-mode
rejection. Both full component inventories pass2813 cases/269 suites, and both
full engine inventories pass1628 cases/122 suites, normally and under ASan/UBSan
with leak detection disabled. Source, selections and hashes are recorded in the
[verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json).
This closes the initial graph-adapter precision gap recorded above. Automatic
reaction frames, body membership through recovery, reference/proxy movement,
GetUp playback and normal-input/view/FPS/restart acceptance remain open.


The untouched standing baseline is now observed before attack input in a fresh
original-game course. All26 bodies and25 constraints already exist with null
world links/constraint owners, both blend gains1, requested/actual motion6 and
counter+24 zero. LiveBA7909 is zero; its independent setting identity is
bAddBipedWhenKeyframed:HAVOK. The 93 post-contact reaction samples retain every
body, constraint and endpoint identity through raw0/4/3/5/6/0. All bodies and
constraints register during Down and leave again in raw6 before animation
completion. Four hardware watches record the main body world stores at8CB652
and8CBE9B and zero proxy world/filter writes. Nine pre-contact controller
samples are separated from92post-contact samples. Exact setup, hashes,25
directed constraint edges, timing and cleanup are in the
[protocol](M15-ORIGINAL-BODY-LIFECYCLE-PROBES.json). This closes the pre-hit
allocation question for this Dog. Partial HIT/constraint callback ordering,
other actors and full fork reaction/recovery acceptance remain open; the
occluded captures close no visual gate.


The physics backend now retains graph ownership across explicit partial and
empty World registration selections. Bodies, constraints, controllers, packed
velocities and modes survive removal and re-admission. New members and routing
storage are prepared before old registrations are released; throwing additions
roll back without removing prior members. Native motion routing supports empty
owners, World steps and AABB refresh skip absent bodies, activation groups follow
selected constraints, and scheduler World-scene guards require body membership.
PhysicsSystem/Scheduler selection operations join workers and keep movement
capsule ownership unchanged.
All2819 component cases/269 suites and1631 engine cases/122 suites pass normally
and under ASan/UBSan with leak detection disabled. Six component cases cover
retention, sparse/empty selections, invalid identities/endpoints, failed body and
constraint additions, routing, activation and absent pose writes. Scheduler
tests exercise0/1/2workers, snapshots and World-scene denial; the existing native
admission World test also verifies capsule retention and changing body-ray
membership. Source, hashes and exact inventories are in the
[verification index](M15-PHYSICAL-ADMISSION-VERIFICATION.json).
This is the registration mechanism required by original05. Native partial HIT
and constraint-callback policy, the automatic reaction/GetUp caller and compound
contact publication remain open. Registration selection is not yet in save DTOs;
pose/controller restore preserves its live selection. Full recovery restart and
normal-input/view/FPS gates remain open.


Version 50 now persists explicit body/constraint World registration selections in
C++ and the independent Python save codec. An absent field preserves legacy live
selection; a present empty field requests no members. Shared single/group snapshot
bridges retain this metadata. Prepared registrations stage routing/topology storage,
admit all requested additions under the World lock, and publish only after the
whole actor group validates. Late admission failure rolls back new members while
retaining every prior pose, selection, body identity and shared clock. Cancellation,
owner destruction, stale generations and retry have focused coverage. The software
save/recreate course now restores one unregistered actor beside a registered
actor and compares sixty explicit controller phases across fresh graphs.

The corrected checkpoint passes all 2823 component tests and 1634 engine tests in
each normal and ASan/UBSan configuration, plus 315 Python tests. Failed initial
engine runs remain preserved: group capture had omitted the new metadata and two
malformed identity fixtures needed matching registration records. Detailed source
fingerprints, hashes, inventories and limits are in the
`registration_persistence_followup` entry of
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. ASan leak checks are disabled.

An additional 1080-case independent original-instruction corpus executes full
8A0A70 admission preparation and actual normal/forced endpoint registration.
With endpoint B's wrapper present and batch gate zero, a failed normal admission
is followed by a diagnostic and forced admission; a nonzero unsigned batch gate
skips both. Initially absent endpoint B inherits endpoint A's upper 16 filter bits
while retaining its lower 16. Native World operations, diagnostic allocation and
recursive associated-constraint callbacks remain declared boundaries. This does
not establish recursive topology policy, native physics or fork acceptance.

A separate 288-case corpus now executes actual 8A47C0/677C70 associated-list
iteration and 8A06E0 constraint dispatch on a declared directed chain with every
member initially absent and batch gate zero. An eligible child's admission
registers ancestors, forcing deferred parents, and adds constraints on recursion
unwind. Starting at a parent leaves descendants absent; a deferred starting body
does not traverse its list. Imported atomic reference operations, the owning World
getter, diagnostic allocation and native World operations are declared boundaries.
This proves that fixture's ordering and filter propagation, not a universal graph
closure, removal cascade or batch-flush policy.

Automatic contact-driven reaction/FSM/GetUp integration, partial HIT policy,
reference and capsule movement, every S4 mastery/effect branch, normal-input
recovery, first/third person and device/FPS coverage, and real recovery save/quit/
fresh-process continuation remain open. Full M15S4 is not delivered.


A new 4292-case original-instruction corpus executes the complete rigid-body and
constraint admission/removal wrappers, both constraint queue drains and native
reference retain/release helpers. Admission uses World-owner+1C; removal uses
+20. Body pending bits suppress subsequent requests, including an opposite
request; queued constraints retain once per request. Immediate and drained
constraint admission also require every non-null endpoint to have a non-null
simulation-island pointer. The constraint admission drain waits for the body-add
queue to empty; a retry of the same pending queue is covered. Removal return
bytes distinguish a requested operation from a successful lower World removal.

The exact matrix, hashes, boundaries, retained initial 4224-case pass and limits
are in `native_world_queue_instruction_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Lower native World operations remain
explicit boundaries. Mixed-phase scheduling, capacity flushes, rigid-body queue
drains, native body-removal cascades and actual fork reaction integration are
not established by this corpus. Static inspection identifies listener dispatch
before the observed native body World-link clear; concrete listener identities
and callbacks remain to be established. Production code is unchanged from
ab1a6921fe, so its prior full normal/sanitized/Python checkpoint still applies.
Full M15S4 and its normal-input recovery/restart acceptance remain open.


Original06 completes a normal-input Dog KO/recovery observation with 93 frames,
raw0/4/3/5/6/0, stable 52-node/26-body/25-authored-constraint identities and no
hierarchy-read errors. Four verified hardware watches identify authored
constraint-owner admission, island migration and removal. The constraint owner
tracks the body island pointer, then clears at 91ED60 during GetUp before the
main body's World pointer and island pointer clear. All captured writer windows
match the pinned executable. The native constraint vtable is A9A274; its virtual C
is 8D9A30. Captured identity plus static dispatch identifies the route through
8CB4E0 and91ED30; deeper debugger stack unwinding is not accepted as proof.

The native master array uses 28-byte internal records, while the slave array
uses 4-byte pointers. The observer's initial master-array pointer interpretation
is restricted to its first entry; later raw object-like fields are not listener
identities. Partial reinspection identifies native constraint instances beyond
the 25 authored NIF wrappers: up to 36 known instances in one frame, 12 additional
distinct pointer addresses across the course and one endpoint outside the authored body
graph. Native master counts total up to 54, so complete solver graph enumeration
and auxiliary-constraint semantics remain open. The existing authored graph must
not be presented as an exhaustive native solver graph.

The initial no-contact attempt, a missing display-tool attempt and unsuccessful
master-array reconstructions are retained. The accepted retry freshly reloads
the pristine save and declares an explicit target offset, orientation and mouse
hold after the initial PlaceAtMe placement differed. All game/Steam/debugger
processes and wrappers are closed; exact cloned inputs and source/clone pristine
hashes are restored. Protocol, identity hashes, scope and limits are in
`M15-ORIGINAL-BODY-LIFECYCLE-PROBES.json`. This is independent original evidence;
automatic fork reaction/GetUp integration and every remaining full S4 normal-input,
view/device/FPS and recovery fresh-process acceptance gate remain open.


The callback identified in original06 now executes independently in a 1548-case
original-instruction corpus: full 8DC1C0 body dispatch, actual A9A274 virtual C
8D9A30, native 8CB4E0 and full 91ED30 array removal. Star graphs with zero through
four edges cover both master endpoint choices, every removed body, two array
orders, managed/unmanaged references and three pending-operation depths. All
incident registered constraints are removed, including those whose authored
association belongs to the other endpoint; nonincident constraints remain.
Master compaction repairs every internal backpointer, slave swap-last repairs
its moved index, and native owner/internal pointers clear with exactly one
managed reference release. Body World pointers remain intact during this phase.

Constraint-info reporting, island statistics, optional listeners/pending replay
and runtime payload storage remain declared boundaries. This establishes the
native removal operation; it does not turn admission into an undirected closure
or enumerate every auxiliary constraint seen in original06. Matrix, hashes and
limits are in `native_body_removal_cascade_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Production code is unchanged, and
automatic fork reaction/GetUp plus the remaining full S4 acceptance gates stay
open.


The combined native physical frame now selects the observed live007F Single
arithmetic throughout scene/body conversion. Initial graph admission already
selected Single, but the following compound frame could switch back to the
legacy Double path. Eight frozen original first-sync fixtures reproduced that
mismatch before the fix. They now pass through actual owned Bullet bodies and
the compound frame; a separate frozen original reverse-bodyT fixture checks
exact scene publication. Existing low-level helper defaults retain Double.

Full normal and ASan/UBSan checks each pass 2824 component tests in 269 suites
and 1634 engine tests in 122 suites, with exact inventories, no failures/skips
and unchanged tested source fingerprint
`50bb7ca741e67c0b940e1e5464a74bee014e74225079921c9ae46d60b146f8d4`.
Sanitizer leak checks are disabled. The failed red reproduction is retained.
Source and artifact hashes are recorded in
`native_physical_frame_precision_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. This closes the conversion mismatch;
automatic reaction/GetUp integration, solver parity and full S4 normal-input,
view/device/FPS and fresh-process recovery acceptance remain open.


Original07 repeats the normal-input Dog KO/recovery course with the corrected
28-byte master/four-byte slave reader. All93 frames retain52nodes,26authored
bodies and25authored constraints without hierarchy errors. Up to51 native
constraints appear in one frame, including26 additional instances;3957 master
backpointers and3957 slave backlinks validate. Auxiliary data consistently uses
A9E038, whose actual virtualC at8F5290 returns11. One outside endpoint differs
from the native World's fixed-body pointer. Its scene association and auxiliary
solver/manifold semantics remain open; numeric type11 alone is not a semantic
classification or a reason to persist these objects as authored constraints.

Actual registration hooks record77 normal and52 removal requests with matching
returns. Initial admission gains are0.3360004425048828;26 raw6 requests remove
bodies at gains1. Twenty-five already-registered normal requests returnfalse.
All observed signed counters, owner operation gates and queue counts are0;
no forced-registration call occurs. This establishes immediate operations for
this course, while other scheduling/queued contexts remain separate evidence.
The four hardware watches repeat original06 writer PCs and removal order, with
captured instruction windows checked against the pinned executable.

The debugger detached, game and private Steam wrappers exited0, exact cloned
inputs were restored, and executable/master/source-and-clone pristine hashes
were reverified. Protocol, counts, hashes and limits are in
`original07_correct_native_arrays_and_registration` in
`M15-ORIGINAL-BODY-LIFECYCLE-PROBES.json`. This documentation adds independent
original evidence; the passing C++ precision checkpoint remains unchanged.
Automatic fork reaction/GetUp and the full S4 acceptance gates remain open.


The retained graph can now prepare explicit body removals with every incident
selected constraint, regardless of authored endpoint orientation or association.
The existing registration transaction holds removals until commit and releases
constraints before bodies. Shapes, bodies, joints, controllers and poses remain
owned for re-registration. Unknown/duplicate requests and stale preparations
reject; absent-body requests are accepted without inventing membership.

The frozen1548-case native removal corpus supplies15 remaining-edge masks.
A258-graph Bullet comparison covers zero through four star edges, every removed
body, all endpoint orientations and reversed joint order. It verifies staged
publication, exact remaining membership, object/pose retention, constraint-before-
body removal and re-registration. Additional cases cover malformed/stale batches,
discarded admission and already absent bodies. Both full component runs pass
2826 tests in269 suites without skips/failures; ASan/UBSan leak checks remain
disabled. Tested source fingerprint: `712d5554d716a6cb6eed2ace57b395c4d75b4dd1a29e47a3bd32e8043a1873e0`.

Hashes and limits are in `retained_incident_body_removal_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. This chunk changes no engine caller
or Python/schema. Native gain/counter and directed admission policy, automatic
contact/reaction/GetUp, and the full S4 normal-input/restart gates remain open.


Owned ragdoll bodies now retain their authored constraint associations in each
body's source order, including repeated references and references shared by
both endpoints. Unique joint objects keep their existing global source-record
order. This preserves the directed-list input needed by native associated
admission; the previous description discarded it while deduplicating joints.
The lists survive source-NIF destruction and property scaling. Full normal and
ASan/UBSan component checks each pass2827 tests in269 suites with no failures
or skips; leak checks remain disabled. Tested source fingerprint:
`69b21610e65bde1267d8aa707272f7e05943a8feab83cbfc1beaf7fc5cabd59c`.

A separate9216-case original-instruction corpus verifies full88F0E0/88F160
counter updates. Increment wraps32 bits and dispatches only when the old value
was0. Decrement wraps first, then clamps a nonpositive signed result to0; the
INT_MIN underflow therefore becomes positive INT_MAX without clamping. Actual
scene flag/update requests depend on rawBA7A8C and flags40/1. A body registration
request additionally requires wrapper/native-body presence, filter low6bits8
and a remembered owner. It uses normal registration. Downstream scene and
World operations are explicit boundaries, and caller/lifetime semantics remain
open. These results must not be replaced by a guessed saturating hold count.

Provenance, editable matrix and check hashes are in
`ordered_native_association_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Neither ordered metadata nor the
counter oracle is an automatic admission/reaction/GetUp executor. Full S4
normal-input and fresh-process recovery acceptance remain open.


Typed physical registration rules now reproduce the verified body admission and
blend-counter decisions. Body plans distinguish removal, addition, remembered-
owner intent, gain suppression and the native return value; true is not treated
as World-operation success. Counter steps wrap32 bits before signed tests,
retain scene flags and request normal registration with resolved body/filter/
owner context. Invalid enums and nonfinite mapped gains reject. The functions
publish no scene, World or ownership state.

A standalone normal and ASan/UBSan comparison passes1968 supported original
body-admission profiles and all9216 original counter profiles. Another432 rows
verify the declared rejection of nonfinite mapped gains. Expected values come
from preserved original-instruction corpora, not production helpers. Both full
component configurations pass2833 tests in270 suites without skips/failures;
leak checks are disabled. Tested source fingerprint:
`056560ada67d61fc22158b41a5c7109b5efa48831c6b49029a48d46321cd395c`.

Hashes and scope are in `typed_native_registration_rules_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Recursive directed graph admission,
counter ownership/persistence, automatic contact/reaction/GetUp and all full S4
normal-input/restart acceptance gates remain open. This component-only chunk
adds no engine caller or Python/schema change.


The native registration planner now follows ordered authored associations with
normal and forced endpoint requests, filter inheritance, constraint admission
and incident removal. It stages a separate graph and preserves remove/re-add
operations and the filter at each admission. Existing same-World membership
skips list traversal; an association at endpointB does not admit endpointA by
an invented undirected closure. Repeated references retain request order while
registered constraints retain unique membership.

The expanded original-instruction probe completes14144 cases across directed,
shared, repeated and cyclic lists, both association endpoints, every initial
body mask and every valid initial constraint subset. The normal and ASan/UBSan
planner drivers match complete request traces, admission-time/final filters,
remembered owners, memberships and returns. Another256 prefixes reach the
declared256-event observation bound; they are retained as nonterminal evidence,
not golden final states or proofs of infinite recursion. The planner rejects
them at its own operation bound without publishing partial state. The initial
instruction-budget failure remains preserved.

Both full component runs pass2839 tests in270 suites without skips/failures;
leak checks are disabled. Tested source fingerprint:
`16f678ebaebed2a03107874e64e59e1f2ac014dd0a02c3673e503e6318c4a1a0`. Hashes, editable matrix and explicit lower
World/solver boundaries are in `directed_registration_plan_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. The actual physics executor must
preserve remove/re-add effects even when final membership agrees. This planner
adds no engine caller, physical publication or persisted registration fields;
automatic contact/reaction/GetUp and full S4 gameplay/restart gates stay open.


The retained physics graph now publishes changes to its configured native
internal collision filters. Preparation stages complete replacement ignore
lists and reserves capacity while preserving collision decisions and contacts.
Commit checks owner/World lifetime, filter and registration epochs and exact
ignore-list snapshots, then uses reserved storage. Other ignore references,
including duplicates for the same peer, survive the policy update.

Actual Bullet contact tests verify that disabling an owned pair releases its
cached manifold immediately, re-enabling recreates contacts without movement,
and outside contacts retain their existing manifold. Invalid layers, asymmetric
policy, malformed identities and stale/foreign/expired tokens reject. Deferred
graphs can prepare filter decisions before their World publication.

Full normal and ASan/UBSan checkpoints each pass 2,844 component tests in 270
suites and 1,634 engine tests in 122 suites, with exact inventories and no skips
or failures. Leak checks are disabled. Tested source fingerprint:
`8e0f0ec742f0d608d3f368daf06213f6666da04fd9355e0499168e30f0f27ee1`.
Both engines were rebuilt against the accumulated registration work; executable hashes and detailed scope are in
`internal_filter_contact_publication_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`.

An isolated design prototype also preserves old contacts through 100 provisional
proxy rollback cycles, then recreates contacts after one reserved-proxy
readmission. Both drivers pass, but they use the ordinary Bullet libraries;
this is not a production publisher or full instrumented Bullet proof.

This adds real internal collision/contact behavior, but no external World
filter routing, ordered registration publisher, saved registration fields or
new gameplay/restart acceptance. Current save version remains 50. Automatic
contact/reaction/GetUp and full M15S4 normal-input, view/device/FPS, interruption
and fresh-process recovery gates remain open.

### Prepared ordered World registration batches

NativeDynamicsWorld now prepares complete owner resource batches and preserves ordered body and constraint removals and additions. Admission reserves capacity across every pending owner and creates provisional proxies while keeping old handles, membership and contacts intact. Failed admission and discarded tokens reclaim provisional resources and allow retry. Commit validates the complete source before releasing registrations, attaches reserved proxies without Bullet allocations in the supported fixtures, and advances native motion routing. Owner and World destruction invalidate retained handles safely, including reused identities and addresses.

Full normal and ASan/UBSan component runs each pass 2,853 tests in 271 suites with exact inventories and no failures or skips. The sanitizer corpus driver also links instrumented Bullet. Both corpus runs exercise 14,144 completed native-derived graph plans, including 124 remove/re-add cases and 12,648 ordered physical operations; 256 bounded planner cases do not publish a physical result. Rollback retains old contacts, removed bodies release contacts, and selected contacts return on detection. Corpus operations use ordinary Bullet groups, so this is not native filter parity.

Tested source fingerprint: `53ec5125cb87b29953ce27608cc829a780346f6aa2d56f859f826b5179581209`. Detailed hashes and boundaries are in `prepared_world_registration_batch_followup` in `M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Engine and Python checks were not repeated for this World primitive.

The actor graph adapter, composite filter publication, owned counter/remembered-World state, external collision routing and persistence remain open. Save version remains 50. No new normal-input fork course or restart acceptance is claimed; full M15S4 remains open.

### Ordered native registration publication in retained actors

ActorRagdollPhysics now retains authored body-associated constraint order and prepares native registration requests using its actual owned blend gains, current filters/membership and explicit caller counter/override/remembered-World contexts. It stages the ordered World batch and final internal filter replacements together. Validation checks both against the same source before releasing anything; commit publishes actual World changes, mask decisions, activation groups and logical selection under the exclusive barrier. Old contacts, handles and filters survive failed/discarded admission. The public standalone filter guards remain strict.

Full normal and ASan/UBSan runs each pass 2,859 component tests in 272 suites and 1,634 engine tests in 122 suites, without skips or failures. Both engines were rebuilt. The actual actor corpus publisher passes 14,144 completed native-derived plans per configuration, including 124 remove/re-add cases and 1,596 changed internal pair decisions. Another 256 bounded plans reject before admission. Original synthetic low-word layers0x30..32 are explicitly normalized to supported layer8/bone indices while preserving upper words and graph inputs; this is not native physical filter parity. The sanitizer driver links instrumented components and Bullet, with leak checks disabled.

Tested source fingerprint: `bb4232da5b9de427f811e28ab6e8d1697875eab83f385a3f699a1a5a825b432f`. `ordered_actor_registration_publication_followup` in `M15-PHYSICAL-ADMISSION-VERIFICATION.json` records source/binary/evidence hashes and exact boundaries.

Authoritative counter/remembered-World ownership, external World filter routing, saved fields and automatic caller/scene/reaction/GetUp integration remain open. Save version remains 50. No new normal-input or fresh-process gameplay acceptance is claimed, and full M15S4 remains open.

### Owned blend registration state and version51 persistence

Mapped blend state now owns its signed32 registration counter and remembered desired-World presence. The request publisher reads those fields directly, accepts one explicit resolved global override, stages final owned state with World/filter/selection changes and rejects stale counter/owner inputs. The external per-body context mirror is removed. Full original fresh constructors independently establish zero/null initialization in 24 cases; another 120 derived-clone profiles execute with a declared base-copy boundary. Clone initialization is distinct from continuation restore.

Runtime version51 stores optional per-blend registration state with strict presence/boolean bytes, signed counters and complete mapped-set validation in C++ and Python. Known zero remains distinct from legacy absence, and unsupported downgrade writes reject. Snapshot adapters preserve actual fields through prepared restoration. The fresh-owner wire test preserves both signed extremes and remembered owners, then produces the same next registration plan as the uninterrupted owner; legacy missing history uses verified constructor defaults, without claiming that absent history has been recovered.

Full normal and ASan/UBSan checks each pass 2,862 component tests in 272 suites and 1,635 engine tests in 122 suites. All 318 Python tests pass. Exact inventories have no failures or skips, and both engines were rebuilt. The owned actor publisher also passes 14,144 normalized native-derived plans per configuration, including 124 readmissions and 1,596 changed internal pair decisions; 256 bounded prefixes do not publish a physical result. A separate codec check passes 12 fixtures through 24 fresh normal/instrumented C++ processes, with Python-to-C++-to-Python byte equality. Leak checks are disabled.

Tested source fingerprint: `ea52a190a9629e220b81f059b1edd26cec19ed5bbf8ff8d2e9b9f078603c22e5`. Detailed source/binary/probe hashes, normalization, retained setup failures and boundaries are in `owned_blend_registration_persistence_followup` in `M15-PHYSICAL-ADMISSION-VERIFICATION.json`.

Automatic counter callers and scene requests, internal packed-filter/group persistence, external World routing and automatic contact/reaction/GetUp remain open. These checks do not establish a fresh-engine gameplay/restart course or close the remaining full M15S4 acceptance gates.

### Internal filter persistence and composite restoration

Runtime version52 preserves complete internal packed filter words, including absent World bodies, in canonical body-record order. Both codecs validate explicit presence, exact count/order, supported layers and downgrade writes while preserving opaque upper bits. Configured owners capture complete words; legacy absence preserves the target policy, and supplied words cannot silently create a missing policy.

PreparedRestore now stages the internal policy with poses, counters and World selection. Whole-source validation permits only its own pending registration tag. The expected membership epoch change uses the private prevalidated filter publisher; independent public filter tokens remain strict. Tests preserve old contacts through admission/rollback, release only changed pair algorithms, recreate enabled contacts without movement and retain unrelated contacts. Later-owner failure and stale ignore/policy state publish no pose/counter/word/selection changes.

The fresh-owner wire test restores distinct group words into a differently initialized policy, rebinds canonical records to winning body order, and proves actual contact behavior before and after restore. Full normal and ASan/UBSan checks each pass 2,867 component tests in 272 suites and 1,636 engine tests in 122 suites. All 321 Python tests pass, with no failures or skips. Seven valid and five malformed codec fixtures pass through 24 fresh normal/instrumented C++ processes with exact Python/C++ binary agreement. Leak checks are disabled.

Tested source fingerprint: `f1e8fd792f0c52e2e8942aa9b37817c38483c8b911ee2010a891dcdd34d84496`. Source/binary hashes, scope and evidence are recorded in `internal_filter_restore_persistence_followup` in `M15-PHYSICAL-ADMISSION-VERIFICATION.json`.

These words control the configured owned-pair policy. Global group ownership/allocation, external native World routing, automatic counter/scene and contact/reaction/GetUp integration remain open. No normal-input or fresh-engine gameplay/restart gate is closed by these backend checks; full M15S4 remains open.

### Native collision routing between configured actors

PhysicsSystem now uses NativeCollisionDispatcher. Published actors with configured native masks bind their complete owned body set and current packed words; absent World bodies keep their binding for later admission. Both endpoints must be configured in the same World, whose immutable native mask tables must agree. Existing Bullet collision/ignore/group-mask decisions remain in force. Generic objects and queries retain their existing routes.

The dispatcher preserves supplied pair order. Full original 8A8060 body-pair wrapper execution confirms that its first and second collidable+1C words reach 8A7F70 in that order, without sorting or symmetrization, in 37,836 cases. Original initialization/CRT/predicate instructions execute without stubbed game calls; return pointer, single-byte output, stack and callee-saved registers are checked. This does not establish identical Havok/Bullet upstream pair order.

Filter publication releases denied cross-owner cached algorithms immediately while preserving overlaps and unrelated generic contacts. Enabling stationary pairs recreates contacts on detection. Bindings follow the stable live vector object across word swaps and v52 composite restoration. Tests cover explicit layer29 asymmetry, conflicting World masks, deferred/absent bodies, teardown, failure after body publication and a reused World address with a retained old actor.

Full normal and ASan/UBSan checks each pass 2,874 component tests in 273 suites and 1,636 engine tests in 122 suites, with exact inventories and no failures or skips. Both engines are rebuilt. A separate normal/instrumented driver updates two production actors for every native wrapper row and checks actual contacts without movement: 14,767 enabled and 23,069 denied per run; swapping arguments mismatches 726 expected results. Leak checks are disabled. Python/schema sources are unchanged; the previous 321-test Python result was not repeated.

Tested source fingerprint: `470ec4565d3d09414012b59243b9dc3bce306055ed7fa659b69803770542cf3d`. Detailed source/binary/oracle hashes and boundaries are in `external_configured_actor_filter_followup` in `M15-PHYSICAL-ADMISSION-VERIFICATION.json`.

Global group ownership/allocation, unconfigured capsule/static/loose-object filtering, shape-child/root and ray/query routes, automatic physical construction/counter/scene/contact/reaction/GetUp and remaining mastery/effect/input/restart courses remain open. These checks add real cross-owner collision behavior, but close no normal-input or fresh-engine gameplay acceptance gate. Full M15S4 remains open.

### Preserve the movement capsule during native graph admission

PreparedNativeActorPhysicalPose now explicitly selects the existing Retain policy. Both native World facade overloads and detached resource commits preserve the already admitted movement capsule while independently owning the retained graph. The generic PhysicsSystem default remains Suspend. An empty graph body/constraint World selection does not remove the capsule.

Reinspection validates hashes and raw fields from the retained original03 and original07 normal-input Dog courses: 94 samples each across raw 0/4/3/5/6/0 preserve controller, proxy, native object, actual ahkWorld link, broadphase word and filter. Original03 additionally observes zero proxy World-link/filter hardware writes during its declared contact-to-recovery interval; original07 supplies sampled fields only. This is reinspection of existing captures, not a newly launched game or a universal collision-response/movement rule.

The strengthened native World integration checks cover workers 0/1/2. They preserve actual movement actor/object/proxy/UID, group/mask/flags and transform through native admission, saved-pose restoration, release and failure/competing-candidate paths. A real PhysicsSystem ray still hits the capsule after every graph body leaves the World. The explicit generic Suspend/Retain cases continue to pass. Full normal and ASan/UBSan engine runs each pass all 1,636 tests in 122 suites with exact inventories and no failures or skips. Both engines are rebuilt; leak checking is disabled. Unchanged component, Python/schema and predicate/corpus checks were not repeated.

Tested source fingerprint: `b82914ef8fe0ecb703566f68535e3e5f379cc88f7b66857ddaaf344b2d541536`. Source/binary hashes, raw capture provenance and limits are in `native_facade_capsule_retention_followup` in `M15-PHYSICAL-ADMISSION-VERIFICATION.json`.

Automatic native construction still needs the actual frame/controller/animation-target adapter and coherent group/filter initialization. Semantic capsule response/movement, contact/reaction/FSM/GetUp and all remaining full S4 acceptance courses remain open. No normal-input fork or fresh-engine gameplay/restart gate is closed here; full M15S4 remains open.


### Settled native actor frame renderer publication

ActorRagdollPoseBinding now publishes settled NativeObjectLocal frames using
updated parent world caches and verified live Single arithmetic. Bit0 gates
physical target reads, bit8 controls author local R/T writeback, and missing
targets preserve animation locals. World-only physical writes install drawing
projections while leaving controller locals intact. All target validation and
projection allocation finish before any write. Exact native world bones are
returned separately from controller-local composition. Pending0x40 first
synchronization remains caller-owned.

Animation applies those frames against its latest sampled animated checkpoint,
without reevaluating callbacks or advancing clocks. Missing KF channels retain
previous animation values. Rejected frames restore physical locals, NIF caches
and drawing handles. Checkpoints retain frame-handle ownership; explicit dynamic
snapshots clear this binding's frame projections. Controller body-node order
comes from current live child slots rather than record order.

Full normal and ASan/UBSan checks each pass all 2,878 component tests in 273
suites and 1,637 engine tests in 122 suites, with exact inventories and no
failures or skips. Both engines are rebuilt; leak checks are disabled. A separate
renderer driver replays 256 settled live007F rows from the retained original
node corpus: local and returned world fields match bitwise in both builds.
Drawing matrices match exactly with signed zeros equivalent. The initial strict
drawing comparison is retained: its only differences were 96 signed zeros in
48 rows. The replay reconstructs one node with its recorded parent as placement;
it does not replay the original multi-node traversal or execute a new game.

Tested source fingerprint: `2a872a045ceabbe383ed6a180cd2befd125a1ef1514488f14df2dcf5e8266a1d`. Source/binary/corpus
hashes and boundaries are in `native_actor_frame_renderer_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Python/schema sources are unchanged;
the previous 321 Python tests were not repeated.

Automatic runtime traversal remains open: a child's animation world target must
use its parent's already updated physical cache, so the existing whole-frame API
with precomposed targets cannot simply be wired in. No approximate coordinator
or automatic construction is enabled here. Native frame/controller/contact/FSM/
GetUp, global group/filter ownership and all remaining full S4 gameplay and
fresh-engine restart gates remain open.


### Object-local per-node native scene traversal

The existing ordinary NIF traversal now accepts an explicit native parent for
an object-local root. Initial and per-frame native caches use this parent;
drawing projections continue to use actual OSG ancestors. Placement must be
supplied on every object-local pass, and mismatched reference/object entry
points reject before controllers. Root look-at reads the explicit parent while
retaining its previous own world cache until projection. Physical local
writeback uses the native parent and preserves separate scale.

A per-node collision-update boundary receives the composed animated world after
own controllers and before physical flags/targets are read. Its input includes
the parent's completed physical projection. Existing selected/rigid policies,
pending40 synchronization and child-cache rebasing remain shared. This provides
the input boundary needed by an actor physical owner; it does not advance that
owner's controllers or establish its global scheduling.

Full normal and ASan/UBSan component runs each pass 2,884 tests in 273 suites,
with identical case names, exact per-binary inventories and no failures/skips.
Both component builds have no compiler warnings. A separate driver matches 512
retained native controller/cache frames and 7,104 field comparisons in each
configuration; maximum drawing matrix error is 2.84217e-14 against 1e-9 tolerance.
It reconstructs the child/leaf branch with recorded parent worlds as inputs,
seeds the initial old child cache and excludes the native fixture's invalid
frame0 leaf old cache. Original root/helper topology and controller time/math
are not replayed. No new original executable or game is run. Leak checks are
disabled; unchanged engine and Python/schema checks were not repeated.

Tested source fingerprint: `b31778c2247afb13d9713b270bda867841a68dea30632a314b1dd8eb3d5f25b2`. Exact source/binary/corpus
hashes and boundaries are in `object_local_root_followup` in
`M15-NIF-SCENE-TRAVERSAL-VERIFICATION.json`.

Automatic actor placement/controller/physics ownership, coherent group/filter
initialization, contact/reaction/FSM/GetUp and all full S4 normal-input and
fresh-engine restart acceptance courses remain open. App binaries need a fresh
engine build when this component primitive is connected to runtime ownership.


### Node-scoped physical controller and collision phases

ActorRagdollPhysics now advances the controllers attached to one actual node
without advancing other nodes. It preserves generated velocity head/tail order,
independent targets, immediate forces and shared clock cache semantics. Existing
public complete-batch APIs remain strict. A separate blend collision entry
reads live owned gains/flags and uses live Single scene conversion; it does not
advance controllers or forces again. Pending40 and unknown blend nodes reject.

The ordinary NIF traversal can receive an absent physical scene target while
retaining actual collision flags. This represents SceneToPhysics and skipped
projection routes without rewriting flags or author locals. Procedural NIF
rotation caches stay untouched when no physical local write occurs. Active
ambiguous providers reject; inactive and pending routes do not read providers.

A coupled test runs real own callbacks, physical controller clocks, parent world
projection, child composition and keyframed Bullet pose publication: parent
animated 110 projects to physical 0, and child animated target becomes 5 instead
of 115. Another test applies a later-node controller force to an earlier body
after its collision drive, retaining 3.2 instead of the erroneous globally
prebatched 1.6. Cross-target gains, rejection, skipped sleeping bodies and
absence of duplicate controller/force advancement are also checked.

Full normal and ASan/UBSan runs each pass 2,891 component tests in 273 suites,
with identical case names, exact inventories, no failures/skips and no compiler
warnings. The changed optional-target projection branch also matches 512 retained
native controller/cache frames and 7,104 field comparisons in each build;
maximum drawing matrix error remains 2.84217e-14 against 1e-9 tolerance. The
recorded-parent branch reconstruction and initial-cache boundaries are explicit;
this is not a new original Havok or game run. Leak checks are disabled.

Tested source fingerprint: `f59e4a8350a1150f712f582a873d7e67f3698207832418aa9340653b3f9f23b5`. Source/binary/corpus
hashes and limits are in `per_node_physical_dispatch_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Engine and Python/schema sources were
not changed; their suites were not repeated for this component-only bridge.

Production actor traversal/controller/physics ownership and scheduling,
coherent group/filter initialization, contact/reaction/FSM/GetUp and every
remaining full S4 normal-input and fresh-engine restart gate remain open.
Reinspection of 168 retained original continuations confirms that controllers
enter with prior physical author locals, rather than a restored pure-animation
checkpoint; actual KF controller bodies remain outside that observation.
Earlier node/controller phases can already be published if a later collision
phase rejects; no whole-frame rollback is claimed. App binaries need a fresh
engine build before subsequent runtime integration or acceptance courses.


### Deferred native scene admission and projection retirement

NifSceneTraversal now prepares object-local caches and projection storage without
changing the model or detaching callbacks. Commit checks all source local and
hidden NIF cache bits, hierarchy, projection ownership and external placement
before installing handles once. Unpublished traversals cannot run updates.
Discard has no scene effect; actor life, generation and callback authority still
belong to the composite engine owner.

Explicit retirement preserves current author locals and callbacks while clearing
only still-owned projection storage. It preserves even a distinct foreign handle
with identical matrix values. A retained pre-fix probe demonstrates that matrix
value equality had cleared that replacement; the unchanged probe now passes in
both builds. Retirement also handles surviving nodes after hierarchy damage,
invalidates moved Skeleton palettes through unique parent chains, rejects
reentry and terminates damaged cycles. It does not invent an animation/recovery
pose. Immediate constructor and destructor policies remain unchanged.

Full final normal and ASan/UBSan runs each pass 2,899 component tests in 273 suites,
with exact inventories and no warnings, failures or skips. Both native replay
drivers pass 512 frames and 7,104 field comparisons, including 64 deferred
admissions/retirements per build. Drawing error stays 2.84217e-14 against 1e-9.
Original topology/controller/time/Havok boundaries remain explicit; no new game
or original instructions are executed. Initial01 results and the failing foreign
handle probe are retained. Leak checks are disabled.

Tested source fingerprint: `15ed0dcc12843c20bb20b85f108ccb8def86f9b37d9315c064bdb316f53f0b59`. Exact source/binary/probe
hashes and limits are in `deferred_admission_retirement_followup` in
`M15-NIF-SCENE-TRAVERSAL-VERIFICATION.json`. Engine/Python/schema checks are not
repeated for this component lifecycle primitive. Composite engine ownership,
physical frame scheduling, automatic construction and all remaining full S4
normal-input and fresh-engine restart acceptance gates remain open.


### Worker-safe engine entry points for native scene nodes

PhysicsSystem and PhysicsTaskScheduler now expose the node-scoped own-controller
and blend collision phases. Both wait for movement workers and hold the existing
collision-World lock. Controller advancement uses the existing scheduler clock
cache; collision publication uses existing native gravity and does not tick the
controllers/cache again. A scene hook must not reenter physics under that lock.

The real World integration fixture checks 0/1/2 workers, stale actors and unknown
nodes, native-derived force publication, scene rejection after a completed
controller phase, accepted publication without duplicate clock advancement, and
restoration of the prior snapshot/cache before the existing complete-batch course.
Its existing generic capsule policy is retained. Full normal and ASan/UBSan
engine runs each pass all 1,637 tests in 122 suites with exact inventories and no
failures/skips. Both engines rebuild against the accumulated traversal/physical
components. Prior 2,899 component tests and unchanged Python/schema checks are not
repeated; leak checks remain disabled.

A separate pinned-original outer blend dispatcher probe executes 393,216 contexts
with explicit successful sync/inner-handler stubs. It distinguishes phase 3 skip,
phase 2 pending-sync deferral and active and pending flags from the already implemented
inner gain rule. Pending blend dispatch directly calls scene sync, whereas the
ordinary dispatcher also calls its reset virtual. This supplies outer dispatch
expectations; inner body effects and actual game-frame selector production remain
unproved, and no automatic outer scene driver is installed by these wrappers.

Tested source fingerprint: `99ba0784681faf9bd09507fb6c4f60ff07b60fe8ed859c84a2ad0643bc223661`. Exact source/binary/oracle
hashes and limits are in `node_physical_scheduler_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Actual actor scene ownership/scheduling,
group/filter initialization, contact/reaction/FSM/GetUp and all remaining full S4
normal-input and fresh-engine gameplay/restart acceptance remain open.


### Outer blend-collision gates in the owned engine scene entry

The verified base blend dispatcher now selects skip, first synchronization or
the inner handler from live collision flags and raw phase. Phase 3 skips all,
phase 2 defers pending 0x40, pending 0x40 otherwise precedes active bit 0, and settled
inactive nodes skip. A clear-after-success mask preserves other live handler
changes. The owned scene entry implements skips and settled blend updates;
required first synchronization explicitly rejects without flag clearing or a
velocity reset. Its real body/scene effects remain unfinished.

PhysicsSystem and its scheduler expose that entry through their existing worker
barrier and World lock. The real World fixture checks 0/1/2 workers with inactive,
pending and active snapshots, unused NaN inputs on skipped routes, stale actor/
node rejection and unchanged controller/shared-cache state. No automatic frame
owner, selector producer or additional timer is installed.

Full normal and ASan/UBSan runs each pass all 2,906 component tests in 273 suites
and 1,637 engine tests in 122 suites, with exact inventories and no failures or
skips. Both engines rebuild. Native integer comparison matches 393,216 unchanged
rows per build, covering the pure rule and owned gate classification; 131,072 of
those rows deliberately reject required synchronization, without claiming its
successful body effects. All four negative controls reject their targeted rows.
The fixed read-only body fixture has no World step/contact comparison. Leak
checks are disabled; unchanged Python/schema tests are not repeated.

Tested source fingerprint: `0592faf0a192cc137139a242237b45c2d3db8e5fb52a30472db8f25d17240827`. Source/binary/oracle
hashes and limits are in `outer_blend_scene_dispatch_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Static inspection also separates the
AddRotation subtype's postpass from the implemented base gate; subtype selection,
math and runtime ownership remain open. First synchronization, actual scene
scheduling/group/filter initialization, contact/reaction/FSM/GetUp and all full
S4 normal-input and fresh-engine gameplay/restart acceptance remain open.


### Pending blend scene synchronization reaches the owned body setter

The previous chunk's required-first-sync rejection is replaced by the verified
base pending path. The renamed owned/public blend scene entry stages native
scene/body conversion, publishes the direct pose for either admitted motion,
preserves packed velocities, forces, actual/stored motion and gains, then clears
only 0x40. It retains phase 2/3 deferral. Invalid poses and throwing scene hooks
leave this collision phase unchanged. The helper keeps forced velocity resets
separate from first synchronization.

Full normal and ASan/UBSan checks each pass all 2,907 component tests in 273
suites and 1,637 engine tests in 122 suites, with exact inventories and no
failures/skips. Both engines rebuild. The actual World fixture exercises
successful sync and rollback with 0/1/2 workers, preserving controllers and the
shared clock before continuing its existing physical-owner course. This is
engine integration evidence, without normal game input or a contact-step claim.

Fresh complete original handlers pass 720 cases; production comparisons each
pass the 240 Single cases/3,600 fields, plus exact driver velocity/mode/ownership
checks. Four captured negative controls distinguish pose omission, inappropriate
velocity reset, premature pending clear and BodyT omission. Original fixtures
have no native World and an already active island. Original World broadphase,
contact and activation-island equivalence remain open; Bullet AABB publication
uses the existing owned setter. ASan leak checks are disabled. Python/schema
are unchanged and were not rerun.

Tested source fingerprint: `9b6b741326d02ecca2de2360e7f2226286db8e4b5c93d910751eb28a3ff0d2d1`.
See `blend_node_first_synchronization_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json` for hashes, retained attempts and limits.
Automatic scene/controller scheduling, subtype handling, group/filter/capsule/
root coordination, contact/reaction/FSM/GetUp and all remaining full M15S4
normal-input and fresh-process gameplay/save-restart gates remain open.


### Complete collision ownership crosses the NIF projection boundary

The NIF traversal now accepts one complete collision result: actual flags and
an optional renderer target. It preserves pending-phase deferral without
invoking generic synchronization again, keeps active no-target locals, and
passes unchanged actual flags to observers. Ambiguous complete/legacy callback
ownership rejects before own callbacks; targets require settled active flags.
The existing legacy paths retain their behavior.

Full normal and ASan/UBSan checks each pass all 2,911 component tests in 273
suites and 1,637 engine tests in 122 suites, with exact inventories and no
failures/skips. Both engines rebuild. A coupled owned-body/NIF scene course
checks defer, sync and subsequent settled projection through child caches and
OSG drawing output while preserving existing physical controller state. The
exhaustive 393,216-row original outer corpus also matches route/final flags/hook count/
projected flags in both builds. Its native inner stubs do not prove body or
renderer arithmetic; fixed-body invariants are asserted by the current driver.
Leak checks are disabled; unchanged Python/schema tests are not rerun.

Tested source fingerprint: `74f21549a38791898829e6fafbc03a3de9d22f41ddac5900deff96ad3a70c3a1`. Exact hashes and scope are in
`owned_scene_collision_result_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. The collision-result boundary is implemented. Automatic actor traversal,
native controller/KF ordering,
AddRotation, selector/time producers, coherent group/filter/capsule/root
coordination, contacts/reaction/FSM/GetUp and all normal-input and fresh-process
gameplay/save-restart gates remain open. Full M15S4 is not complete.


### AddRotation reaches the owned NIF collision projection

The collision result can now supply the subtype's extra matrix. The traversal
executes its two native matrix products after base writeback/synchronization,
using current author locals, and publishes the completed world rotation before
children. It preserves locals, position/scale, flags and the base change mask.
Skipped, deferred and absent-body base routes still execute the postpass.

Full corrected normal and ASan/UBSan checks each pass all 2,917 component tests
in 273 suites and 1,637 engine tests in 122 suites, with exact inventories and
no failures/skips. Both engines rebuild. Initial 01 fixtures failed explicit
retirement-before-rebinding requirements; their failures are retained and the
fixtures are corrected before full 02. Production code is unchanged between
those two attempts. Six new unit cases cover exact matrix stores/precision,
invalid/overflow inputs, skipped-base postpasses, post-writeback local reads,
child rebasing/composition and missing-parent/invalid-extra boundaries.

Fresh full original handlers pass 720 active/pending cases alongside the prior
720 skip/bypass cases. Owned-body/NIF comparisons each pass 720 canonical cases
and 26,160 checks; declared scene fields are exact, body XYZ error is zero and
body basis error stays within the predeclared 1e-6. Independent controls reject
postpass omission and stale-local reads. Original no-World/active-island and
precision domains remain bounded; no contact step or gameplay acceptance is
proved. Body position fourth-word production remains open. ASan leak checks
are disabled; unchanged Python/schema tests are not repeated.

Tested source fingerprint: `59df612c3567a8b54ba69d0717010578a8fdb29a0030617cf7811ab775fffb48`. Exact hashes and limits are in
`addrotation_scene_postpass_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`. Actual subtype construction and live
extra-matrix production, automatic actor traversal, KF/controller ordering,
selector/frame producers, group/filter/capsule/root coordination, contacts/
reaction/FSM/GetUp and all normal-input and fresh-process gameplay/save-restart
gates remain open. Full M15S4 is not complete.


### Native scene ownership in normal renderer updates

Animation can now prepare and commit a native scene owner that runs through
ordinary OSG update traversal. Resident and selected callbacks remain live;
partial controllers start with previous physical author locals. Each frame
obtains current placement and a collision dispatcher from an explicit caller
source, then evaluates each node before collision projection and children.
Root StateSet callbacks run once, with descendant properties preserved. An
ordinary Group/Skeleton forest applies one native placement independently to
its roots. Asset-scoped binding excludes foreign part record collisions.
Prepared identities/callback chains and live owner guards reject stale use;
teardown preserves foreign callbacks/projections and current author locals.

Corrected full normal and ASan/UBSan checks each pass all **2,917 component
tests in 273 suites** and **1,645 engine tests in 122 suites**, with exact
inventories and no failures/skips. Eight new engine cases include an actual
OSG/owned-Bullet-body course through selector 2/3 deferral, first synchronization
and later physical pose continuation, retaining packed velocities and request.
Initial full01 attempts each have one forest-fixture failure: its second root
was not declared as a physical target. Only that declaration changes for02;
production code is unchanged. Initial failures remain retained. Leak checks
are disabled; unchanged Python/schema checks are not repeated.

Fresh normal/sanitized regression comparisons each pass the retained 720 native
cases and 26,160 checks, with identical declared scene output and the existing
body tolerances. No new original-game probe, contact step, normal game input or
restart course is claimed. Tested source fingerprint: `02dca996945e4f527d7b367ba519020f4a01c865cc34c8b6cec8734d8e8a33db`.
Exact hashes and limits are in `native_renderer_scene_owner_followup` in
`M15-PHYSICAL-ADMISSION-VERIFICATION.json`.

This delivers the renderer driver boundary. World admission still uses generic
AnimatedTargets; automatic construction and authoritative frame/selector sources
must be integrated. Actual KF ordering, subtype/extra-rotation producers, special
node/attachment policies and live migration/morphology remain open, alongside
group/filter/capsule/root coordination, contacts/reaction/FSM/GetUp and every
remaining normal-input and fresh-process gameplay/save-restart gate. Full M15S4
is not complete.


### Frame inputs traced before World activation

The original main caller's frame input now connects to full timer47D170 and the
GetTickCount import. Executable setting metadata and actual setup prefixes map
raw mode to iUpdateType:HAVOK, smoothing to fChaseDeltaMult:HAVOK, and fixed
substep to fMaxTime:HAVOK clamped to at most1. Current installed/shipped INI texts
both specify0.0167; using the compiled1/60 alone changes96 sampled frames. Text
values remain supplied fixtures, without a live original INI-parser observation.
The frame-limit flag comes from SetSceneIsComplex. Full47C990 separately brackets
scene callbacks with raw collision selector3, then resets it to0.

A manifest-driven original probe passes1,935 timer/configuration/caller/wrapper
cases with declared virtual scene boundaries. Normal and ASan/UBSan comparison
of the unchanged actual frame helper each matches768 Double-domain cases and
4,608 fields. The separate384 Single-precision observations expose six one-ULP
smoothing/substep differences. Live precision at the main call site is therefore
an explicit next runtime check, alongside initialized settings and selector
context. Both harness setup failures and exploratory differences are retained.
Editable inputs are in `M15-PHYSICAL-FRAME-PRODUCER-PROBE.json`; exact evidence
is under `native_physical_frame_producer_followup` in the verification index.

Production code and engine tests are unchanged; full suites/app builds are not
repeated. No original game/input course, physical step, contact or fresh-process
acceptance is claimed. Clock lifecycle/persistence, indirect selector2 production,
native World activation and all remaining M15S4 gameplay/restart gates stay open.
Full M15S4 is not complete.


### Live frame precision gap corrected

Read-only original-game entry/return observation confirms FPCW007F and initialized
fMaxTime0.0167/fChaseDeltaMult0.05 in separate mode0/10 processes. The previous
Double helper differs on one of64 actual mode10 frames. An explicit Single
precision overload now matches that observation and the retained instruction
corpora. Quotient/product rounding also produces verified negative carry;
nonpositive accumulated frames take no step and preserve signed clock state.
Three new unit cases cover the live store and carry behavior. The existing
five-argument helper keeps its Double contract. This does not activate a new
World clock or route signed time through scene/body consumers.

Full normal and ASan/UBSan checks each pass **2,920 component tests in273 suites**
and **1,645 engine tests in122 suites**, with exact inventories and no failures/
skips. Each standalone comparison matches29,538 cases and177,228 fields,
including104 actual original observations. Wrong-precision controls change41
cases. Both original processes exit0, the owned Steam process exits0, cloned
inputs restore exactly and pristine saves are unchanged. No owned process
remains. Debugger overhead precludes timing/FPS/contact or restart acceptance;
no new fork gameplay course is claimed. ASan leaks are disabled.

Tested source fingerprint: `48e4074942d711ee7014f110ac69f81d18ffee5d51065d71fc75a4fcf712390b`.
Exact reports and limits are in `live_native_physical_frame_precision_followup`
in the verification index. A supplementary correction identifies the controller
wrapper's excluded RTTI as NiMultiTargetTransformController; earlier billboard/
child fixture labels were misleading. Corrected native rerun preserves all
numerical frame rows. Main timer publication and clock lifecycle, native World
owner activation, KF/subtype/group/filter/root/capsule coordination and the
remaining contact/reaction/FSM/GetUp and gameplay/save-restart gates remain open.
Full M15S4 is not complete.


### Native scenes advance before the shared physics pass

The Oblivion viewer now separates native-owned scene advancement from its later
update traversal. World snapshots owned renderer scenes before existing physics;
live properties, own controllers and collision run once per finite frame. The
later viewer pass refreshes drawing placement and RigGeometry skin space/bounds
from retained native worlds. It preserves author locals and does not repeat
native callbacks. Unowned scenes keep ordinary viewer updates. Late admission
waits for the next pre-pass; failed preparation does not consume that frame.
Root properties receive the full ancestor path and cannot rebind, release or
detach their owner during the pass. Focused red tests establish both defects
before the final fix.

A read-only original-game trace retains eight complete loaded stock prison idle
frames, counters1856..1863. All1,160 captured blend-controller and1,168 collision
calls precede the world substeps;24 regular world steps occur. Own-controller
calls also occur afterward:6,648 before and1,230 after. Their post-step identities
and original callback lists88A660/88A790 remain unresolved. No velocity-controller
path occurs in this baseline. Debugger overhead forces capped slow frames, so
three steps per sampled frame have no uninstrumented timing/FPS implication.
Code/PID guards, exact cloned-input restoration, pristine-save identity and
owned game/Steam exit0 are verified. No owned process remains. A copied wrapper's
obsolete final summary field fails after successful detach; the complete native
observation and that failure are both retained.

Full final normal and ASan/UBSan engine checks each pass **1,651 tests in122
suites**, exact inventory and no failures/skips. Earlier full01 checks each pass
**2,920 components in273 suites** and1,650 engine tests; component sources/tests
are unchanged by the final owner/path fix. Final tested fingerprint:
`e65a7346fbcc4a8e8fc758279128239d680814cc2f5e73b751760273d961bd1e`;
full01 fingerprint:
`5ed9db75ac1280768be00a9b0f61499fd292dc30110aeba5a6c1a9c5118cd34f`.
The real owned-Bullet-body regression now uses the pre/post boundary through
selector deferral, first synchronization and physical continuation. A real
RigGeometry vertex/bounds case checks drawing reanchor. No contact simulation
or fork normal-input course is added. ASan leak checks are disabled.

Editable observation inputs are in `M15-PHYSICAL-PHASE-ORDER-PROBE.json`; exact
hashes, failed attempts and reports are in `native_prephysics_scene_phase_followup`
in the verification index. World admission still uses generic AnimatedTargets;
automatic native owners, shared authoritative frame clock/step counts, complete
native post-world publication, KF/selector/subtype/group/filter/root/capsule
coordination and all remaining contact/reaction/FSM/GetUp and normal-input/
fresh-process gameplay/save-restart gates remain open. Full M15S4 is incomplete.


### Post-world callback identities separated from actor pose publication

Full original88A660/88A790 and listener dispatcher8A7830 pass336 manifest-driven
instruction cases:240 registered callback and96 queued-pair cases. Null entries,
zero/signed finite frame arguments, gates, forward callback order, reverse queue
order, listener dispatch and unconditional queue clearing are checked. Virtual
entry/listener bodies and Windows locking remain declared boundaries. The initial
report's hard-coded720 registered-case label was wrong; corrected runs derive
and assert the inventory and retain the identical numerical case array.

A separate read-only stock prison idle process captures8 complete frames,
counters3456..3463, with24 world substeps and1,234 later own-controller calls.
Original MSVC RTTI identifies both registered post-world objects:
TESTrapListener at vtableA56350 (frame virtual537630) and TESArrowTargetListener
atA562F4 (frame virtual68F970, ret4). The list is not a generic actor-pose
publication pass. The other queue contains6 object/color pairs per frame with
its dispatch gate null. Its producer88BB60 and packed-byte caller path are
retained. Later scene callbacks include dungeon props, particles, lights,
cameras and HUD compass; classifying every post-world call as UI is incorrect.
Live RTTI getter bytes agree with the pinned original image. This baseline
does not establish complete actor/ragdoll post-step publication or trap/arrow
contact outcomes.

GDB exits0 and detaches after the complete capture. A copied wrapper's final
summary then fails on obsolete nested fields; that failure is retained and its
formatter corrected without repeating/discarding the observation. The game and
owned Steam process exit0; exact cloned inputs restore, pristine quicksave hash
is unchanged and owned process epochs are absent. Debugger overhead precludes
any timing/FPS interpretation. No fork gameplay/contact/restart acceptance is
claimed. C++ and tests are unchanged fromcfcf9f9de9, so passing full builds/suites
are not repeated. Editable inputs are in `M15-PHYSICAL-POSTWORLD-PROBE.json`;
exact reports, hashes and limits are under `native_postworld_identity_followup`
in the verification index. Main clock ownership/shared scheduling, World native
owner activation, complete actor publication and all remaining M15S4 gameplay/
fresh-process gates remain open. Full M15S4 is incomplete.


### Prepared native frames drive the shared physics scheduler

PhysicsTaskScheduler now accepts a prepared native frame's exact step count and
duration through the existing movement jobs, barriers, native velocity handling
and single Bullet world pass. This entry does not recalculate performance-budget
steps or introduce another accumulator. Caller-owned signed carry remains intact;
nonpositive frames execute no physics steps. Malformed/nonfinite/inconsistent
frames and nonrepresentable native gravity durations reject before consuming
queued work. Existing movement presentation interpolation remains unchanged;
the no-step case uses its existing default divisor without advancing bodies.

Four new cases at0/1/2 workers add12 engine regressions. An actual helper-to-
scheduler-to-world course records projectile position at each of3 exact0.0167
Bullet ticks while actor and loose bodies share velocity/pose evolution. Fixed,
variable and smoothed durations, two-step limiting, signed carry/continuation,
nonempty queue preservation on rejection and valid retry are checked. A zero-step
follow-up publishes previous async movement results without another body step.
The initial normal/sanitized runs each fail only the2 async fixture cases that
incorrectly expect publication from a worker-only wait. Those attempts remain
retained; the fixture now explicitly checks the next publication boundary.

Final full normal and ASan/UBSan engine checks each pass **1,663 tests in122
suites**, exact inventory and no failures/skips. All four build logs are free
of warning/error diagnostics. ASan leaks are disabled. Tested source fingerprint:
`20b6afe14bdec09860c4af78bf4d12940c534ef2cc4956298f40e27648169a6a`.
Unchanged component/Python/schema checks are not repeated. Exact binary/source
hashes, retained failures and reports are in `native_prepared_scheduler_followup`
in the verification index.

This implements shared step consumption. Authoritative Engine/World clock,
settings, scene-complex lifetime/time publication and activation of this entry
remain required, together with native owner admission, KF/selector/subtype/
group/filter/capsule/root/morphology coordination and complete reaction/FSM/GetUp
publication. These cases run actual Bullet steps but no contact or normal-input
combat/restart course. They do not establish native capsule presentation or
Havok trajectory equivalence. All remaining S4 gameplay/fresh-process gates stay
open. Full M15S4 is incomplete.


### Native millisecond timer producer implemented

The physical frame input now has a typed model of full original47D150/47D170:
supplied origin tick, uint32 elapsed wrap, shared raw-counter rotation, pause
byte,1ms floor,166ms cap, override/fraction carry and signed64 truncation with
low32 retention. The uncapped raw elapsed tick is stored independently of the
published interval. The multiplier is the original double representation of
binary32 0.001f, with explicit float stores before/after scale. A literal double
0.001 is observably different. Single/Double arithmetic is explicit; consumed
supported-domain input validation and output overflow reject timer/counter
changes atomically. Paused/inactive fields are preserved without consumption.

Pinned full original instructions produce4 initialization cases,8,064 timer
update cases and120 consecutive timer-to-full889810 frame rows without stubs.
Independent input audit verifies the exact manifest product and uniqueness.
Fresh normal and ASan/UBSan C++ comparisons each match **8,184 cases/65,832
fields**. Replacing only the multiplier with literal double0.001 changes144
timer cases; that negative-control failure is retained. The initial connected
harness incorrectly assumed callee argument cleanup for889810; corrected02
asserts its actual cdecl stack and reruns all paths. The setup failure remains.

Seven new component cases cover initialization, cap/wrap, override/fraction and
large low-word behavior, inactive inputs, atomic rejection/retry and consecutive
native frame words. Full normal and ASan/UBSan checks each pass **2,927 tests
in274 suites**, exact inventories, no failures/skips and no build warnings.
Tested source fingerprint:
`751cd91b77b037ceaa1ecde2b509df024c61d341661e747817688f00fb478e15`.
ASan leaks are disabled. Engine/app/Python/schema are unchanged and not rerun.
Editable inputs are in `M15-PHYSICAL-TIMER-PROBE.json`; reports, hashes and
retained failures are in `native_physical_timer_producer_followup` in the
verification index.

This implements the timer producer rule, not its main-loop activation. Host
tick source, pause entry/resume transitions, initialized settings and shared
process/Engine clock ownership/publication must still connect to World and the
prepared scheduler. No Windows timing, parser, original game process, fork
normal-input combat/contact or restart acceptance is claimed. Actor admission,
KF/selector/subtype/group/filter/capsule/root/morphology, full reaction/FSM/GetUp
and all remaining S4 gameplay/fresh-process gates remain open. Full M15S4 is
incomplete.


### Native timer pause/resume transition rules verified

Full original47D120/47D0F0 now have typed pausePhysicalTimer/resumePhysicalTimer
counterparts. Pause increments the raw byte and rebases both tick fields only
from zero; resume ignores zero, otherwise decrements and rebases only when
reaching zero. Both use uint32 subtraction and preserve all float payloads and
shared counters. Raw byte255 pause wraps to0 without rebasing; this preserves
the original raw rule, while a runtime owner still needs balanced pause sources.

Full original paths cover4,096 unique transition inputs across all256 byte
values, both operations, wrapped/extreme ticks and inactive float payloads.
Only the actual GetTickCount import is supplied;16 transition cases reach it.
A13-action nested pause/update/resume cycle reaches the import4 times. Its first
resumed update publishes the166ms cap, followed by an ordinary16ms update.
The two tick-field rebases do not justify assuming a simple frozen clock that
excludes the pause gap. Fresh normal and ASan/UBSan C++ comparisons each match
**4,109 cases/32,872 fields** against these original outputs.

Three new component cases cover outer/nested transitions, byte wrapping and
unmatched resume, unchanged float payloads and the complete native cycle words.
Full normal and ASan/UBSan checks each pass **2,930 tests in274 suites**, exact
inventories, no failures/skips and no build warnings. Tested source fingerprint:
`0cde71180751f77a6938388ffa6fce17beb73b80a75622465cb61ee96d1be7e1`.
ASan leaks are disabled. Editable inputs are in
`M15-PHYSICAL-TIMER-LIFECYCLE-PROBE.json`; exact code/binary/source hashes,
import counts and reports are in `native_physical_timer_lifecycle_followup` in
the verification index.

The helpers accept host ticks; they do not map Engine focus/loading events,
manage pause-source ownership or activate the process-frame clock. Engine/World/
renderer/physics consumers are unchanged, so their full builds/suites are not
repeated. Initialized settings, main-loop frame publication/prepared scheduling,
actor admission and complete native reactions remain open. No real game process,
host timing, contact or normal-input gameplay/restart acceptance is claimed.
All remaining M15S4 gameplay/fresh-process gates stay open. Full M15S4 is incomplete.


### Prepared native frames reach PhysicsSystem preparation

PhysicsSystem now snapshots and validates a prepared native frame before
collision/trigger updates or movement-queue consumption, then uses the existing
shared worker/world pass. Native movement/jump preparation keys off its supplied
step count rather than legacy accumulated time. Native time does not add to the
legacy accumulator. Skip/zero handling retains discard/synchronization behavior
without deferring body steps. World lookup is delayed until an active mobile
actor needs it, and weather is fetched only for actor movement. Owned bodies
and projectiles can step without World/weather services.

Twelve new cases at0/1/2 workers exercise actual PhysicsSystem preparation:
a10ms native step below the legacy1/60 threshold, fixed/variable/smoothed shared
movement/body timing, rejection before projectile velocity consumption, valid
retry, skip/nonpositive frames and no native time leaking into a later generic
zero-delta pass. These fixtures use a loose body and projectile; mobile capsule/
jump and full game-controller acceptance still require their own courses.

The initial normal suite passes1,675 tests, but the initial full sanitizer run
finds an existing OpenAL device-enumeration heap use-after-free during SoundRuntime
setup. Main-thread enumeration reads a list invalidated by background default-
device probing. Fix4ec21e105d serializes probes across output instances and copies
device names while locked. A new concurrent-output test performs128 enumerations.
The original ASan log/source identity/build ID are retained; its failed binary
SHA was not captured before rebuild. No suppression or rerun-only workaround is
used. This closes the observed UAF, not melee audible-feedback acceptance.

Final full normal and ASan/UBSan engine checks each pass **1,676 tests in123
suites**, exact inventories, no failures/skips. Tested dirty source fingerprint:
`c912210f7717813113469ef4b65e2765412122c0baf8492b0beaad6eda545f38`.
Both chunks were tested together against e00d863422 before separate commits;
exact code/binary hashes are retained. Final build logs have no warnings; initial
broad builds retain two existing warnings in unchanged World tests. ASan leaks
are disabled. Unchanged component/Python/schema checks are not repeated.
Reports and failure scope are in `native_physics_system_frame_followup` in the
verification index.

World still calls the legacy physics entry. Engine/World process-clock settings,
pause-source ownership, frame publication/activation, native actor admission,
KF/selector/subtype/group/filter/root/capsule/morphology and complete reactions/
FSM/GetUp remain open, alongside all remaining normal-input/view/device/FPS/
fresh-process gameplay/save-restart gates. No new physical contact or game-input
course is claimed. Full M15S4 is incomplete.


### Prepared native frames enter World physics

World now accepts a prepared native frame through an explicit Oblivion entry.
It snapshots and validates the frame and carry, and checks required services,
before native scene updates or stats publication. Active forwarding retains the
existing door/projectile/physics/hit/actor synchronization order: World uses the
input delta and PhysicsSystem receives the prepared substeps. The generic entry
continues through the shared legacy path.

Four new World cases cover invalid frames/carry, missing services, nonnative
profile rejection and paused loose-body preservation with zero worker stats at
0/1/2 workers. Both full normal and ASan/UBSan engine checks pass **1,680 tests
in123 suites**, exact inventories, no failures/skips. Tested dirty fingerprint:
`94de9d15e6abff2f23cc6bfd7efdc6e8bbcb6acee9548c52896070e4a3ab4d0c`.
Both helpers terminated before documentation or commit changes. Existing broad-
build warnings remain in unchanged World tests and normal CharacterController
code; no warning points to changed code. ASan leak checking is disabled.
Exact source/binary hashes and reports are in
`native_world_prepared_frame_followup` in the verification index. Unchanged
component/Python/schema checks are not repeated.

Engine still selects the legacy World entry. The new active forwarding path is
compiled, but these World tests exercise pause and rejection, not a complete
active rendering/projectile backend. Prior lower PhysicsSystem/scheduler tests
prove shared stepping only at their layer. Native process-clock activation,
settings/pause ownership, actor admission and complete reactions/FSM/GetUp remain
open. No new physical-contact, normal-input or fresh-process gameplay/restart
course is claimed. All remaining S4 acceptance gates stay open. Full M15S4 is
incomplete.


### Engine activates the native process clock and prepared World path

Oblivion now automatically selects one Engine-owned native millisecond timer
and physical frame clock. Events are captured before the SDL unsigned tick is
sampled; duration-dependent input, scripts/mechanics/World and simulation/render
clock publication use the native input delta. World receives the separate
prepared substeps/carry. The legacy profile retains its prior main-loop double
time. Hidden-window polling owns one suspension and preserves carry. Three
explicit OpenMW Physics settings default to fixed step0.0167, mode0 and
smoothing0.05; this does not import an external Oblivion INI.

Ten new cases cover exact retained original bit rows across tick wrap/caps,
divided/smoothed configuration, hidden polling and atomic rejection, event order,
nested loading control-disable preservation, and clock-to-PhysicsSystem shared
body/projectile stepping at0/1/2 workers. Both full normal and ASan/UBSan checks
pass **1,690 engine tests in124 suites** and **2,930 component tests in274 suites**,
exact inventories, no failures/skips. Tested dirty fingerprint:
`cb1d13164eef5c5e0f3b69dd75c1034c77d7ca41f97f84fb5ebdd12f2d5c6440`.
Both initial1689-case attempts retain three new fixture failures: the test
queued projectile velocity only once across valid zero-step frames. Corrected
fixture supplies it every frame, as actual ProjectileManager does. No queue
semantics were changed. Final incremental build logs have no warnings; broad
initial builds retain warnings in unchanged World tests, CharacterController
and sanitizer libstdc++ regex instantiated through unchanged console.cpp.
ASan leaks are disabled; Python/schema code is unchanged and not retested.

Real-content normal-input first-person unarmed miss and idle fresh-process
continuation pass in PIDs351102/351292, both terminal. One miss advances action2
->3 and spends Fatigue140->133; no strike/miss/swish replays after load. Nine
native AV arrays, four Player items and combat RNG survive; schema29 absent
optional extra_scale/ownership fields are normalized to their declared defaults.
Actual PCM matches stock swishhand02 once (correlation0.9999203797) and none
after restart. Ready/restart images were directly inspected: dungeon and drawn
first-person hands render. Recorded134 native frames all take166ms/3 steps;
130 have positive worker timing. This is a capped-frame course, **not steady
30FPS acceptance**. Other modes and short/zero frames retain lower-layer tests.
The first runtime attempt retains missing engine timing collection and an old
verifier's absent-extra_scale mismatch;02 enables the existing stats list and
normalizes only the schema-defined absent value, without lowering assertions.

Editable recipe: `M15-NATIVE-FRAME-CLOCK-RUNTIME-PROBE.json`. Exact source/binary/
defaults hashes, failed attempts and reports are in
`native_engine_process_clock_followup` in the verification index. Static review
confirms message6 dispatch to406130, but B333F0 has object-pointer cleanup at
A166D0: it is not a proven background boolean. Original focus/loading ownership
and scene-complex command lifecycle remain open; the current adapter retains
OpenMW visibility policy. Native actor admission, complete contact/reaction/FSM/
GetUp and all remaining view/device/FPS and in-action restart gates remain open.
The real-content course uses the normal binary; sanitizer evidence here is the
synthetic full inventories. Full M15S4 is incomplete.


### Contact-selected armor impact feedback reaches live playback

Committed ordinary unblocked hits no longer mute all impact sound merely
because the victim wears armor. The already prepared wear selection carries
its Light/Heavy material into the controller's existing once-only sound result.
No selected item means body/weapon layers, including positive-Fatigue punches
that skip armor selection. Material survives the Player god-mode condition-write
guard. Creature victims now bypass NPC armor selection/RNG, matching5FFBE3;
equipped-block playback is unchanged.

The pinned executable executes **2,160 caller cases** at007F/027F/037F across
Character/Creature, armor fraction/wear gates, absent/Light/Heavy selected entry,
prior block sound and weapon types. Actual base/armor getters execute; selection,
condition writer, sound-call and teardown boundaries are explicit. Existing
independent palette and RNG oracles remain their own layers. Four World cases
are expanded for material selection, god-mode no-write, positive hand Fatigue
and Creature rating without NPC RNG. Full normal and ASan/UBSan engine checks
each pass **1,690 tests in124 suites**, exact inventories, no failures/skips.
Tested dirty fingerprint:
`aea693de940c65f889e6102e90cfacd240fd216998816e1f3a69ba457186da05`.
Only existing warnings in unchanged CharacterController/World-test code remain.
ASan leaks are disabled; unchanged component/Python checks are not repeated.

A valid identical staged input reproduces the old bug on9767ef4638: one actual
shortsword hit, correct damage/wear/three RNG draws, zero impact requests/PCM
cluster. Corrected normal processes358525/358625 are terminal and pass the same
contact plus fresh continuation. The three Heavy/Flesh/BladeFlesh voices play
once; actual PCM jointly matches stock clips at **0.9996168549**, with each family
contributing well above the predeclared1% threshold. Baseline and continuation
have no matching cluster. Nine native AV arrays and Player/target inventories
match baseline exactly; combat seed1->415139642, weapon55.875->55.5750007629 and
armor250.125->249.788726807 occur once, with no replay. Both images were directly
reviewed: named Valen Dreth stands in the staged cuirass, drawn shortsword/HUD
and dungeon render.

The old fixture first fails envelope admission (T4VR21 vs T4ST27), then unknown
legacy Active process fields. Both attempts remain retained. Valid03 repairs
only the copied envelope, declares action-1/knocked0 initial idle fields for9
actors and uses the typed writer to52; it injects no contact outcome and proves
no legacy unknown-state migration gate. Its historical Health expectation was
also wider-FPU: full original weapon+mitigation replay at previously observed
live007F yields1062088582 rather than1062088581 at027F/037F. Both baseline and
corrected code match the live value exactly; no arithmetic/tolerance changed.

Editable inputs and all source/binary, PCM/asset, process/save and failed-attempt
receipts are in `M15-ARMOR-HIT-FEEDBACK-PROBE.json` and
`native_armor_hit_feedback_followup` in the verification index. This is a normal
build, staged unblocked Heavy-armor shortsword course at1FPS. Light/unarmed/
Creature crosses retain lower-layer checks; remaining live material/weapon,
view/device/FPS, actor admission, reaction/FSM/GetUp, mastery/breakage/effects
and in-action restart gates stay open. Full M15S4 is incomplete.


### Retained melee weapon breaks publish with the contact

Attacking Player weapons and NPC quest weapons can now reach zero condition in
an ordinary/standing-power melee contact without rejecting the entire hit.
Native readiness gates the consequence: an Active weapon-out process requests
unequip; an undrawn/Low process commits wear without an equipment request.
Ordinary drawn NPC weapon drops still reject. Defensive weapon/shield and
selected-armor break paths retain their existing restrictions.

The prepared contact stages carried-right unequip with condition, Health,
Fatigue, RNG, action consumption and projections. World reserves a separate
deferred notification for each item. Observers see the complete transaction;
a throwing first observer cannot lose the other item's queued notification.
Inventory clear, stale draw/condition, malformed or duplicate requests and
World clear cannot publish or replay a stale contact. God-mode suppressed wear
does not publish an unequip. Two new World cases exercise these transaction
properties; the existing weapon-policy case adds Player, quest and undrawn
breaks while preserving ordinary NPC drop rejection.

Full normal and ASan/UBSan checks each pass **1,692 tests in124 suites**, exact
unfiltered inventories, no failures/skips. Tested dirty fingerprint:
`f069dc38863f106ad54bb8fc96a306223b8cef8914773b6d6c315e40c928e56b`.
ASan leaks are disabled; unchanged component/Python checks are not repeated.
Normal01 retains a new-fixture error from full save capture without a Player;
the equivalent sanitizer01 build was stopped before correcting the assertion.
Corrected02 uses the nonpublishing dynamic-key preparation for serial checks.
Only the test fixture changed, and the live binary matches final normal exactly.

Normal-input processes360170/360273 are terminal. A staged shortsword starts at
condition1.125; one keyboard left attack commits ID1, spends Fatigue140->132.2,
and damages unarmored Valen Dreth by exactly **0.42397457361221313**, native bits
1054413618. The weapon reaches condition0, stays at count1 and becomes unequipped.
The next actual controller update interrupts the sword animation; both reviewed
captures show fists and Hand to Hand HUD. Two impact voices request playback
once; no new PCM waveform analysis is claimed. Fresh load uses the exact first
output, preserves the broken unequipped item and Health debit, and replays no
contact or sound. All9 native AV arrays, full Player/target item metadata,
RNG1, action next2/empty pending and unchanged dynamic serial are checked.

The verifier's retained failures concern absent optional Damage and inventory
serialization order. Corrections accept the schema-defined initial zero and
compare complete records by base key; exact result bits/metadata remain required.
Independent expectations and receipts are in `M15-RETAINED-MELEE-BREAK-PROBE.json`
and `native_retained_melee_break_followup` in the verification index.
This is first-person keyboard replay at1FPS with staged equipped gear and idle
restart after interruption. NPC live quest breaks, ordinary NPC drops, defensive
breaks, reactions/FSM/GetUp, mastery/effects and the complete actor/view/device/
FPS/in-action restart gates remain open. **Full M15S4 is incomplete.**


### NPC attacking weapon breaks create real drops; full reload remains open

Ordinary NPC attacking weapons can now wear to zero and drop through the actual
contact owner. Explicit None/Unequip/Drop policy preserves Player/quest retained
unequip and undrawn/Low no-request behavior. World prepares the native equipped
weapon factory with its current renderer, animation and physics. External scene
admission and final source/resource revalidation precede damage, Fatigue, wear,
RNG, source removal, drop relations, dynamic serial, registry and cell publication.
Observers run afterward through the existing deferred queues. A rejected or
throwing scene admission leaves the contact pending and resources untouched.

Three new World cases cover real owned render/animation/Bullet context, scene
rejection/throw, late god/draw/condition changes, mismatched contexts, destroyed
owners and a drop plus retained unequip with a throwing first observer. The
production path rejects missing rendering; the explicit context API validates
exact Drop/request correspondence and uses real assets, not a headless bypass.
The existing policy case now expects the ordinary NPC Drop disposition.

The initial live stock sword fails loose-object pose binding because cloned
static Drawable geometry is shared with its cached template. The preserved GDB
throw trace locates that path. A focused baseline reproduces the same failure;
its physical-transform/cycle rejection case still passes. The renderer fix in
`adc5a538cd` excludes Drawable leaves only. Every transform/path/identity guard
remains. Two component cases prove private pose writes and shared-ancestor/cycle
rejection. Full normal and ASan/UBSan runs each pass **1,695 engine tests/124 suites
and 2,932 component tests/274 suites**, unfiltered, no failures/skips. Fingerprint:
`635b808014be1ef74202b796d1a7a91a49f1b2c88e31125b45f64dd36fdc098c`.
ASan leaks are disabled. Both helpers finished before the separate renderer
commit; no C++ bytes changed afterward. Python implementation is unchanged.

Normal and instrumented normal-input courses activate the deterministic NPC
through Space, then use the production AI/controller/contact. One ID2 attack
spends NPC Fatigue155->147.2 and damages the unblocked Player by exactly
**0.5148262977600098**, bits1057213352. NPC sword condition1.125 reaches0, its
inventory count reaches0 and one real count1 loose reference is published at
`dynamic:native-reference:0000000000000001`. Serial1->2, unowned item metadata and
both drop relations survive exact-lineage fresh processes without repeated
contact, item, action or hit sound. All9 native AV arrays, lifecycle/death state,
engagements and Player inventory are checked; combat seed505908858 is unchanged.
The initial instrumented save catches partial recovery under slower frames;
corrected02 waits for a read-only actual155 recovery acknowledgment before F5,
keeping the exact final channel assertions. No resource or outcome is injected.

Both normal captures show the NPC using fists while Player keeps its own sword.
An additional copied-save camera pitch/yaw setup directly shows the separate
stock sword on the dungeon floor and preserves the committed resources/item.
This is visual inspection, not normal look-input or exact-lineage restart proof.
The first verifier overconstrains resident-reference cache growth. Corrected
checks require exactly one dynamic identity, preserve every original reference
and independently resolve all added static identities/base/CELL against the
unchanged master. They do not claim unchanged whole-world cache size.

Editable inputs and source/binary/content, failed-attempt, process/save/image
receipts: `M15-NPC-MELEE-BREAK-PROBE.json` and `native_npc_melee_break_followup` in
the verification index. This delivers initial attacking-NPC contact/drop
publication. The instrumented images expose a hard reload gap: the loose sword's scabbard reappears and generic
static insertion replaces native loose physics. Resource/identity persistence
is proven, while full drop/restart acceptance fails. Persist and restore native
loose-body/render state, then repeat the falling-item restart course. Defensive
weapon/shield/armor breaks, mastery/effects, complete actor admission and native reactions/FSM/GetUp, remaining weapon/view/device/FPS and
in-action restart cases stay open. **Full M15S4 is incomplete.**


## Loose-weapon save contract — schema53 checkpoint

Runtime schema53 adds a separate stable-reference map for native loose weapons.
It carries the normalized model and opaque NIF hash, body/node identity, rigid
world pose, spatial and exact four-lane native velocities, dynamic motion,
admitted physical scale, scene collision flags and optional cached local root.
Actor ragdoll ownership still requires its own native values/life authority.
The new collection follows crime state; versions1..52 keep their existing wire
layout and do not invent lost physical state. Both C++ and Python codecs enforce
these contracts, including signed-zero lanes and absent-root state.

All **2934 component cases /274 suites** pass normally and under ASan/UBSan,
with exact unfiltered inventory/XML agreement, no failures or skips. All **324
Python cases** pass. Evidence: `S4/native-loose-weapon-schema53-normal-02` and
`-sanitized-02`. Tested dirty-source fingerprint:
`1c0ec2a2bd471f009621773d18275c94fc567cf843dbb798bf8399e2d57ebfa0`.
ASan leak checks are disabled. The first full attempts are retained: two old
clock/crime wire tests assumed crime ended the payload; corrected tests account
for the schema53 tail and explicitly pin the schema43 fixture.

Both codecs match a literal new wire suffix, reject malformed/truncated or
duplicate snapshots, and preserve legacy absence. The save-rewrite test preserves
opaque records and source bytes; invalid legacy-labeled physical state leaves
the destination untouched. Receipts are under `native_loose_weapon_schema53` in
the verification index. Later evidence documentation does not change tested code.

This is **codec and validation evidence only**. World does not yet capture this
map or restore physical/render ownership from it. The previous falling-weapon
restart failure remains open: generic static insertion and the returning
scabbard still require the scene-load bridge. Capture loaded bodies, retain
unloaded snapshots, stage restored bodies and the physical model before scene
publication, then repeat the airborne fresh-process course. No engine or new
runtime acceptance is claimed. **Full M15S4 remains incomplete.**


## Detached loose-body restoration — physics checkpoint

PhysicsSystem and the scheduler can capture a typed native loose-weapon snapshot
and prepare its restored body privately. Restoration checks the supplied asset,
base/model and body/node identities, scene flags and reference-root mapping,
then rebuilds shape/mass with the saved admission scale and restores pose,
velocities and cached root before collision publication. Current visible scale
remains independent. Cancellation and rejected publication leave no owned body.

All **1701 engine cases /124 suites** pass normally and under ASan/UBSan, with
exact unfiltered inventory/XML agreement, no failures or skips. Evidence:
`S4/native-loose-snapshot-bridge-normal-02` and `-sanitized-01`. Tested dirty-source
fingerprint:
`15ed51dc704ed9519a9a69ee0c44b5b4564740c8b488fc37948423924353c3b3`.
ASan leak checks are disabled. The retained normal-01 failure omits required
Player race/class identities in the new save fixture; corrected02 supplies them
and adds shape/mass comparisons without changing production code.

The new synthetic one-body cases run with 0/1/2 workers. After a binary DTO round
trip, eight resumed airborne steps match uninterrupted linear/angular velocity
exactly and position within .002 game units. Shape bounds and inverse mass match
exactly with admission scale 1.25 and visible scale .75. Cached-root state,
cancellation, stale asset/reference rejection, failed insertion/retry and cleared
preparation ownership are checked. Receipts and boundaries are under
`native_loose_snapshot_bridge` in the verification index.

World does not yet capture these bodies or call this admission path on load.
The prior real-content scabbard/static-body restart failure remains open. Also
retained is a separate failing Python two-owner ordering reproduction: serialized
key sorting differs from C++ FormKey ordering for namespace prefixes (`a`, `a-`).
Correct that codec edge case, wire retained World state and physical scene
admission, then repeat the airborne fresh-process course. No fresh runtime or
component/Python verification is claimed for this physics chunk.
**Full M15S4 remains incomplete.**


## Loose-weapon owner ordering — codec correction

The Python writer now orders the new loose-weapon collection by FormKey's
kind/namespace/value components. Sorting the whole serialized key incorrectly
put `dynamic:a-:...` before `dynamic:a:...`; native C++ orders namespace `a`
first. A failing two-owner reproduction is retained in
`S4/native-loose-weapon-order-baseline-01`. Extended C++/Python wire assertions
pin the same native owner order and literal body-entry tail, including reversed
Python input order. The single-owner golden bytes and schema53 fields do not
change; readers still accept prior owner order.

All **2934 component cases /274 suites** pass normally and under ASan/UBSan,
with exact unfiltered inventories, no failures or skips. All **324 Python cases**
pass. Evidence: `S4/native-loose-weapon-order-normal-01` and `-sanitized-01`;
receipts are under `native_loose_weapon_owner_order` in the verification index.
Tested dirty-source fingerprint:
`2c93052fde9d75f3072842acad1fae60a8e75460e35ba26fb323fe9b8e942f96`.
ASan leak checks are disabled. Engine C++ behavior is unchanged from the prior
physics checkpoint; no new engine or runtime acceptance is claimed here.

The earlier ordering follow-up is resolved. World retention and loaded-item
physical/render admission, followed by the falling-item fresh-process course,
remain next. **Full M15S4 is incomplete.**


## World loose-body save/load integration — floor gate open

World now captures and retains native loose-weapon snapshots, validates winning
base/model identity, and admits a private physical model and restored body during
scene load. Immutable retained state and reference liveness guards protect
reentrant publication; rejected or retired publications roll back. Legacy drops
bootstrap physical models with zero velocity because older saves lost motion.
Tests cover both reference-root forms, scabbard removal, binary restoration,
continued motion, rejected publication, reentrant load/clear and legacy cleanup.

All **1704 engine cases /124 suites** pass normally and under ASan/UBSan,
with exact unfiltered inventories and no failures or skips. Evidence:
`S4/native-loose-world-load-normal-02` and `-sanitized-01`; tested fingerprint
`ec6305047fc8504651f1b3e5931fd6dcec4de1dfbefe051aa8397b32dc772097`.
ASan leak checking is disabled. Receipts: `native_loose_world_load`.

The actual NPC weapon-break course saved a moving loose body in schema53 and
loaded those exact bytes in a fresh process. Identity and downward motion
survived, but the sword continued below the dungeon floor: saved z changed
from -73.755 to -341.654. The existing 128-unit height bound failed. Static
world collision masks exclude the World category currently used by loose
bodies. Retained failure: `S4/native-npc-melee-break-reload-runtime-normal-01/physical-restart-failed-01.json`.
This is not runtime acceptance. Fix collision routing and repeat the real
course. **Full drop/restart acceptance and M15S4 remain incomplete.**


## Loose weapon collision routing — bounded physical restart

Loose native weapons now carry a separate collision bit alongside World
membership, on initial drop, schema53 restoration and legacy bootstrap. Static
objects accept that bit while continuing to exclude other static geometry.
World selection/LOS queries still see the weapon; actor-only queries exclude it.
No native arithmetic, collision shape or initial drop pose changes.

A retained baseline fails with 0/1/2 workers against an actual MWPhysics::Object
floor. The corrected case settles at the native-scaled sphere radius and checks
world-ray visibility and actor-ray exclusion. All **1707 engine cases /124
suites** pass normally and under ASan/UBSan, unfiltered, no failures/skips.
Evidence: `S4/native-loose-static-mask-normal-01` and `-sanitized-01`; fingerprint
`1c7e892709cf61f65621224db64c23ce1f6d901fdac918a0e1fadf377c0fb1b8`.
ASan leaks are disabled. Only editable runtime manifests and documentation follow
the frozen C++ checks. Component/Python implementation is unchanged.

The normal NPC break saves a naturally falling sword at z -73.755 with vertical
velocity -251.421. A new process loads those exact bytes and lands the same
asset/body at z -106.826, speed about .20. Its first typed loose-body snapshot is
identical to the earlier failed-floor course; the continuation now meets the
unchanged placement bounds. Instrumented body lands and settles, but its saved reference trails the body
by .016 game units and fails the preregistered .01 translation tolerance.
Reference/body save coherence remains open.
All nine native AV arrays, exact Player Health damage, inventories, broken
condition, item ownership/dropper relations, serial, action counter and RNG are
checked, with no duplicate contact/drop/sound request after load. Normal evidence:
`S4/native-loose-floor-runtime-normal-02`; instrumented evidence:
`S4/native-loose-floor-runtime-sanitized-02`. Instrumented02 reuses the actual
terminal first epoch/output from01, then starts a new continuation epoch with
300 seconds of wall settling. Its earlier01 save was still airborne and failed
the same speed bound; neither binary, source save nor acceptance bounds change.
The unsupported offscreen mouse preflight failure is retained separately.
The longer run also exposed a parent-group-only auditor error for four exterior
persistent markers: unchanged master positions resolve to CELL(12,17) in the
same original worldspace, rather than its persistent storage CELL(0,0).
Corrected audit02 independently checks record/base identity, original positions,
Persistent flags, worldspace and CELL coordinates; the initial audit failure
is retained. No placement or pose tolerance is relaxed.

A directly reviewed normal camera fixture shows the exposed stock shortsword
resting on the dungeon floor, NPC unarmed and Player retaining the drawn sword;
no scabbard covers the dropped blade. Only copied Player pitch/yaw were staged
for this additional visual inspection; it is not ordinary look-input or exact
restart-lineage evidence. Receipts and editable recipe:
`M15-NATIVE-LOOSE-RESTART-PROBE.json` / `native_loose_collision_routing`.

This closes the earlier generic-static/scabbard and floor failures for the
bounded ordinary attacking-NPC course. Exact real-content uninterrupted
trajectory equivalence, defensive breaks, mastery/effects, other views/devices/
FPS, all named interruptions and the combined gameplay gate remain open.
**Full M15S4 remains incomplete.**


## Preserve native reference/cache separation across restart

The preceding instrumented .016 reference/body difference is **not a proved
save defect**. The absolute reference/cache equality assertion was invalid.
Reinspection of the hash-verified original registered-root corpus finds **64
of512 contexts** where no reference callback fires and local/reference
translation differs by more than .01. Native traversal compares each desired
pose with the previous local, while reference fields retain the last selected
callback. Small per-frame motions can accumulate beyond that threshold.
The original failed assertion is retained and explicitly superseded.

A fresh normal and ASan/UBSan comparison of the current production functions
matches all **512 original cases bit exact**, including node local/world, mask
and reference float lanes. This reuses the existing original-executable corpus;
it is not a new original-game run. Evidence: `S4/native-loose-reference-policy-audit-01`
and `native-loose-reference-policy-compare-01`. The original executable remains
SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.

The new World regression makes four successive .006-unit steps with no reference
callback, saves the resulting distinct reference/cached-local/body state, and
binary-restores it through actual model/body readmission. Exact cached state
and stored reference fields survive; a stationary traversal preserves reference
fields, then a .02-unit step publishes position only. **1708 engine cases /124
suites** pass normally and under ASan/UBSan, exact unfiltered inventories, no
failures/skips. Evidence: `S4/native-loose-reference-cache-normal-01` and
`-sanitized-01`; fingerprint
`234dee25779f3631c0e6e1fc77b4be9777da6a84cae750b71cdc59d379857776`.
ASan leaks are disabled. Gameplay code is unchanged in this follow-up.

Corrected02 acceptance of the existing normal and instrumented physical courses
checks body/cached-root agreement with the original .01 tolerance and preserves
the independently published reference policy. Placement bounds, resources, item
identity and no-replay checks are unchanged. Both bounded courses pass. The
instrumented sword settles at z -106.735 with speed about .00003; its reference
z -106.751 remains a distinct native publication. This corrects an expectation
after observing the failed check; it is not presented as a preregistered pass.
No exact real-content uninterrupted trajectory comparison is claimed.

The corrected persistent-exterior audit also rejects eight private negative
controls: changed base or one-ULP position, wrong physical cell/world/coordinates,
missing reference/parent Persistent flags and interior target. Original files
remain unchanged. Editable audit/verifier sources, expectations and receipts are
in `M15-NATIVE-LOOSE-RESTART-PROBE.json` and the verification index under
`native_loose_reference_cache_persistence`. The earlier failed harness and
invalid equality assertions remain in their evidence directories.

The bounded ordinary attacking-NPC physical/render restart gap is resolved.
Defensive breaks, mastery/effects, native reactions, remaining input/view/FPS
and in-action restart matrices, plus the combined gameplay gate remain open.
**Full M15S4 remains incomplete.**


## Blocking weapon wear-to-zero publishes equipment and posture together

A drawn, active Player or quest weapon that breaks while blocking now unequips
with its contact resource/condition changes. An ordinary NPC's blocking weapon
uses the existing owned physical-drop transaction. Both withdraw process action6
to -1 atomically. Scene admission failure restores the blocking posture, carried
item, resource arrays, action ownership and dynamic serial. God-mode suppression
keeps the Player's weapon, condition and posture together. Deferred observer
failure cannot replay the committed contact.

This follows three new isolated original-instruction probes against executable
SHA256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`:

- **24 caller contexts** execute005F5C69..005F5CD6. A present blocking item calls
  condition virtual+2C4; its true AL return calls005F4AE0(false) on the defender.
  Condition arithmetic/equipment mutation and the withdrawal body are boundaries
  in this caller probe.
- **8 withdrawal contexts** execute005F4AE0(false), including the actual Player
  getter0065D750. With a valid supplied skeleton and action6, native code requests
  animation1, action-1/flag0 and clears auxiliary+49; action0 does nothing. Sequence
  getters, animation requests and action application remain recorded boundaries.
  The supplied .125 fade value is a fixture, not a winning default or timing claim.
- **12 post-wear sound contexts** execute005FF914..005FFB96 and the real006AFFF0
  selector. The caller re-reads carried equipment after wear. An empty slot picks
  `WPNBlockHand` for the defender's layer while retaining the weapon-block branch.
  Item/position getters are supplied and sound lookup records EDIDs; this is not
  actual original-game playback. All contexts agree at x87 control027F/037F.

The previously verified 720-context original generic break-dispatch corpus supplies
Player/quest retention versus ordinary NPC drop policy. No new block arithmetic
was inferred from fork results. Editable probes, boundaries, report hashes,
runtime recipes and verifier sources are in `M15-DEFENSIVE-WEAPON-BREAK-PROBE.json`.
The initial withdrawal harness had an incorrect Player getter stack-pop boundary;
the corrected04 probe executes that original getter. Failed harnesses remain local.

The added World regressions exercise Novice/Apprentice retained breaks, stale
posture rejection, deferred observer failure, consumed-action binary restore,
ordinary NPC physical/render drop and scene rollback, and Player god-mode
suppression. Initial attempts exposed test setup errors: a missing winning placed
record, the copied attacker's position behind the defender, and clearing the
blocking weapon's enchantment while retaining its charge. All failed runs remain
in their original evidence directories; none is presented as an engine defect.

Actual normal and ASan/UBSan gameplay courses hold the remapped keyboard block
against one production-policy NPC sword strike. Only the Player sword condition
is staged near zero in the pristine source; input, contact, damage and break
consequences use the production path. At the committed hit, `victim_action=-1`,
the blocking sword is retained at condition0 and unequipped, and no world drop or
serial is allocated. Native Player Health damage is exactly float bits1065087139;
all nine actor-value arrays otherwise match the independent expectations. The NPC
retains its sword at55.7 after attacking wear. Actual onehandblockidle releases;
a held input may begin handtohandblockidle, and key release releases it too.

Each completed-contact save is the exact input to a distinct fresh engine process.
Both saves retain posture-1, the broken unequipped Player sword, action-next3 and
combat RNG505908858. There is no replayed contact, debit, condition change or sound.
Directly reviewed screenshots show first-person bare hands and Hand to Hand HUD
before and after restart, with the NPC's sword retained. Camera input was not staged.

Captured mixer PCM matches one weighted stock Blade+Hand mixture before restart
and zero afterward. The unchanged preregistered .75 correlation threshold, positive
component gains with minimum .2 fraction, and relative start bound ±25ms reject
all six single-component controls and silence. All27 stock-pair/start-offset
positive controls pass: **34 preflight cases** total. Normal maximum correlation
is .9983668; ASan/UBSan is .9874182. This establishes captured mixer output for this
course, not hardware listening or native pitch/reverb equivalence.

This completes the bounded retained Player defensive-break gameplay/restart route
and owned NPC defensive-drop integration. Normal-input NPC defensive-drop gameplay,
shield/armor break, initially broken equipment, creature defense, Expert/Master
procs, effects, exact reaction fade timing and broader input/view/FPS/in-action
restart matrices remain open. **Full M15S4 remains incomplete.**

Final engine checks: normal `S4/native-defensive-weapon-break-normal-05` and
ASan/UBSan `S4/native-defensive-weapon-break-sanitized-05`. Both rebuild engine/tools
and pass all **1711 cases /124 suites**, exact unfiltered inventories, no failures
or skips. ASan leaks are disabled. Tested source fingerprint: `bc4dfa57bbb12f5fc563272a5b187f7bd3f5ebb662fc9a51d75d38f7223dbf19`.
Actual runtime binaries retain their verified pre-launch hashes after the final
test-fixture-only rebuilds; no gameplay source changed between courses and checks.


## Defensive NPC drops and thin compound collision

The ordinary NPC defensive-break route now has normal Player-input evidence.
The Player unequips its own sword in the staged source and attacks unarmed;
the NPC holds public Actor block controls until the carried slot disappears.
No script writes damage, condition, contact outcomes or resources. The NPC's
base Block25 selects Apprentice: fraction .125, no block Fatigue debit, incoming
unarmed Health1.42/Fatigue1.71, and remaining Health float bits1067387453.
Original instruction probes against executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`
freeze those values before the first course. The original block/cost/wear and
physical-sink slices have explicit reader/writer boundaries; this is not a new
original-game run. Generic break policy and post-wear sound caller evidence
remain in `M15-DEFENSIVE-WEAPON-BREAK-PROBE.json`.

The first normal course exposed a real physical failure: the blade saved at
z -621.521 below the floor. Read-only GDB traces show admission above the floor,
correct group257/mask319, initial floor contact, then loss through the surface.
An isolated stock-asset/triangle-floor reproduction also tunnels. The existing
static collision routing is correct; discrete collision detection misses thin
compound parts between poses, and solver position correction can cross the
surface before final motion integration.

Loose bodies now opt into compound CCD in the Bullet adapter. Actual convex
children supply predictive contact points and lever arms. When the mass center
lies inside a convex part, a smaller contained sphere guards the complete step,
including solver position correction. GJK/EPA signed distance establishes that
interior; the helper returns zero for a gap between disconnected parts. The
adapter preserves compound geometry, mass/inertia and native gravity, damping
and frame rules. It uses Bullet's existing penetration allowance for sweep
clearance. No original Havok quality/CCD or exact trajectory equivalence is
claimed.

The owned regression fails on all three worker counts before the change. Its
final form exercises two step sizes (1/60 and .05), two orientations and both
allowed/denied floor masks in a shared worker pool. A separate component
check verifies interior radius, nested transforms, unchanged AABB and no sphere
across a compound gap. Twelve authored stock-blade orientations settle with zero
saved vertical velocity through the actual production library, normally and
under ASan/UBSan. These are fork adapter checks, separate from original-rule
oracles and actual gameplay.

Failed prototypes are retained. Child prediction alone, incomplete lever-arm
mapping, and guards starting after the solver each leave orientation-dependent
failures. A contained sphere alone prevents escape but leaves one persistent
velocity case. The combined final prototype passes all twelve orientations
without changing the .2 speed criterion. The first instrumented engine run
passes the new cases but later exhausts Bullet's global increasing thread-index
counter: the fixture originally created eight pools per worker-count case.
Reusing one pool across its scenarios fixes that fixture resource failure.
Component checks are unchanged and reused; both engine inventories are rerun.

Both completed-contact gameplay saves retain one broken Rusty Iron Shortsword
reference/body, source NPC weapon count0, correct dropper, absent owner/charge,
serial2, action-next3 and unchanged combat RNG505908858. NPC posture is -1 at the
committed hit and in both saves. Stored Health damage matches the original bits;
the contact log displays Fatigue1.49625, Player spending140 ->133, and .125 block
with no block debit. After regeneration, all nine actor-value arrays match the
expected state. No repeated contact, sound request, allocation or resource debit
occurs in the exact-byte fresh-process continuation. Body/cached-local agreement
uses .01; independently published reference fields retain the established native
masked-callback policy.

The old continuation fixture removed the NPC Lua source and was correctly
rejected for a content-fingerprint mismatch. Corrected courses retain identical
content across epochs. The first verifier also confused absent owner `None`
with a `"null"` FormKey string; both failed receipts and corrected expectations
are retained. Those fixture/schema errors are separate from the below-floor bug.

Actual forward-view screenshots show both actors unarmed after the break and
restart. An additional copied-save camera inspection shows the exposed blade on
the floor and the Rusty Iron Shortsword crosshair tooltip. That inspection changes
only Player camera orientation; it is not ordinary mouse-look or an exact-lineage
restart claim. Audio mixer output is retained and both post-wear Hand sound
requests report playback; identical IDs cannot establish two separate voices
when the same stock variant is selected.

This verifies the bounded ordinary NPC defensive contacts and completed-contact
physics restart in both builds, with floor-blade rendering in separate normal
and instrumented camera diagnostics. The first instrumented altered-pitch capture
was dark and remains retained. A retry with identical staged-save bytes and
executable, explicit window focus and a 90-second wall wait shows the floor, blade
and Rusty Iron Shortsword tooltip. Both focus and wait changed, so this does not
isolate the earlier capture cause or establish a camera code fix. The retry is
`S4/native-defensive-npc-break-floor-visual-sanitized-02`; its preregistered
acceptance, terminal process receipt, source/binary pins and directly reviewed
image hash are recorded in the probe document. No gameplay source changed.
Shield/armor and initially broken items, remaining
mastery/effects/reactions, full input/view/FPS and in-action restart matrices, plus
the combined named-target/block/miss/wall/stagger course remain open.
**Full M15S4 remains incomplete.**

Full component checks: **2935 cases /275 suites** normally and under ASan/UBSan,
`S4/native-loose-compound-sweep-normal-01` and `-sanitized-01` (component result).
Final full engine checks: **1714 cases /124 suites**, `-normal-02` and
`-sanitized-02`; exact unfiltered inventories, zero failures/skips. ASan leak
checking is disabled. Component fingerprint `4281f58fa3b6f4a17bb3a1b40fa3fac1768b5e634321c2aad9e1f3dbb4a24067`;
final engine fingerprint `969a35cbd4090ad0349fb9299a494c6bf5d495f5e558b237cbfd79bb87e76223`.
Only the worker-pool test fixture changes between those source snapshots.
Editable expectations, native probe reports, retained failures, current production
comparison and actual runtime receipts are in `M15-NPC-DEFENSIVE-DROP-PROBE.json`.

## Body armor condition zero preserves equipped inventory

Ordinary melee contacts now admit body armor wear to zero instead of rejecting
the entire hit. The shield-mask guard remains: shield equipment consequences are
not inferred from body armor. Original5F3870 calls actor virtual+2C0 before its
zero-condition branch. Player and Character resolve that slot to60E260, which
stores the original -1 armor-cache sentinel. Actual468FF0 slot13 membership then
gates equipment consequences; body armor never enters them. The fork's armor
query recomputes equipment directly, so no new serialized cache is needed.

The new original-executable probe executes672 body cases and48 shield controls,
both x87 words, against SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
It includes Player/NPC dispatch, all six body slots, a combined body mask,
positive/nonpositive resulting condition, process presence and drawn states.
Six additional contact cases execute5FFC56..5FFC74 through actual NPC5F3870 or
Player65FF10 and the original cache invalidation. The caller ignores broken AL.
Reader/writer/GMST and shield process-draw boundaries remain explicit; this is
instruction evidence, not a new original-game run.

The retained full484F80 precision probe supplies raw weapon damage1062506497 at
the previously observed live CW007F, versus1062506496 at027F/037F. With the
independently established IronCuirass condition1/max300 armor rating1, original
mitigation yields Health bits1062367192 at007F and1062367191 at the wider words.
The actual courses use the declared007F expectation. A prose description in the
runtime preregistration still names the wider value; its numerical criterion,
precision note and verifier use1062367192, established before either launch.
The canonical acceptance source corrects that descriptive typo.

The expanded World integration case exercises current conditions0,.5 and1,
preparation without mutation, one committed resource/condition/RNG publication,
retained cuirass slot, binary restore and consumed-action rejection. Its first
normal/instrumented runs fail only an incorrect second-hit expectation: the
fixture reused a worn sword and depleted Fatigue but expected first-hit damage.
The corrected fixture restores the independently verified initial profile for
each boundary. Those failed receipts remain; gameplay C++ does not change.

Actual first-person keyboard courses attack the staged restrained Valen Dreth
using the normal left-swing input with a1FPS cap. Only NPC IronCuirass condition is changed
from250.125 to1; the existing schema52 source is promoted to53 with an empty
loose-body list. The hit commits Health0.822019100189209, Player Fatigue140->132.2,
sword55.875->55.57500076293945, cuirass1->0 still equipped, and combat RNG
1->415139642 through three selection draws. All nine actors' complete value
arrays and prior inventories retain every undeclared value after regeneration.
No dynamic dropped item is allocated. Exact first-output bytes load in a fresh
process; no contact, sound, random or resource publication replays.

Directly reviewed post-hit and continuation captures show the named NPC still
wearing the cuirass, Player sword and dungeon. Actual PCM uses the unchanged
joint-template analysis from the prior Heavy/Flesh/Blade feedback course:
correlation >=.90, at least1% explained energy per family, one accepted first
impact and zero in continuation. Requests alone are not the audio criterion.
Normal and ASan/UBSan runtime receipts and hashes are in
`M15-BODY-ARMOR-BREAK-PROBE.json`.

The first sanitizer launch omitted the ASan-first preload required by SDL
replay and exits before native load; it is retained separately. The corrected
launch uses the existing verified preload with identical staged save bytes. A
second completed gameplay run has no accepted epoch receipt because its copied
monitor still matched the normal binary; it is retained and repeated using the
previous passing sanitizer monitor. Final acceptance uses the third cohort.
The older source also constructs additional persistent exterior ACHR records.
The first auditor's REFR-only alternative rejects those; the corrected auditor
requires original identity/base, both Persistent flags, the same original
worldspace, unchanged master position and exact physical CELL coordinates for
REFR/ACHR/ACRE. No broad whole-world preservation claim is made.

Final full engine checks cover1714 cases /124 suites normally and with
ASan/UBSan, exact unfiltered inventories and no failures/skips. Leak checking is
disabled. Component code is unchanged and its previous passing checks are
reused. Final tested fingerprint: cdf321788e2cf22798af74eba9020bf3e634169d71d9a5b58a37d0a7d0a435e0.

This closes the bounded NPC body-armor-zero contact and completed-contact
restart route. Shield breaks, remaining weapon initial-zero routes, mastery,
effects/reactions, full input/view/FPS and in-action restarts, and the combined
named-target/block/miss/wall/stagger course remain open.
**Full M15S4 remains incomplete.**

## Native sound folders and the next contact after armor reaches zero

Native SOUN folder selection now respects path component boundaries. VFS
recursive ranges intentionally match raw prefixes; the old native sound caller
removed the stored trailing separator and passed`bladeflesh` rather than
`bladeflesh/`. This admitted the adjacent`bladefleshenchanted` folder. Ordinary
Rusty Iron Shortsword hits could therefore play an enchanted impact asset.
The fix adds a separator for native folder enumeration while preserving direct
filenames, supported formats, recursive descendants, sorting and the existing
random-choice mechanism. VFS prefix semantics are unchanged.

Independent raw Oblivion.esm SOUN audit identifies ordinary18B9F1 with
`fx\wpn\weaponhit\bladeflesh\` and enchanted0C73BC with the distinct sibling
folder. Master SHA256 is
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`;
the sounds BSA is
`f11e92315666b7e6ee5f6936fa68300fb5234215c59d5e3836375f262fba05ec`.
The archive contains three ordinary and three adjacent enchanted variants. This
is a raw content/path audit, not a new original-executable folder-search probe.

The next-contact course exposed the defect through actual mixer output. The
first ordinary-folder logs select enchanted siblings, and plain-impact fits
fail the declared .90 correlation criterion. A diagnostic model using each
logged unwanted enchanted asset matches those failed recordings at .99964 and
.99924. Those fits identify the wrong route; they are not correct-route
acceptance. An earlier Doppler hypothesis is discarded: the normal zero-Doppler
control happened to select an ordinary file, while the instrumented zero-Doppler
control still selected an enchanted file and failed. No threshold is lowered.
Final corrected courses restore default Doppler.25 and audit every logged
native directory selection for the required component boundary.

Two owned VFS regression cases fail before the correction: ordinary selection
includes sibling folders and an adjacent same-stem file; a missing folder borrows
sibling audio. Both pass after the fix, including supported formats, sorted
order, real recursive descendants and an already terminated directory prefix.
Final full engine inventories pass **1716 cases /124 suites** normally and
under ASan/UBSan, without failures/skips. Leak checks are disabled. Tested parent
is585f364123 plus dirty source fingerprint
`482cc55dc0de776612120cde726b37e2084ef22698811318ecab927fc0215582`.
App executable hashes are recorded separately from test-binary hashes.

The further melee course starts from exact real body-armor continuation output,
SHA256`a788e9cacb984210349824b4c8a48f5bd4a0c7bb1613d827dd0ad9a9a2279221`,
without condition/resource/inventory/pose/AI edits. Its cuirass is already
worn at0 and its sword is55.57500076293945. Full original484F80 and
5FF712..5FF7BD execution freezes new damage1062469140
(0.8280956745147705), with the established native zero armor entry, in all three
x87 words. Action2 commits cumulative Health Damage bits1070806774
(-1.6501147747039795), sword condition bits1113397658, Fatigue140->132.2,
zero armor writes and zero combat random draws. RNG stays415139642; the cuirass
remains equipped at0. This exercises actual post-load armor mitigation through
a new ordinary contact. The melee implementation is unchanged from the body
armor chunk.

Both corrected normal/instrumented courses verify all nine actors' complete
value arrays, complete prior inventory multisets and declared lifecycle
contracts. Exact first-output bytes enter a distinct process. Action-next3,
empty pending/owner lists and no replay are verified. Directly reviewed captures
retain the named NPC, cuirass and drawn Player sword. Actual PCM contains the
required Flesh/ordinary BladeFlesh impact, with no Heavy request, then zero
impact clusters in continuation; default Doppler and the original .90 joint
criterion /1% per-family energy bounds apply.

Editable probes, retained failures/controls, full check receipts and current
runtime evidence are in`M15-NATIVE-SOUND-DIRECTORY-PROBE.json`; the body probe
links this subsequent-contact result. Canonical follow-up manifests are source.
This remains first-person keyboard with a1FPS cap, prior authored NPC restraint
and completed-contact restarts. Shield and initially broken weapon policies,
mastery/effects/reactions, full input/view/FPS and in-action restarts, and the
combined named-target/block/miss/wall/stagger course remain open.
**Full M15S4 is incomplete.**

### Selected-shield armor wear uses the attacker's posture and base mastery

The original caller's ordinary armor-selection path has an additional shield
branch that the earlier concatenated wear oracle did not include. At5FFC3D it
queries the attacker inEDI through5E5670, requiring native action6. It then
queries that attacker's base Block through5F23B0 and bypasses the selected
shield's further condition write at Journeyman or higher. This does not bypass
armor selection, consume fewer draws, change the selected material, or undo
any earlier blocking-item wear. Production now preserves those distinctions.

The prior paragraph describing unconditional ordered double shield wear is
superseded by this caller gate. The old equipped-block-oracle-03 concatenated
individual formulas and wear functions; it did not execute5FFC3D..5FFC54.
Its arithmetic remains evidence for the branch where a second write is admitted.

Original SHA256 remains
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
A576-case full shield wear probe executes actual ARMO RTTI vtableA441BC,
retention getter4D7030, worn-entry getters64B2D0/484E80/41DF10,
slot13 membership468FF0, armor-cache invalidation60E260, and drop-node rotation
conversion711440. The active process and worn bits gate shield consequences;
the weapon-out byte does not. Player/flag400 requests unequip retention;
ordinary NPC requests a drop. Condition/inventory mutation and drop construction
are boundary sinks, not original-game acceptance. The old body-armor probe's
supplied +F8 stub represented shield presence despite its `drawn` variable name;
its body conclusions stand, while that shield-control label is superseded.

A3456-case concatenation runs full shield wear, contiguous5FF914..5FFD69 with
full5E5A00/47DF80/9859DD selection, then the actual6AF880 sound selector prefix.
Clearing the supplied worn flag at captured unequip/drop sinks removes the early
shield sound and makes all seven later selection draws fail in the shield-only
fixture. General Flesh/weapon-Flesh sounds then replace the shield layer.
The surviving shield keeps its earlier sound, while the later selected entry
passes the attacker gate independently. Allocations, item count, inventory
lookups, actor AV getters and absent sound records are explicit boundaries.
Separate exact and adjacent-threshold cases cover base Block24/25/49/50 and
all three x87 words. Condition100.125 minus blocking wear7.5*1.5 is88.875
(bits1118945280); the gated extra wear produces55.125 (1113358336).

The owned engine matrix now uses independent NPC bases and distinguishes idle
attacker, blocking Novice attacker, and blocking Journeyman attacker across its
existing unarmed/weapon, shield/weapon and defender mastery profiles. It retains
prepare immutability, stale condition rejection, atomic condition/resource/RNG
publication, consumed-action rejection and binary restore. Initial full checks
fail only this matrix because its original two NPCs shared base800: victim SetAV
also changed the attacker's authoritative base. Those attempts are retained;
the fixture correction gives the attacker base812 without changing the gate.

Final full engine checks pass1716 cases/124 suites normally and under
ASan/UBSan, without failures/skips; leak checks are disabled. Both use parent
dd1728f39d and dirty source fingerprint
`e64d69111875315c29b1d28b58b4b71863a42825e0ac756d9e3c653a2a22ec9b`.
The unchanged component suites are not rerun for this engine-only change.

The actual normal/instrumented course stages stock IronShield0352C1 at
condition1 on Valen Dreth with a declared shared-base Block50 override and
matching actor channel; public NPC Lua disables AI and holds block while the
left slot exists. Player equips the prior sword and performs one keyboard
ordinary strike. No outcome/contact is injected. Raw master ARMO audit freezes
health225, armor1200 hundredths, shield mask2000 and heavy flag80, with record
and master hashes. Shield condition stays1 and equipped, NPC sword stays55.875,
Player sword becomes55.57500076293945, and original selection consumes four draws
505908858->773150046. All nine full actor-value arrays and full prior inventory
multisets are checked, with only the declared Player sword and one nonlethal
NPC Health decrease. The damage magnitude is observed, not independently frozen
by this particular course. Fatigue regenerates before saving; action-next3,
empty pending/owner lists, no loose body and unchanged dynamic serial1 persist.

Exact first-save bytes enter a distinct process in both builds. NPC Lua
re-establishes its held block after loading, but no attack/contact/selection or
impact sound replays. The earlier normal-01 continuation wrongly inherited a
no-block-start rule from the dropped-weapon course; both games exited0 and saved,
but the scenario correctly rejected that contradictory expectation. Its
manifests/results remain unchanged. Repeated normal-02 and sanitized-01 courses
require the intended held posture while retaining every contact anti-replay gate.
Three successful shield/Flesh/Blade sound requests and mixed output are retained;
no quantitative PCM identification is claimed.

Visual review is explicitly incomplete. Normal first/fresh captures and the
instrumented first capture show the named NPC in a sword-block pose, with no
visible Iron Shield on its left forearm. The instrumented continuation capture
shows only the foreground sword/HUD/target label, without scene geometry. These
are not accepted shield-rendering or instrumented visual-restart results.
Renderer shield binding, correct defensive presentation and cold-frame readiness
remain work for full S4. Mechanical checks are recorded separately.

Editable original probes, retained attempts, exact check receipts and runtime
recipes are in`M15-SHIELD-SELECTED-WEAR-PROBE.json`; canonical first/continuation/
acceptance manifests are committed source. This is first-person keyboard1FPS,
authored NPC block control and completed-contact restarts, not the complete
input/view/FPS or in-action restart matrix.

Full shield break/drop publication remains open, together with initially broken
equipment, mastery procs, effects/reactions and the remaining input/view/FPS and
in-action restart matrix. These observations do not implement the shield's
physical drop or establish live original-game behavior. **Full M15S4 remains
incomplete.**

### Authored shield attachments and native shield BlockIdle

The selected-shield course exposed an actual renderer gap: a worn shield had
no visible forearm attachment, and the controller chose the sword's blocking
animation. Native shield presentation now follows two independently verified
rules. Rigid shield models use their exact `Prn` parent, with the parsed NIF's
content identity checked against the actual scene template. Named-parent lookup
is case-sensitive and searches self before children. The model is cloned and
attached with its complete authored local transform; TES3 `BoneOffset`
extraction and Left-parent mirroring do not apply. Invalid/missing parent data
or a mismatched template rejects attachment with a diagnostic, without borrowing
the actor root. Player shields are included in first-person body parts, and the
legacy carried-part route no longer creates another armor model at Torch.

Original479140, actual6FF9C0/452A60 extra lookup and36 cases prove exact `Prn`
lookup, optional-object precedence, wrong-type rejection without fallback and
resolved-parent attachment requests. Actual NiNode70AC00/7073F0 executes five
case-sensitive self/child/null/duplicate-name observations without boundary
stubs. The whole parent-selection probe retains explicit RTTI, OS, node resolver,
AttachChild and final-update sinks; it is not a native live-game run.

Full5E5690 with actual51A9B0 encoding executes280 profiles. The actual worn-entry
getter64B2D0/484E80/41DF10 makes groups27/28 use prefix0 whenever a worn shield
exists, independently of the right weapon family and force-hand flag. Adjacent
26/29 and attack32 retain their family. Production held shield blocks now use
unprefixed`blockidle`; ordinary attack groups are unchanged. The BlockHit family
is independently observed here but hit-reaction playback is still separate work.

A raw master ARMO/BMDT/model inventory finds206 shield records and34 unique
models across ground/male/female roles. Existing generic NIF parsing confirms
all34 are unskinned Oblivion-format models with one authored `Prn`, naming a node
present in both stock male and first-person skeletons. Twenty-nine models are
in the default runtime mesh archive; five are statically checked from the
installed Shivering Isles mesh archive, which the six-archive course does not
register. That distinction is not a claim of expansion runtime acceptance.
IronShield root0 is named Shield, its `Prn` extra is record2 naming
`Bip01 L ForearmTwist`; the skeletons themselves have no Shield bone.
Master/archive/model hashes and editable probes are committed as metadata;
extracted assets and original disassembly stay ignored.

Four owned renderer cases cover exact/optional parents, malformed data and
source identity, native C-string boundaries, one owned model clone, a nontrivial
parent/local transform, hidden/case-colliding parent names and PartHolder removal.
The additional Left/BoneOffset regression fails against the preceding engine
library: generic SceneUtil attachment inserts a mirror/offset wrapper. It passes
with direct native clone/attach. Initial engine01 compilation failures omit the
test's animation.hpp declaration; those attempts remain intact. Intermediate02
checks pass1719/125 before the transform regression and are superseded.
Final normal/ASan/UBSan engine03 checks pass **1720 cases /125 suites**, without
failures/skips. Leak checks are disabled. C++/CMake source hashes and the same
tested dirty fingerprint are recorded in the probe. Components are unchanged
and their suites are not rerun for this engine-only change.

Final normal03 and ASan/UBSan01 runtime courses each complete two distinct
process epochs with terminal launcher exit0. The actual first output is the
second input, with save hashes, PIDs/start ticks and executable identities
recorded. Normal binary SHA256 is
`06a5eeef02308f7091e41a8228dcfc92d86e3ab2315ddecbe2f6c6e8142c9e1f`;
instrumented binary SHA256 is
`bedd94d667d78219af1ba4579196c3fbb324d134aac706fab65f4f8a98d0b02f`.
Both use the tested dirty fingerprint
`2c0a09abf6d5df17ca8728d5501f731a2449068616cb41a5ee6d1e5eef2e7e23`
on parent `ef97e9b346`. The sanitizer manifests retain ASan-first preload,
`detect_leaks=0:halt_on_error=1` and UBSan halt/stacktrace options.

An editable stage preparer adds only a copy of the existing stock NPC shield to
the Player inventory. Its output reproduces the tested staged save byte for
byte. Ordinary keyboard input draws the sword, makes one contact against the
NPC's public-Lua held block, then holds/releases Player block in first and third
person, returns to first person and saves. The view toggle is R/channel30;
ordered assemblies and two distinct block/release pairs are checked. All nine
actor-value arrays and full inventory multisets are checked. NPC health damage
is observed as bits1053978584 within the preregistered nonlethal bound, then
preserved exactly; this course does not freeze an independent health magnitude.
Only Player sword condition changes to bits1113476301; NPC sword stays55.875,
and both shields stay condition1/equipped. Combat RNG505908858 advances by the
four independently observed selection draws to773150046, with no fresh-process
replay. Next action3, empty pending/owner state and unchanged dynamic serial1
are verified. Three successful sound requests and mixer output are retained;
quantitative PCM identification is outside this course.

All eight final captures are directly reviewed: Player first-person shield,
actual third-person raised shield without a duplicate Torch model, released
first-person Player sword with the NPC shield, and the fresh-process dungeon
with NPC shield. The instrumented continuation focuses before waiting for the
NPC block and allows90 seconds for cold scene readiness; its final capture
contains the world. Readiness is distinguished from saved mechanics acceptance.
Prior normal01 opened inventory with Tab and fails visual acceptance. Corrected
normal02 first epoch completes, but its orchestrator is deliberately stopped
before continuation after discovering the generic attachment transform issue.
Both attempts remain diagnostic evidence; final acceptance uses normal03 and
sanitized01 with the direct clone implementation.

`M15-SHIELD-PRESENTATION-PROBE.json` records the editable instruction/content
probes, baseline failure, final checks, actual process epochs and runtime
verification. Canonical first/continuation/acceptance JSON files are source.
This closes the previously rejected stock shield attachment and standing
BlockIdle presentation in the bounded first/third-person keyboard1FPS course.
It does not implement shield break/drop, ordinary inventory equipping, BlockHit
or other hit reactions, directional/mastery procs, effects, gamepad/FPS or the
remaining in-action restart/combined-course matrix. **Full M15S4 is incomplete.**

### Native shield break/drop anchor follow-up

`M15-SHIELD-DROP-ANCHOR-PROBE.json` records96 full6541A0 cache-refresh
profiles and1152 full Player65FF10/NPC5F3870 wear/break profiles, all against
the pinned original executable. Both independent instruction probes pass.
The cache writer requests exact `Bip01 L ForearmTwist` from the raw skeleton
root. NPC process+104 stores that resolved node; actual64B1B0 selects it when
Character4349A0 supplies no node-index, or reads index7 through actual477EC0.
The full NPC break path distinguishes supplied cached/index nodes by their
world translations and passes the chosen transform to the captured drop call.
The stock shield model root named Shield is not this anchor.

Two cache harness attempts initially supplied an incorrect node-index layout;
they fail assertions and remain retained. Corrected03 uses the actual
index+8+slot*8 layout. Named scene resolution and mutation/drop constructors
remain declared boundaries; identity node rotations are used in this harness.
This adds independent evidence for the upcoming production transaction, without
claiming ARMO drop, live original-game or full M15S4 acceptance.

### Blocking shield break retention

Native blocking wear can now prepare a retained shield reaching zero: Player
or NPC equipment with flag `0x400` stays in inventory and its exact carried-left slot is
unequipped in the same contact publication as resources, condition and posture
withdrawal. This uses the existing prepared unequip and deferred notification
path, preserving stale-state rejection, exact-once action consumption and
observer exception behavior. The right weapon remains equipped.

Later armor selection reads the projected inventory after this early shield
break; it cannot select the removed shield. The contact uses general
Flesh/weapon-Flesh sounds when the early blocking shield has disappeared.
It keeps mitigation already calculated for the accepted block. Native sound
ordering is explicit: a shield surviving blocking wear can request its shield
sound before the later selected-armor wear. Later selected-wear shield breaks
remain rejected pending their distinct posture/equipment audit. Ordinary NPC
ARMO drops remain rejected until actual reference/render/physics admission is
implemented. They do not silently become retained items.

Original `0x5F5C69..0x5F5CD6` executes 1,152 blocking-wear profiles with the actual
Player/NPC condition virtual. It requests `0x5F4AE0(false)` when the condition
mutation reports broken. The prior withdrawal probe independently identifies
its process-action effects. Another 48 Player profiles execute full wear,
contiguous contact/armor selection and the sound dispatcher prefix: early
retained removal makes all seven armor draws fail, seed 1 becomes 752224798
and runtime seed 505908858 becomes 2832368235; requested sound EDIDs are
PHYDamageFlesh and WPNHitBladeFlesh. Readers, mutation/drop sinks, scene lookup
and sound lookup remain declared boundaries, not live original-game acceptance.

Another 48 original Player profiles start condition 20 and survive blocking wear
at 8.75, then can reach zero from later selected armor wear. Their sound request
retains the early shield layer, confirming its ordering independently of the
final equipment state. Full selected-wear unequip/posture side effects remain
outside that boundary-sink probe.

Final normal and ASan/UBSan engine02 checks each pass **1,722 tests / 125
suites**, without failures or skips. Both use dirty source fingerprint
`31558d3cdcafabf4d685a2629c6f0f1ef2d0abbb7bd0bc574aee92a1e622abe9` on parent `9a503caf0d`. All three recorded C++ hashes remain
unchanged after those checks. Components are unchanged and are not rerun.
Initial engine01 checks also pass 1,722/125, but final02 supersedes them after
the sound-ordering guard, explicit unsupported NPC-drop rejection, original
seven-draw seed assertion and test cleanup. Leak checks remain disabled;
ASan/UBSan halt-on-error and ASan-first runtime preload are explicit.

The new owned cases exercise 16 retained-NPC profiles across fist/sword,
drawn/undrawn, base Block 5/25 and throwing/nonthrowing deferred observers, plus two
Player normal/god-mode profiles. They verify exact resources and condition,
left-slot removal, retained count/charge/scale, projected seven-draw RNG,
posture, stale-state rejection, deferred observer ordering, replay rejection
and authority restore. Ordinary NPC nonretained shield breaks remain rejected
without changes. Explicit god-mode break requests suppress equipment and
posture effects together with wear.

The normal course uses executable SHA256 `66dc9d9f89528af28721471bd094210157cc61426fa02840cd04d9fae26988e2` in two distinct recorded process epochs (PIDs 519774 and 519869).
The sanitized course uses executable SHA256 `c10f13d186e2ed5b60b8158071df45e9469842966d4335f8d8a32b59769e4534` in two distinct recorded process epochs (PIDs 522585 and 522768).
Both actual first saves enter their fresh second process byte for byte. All
launcher epochs exit 0 and are terminal. Full nine actor-value arrays, full
inventory multisets and native bases are checked. Player sword 55.875 stays
equipped; shield condition 0/count 1 stays in inventory unequipped. NPC sword
condition bits 1113509069, next action 3, empty pending/owner state, dynamic
serial 1 and no loose world item are verified. Combat seed 505908858 advances by
seven independently observed selection draws to 2832368235, without replay.

The original course preregisters Player health damage inside (0,2). A separate
native calculation is derived after the normal first contact, before decoded
output-save inspection and before sanitizer launch. Full original `0x488CB0` stock
IronShield (armor 1200 hundredths, health 225, condition float 0.0001, Heavy Armor 15, Luck 50) yields armor
rating 1. Original mitigation, shield Block 5/full Fatigue and Health stores
produce damage bits 1064502899; both runtime courses match those bits exactly
and preserve them through continuation. A further original path uses the exact
absorbed-damage wear store and full Player `0x65FF10` mutation, observing condition 0
and the retained-unequip request. Declared max/current-value/condition readers
and mutation sinks remain boundaries. Initial native damage-probe attempts
incorrectly reuse the emulated stop address as a float-store instruction;
retained diagnostics show the native local is 1 while the stale store reads 0.
Final05 uses an immutable separate store and verifies its terminal EIP.

All six final captures are directly reviewed: a raised Player shield before
contact, its absence with sword still drawn afterward, and the same visible
world/weapon state in the fresh process, for both builds. The initial verifier
incorrectly forbids every shield attachment in the fresh world; an unrelated
original Blades guard legitimately carries one. The corrected check requires
Player attachment before contact, no Player reattachment after contact/fresh,
and all prior mechanics/epoch assertions. Failed verifier/log evidence is
retained; completed normal scenarios are not replayed for that assertion fix.
Two successful Flesh/BladeFlesh engine requests and actual mixer output are
retained; quantitative PCM identification is outside this course.


Full M15S4 remains incomplete. This chunk does not close NPC shield world
drops, later selected-wear shield breaks, initially broken equipped gear,
BlockHit/reaction playback, mastery/directional procs/effects or the remaining
input/view/FPS/in-action restart and combined acceptance matrix.

### Typed shield world-admission foundation

The existing loose-weapon transaction now accepts typed native shield references
alongside weapons. It still reserves one registry group, admits all scene nodes
before bodies, validates every participant, and publishes the contact, exact
inventory debits, stable keys and cell nodes in one final interval without
callbacks. Private typed insertion nodes outlive their borrowed rendering and
physics preparations. Mixed weapon/shield drops share this pipeline and its
rollback path rather than publishing one item at a time.

The equipped-removal primitive accepts an explicit carried-left shield slot;
its default remains the carried-right weapon. It requires one exact projected
instance, rejects aliases and changed count/scale/kind/owner, preserves final
wear and unrelated inventory, clears the appropriate selection and notifies
observers separately. This is the source debit required by an NPC drop, distinct
from retaining a broken shield unequipped.

Normal and ASan/UBSan engine `native-shield-admission-{normal,sanitized}-04`
each pass the full unfiltered 1,724 tests / 125 suites with no failures or skips.
Both identify parent `08a6d2247ea5a35daf5194257ef17482a95db3c2` and tested dirty
source fingerprint `3e1f9de5f0cbfae43c44c5af13bf27a81cb5e7019d8f164fc8dd59a559c19c32`.
Sanitizers halt on errors; leak detection is disabled. The five source hashes
are pinned in the probe manifest; only evidence documents change after checks.
Existing build warnings remain recorded, including the unrelated animation
queue initialization warning; this is not a warning-free-build claim.

Earlier normal attempts retain an incomplete-type compile error, a duplicated
same-line `SCOPED_TRACE` compile error, and a full run with two failed tests.
That last run exposed a real borrowing regression: a copied request variant
hid caller changes made after request construction. Reference-wrapper variants
restore the original live-input contract for both types. The new malformed-kind
fixture also attempted forbidden static-store replacement; it now changes and
restores only its owned instance definition. Each corresponding unfinished
sanitizer build was stopped after the normal failure established a required
source correction, with owned-tree cancellation and terminal receipts retained.
Those interrupted runs provide no sanitizer acceptance.

Owned compound tests exercise seven single-shield outcomes and weapon/weapon
plus weapon/shield groups in the same managed cell. They check actual typed
references, registry/cell counts, rendered nodes, physics owners, resource and
condition deltas, serial allocation, exact source quantities and rollback after
scene/source/actor/god-mode invalidation. The new primitive case checks invalid
slots, stale counts/scales, malformed projected kind, inventory replacement and
preservation of the right-hand weapon. These owned scene/body inputs prove the
publication mechanism; they are not normal-input stock NPC drop acceptance.

The raw installed-master inventory contains 206 shield records. World MOD2/MOD4
and effective carried MODL/MOD3 paths coincide for all of them. The
factory will still distinguish the record roles and source identities. Existing
generic NIF/NIFBullet readers accept nonempty carried and normalized reference
body definitions for all 34 unique model files, with identified reference roots
and matching 16-byte source hashes. Twenty-nine models are in the default mesh
archive set; five expansion models are audited statically only. Some assets
require the loader's synthetic reference root to preserve their authored root.
This is content/parser capability evidence, not an independent original-game
physics oracle or expansion runtime acceptance.

Forty original-instruction selector cases execute actual ARMO RTTI, the biped
model selector and TESModel getters with distinct carried/world paths. They
confirm male MOD2 by default, female MOD4 for a resolved female NPC owner, and
female-to-male world fallback when MOD4 is empty. Owner
resolution and NPC sex are declared boundaries in this selector probe; owner
inheritance has its separate native audit. The factory must select by resolved
reference owner, rather than assuming the dropper's sex determines the model. The first harness lacked the
exception-registration page used by MSVC RTTI; its retained failure is fixed
without replacing RTTI. A private draft had mislabeled model roles; the
corrected raw-field inventory is published here.

`M15-SHIELD-ADMISSION-PROBE.json` records source/check receipts, editable content
probes and retained failures. No runtime-state wire format changes. Actual NPC
shield scene binding, world-model factory/ownership, physical save/load and
normal-input break/drop/pickup/restart still require integration. The ordinary
NPC shield-drop policy guard remains until that pipeline is complete. Full
M15S4 remains incomplete, including the other mastery/effect/reaction and
input/view/FPS/in-action restart/combined gates.

### Exact carried-shield scene and forearm capture

`ESM4NpcAnimation` now binds a rendered shield to the exact live projected
carried-left inventory instance. Native equipment entries retain their ARMO
identity while assembling parts. Attachment uses the same NIF/template
identity check as the established rigid Prn path and records its 16-byte source
hash. The body-part holder/count remains shared with the existing actor
assembly; weapon sheathing has its independent lifetime.

Read-only capture returns both the shield's authored attachment scene and the
native `Bip01 L ForearmTwist` drop anchor. Exact native-string skeleton search
is independent of the folded animation map and of model Prn or Weapon. The
validator checks item/slot/count/kind, winning native model, source identity,
scene nodes, placement and both transforms. Capture selects the original NIF
root record inside the loader wrapper and requires a live weak inventory storage
identity; pointer equality alone cannot detect replacement with address reuse. Equipment refresh retires the
binding before detaching parts. Inventory replacement and animation destruction
retire old captures, while showWeapons/sheathing preserves the shield binding.
No new source debit, contact or world-reference publication occurs here.

A supplementary sixteen-case original `469330` carried-model selector probe
executes both sex inputs, both empty-role cases and cached/sentinel string
length handling with actual TESModel getters. It confirms female-to-male
fallback only: an empty male MODL remains empty even when female MOD3 exists.
The pre-existing generic renderer also falls back male-to-female and to the
compatibility mModel field. This scene-binding change preserves that policy;
the newly verified native fallback discrepancy remains explicit follow-up.

Normal and ASan/UBSan engine `native-shield-binding-{normal,sanitized}-04`
each pass the full unfiltered 1,726 tests / 125 suites, with no failures or skips.
Both identify parent `f87e3df4b0729942236e5c3babb9039356b9f8b8` and tested dirty
source fingerprint `ffec5721879a2f53a3380c917b8e58a8d208764caf1fafc99229f215f85c4e05`.
The six source hashes are pinned in the probe manifest; only evidence documents
change after checks. Sanitizers halt on errors, with leak detection disabled.
Existing unrelated build warnings remain recorded; this is not a warning-free
build claim.

The owned actor fixture serializes a real TES4 NIF with a Custom Socket Prn,
sets its carried model to a different path from the projected/world model, and
includes native forearm, folded-case and Weapon skeleton nodes. It observes
attachment world translation (111,22,33) and independent native forearm
translation (103,20,30). Hash/count/placement/anchor-name changes, equipment
refresh, copied inventory replacement and destruction invalidate captures;
NUL-suffixed bone names use original C-string comparison. The same fixture
checks preservation across weapon visibility changes. The extended NIF builder
preserves every existing weapon fixture when its optional Prn argument is empty.
An isolated build of that exact builder also passes the existing generic
NIF/Bullet parser with nonempty carried/reference definitions. These are owned
scene/parser checks, not original physics or stock NPC drop acceptance.

Two pre-test build pairs are retained as deliberate cancellations after source
review: first for exact native NUL-terminated anchor-name validation, then for
a pointer into the temporary value returned by CellRef.getRefId(). The latter
now retains a local RefId through its variant lookup. Each owned build tree and
helper is confirmed terminal; interrupted attempts provide no test acceptance.
The full normal03 run additionally fails the new binding case: it captures the
loader wrapper rather than the original NIF root, and accepts copied inventory
replacement when its allocator reuses the source item address. The source-root
record lookup and weak storage token fix those production defects; the original
attachment expectation (111,22,33) is retained. The corresponding sanitizer build
is stopped after that failure establishes the required correction. A standalone
ResourceSystem/VFS run of the exact generated NIF plus production attachment/root
helper now verifies original record0 at expected socket-plus-root (11,2,3),
distinct from its wrapper. Missing/duplicate source-root mappings also have
explicit engine coverage.
`M15-SHIELD-SCENE-BINDING-PROBE.json` records the source/check receipts and the
existing original-instruction anchor/presentation/model-selection provenance.

The original owner NPC sex getter `519D20` additionally passes 3,072 direct
flag/type cases with no game-function hooks: NPC type23 reads base+28 lowbit;
other form types return -1. The forty world-model selector cases are rerun with
that actual getter, leaving owner resolution as their declared boundary. The
future factory must guard the resolved owner's native base flag, rather than
querying the dropper's rendered actor sex.

The ordinary NPC shield-drop guard remains. The production factory must still
select the world model by resolved reference owner, capture the native drop
placement, publish the compound inventory/contact/serial/world transition and
integrate loose ARMO physical save/load. Normal-input stock NPC break/drop,
pickup and fresh-process courses remain required, as do the other full M15S4
mastery/effect/reaction/input/view/FPS/in-action restart and combined gates.


### Native NPC blocking-shield drop and physical restart

This checkpoint supersedes the ordinary NPC shield-drop guard described above.
An ordinary blocking-shield break now publishes its condition, carried-left
removal, posture withdrawal, contact resources, serial, dropped-reference
relations and world render/physics admission through the existing compound
transaction. The right-hand weapon remains equipped. The factory accepts typed
WEAP4/ARMO4 groups, validates the exact carried shield and independent native
forearm placement, and loads the dropped body from the selected **world** model.
Source and world NIF hashes can differ. Late owner/model/source/scene changes
reject the whole publication. Loose-shield save, validation, restoration and
synchronization use the existing generic schema53 payload.

Native biped model selection now uses female-to-male fallback only, shared by
carried rendering and ARMO/CLOT world queries. World sex comes from the resolved
reference owner's raw NPC flag: direct owner, teleport destination's direct
owner, then current CELL owner. It does not initialize an owner actor or use the
dropper's rendered sex. Snapshot validation resolves pending saved ownership
before restoring scenes. An additional forty original-instruction CLOT selector
cases verify its shared native virtual dispatch; earlier ARMO/owner-getter
provenance remains in the shield binding probe.

Normal `native-shield-factory-normal-03` and ASan/UBSan
`native-shield-factory-sanitized-04` each pass the unfiltered **1,731 engine tests /
125 suites and 2,937 component tests / 276 suites**, with exact inventories and no
failures/skips. They share parent `e4916dac28a569871257c1c030a04a53d02ad703`
and tested dirty-source fingerprint
`aca7015e5bb8c8f365802e342c5c2efb8cdd36b6ceab92ee19d6a505e742f243`.
Fifteen implementation/build/test file hashes are pinned separately. Sanitizers
halt on errors; leak detection is disabled. Existing unrelated warnings remain.
The retained normal02 failure revealed a shared virtual skeleton filename in
fixtures; shield fixtures now have a distinct name. Sanitized03 identifies an
older source fingerprint and binary containing only the old skeleton name. Its
five failures and GDB diagnostic are preserved; it is not accepted current-source
evidence. The fresh sanitizer04 rebuild contains the corrected fixture.

The editable `oblivion_m15_native_shield_drop*.json` scenarios and
`m15_native_shield_drop_acceptance.json` describe the runtime course. A normal
Player unarmed strike hits Valen Dreth's near-broken Iron Shield while he holds
public block intent. NPC Block25, Heavy5, shield condition float0.0001 and intact
right sword55.875 are frozen before contact. Original executable instructions
independently produce Health damage bits1065809137, contact Fatigue
bits1067615414, shield condition0 and one drop/notification. Both x87 precision
profiles agree. These probes declare AV/settings, resource and world boundaries;
they do not establish original-game physics or gameplay acceptance.

Normal `native-shield-npc-break-runtime-normal-01` and instrumented
`native-shield-npc-break-runtime-sanitized-02` each pass actual hit/save/quit and
fresh-process load/resave courses. All nine saved raw AV arrays match the frozen
expectation, including exact Health damage; Fatigue has regenerated before save,
so contact Fatigue is observed only at the existing six-digit log precision.
Each output has one broken shield/reference/body, preserved right sword, serial2,
action-next3, RNG505908858 and no pending strike or replay on restart. The body
settles within preregistered bounds. Both requested Flesh/HitHand sounds report
played, and mixer PCM is retained; this checkpoint does not claim stock PCM
correlation or isolated listening acceptance. All six captures were directly
reviewed. The normal fresh capture is third person and the instrumented one first
person; uninterrupted camera, timing and trajectory equivalence is not claimed.

An additional fresh normal Space activation collects the shield exactly once
with condition0, deletes the world reference and removes its model/body, while
preserving raw values, right-sword inventory, actions, RNG and serial. Only player
position/aim is staged in a copy of the second output. Both pickup captures were
directly reviewed. The first aiming attempt toggles to an obstructed third-person
view and fails; the retained corrected attempt omits that toggle. Collection
exposes a remaining stale NPC dropped-list entry. A separate160-case original
reference-finalization/removal probe grounds the helper behavior, but the full
pickup-caller audit and atomic cleanup integration remain open. Collection is
therefore reported separately from full native pickup acceptance. The first
instrumented launch additionally fails before gameplay because ASan follows the
SDL preload; the corrected new course places the actual ASan ELF first.

`M15-SHIELD-DROP-VERIFICATION.json` records receipts, source hashes, independent
expectations, runtime epochs and these limits. Later selected-shield wear-to-zero,
initially broken equipment, mastery/effect/reaction and interruption branches,
the input/view/FPS and in-action restart matrices, and combined S4 acceptance
remain open. **M15S4 is still incomplete.**

## Native pickup dropper-list cleanup checkpoint

The pickup transaction now removes the collected reference from its dropper's
list before observers run, alongside inventory, source and scene publication.
Loaded actor extras override an older runtime cache; cached actors can be updated
without constructing their cells or inventories. The actual compatibility Player
uses its accepted runtime identity despite having no native CellRef key. The
collected reference retains its separate dropper metadata. Empty list extras are
detached, absent extras stay absent, and the first matching entry is removed
without changing the remaining order.

Independent original `4D7D80` finalization, `4203C0` dropper lookup and
`424C00`/`65C620` removal instructions pass **960 cases** against executable
SHA-256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Reference virtual dispatch and actor caller slots were audited. Extra lookup,
allocation/free, changed-reference registration, modification notification and
Set3D are declared boundaries; this does not prove the original whole pickup,
rendering or physics path.

Normal `native-pickup-cleanup-normal-02` and ASan/UBSan
`native-pickup-cleanup-sanitized-02` each pass **1,735 engine tests /125 suites**
and **2,938 component tests /276 suites**, exact inventories, no failures/skips.
They test dirty-source fingerprint
`9bf9acf9c38ef82089dedc10c988eda46dda0dc05560e46e8d70bc2c8c105bd8`
on parent `77edba887cdcdf0a0e76406522bbe82a3eb469de`.
Nine literal helper cases and four new World tests cover extra presence/order,
resident but unregistered actors, stale caches, callback reentry/failure,
preparation failure/retry, cached-only fresh World roundtrip and absent authored
actors/Player. The first normal attempt's three fixture failures, the separately
found Player identity defect and canceled earlier sanitizer build are retained;
only the corrected full02 runs are accepted.

Both `native-pickup-cleanup-runtime-{normal,sanitized}-01` courses use the same
unchanged pre-pickup save from the prior accepted physical shield-drop course.
Actual Space collects one condition-zero shield; save/quit and a fresh process
loading that exact output then repeat activation. Both saves detach the NPC list,
retain the source dropper on the deleted reference, preserve the right sword and
contain no loose body. All **913 present float32 actor-value stores /nine actors**
match the input bit for bit, including signed zero; inventory, action-next3,
RNG505908858 and dynamic serial2 remain exact. Additional content references are
audited against the unchanged master. Three captures per profile are directly
reviewed for shield presence, collection and continued absence after reload.
These are completed-pickup restart checks, not an in-action restart matrix.

Normal runtime began after its own full checks while the unchanged sanitizer
source was still building; an immutable own-profile receipt records this order.
Final acceptance requires both full profiles and both runtime courses. Application
ELF hashes are pinned separately from the test binaries. ASan leak detection is
disabled; this is not leak coverage. The editable pickup/continuation manifests,
acceptance recipe and `M15-PICKUP-DROPPER-CLEANUP-VERIFICATION.json` record these
layers and limits. The old stale-list observation in
`M15-SHIELD-DROP-VERIFICATION.json` remains historical evidence. No migration of
already-collected stale-list saves is claimed. **M15S4 is still incomplete.**

## Later selected-shield break transaction checkpoint

Later selected armor wear now applies the native shield equipment consequence
when it reaches zero. It merges with an earlier blocking-wear change while
retaining the original pre-contact condition for validation. The shared prepared
transaction then retains/unequips Player or quest armor, or publishes one ordinary
NPC dropped shield and body, with posture withdrawal and resource/RNG publication.
Body armor zero-condition policy is unchanged. Sound material captured before this
later wear is retained, unlike a break during the earlier blocking-wear stage.

Original-instruction probes cover 3,456 contexts, including 432 later breaks
(324 retained unequips and 108 NPC drops/notifications), plus 12 exact condition40
regression contexts. First wear leaves28.75; later wear reaches0 only under the
native attacker action6/baseBlock-below50 gate. The executable hash, full executed
paths, original caller/material reads and inventory/scene/audio boundaries are
recorded separately. The first probe using condition.4 broke at the initial wear
and exercised zero later breaks; its apparent generic pass is not accepted for
this branch. The corrected probe requires initial survival and later-break cases.

World tests cover Player/quest retention, mastery/posture controls, Player god
mode, original expected-condition preservation, single and merged wear changes,
ordinary NPC body creation, scene-failure rollback/retry and no duplicate commit.
The first normal attempt exposed two fixture errors: a missing actor reference
in a serialized snapshot and a factory attack weapon retaining its enchantment.
Those failures are retained with corrected-input provenance.

This checkpoint is not normal-input E2E acceptance. Current held-block handling
returns before attacking; beginBlocking rejects a pending melee strike, while
melee playback accepts actions-1/2/3. The actual native later-wear gate queries
the attacker action6, so controller reachability remains unproved. No fabricated
contact or exercise-time state mutation substitutes for that missing evidence.
The full S4 controller/input/mastery/reaction/effect and restart gates remain
open. **M15S4 is still incomplete.**

Normal `native-selected-shield-normal-02` and ASan/UBSan
`native-selected-shield-sanitized-01` each pass **1,737 engine tests /125 suites**,
exact inventories with no failures/skips. They share dirty-source fingerprint
`2f473dd5c67e2b98b1c286473bf0911c64607368eaf3d318dc66077a92c506c4`
on parent `0c58180c2ea922eab0bf0a0fe523dd501d63ec2e`. Only engine source and its
World tests changed; the prior 2,938 component results remain historical evidence
for the unchanged components. The sanitizer run disables leak detection and is
not leak coverage. The test fixture's zero/enum conditional emits a compiler
warning; this checkpoint does not claim a warning-free build.
`M15-SELECTED-SHIELD-BREAK-VERIFICATION.json` and the editable
`m15_selected_shield_break_contract.json` record the source pins, original case
receipts, test inventories, retained failures and the open input gate.

## Initially broken blocking equipment checkpoint

A worn condition-zero item no longer rejects the whole contact. Native blocking
can still absorb damage; the ordinary caller's base Block thresholds decide
Fatigue spending and item wear. Positive wear below Journeyman reports a break
and uses the existing atomic condition/equipment/body/posture transaction.
Zero wear leaves the worn item in place, including the tested base Block50
controls. Active shields and weapon-out active weapons retain their distinct
native removal gates. Undrawn broken-weapon positive-wear blocking remains
rejected in the current admitted contact domain; the controller requires a drawn
stance to maintain blocking.

Independent original instructions pass **640 shield-wear cases**, **192 block
caller cases** and **128 weapon-wear cases**, with the actual armor/weapon class
and actor/process dispatch distinctions. The original zero-shield runtime and
resource-sink profiles additionally agree in both x87 precision modes. A zero
Iron Shield contributes armor rating0, whereas the prior near-broken fixture
contributed1. The resulting native Health damage bits are1065898475 and contact
Fatigue bits1067722998 for this Player-hand/NPC-Block25 profile. The unaltered
weapon-block arithmetic profile expects Health1067387453/Fatigue1069516063.
Reader/settings/AV/current-health, condition-writer, equipment/scene and resource
sink boundaries remain declared; these probes are not original-game gameplay
or whole-scene emulation. Weapon-node fixture failures and the intermediate
probe-edit syntax failure remain in their original private evidence directories.

Normal `native-initially-broken-normal-01` and ASan/UBSan
`native-initially-broken-sanitized-01` each pass **1,737 engine tests /125 suites**,
exact inventories, no failures/skips. The tested dirty-source fingerprint is
`8e6d0d804b78b38458add884eebc9313ad43f56fb094f6151c6cf082f3dfa642`
on parent `5c9e28ba5886884a206e43450f04d7aabb2eb8a3`. Five existing World tests now
exercise48 initial/near-broken, retention, god-mode, observer and scene-failure
combinations plus two zero-wear mastery controls. The test inventory count is
unchanged. Components were unchanged and their earlier results remain historical;
the sanitizer run disables leak detection. No warning-free build is claimed.

The normal and sanitizer `native-initially-broken-shield-runtime-*-01` courses
and normal `native-initially-broken-weapon-runtime-normal-01` each complete a
normal Player strike, save/quit, exact-output fresh-process load and resave.
All nine actor arrays match the preregistered expected float32 bits, including
signed zero, except the independently expected Health delta. Fatigue regenerates
before save; contact Fatigue is checked only at the existing six-digit log
precision. Each course retains one dropped reference/list entry/body, action-next3,
RNG505908858, serial2 and saved posture-1 with no repeated contact, impact sound or
allocation. The shield course retains the right sword55.875 and original clothing;
the weapon course retains the clothing and switches to the drawn unarmed stance.
Additional content references are independently audited against the unchanged
master, including persistent exterior storage mapping. Three captures per course
were directly reviewed. The final forward-facing captures put the dropped body
below the frame and alone do not prove its floor placement.

A separate camera/position-only copy of the completed normal shield output then
shows the persisted shield at the NPC's feet with its Iron Shield focus label.
The original item/body/inventory/resources were unchanged during setup. The
additional actual load/save/quit preserves all actor bits, one body and its
identity, with body displacement0.01124 within the preregistered4-unit bound and
no new contact/pickup/sound. This is direct floor-model inspection, not ordinary
mouse-look or a replacement for the exact-lineage restart course. Its frozen
verifier passed state checks but failed on a string-versus-Path hashing bug;
the preserved original verifier/pins and output were checked by a corrected
verifier with the same expectations. The pre-launch helper import-path failure
and an ad hoc inventory check that omitted original clothing are also retained.
Neither changed the preregistered outcomes or production code.

Normal runtime began after its own full checks while the source remained frozen
for the sanitizer build; immutable own-profile receipts preserve that order.
Final acceptance includes both full builds and all three completed courses.
`M15-INITIALLY-BROKEN-EQUIPMENT-VERIFICATION.json`, editable shield/weapon/floor-view
manifests and `m15_initially_broken_equipment_acceptance.json` retain receipts,
source/application hashes, oracle expectations and these limits. No quantitative
stock-PCM correlation or full autonomous defensive-AI acceptance is claimed.
Mastery/recoil/reaction/effect, complete interruption/input/view/FPS, in-action
restart and combined named-target/block/miss/wall/stagger gates remain open.
**M15S4 is still incomplete.**

## Reaction ownership and schema54 checkpoint

The combat service can now publish typed BlockHit, Recoil and Stagger ownership
in the same physical-contact transaction as resource, condition and RNG changes.
The reaction retains its already consumed causal contact; it allocates no new
strike. Participant strikes and queued input are canceled together, unrelated
actor state survives, and stale participant snapshots reject publication.
Equipment withdrawal and external action changes clear superseded ownership.
Victim incapacitation cannot restore the attacker's cleared targeting intent.
BlockAttack remains a distinct attack protocol.

Schema54 records the selected reaction group, renderer progress, native sequence
activation timing, Start progress and an optional positive retirement interval.
The interval can remain unanchored across a save before its first manager update.
C++ and Python reject replaying causes, mismatched actions, missing clocks and
conflicting strike/bow ownership. Legacy raw actions6/7/8 acquire no invented
reaction or timer. Fifteen actual C++ binary/JSON fixtures independently decode
and re-encode identically in Python in both build profiles, including signed zero.

Final normal `native-reaction-ownership-normal-02` passes **2,944 component,
1,741 engine /125 suites and327 Python tests**. ASan/UBSan
`native-reaction-ownership-sanitized-02` passes **797 selected ESM4* component
and1,741 engine /125 suites**. Inventories match exactly with no failures/skips.
Both pin dirty-source fingerprint
`81c59b15ef34b6a0ecf9b9a18d3934be2c1d95c54ce5d548721150f2a39b82ab`
on parent `58d33f47fda2f12dc1bce8683da87c7f9f3d4c6c`, unchanged throughout.
Leak detection is disabled. Existing warnings and one new GoogleTest macro brace
warning remain; this is not a warning-free build.

The earlier passing candidate, fingerprint
`027004f89616268973b17124ae8f7ddcf601805e79b77c75e6b5e7c1048d0b4f`,
also completed the normal legacy shield-break course and exact-output
fresh-process continuation. Its input schema53 became54 in both output saves;
all reaction entries remained null. The original expected damage, actor bits,
inventory/body identity, RNG and no-replay outcomes were retained. Three captures
were directly reviewed. This is prior-source codec regression evidence, not
final-candidate runtime or owned-reaction playback acceptance. It adds no floor
placement or quantitative stock-PCM claim.

`M15-REACTION-OWNERSHIP-VERIFICATION.json` records the source and evidence hashes,
checks, historical runtime lineage, retained fixture failures and limits. Normal
contact adapters do not yet create these reaction descriptors. Controller
binding, rendered activation/retirement, inactive-sequence ownership until the
following actor recovery, outgoing sequence layers, and in-action restart remain
unfinished. A restored non-null reaction currently prevents new strike/bow/block
admission but has no controller recovery. Existing ReactionOnly and Expert/Master
contact guards remain. **M15S4 is still incomplete.**


## Native sampled-sequence pose projection checkpoint

The renderer can now project ordered caller-owned sequence samples onto NIF
nodes using their raw per-bone priorities, activation weights and ease factors.
Winning-source metadata retains authored weight and ordered target/priority
bytes; imported node blend settings survive inactive controllers and ambiguous
bindings reject preparation. Private track clones preserve the resource cache.
Equal-topology updates replace frame values without rebuilding callbacks.
Sampling completes before publishing node channels, participates in animated
physical-target sampling, and releases on model/scene teardown. This projection
owns no event traversal, root motion, combat actions, RNG or lifecycle clock.

Independent original instructions execute the constructor, real slot insertion,
nonempty sequence application, virtual transform update, normalization and
composition for **21,440 observations /10,720 contexts**. Both x87 precision
words agree. Production matches every normalized weight, valid pose float32 bit,
channel mask and return status. The corpus includes unequal weights, signed
priority bytes, one to five tracks, fade/threshold branches, missing channels
and quaternion hemisphere changes. Synthetic constant transforms and preallocated
storage are supplied; Windows atomic refcount increment is a boundary. No game
function is stubbed. This proves sampled-pose blending, not KF interpolation,
native group/variant resolution, geometry or original-game gameplay. The
separate two-track threshold behavior and many-track priority suppression remain
as observed, including the two-track path's repeated write to the second weight.
Eighteen editable original cases are checked in the component suite.

Normal `native-sequence-blend-normal-03` passes **2,947 component and1,745 engine
/125 suites**; ASan/UBSan `native-sequence-blend-sanitized-03` passes **800 selected
ESM4* component /62 suites and1,745 engine /125 suites**. Exact inventories agree,
with no failures/skips. Both pin dirty-source fingerprint
`08a9941b447f06963e5981bccd509204bab97ea2304a067cca3691b64b85ed61`
on parent `6ae45f660cfa600269aec8117d65807d54e84bb7`, unchanged throughout.
Four actual renderer tests cover pose bits and caller clocks, callback reuse,
rejected replacement/winning overrides, physical target sampling, sample-failure
publication, copy isolation and release without replay. Leak detection is
disabled. Existing compiler warnings remain; no warning-free build is claimed.

The initial independent comparison caught a reversed reading of the many-track
priority branch. The first build attempts then exposed unresolved record pointers
in the synthetic NIF fixture, and the second engine attempts caught a double
coordinate used in a float32 bit assertion. Each failed attempt is retained;
corrected expectations came from original execution and fixture fixes did not
change the production parser or weaken the comparison. Static base-archive audits
also confirm distinct per-bone priorities and numbered weapon-recoil filenames;
they do not prove native variant selection, RNG or winning overrides.

`M15-NATIVE-SEQUENCE-BLEND-VERIFICATION.json` records source/build/oracle hashes,
comparisons, tests, static asset scope and retained failures. CharacterController
and normal contact adapters do not yet call the new projection. Owned inactive
state until the following actor recovery, outgoing/replaced sequence persistence,
retirement, group/variant resolution and normal-input in-action restart remain
open, along with the other S4 mastery/effect/control/view/FPS gates. No application
runtime or full owned-reaction acceptance is claimed. **M15S4 is still incomplete.**


## Native reaction retirement persistence and service-frame checkpoint

Schema55 now distinguishes an inactive owned reaction awaiting the following
actor-prefix recovery from ordered outgoing sequence layers that still fade.
Both codecs retain raw timing, optional retirement anchors and signed zero.
Legacy promotion creates no timer or layer. An anchored outgoing fade requires
initialized timing; an interruption before the first manager update remains
representable with an unanchored fade.

Contact preparation can retain an explicitly supplied replaced strike after
checking its source action, group, time and native timing. Prepared service
frames stage the actor clock, strict Start/inclusive End checks, manager timing,
retirement and outgoing expiry. Stale handles reject publication. Positive fades
anchor on the manager update and become inactive at their stored end; nonpositive
fades deactivate immediately. Action recovery is separate: BlockHit retains
posture6 only when the caller supplies actual BlockIdle presence. These
transitions consume no new action ID or RNG.

Final normal checks pass **2,948 component, 1,746 engine and 329 Python tests**.
ASan/UBSan checks pass **801 selected ESM4* component and 1,746 engine tests**.
Exact inventories agree with no failures/skips; leak detection is disabled and
existing compiler warnings remain. Normal and instrumented schema55 binary/JSON
fixtures round-trip exactly through Python. The world service test covers all
three reaction kinds, immediate/positive fade, freeze/stale preparation,
save/restore during retirement and after deactivation, following-prefix recovery,
continuing outgoing fade and unchanged action/RNG state. It uses synthetic world
fixtures and does not establish normal-input application acceptance.

`M15-REACTION-FRAME-VERIFICATION.json` pins final02 source fingerprints, binaries,
inventories, XML and codec comparison receipts. Earlier01 checks passed before
the final anchored-timing validation and are retained separately. A supplemental
original-instruction probe confirms only that low-level activation refuses an
already nonzero sequence state; cached resolution and caller replacement remain
open. Existing guards against replacing an owned reaction remain.

CharacterController and normal contact adapters do not yet consume these APIs
or generate the descriptors. Sequence projection, actual group/variant/RNG
selection, interruption handling and normal-input fresh-process reaction courses
remain open, along with the other S4 gates. **M15S4 is still incomplete.**


## Owned-reaction controller and visible continuation checkpoint

CharacterController now consumes the prepared reaction frames and projects
ordered outgoing/current sequence samples through the native blend renderer.
The current shared track traverses its own text keys; private outgoing samples
emit no events. Background layers follow their existing shared animation clocks,
so they sample the current frame after runAnimation rather than lagging one
frame. Incoming/outgoing-only updates advance the actor clock once; detached
layers beside a strike use its already advanced clock with duration0. BlockHit
keeps BlockIdle resident through a distinct shared priority. Recoil/Stagger
remove cached BlockIdle. Existing interruption paths release typed ownership
and projection; unowned legacy actions receive no invented timer.

A real cold third-person load exposed a missing shared event track: camera/model
restoration replaced the animation after the first bind, while the render ID
survived. Reaction and strike binding shortcuts now also check actual shared
track presence and rebind from durable time. The failed attempt is retained.
An instrumented continuation then showed why wall time is insufficient evidence:
its scenario exited successfully after45seconds, but independent inspection
found Stagger still active at1.826seconds, below End2.03333. The scenario now
awaits actual actor-prefix recovery before saving, retaining the same decoded
state assertions. No native duration or phase rule was changed to pass it.

Final03 engine checks pass **1,747 tests /125 suites in each profile** with exact
inventories and no failures/skips. Unchanged component test binaries still match
passing01 inventories: **2,949 full normal and802 selected ESM4* instrumented**.
Forty editable original ease cases and the borrowed-background-clock renderer
case pass. Current production ease matches all **320 original published samples**;
the rebuilt blend translation unit still matches **21,440 original observations**
with zero mismatches. ASan leak detection is disabled; existing warnings remain.

Normal third-person03 and instrumented third-person02 both save an active owned
Stagger with a progressing outgoing layer, quit, and load the exact first output
in a fresh process. Actual recovery precedes the second save; reaction becomes
null, outgoing layers empty and action-1. Ledger/combat RNG and all nine actors'
Health float bits remain unchanged. Directly reviewed captures show the visible
Player leaning/hunched with a raised arm during the reaction, then upright idle.
The four editable scenario manifests and typed fixture are in scripts/data.

`M15-REACTION-CONTROLLER-VERIFICATION.json` pins code, inventories, binaries,
original comparisons, manifests, source inputs, epochs, saved states, visual
reviews and retained failures. This is **injected owned-state continuation**:
its outgoing one-second span is synthetic. It does not prove a normal contact,
stock caller duration, quantitative original-game pose/root-motion/audio
identity, or the full input/view/FPS matrix. The earlier first-person course
used an older candidate and hid the Player body; it is not final first-person
visual acceptance. General INI defaults are reviewed, while custom INI import
and live byteC4 mapping remain open.

Normal contact adapters still create no reaction/outgoing descriptors. Native
caller decisions, activation weights, cached/repeated group semantics,
variant/RNG selection and the remaining mastery/effect/control cases must be
finished before closing the normal-input gate. **M15S4 is still incomplete.**

## Cached reaction restart and native caller checkpoint

Prepared contact publication now supports repeating an owned BlockHit or Recoil
with the same selected resource and an explicit no-ease start. The new contact
replaces the old cause, resets playback and Start/retirement/inactive state, and
retains no outgoing copy of that cached sequence. Unrelated retiring layers
survive. Different resources/kinds, eased replacements, supplied outgoing copies
and conflicting detached instances remain outside this bounded path. General
previous-reaction replacement and Stagger channel retention remain open.

The full engine inventory passes **1,748 tests / 125 suites in each of the normal
and ASan/UBSan profiles**, with no failures or skips and unchanged source during
both runs. The new test covers two reaction kinds and four prior native sequence
states, rejected requests, stale preparation, atomic Health publication,
unrelated layers, unchanged RNG/clock/action allocation, codec restoration and
replay refusal. Leak detection is disabled; existing warnings remain. Component
and Python implementations did not change in this chunk.

Independent original-instruction execution now records **1,008 contexts** across
NPC and Player callers, group resolution, cache selection, initialized CRT RNG,
sequence activation and following channel updates. AttackLeft, BlockHit,
BlockAttack and Recoil use channel3; Stagger uses channel0. Recoil/BlockHit can
crossfade the active channel3 attack. Stagger instead leaves channel3 resident
and advancing through its attack phases in the tested update prefix. The first
incorrectly placed attack fixture was rejected and retained; it is not evidence
of native attack replacement.

Native Single caches select without RNG. Multiple caches select with a CRT draw
modulo their count, even when an explicitly supplied Multiple cache contains one
entry. NPC group resolution and playback both select, and the second selection
chooses the played resource. Player group resolution also depends on camera
routing between body and first-person caches; first-person basename matching
can avoid a further draw, while its fallback can consume one. Mixed cache kinds
therefore have distinct draw counts. These are synthetic resource/cache and
scene fixtures executing original code, with each boundary stated in the
receipt. Resource insertion order, winning overrides and actual scene-pose
capture are not established by these cases.

`M15-CACHED-REACTION-VERIFICATION.json` pins the implementation, both builds,
editable expected values, original corpora, verifier scripts and retained failed
fixtures. It includes the separate 16-case full actor same-cached restart proof
and Player results. There is **no new rendered or normal-input restart course**
in this chunk. The prior owned-state continuation remains historical controller
evidence and its Stagger/outgoing duration is synthetic. Normal contact adapters
still generate no reaction descriptors. **M15S4 is still incomplete.**

### Reaction scene-gate correction

The provisional47F7B0 ancestry label is now resolved: full original execution
with no stubs shows a world-bound-sphere/camera-frustum test. Twenty cases agree
under both precision words. `M15-REACTION-SCENE-GATE-VERIFICATION.json` records
fixtures, native paths and hashes; the cached-reaction receipt links this
correction. Its supplied false boundary and recorded outputs remain unchanged.
Visible actors can select the separate scene-pose capture path, which still
requires implementation and runtime acceptance. **M15S4 remains incomplete.**

## Captured pose and transition destination checkpoint

Owned melee reactions can now persist a captured local pose in schema 56 and
project it through a silent constant renderer layer. Capture copies translation,
unscaled rotation, scale and per-bone priority from the current controlled nodes.
Later changes to those nodes do not change the snapshot. The destination stays
at its requested frame during the transition, then rebases its clock when the
first manager update reaches transition end. The snapshot is removed on that
update; the following actor prefix resumes the normal reaction guards.

Independent execution of original `6C5C70` now includes actual manager/sequence
constructors, registration, pose capture and transition setup. It produces
snapshot state 4 and destination state 5. One real transform-controlled block
retains the captured local after deliberate node mutation, and actual sequence
and blend updates reproduce all seven observed transforms. Native zero-duration
transitions clamp to binary32(1e-4); callers of the new owned-state path must
supply a positive transition span.

The production matrix-to-quaternion conversion matches all **2,060 observations
from 1,030 original matrices** under precision words `027F` and `037F`. The transition
comparison matches **448 updates across 64 contexts**, including nonzero begins,
requested frames and large clock origins. Sequence `+3C` contains the clamped
sample; accumulated history is `+38`. The editable fixture retains 80 rotation
observations and all 448 timing samples.

Final normal checks pass **2,952 component tests, 1,751 engine tests and 330 Python
tests**. The separate ASan/UBSan build passes **805 selected ESM4 component tests
and 1,751 engine tests**. Exact inventories agree with XML, with no failures or
skips and unchanged tested source. C++ binary/JSON and Python codecs reproduce
identical schema 56 bytes, including signed zero. The tested source fingerprint is
`642f06ccfc4b98d96f61e66919e6126b8b2c03be4ad4c2568772b61ea6c2a6ca`.

Normal and instrumented real-engine courses save during an explicitly injected
one-bone pose transition, quit, load the exact output in a fresh process, await
actual reaction recovery and resave. The first output preserves the pose exactly
with destination time 0 and easing active. The final output has no reaction or
snapshot and action -1. Ledger/combat RNG and all nine actors' Health bits remain
unchanged. All four captures were directly reviewed: the deliberately artificial
pelvis pose holds the Player nearly horizontal, then recovery returns upright
idle. The typed editable fixture reproduces the staged save byte-for-byte; four
editable manifests retain the actual courses. The four-second span and pelvis
transform are synthetic, not stock caller duration or live pose-capture evidence.

The full desktop-engine LeakSanitizer run remains **failed** on 19,738 bytes in 372
allocations, despite all 1,751 assertions passing. An existing 13-test input/window/
local-map subset reproduces that exact total without the new pose tests; an
offscreen backend plus SDL's DBus shutdown debug hint still leaks. One traced
root enters SDL/DBus; other roots are unsymbolized. Full ASan/UBSan acceptance
therefore uses leak detection disabled. Separately, all 805 selected ESM4 component
tests and the three new pose engine tests pass with leak detection enabled. The
rendered courses also disable leak detection. No full-engine or rendered-session
leak-free claim is made.

`M15-POSE-SNAPSHOT-VERIFICATION.json` pins the original artifacts and declared
allocator/CRT-registration/OS boundaries, expected values, source hashes, checks,
codec interoperability, runtime epochs/saves/captures and retained failed attempts.
Normal contact adapters still generate no reaction or snapshot descriptors.
Player body/first-person ownership, general reaction replacement, Stagger's
resident attack channel and the full normal-input matrix remain open.
**M15S4 is still incomplete.**

## Authored sequence identity and native cache census (2026-10-19)

The KF loader now retains the authored `NiControllerSequence` name separately
from its renderer alias. The original loader truncates the first underscore
suffix from this name to classify the gameplay group, then renames the sequence
to its resource path. Treating every filename stem as the gameplay group would
split variants such as `Recoil_01` into unrelated groups. The new field preserves
case and suffixes verbatim and survives independent holder copies; the existing
renderer alias continues to select the resource.

Original executable execution verifies **192 cache insertion contexts / 576
insertions**, including promotion from Single to Multiple, insertion order,
duplicate names and shared/distinct metadata. Insertion draws no RNG. The second
eligible resource promotes the collection; the native table flag alone does not
make its first resource Multiple. A separate **288 classifier contexts** cover
BlockHit/Recoil/Stagger, four movement prefixes, six weapon-family prefixes,
mixed case, underscore suffixes and both precision words. Native family codes
2/4/5 mean OneHand/Staff/Bow. Original CRT `98262D` compares ASCII case
insensitively; the earlier cached-reaction receipt's `strcmp` label is corrected.

A fresh isolated original-game process supplies four accepted read-only cache
samples: first/third-person idle, then third/first-person drawn unarmed idle.
Both animation clocks match exactly at each paused sample and advance between
them. Initial body/first-person caches contain **278/277 groups and 290/289
registered sequences**. Drawing adds **six groups and seven registered sequences
per view**. Existing resource order stays intact; all 16 initial Multiple
collections agree with this installed archive's listing order. In particular,
HandToHand AttackLeft is `hook`, base, `jab`, which is not alphabetical. These
observations do not establish arbitrary loose-file or override ordering.

The original game and owned Steam both exit with code 0. Exact private inputs
are restored; source prefix, pristine saves, executable, master and meshes archive
hashes remain unchanged. A long R press that left the view unchanged and the
first analyzer's incorrect fixed-cache-count assertion are retained. The accepted
view switch uses a short tap, verified by the live camera byte and direct image
review. No attack, injected contact, memory write or damage measurement occurs.

The focused `ESM4NativeAnimationMetadata.*` test passes once in each normal and
ASan/UBSan component build, with exact inventories, no failures/skips and tested
source fingerprint `00a72a6b8149d3676552e258479b977c75e6df7aad46b9512401a4a0d7d9d2d8`.
This is focused loader validation; no new full engine run or fork gameplay
acceptance is claimed, and the sanitizer helper disables leak checking.
`M15-NATIVE-SEQUENCE-CACHE-VERIFICATION.json` pins source, checks, original
instruction reports, live observations, boundaries and retained failed attempts.
The editable cache fixture records input axes and selected original orders.
Normal contact descriptor generation and the remaining view/channel work are
still open. **M15S4 remains incomplete.**

## Active owned reaction replacement (2026-10-19)

The contact transaction now accepts a distinct BlockHit/Recoil resource replacing
an already active owned BlockHit/Recoil. It moves the exact old causal ID,
resource, sample and timing into a silent outgoing layer, starts the new incoming
crossfade, and publishes the resource deltas and process action together.
Preparing the replacement leaves live state unchanged; a clock change invalidates
the prepared lease. Saving during retirement preserves both owners and clocks,
and inclusive fade completion removes only the old layer.

This follows 16 previously published full original NPC caller contexts selected
from the independent corpus: old actions6/7, new BlockHit/Recoil, two supplied
prefixes and both precision words. Native474530/6C9D30 retires the old channel3
resource as state3 and activates the distinct target as state2. These are selected
existing cases, not additional oracle runs. The original cache resources are
supplied fixtures; the engine's private causal ledger is separately tested.

The new world test covers all four old/new reaction pairs, actual transaction
publication, stale leases, Health-once, exact outgoing identity/timing, binary
save/restore before and during retirement, continued old samples and no RNG/ID
allocation or replay. It rejects altered causal/sample/clock inputs, prematurely
anchored retirement, missing outgoing ownership, same-resource fade duplication,
Stagger, and an existing detached layer for either old or incoming resource.
The latter collision was found during review: both initial builds were stopped
before editing, and their failed reports/logs remain retained.

Final normal and separate ASan/UBSan full engine runs each pass **1,752 tests /125 suites**, exact inventories, no failures/skips and unchanged tested source. Fingerprint `276139b1a9ae3e11dd5838886d4f342413fa55a9499cc26c5bc52279d4db7fbb`. Sanitizer options are `detect_leaks=0:halt_on_error=1` and UBSan halt/stacktrace; the previously recorded desktop LSan failure is still open. `M15-OWNED-REACTION-CROSSFADE-VERIFICATION.json` pins source, independent expected cases, build reports, actual environment and retained stopped builds.

This implements only active state1 sources with initialized timing and positive
direct crossfade descriptors. State2/3/5 replacement, incoming pose transitions,
Stagger's separate channel, Player view ownership and normal contact descriptor
generation remain outside this checkpoint. No new fork application runtime course
is claimed. **M15S4 remains incomplete.**


## Normal NPC BlockHit and actual captured-pose restart (2026-10-19)

Ordinary accepted NPC blocks now generate a Single BlockHit descriptor through
the real contact transaction. The factory resolves the winning equipped item,
native movement/family group and same-base cache fallback. It uses the prepared
numerical lifecycle to suppress reactions for victims that will no longer be
Alive, and retains animation/root leases until publication. Selection allocates
no extra action ID and consumes no RNG for these installed Single resources.
Multiple selection remains rejected until its ordering relative to armor-wear
RNG is integrated. Player, Creature, normal Recoil/Stagger and mastery factories
remain open.

The resource catalog uses exact authored sequence names and winning compressed
BSA listing order, independently of the sorted VFS iterator. Production loader
inspection reads all **323 body and 322 first-person root KFs**; its reaction
catalogs match the captured original caches exactly: **15 groups/19 resources**
and **14 groups/18 resources**, respectively. Mixed-archive or loose variant
ordering is not inferred. Independent production comparisons match **288
classification, 864 reaction-fallback, 1,078 frustum and 896 NPC-selector
observations**. These totals include seed/precision replicas. Raw byte 114 is
not proven to mean Spell; the public primitive calls it `mFlag114`, and this
live factory admits Weapon draw 115=true. Native 5E5690 returns the fallback;
its reaction callers perform the low-byte group check.

The first actual course exposed a capture error: the stock KF authors 73 tracks,
but this actor binds 64. Original6C9F10 skips unbound blocks. Six new original
pose-activation contexts, with missing controller/interpolator/both and both
precision words, preserve the valid captured pose and all 42 blend samples from
the published one-block control. Capture now uses admitted controller bindings.
A renderer regression retains the valid pose while skipping missing targets and
an existing uncontrolled node. A separate world-bound test prevents applying a
model transform twice and rejects ambiguous scene placement. The original float
frustum predicate is verified separately; exact native world-bound/camera geometry
parity is not claimed.

Normal and instrumented keyboard1FPS courses each commit one sword contact
against the public-Lua held NPC shield, bind stock `blockhit`, and actually save
its **64-track captured pose** at reaction time 0. The exact first output enters
a fresh process, rebinds 64 tracks, reconstructs held BlockIdle, and recovers
directly to action 6 without block release or contact replay. A second failed
course had recovered action -1 before restarting BlockIdle; the corrected renderer
reconstructs that resident layer using existing action 6, without calling
`beginBlocking`. Independent cyclic BlockIdle phase and release-across-load
behavior remain open.

Both accepted courses preserve observed Health Damage **-0.4110095500946045**
exactly through continuation, retain next action ID 3 with no pending owners, and
retain combat RNG **773150046** after the declared four draws. Health magnitude
is checked against the preregistered nonlethal 0..2 range, not claimed as an
independently frozen original-game magnitude. Player sword wear matches its
expected float bits; NPC equipment and all other settled AV channels remain
unchanged. Additional loaded references independently match original-master
identities, bases and CELL placement; none is a new dynamic drop. All four
accepted images were directly inspected. Successful impact requests and mixer
output are retained, without a new quantitative PCM or native pixel/motion claim.

Final full normal and ASan/UBSan engine suites each pass **1,754 tests/125 suites**
with exact inventory/XML agreement and no failures/skips. Tested fingerprint:
`cd3541a7311ef5ce25ad2d31051758b1bcb599932d0af258041e27a575f78a83`.
Before the renderer-only corrections, unchanged component/Python inputs passed
**2,957 normal components, 810 selected sanitizer components and 330 Python tests**.
The two stopped sanitizer builds are not overall passes; their completed results
and SIGTERM reasons are retained. Final sanitizer tests and runtime use
`detect_leaks=0:halt_on_error=1` and halting UBSan with stack traces. The previously
recorded desktop LSan failure remains open.

[M15-NPC-BLOCKHIT-VERIFICATION.json](M15-NPC-BLOCKHIT-VERIFICATION.json) pins the
source, actual executables, exact checks, independent native evidence, editable
manifests, save/reload assertions, images and retained failed attempts. This is
bounded held-NPC reaction acceptance, not the complete opponent/input/view/FPS
course. **M15S4 remains incomplete.**


### Player mirror clock and service ownership checkpoint (schema 57)

Schema 57 stores an optional Player `first_person` clock and owned reaction
separately from the body. Legacy promotion adds `null`, without inventing a
mirror or its clock. Both codecs share their existing reaction wire/JSON helpers;
strict owner/cause/timing validation, truncated payload rejection and signed-zero
storage cover the added field. The clock survives ordinary mirror cleanup.

Contact preparation can atomically publish a fresh direct mirror with the body's
consumed cause. Frame preparation distinguishes `(view, group)` metadata, anchors
and updates each view from its own clock, and retains a mirror after body
recovery. Only the body owns the process action. Mirror-only cancellation and
inactive cleanup preserve queued body input and process action. Prepared contact
and frame leases compare both clock bit patterns, rejecting a `-0.0` to `+0.0`
change as well as a mirror increment hidden by a coarse unchanged body clock.
Unsupported old-mirror replacement and strike/bow coexistence stay guarded.

Independent original instructions execute Player BlockHit/Recoil/Stagger helpers,
body playback, process assignment, `65D790`/`474BE0` mirroring and `6CA950` updates.
The initial 48 supplied-cache contexts produce 240 sample pairs, 96 with different
body/first-person output bits. Four independently supplied first-person clocks
are exercised across both camera bytes and x87 precision words. This establishes
offset and rounding behavior, not divergence of real-game clocks; the earlier
four paused live samples had equal clocks. An expanded 96-context probe includes
48 repeated controls, 32 same-slot BlockHit/Recoil crossfades from an old
first-person strike, and 16 Stagger cases retaining that strike in another slot.
The empty-controller/cache/scene boundaries remain explicit.


The direct first-person clock/replacement observations use a supplied `660110`
return distinct from both AnimData roots. A separate **72-case dispatch-prefix**
probe executes actual `660110(true/false)` and `612220`: equality with supplied
Player `+3C` or `+5D0` roots bypasses frustum testing **into `6C5C70` pose capture**
(48 cases); foreign roots with false frustum enter direct `470B20` activation
(24 cases). It stops at those manager entries and does not accept the subsequent
allocation/activation. The initial interpretation that matching roots skipped
capture was backward. The retained full-helper attempt reached pose allocation
and faulted on the absent allocator fixture. Four earlier accepted live samples
confirm first-person AnimData root equals Player `+5D0`; body equality to `+3C`
is still a separate live/constructor check. Normal Player mirror activation
therefore remains unimplemented, including its required owned-root captured
pose. The narrower direct packet/service tests do not close that gate.


All **1,757 engine tests/125 suites** pass normally and with ASan/UBSan at final
fingerprint `c84316dceb00917e6b0932f75549f3c53f715710ac5aa288b17987b55986c521`. Unchanged codec/component/Python inputs
also pass **2,960 full normal components, 813 selected sanitizer components and
333 Python tests**. Six C++/Python phases reproduce identical binary bytes and
C++ JSON-to-Python encoding, including body-gone/mirror-active, retirement and
negative zero. The initial World fixtures gave Player zero Endurance and hence
zero derived Health: the one-point contact correctly became lethal and rejected
reaction start. Only the fixture attributes and explicit Health precondition were
corrected. A stopped sanitizer build retains its completed component pass but
is not an overall pass. Leak detection remains disabled; the prior desktop LSan
failure is still open.

Fresh final-app normal and instrumented NPC BlockHit regressions each perform an
actual keyboard contact, save its 64-track pose in schema 57, then load that exact
output in another process and recover to held action 6. RNG, consumed-contact
ledger, observed Health damage bits, equipment and other settled AV channels
survive without impact replay. All first-person fields remain `null`; all four
captures were directly inspected and loaded additional references independently
audited against the original master. These are schema-migration/existing-NPC
regressions, not runtime acceptance of new Player mirror rendering.

[M15-PLAYER-REACTION-OWNERSHIP-VERIFICATION.json](M15-PLAYER-REACTION-OWNERSHIP-VERIFICATION.json)
pins the source, exact test inventories, native probes, editable expectations,
actual executables and runtime checks. Normal Player descriptor generation,
separate persistent renderer roots/caches, prior first-person outgoing/parallel
layers and actual view scheduling remain open. `CharacterController` still
projects the existing body path. Broader reaction replacements, Multiple RNG
ordering, mastery/effects/directional powers and the full opponent/input/view/FPS
course remain open. **M15S4 remains incomplete.**


### Player owned-root pose activation and independent fade completion

A new isolated original-game episode closes the root-identity condition left
open by the preceding ownership chunk. Four accepted, read-only memory samples
cover idle/drawn hands in first/third person. In every sample body AnimData root
is Player `+3C`, first-person root is Player `+5D0`, and each sequence manager
points to its own root. Both roots and AnimData objects persist across these
camera/draw changes. Equal clock bits in these four samples establish only that
observation; combat scheduling remains open. A retained first-person drawn
attempt showed the automatic vanity camera and is excluded from the accepted
visual matrix. The game, Steam and private display processes exited, private
inputs were restored by hash, and the source prefix/content remained unchanged.

Independent original `474530` now executes through actual `660110`/`612220`,
owned-root selection, `6C5C70`/`6C9F10` capture, `6CA950` sequence updates and
`6CC3A0` transform blending. Real manager/sequence/controller/blend/interpolator
constructors bind one prescribed transform per view. The allocator, OS imports,
scene predicates and scene-update boundaries remain supplied; this is not the
full Player helper/cache/resource path or rendered gameplay acceptance. Two
48-context matrices cover both root roles, four initial clocks, three reaction
groups and both x87 precision words: 336 samples each, absolute-clock control and
cumulative float32 frame increments. Captured pose data remains frozen after
changing the live root. All group/precision replicas agree per role/clock.

Incoming state 5 and captured state 4 last until the first update noticing the
inclusive stored fade end. That update anchors destination time zero at its
actual clock, including overshoot. With cumulative .025 frame durations, an
initial clock .5 is still below its stored end at nominal .100 and finishes at
.125; initial clock zero finishes at .100. During state 5 the original offset
field is the destination frame, represented by `RuntimeMeleePoseSnapshot`, not
the service's unused backup offset. The captured quarter-turn quaternion has
components `0x3f3504f3`; the two-slot initial blend renormalizes to `0x3f3504f4`
even with incoming ease zero.

Contact preparation now accepts a fresh captured first-person pose alongside
its body reaction. It requires uninitialized timing, a positive easing span,
the matching consumed cause/kind and exact persistent view-clock bits. Invalid
initialized timing, easing without a snapshot and malformed spans reject
atomically. Old first-person owner replacement remains guarded. New World cases
compare the original timing bits for both clock orders, all three reaction
kinds and five clock pairs, restoring every intermediate state. They cover
one-view-only pose ownership without changing settled AV/RNG/contact state or
body process ownership. The component test compares all 56 retained native
blend samples; a renderer test applies the captured first-person quarter turn
at incoming ease zero. No wire/schema change is needed.

This extends packet/service and synthetic renderer acceptance. Normal Player
resource generation, persistent dual renderer roots, old first-person outgoing
or parallel layers and normal-input save/restart acceptance remain open.
Broader M15S4 reaction, mastery/effect and runtime-course gates remain open.
**M15S4 remains incomplete.**


The initial normal engine run passed1,758 tests and failed only the new renderer
assertion: it compared a separately stored, unscaled rotation matrix against a
scaled expectation. Translation and the separately stored scale passed, as did
the full new World test. The corrected assertion checks the final drawable
matrix, including scale. Both original failures remain retained; production
behavior and native expected bits are unchanged by this test correction.


Final fresh normal and ASan/UBSan engine runs each pass **1,759 tests in 125
suites**, with exact inventory/XML agreement and no failures or skips. Their
shared tested fingerprint is
`616f3c9b166367c3fab4eace7d6d33ddc6a80eb0f25e55d6b7a40b993c32e2d1`
(parent `f29387e476`). The unchanged component inputs pass six selected normal
blend tests and all **814 selected ESM4 sanitizer component tests**. Actual
sanitizer engine process environment is retained: ASan disables leak detection
and halts on error; UBSan halts with stack traces. This is not leak coverage.
The earlier prior desktop LSan limitation remains open. Only documentation is
added after these checks; the tested code and editable numeric corpus stay
unchanged. The initial stopped builds, both full runs with the incorrect renderer
assertion, venv-path failure and excluded vanity-camera observation are retained.

[M15-PLAYER-OWNED-POSE-VERIFICATION.json](M15-PLAYER-OWNED-POSE-VERIFICATION.json)
records this continuation. It supplies live root identities and full manager
pose activation beyond the preceding dispatch-prefix proof. It does not supply
normal Player reaction rendering or new normal-input gameplay acceptance.

### Persistent Player body and first-person renderer roots

The TES4 Player now owns a normal body animation and a separate passive
first-person animation throughout each loaded world. World animation lookup
continues to return the body. Camera switches select visibility and tracking
without rebuilding either skeleton. Private insertion groups use cull-only
visibility, so hidden roots still receive update traversal. During replacement,
unpublished groups have zero masks; the old pair stays alive until both new
roots are ready and the camera is rebound. Morrowind retains the single-root
construction and view-change path.

Ordinary playback and equipment presentation commands reach the peer's own
resource catalog. The peer cannot send duplicate text-key or Lua animation-end
notifications, equipment sounds, equipment world notifications or blink-timer
RNG draws. Missing peer groups preserve its existing same-priority state.
Native sequence layers and explicitly view-scoped event targets remain local.
Ordinary frame targets clamp to the peer's own authored interval: this is
command mirroring, not acceptance of independent native first-person strike
timing. The body retains normal race proportions across POV changes; the eye
pivot uses race height independently of the current camera mode.

Five renderer tests cover distinct resources/poses, passive event suppression,
view-local native ownership, hidden update traversal and stable roots/controllers,
independent frame bounds and missing peer resources. Fresh normal and ASan/UBSan
engine checks each pass **1,764 tests in 125 suites**, with exact inventory/XML
agreement and no failures or skips. Their tested source fingerprint is
`cf666d8a1e0ac41b3a0a6f08bcf410965ccfbd6831f221dd9db6903445a091cd`
(parent `5ae581273e`). ASan disables leak detection and halts on errors; UBSan
halts with stack traces. This supplies no leak coverage.

The runtime course uses normal draw, turn, short right step, two miss strikes,
sheathe, held block/release and camera input, then F5/F9/F5 and a fresh process
loading the actual output. Read-only GDB samples identify both roots, private
parents, cull visibility, camera tracking pointers, resource holders and selected
bone transforms. Each observed profile supplies 11 samples before process exit
and 2 in the fresh process. Roots stay stable across POV/input changes within a
world segment; F9 replaces both roots. The hidden body pose changes on the
first-person draw, and the hidden first-person pose changes on third-person
sheathe. The first-person idle's own metadata has 66 raw transform blocks; the
body's metadata has 73 raw blocks, with 64 bound controllers. These are distinct
counts. The observer never calls or writes into the inferior. Additional layout
metadata is loaded only into GDB. Observer pauses preclude exact FPS or original
hidden-bone scheduling claims.

The editable public sources are
`oblivion_m15_native_player_paired_views.json` and
`oblivion_m15_native_player_paired_views_continuation.json` under
`scripts/data/oblivion_compat/`. They take `miss_fixture`, the existing generated
`M15PlayerRemoveItemOpponent.esp` fixture from the declarative
`m15_player_remove_item_fixture.json`. Stage a private copy of the pinned input
at `OUTPUT/userdata/saves/M15Review/Quicksave.omwsave`; never load a historical
input slot directly. Copy the first process's actual output into the second
private slot. The public course is additionally run without GDB or memory/copy
barriers, with the same selected state/audio outcomes.

Root-course saves preserve all native actor-value channels and original
inventories, after admitting only three schema migration defaults:
`ownership_rank:null`, `ownership_global:"null"`, `extra_scale:null`. Final
combat RNG remains 415139642, next action is 4 with no pending owners, and no new
Player reaction/first-person native state is claimed. Exact save/live hashes
connect the real F9 load and fresh-process continuation. Actual output PCM
matches two stock hand swishes in the first process and none in continuation.
The separate NPC shield-hit regression preserves the single HealthDamage
`-0.4110095500946045`, RNG 773150046 and next action 3, saves an active 64-track
BlockHit pose, and finishes recovery after fresh-process load without replaying
the contact. Added cached references are independently checked against the
hash-pinned original master; whole-world cache counts are not fixed outcomes.
The normal Morrowind startup/render/quicksave smoke test retains no TES4 save
markers. Its terrain-facing screenshot supplies no character-pose acceptance.

Image review also finds a large skin polygon during first-person held block.
A separately linked diagnostic copy using single-root construction reproduces
it; the paired-root feature is not required. That control is current-code
counterfactual evidence, not an original-game oracle. Clean held-block rendering
remains open. The initial diagnostic failed before world construction because
its directory lacked default data files; the corrected fresh attempt is kept
separately. The first sanitizer observer attempt exceeded its first-capture
180-second deadline during loading/readiness and was stopped; the fresh attempt
uses a 600-second external observation window with unchanged gameplay assertions.
Earlier observer/type-layout failures and pre-final courses remain retained.

[M15-PLAYER-PAIRED-ROOTS-VERIFICATION.json](M15-PLAYER-PAIRED-ROOTS-VERIFICATION.json)
pins the source, binaries, test inventories, manifests and runtime reports.
Independent native first-person strike timing, normal Player reaction generation
and dual projection, outgoing/crossfade/parallel view layers, complete original
hidden-view scheduling, mastery/effects, directional attacks and the full
opponent/input/view/FPS/cancellation and in-flight restart courses remain open.
**M15S4 remains incomplete.**

## Fresh Player BlockHit in both owned views

Ordinary blocked contacts now prepare fresh Player BlockHit owners for both the
body and first-person roots. Each owner captures its own pose, retains its own
sequence metadata and blend span, and advances on its own animation clock. The
body controls process action; first-person events remain passive. Prepared
contact leases retain both renderer roots. Layer publication validates both
view plans before updating either view; rejection tests cover invalid frame
times, missing resources and unbound poses while preserving previous frames and
controllers. This does not claim atomic recovery from allocation failure.

The factory admits living drawn-weapon Player contacts with fresh owners,
unit-weight Single resources and no active strike, reaction or retiring owner.
It keeps the NPC factory route. The body selects the reviewed native fallback;
the first-person cache must contain that exact resolved key. General missing
resources and Multiple selection are outside this accepted domain. Catalogs
are warmed while the new roots remain private to avoid a first-contact load
hitch. This is resource preparation, not proof of original constructor or
hidden-view scheduling parity. Runtime schema remains **57**.

The independent original-executable selector check executes actual `5E5690`
with the Player vtable and `65D720` active-view/override lookup, common cache
methods and captured body/first-person cache topology. It records **7,168
observations**, **3,584 unique contexts**, **1,792 drawn-unforced production
selector comparisons**, and **7,168 fallback comparisons**. Worn-right/left
queries are typed entry boundaries; inventory traversal, file loading and
actual gameplay are not executed. The optional override fixture uses the same
declared cache as the selected view. The image SHA is
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Original `65D790` mirrors the resolved body key directly for Single resources,
without another first-person family fallback or RNG draw.

Normal and ASan/UBSan engine checks each pass **1,765 tests in 125 suites**, with
exact inventory/XML agreement, no failures or skips. All **335 Python tests**
pass, including configured log-poll validation and real file polling. Tested
engine/harness fingerprint:
`775607e1047593ac5f139f718a632681ea2db969ea2eb82f193f8d8877311902`
(parent `279c6d12c3`). The later declarative camera-course correction changes
only its POV ordering and passive camera observer. Reconstructing the build-time
Git diff with only that manifest restored in a temporary index reproduces the
exact checked fingerprint; the actual index remains unchanged. This verifies
that the engine/harness sources did not drift. A subsequent 335-test Python
run also passes. ASan disables leak detection and halts on errors; UBSan halts
with stack traces. Actual engine environments are recorded. No leak coverage.

The public `oblivion_m15_native_player_blockhit_views*.json` sources use the
existing declarative stationary opponent fixture. Normal B block and Space
activation produce one real opponent contact. At the first actual save both
captured poses remain active: **64 body tracks and 66 first-person tracks**.
The fresh process loads that exact output, rebinds both poses, recovers action
**-1** with block input unheld, removes both owners and advances the surviving
first-person clock. No replayed contact, factory selection or sound request is
allowed. Normal and sanitizer courses cover first-person and third-person
contact. Third-person is verified by passive camera-mode observation and direct
image review; its continuation screenshot also shows third-person. This does
not establish a general camera-restoration guarantee.

Exact selected consequences are Player HealthDamage **-0.9841405749320984**,
Player/opponent sword conditions **55.84976577758789 / 55.70000076293945**,
combat RNG **505908858** and next action **3**, without pending owners. First
Fatigue Damage permits only the preregistered real regeneration ranges; the
remaining channels are exact, and continuation Fatigue fully recovers. Original
reference inventories are preserved apart from the two declared sword-condition
writes and the three migration defaults. Newly cached references are checked
against original master identity/base/CELL, including persistent exterior
storage mapped to physical coordinates. No fixed whole-world cache-size claim.

Actual PCM matches a stock blade-block waveform in **one temporal cluster** at
contact and **none** after restart, using the inherited 0.75 normalized
correlation threshold. Logs independently show two legitimate played
`WPNBlockBlade` requests at contact and none in continuation. Correlation alone
does not establish coincident voice count or amplitude. The shared NPC shield
BlockHit regression also passes both profiles: active 64-track pose,
HealthDamage **-0.4110095500946045**, RNG **773150046**, next action **3**, no
Player first-person native state, and fresh-process action6 recovery without
replay. Images receive direct review; static images do not prove interpolation
or original-game visual parity. Clean first-person held-block skin rendering
remains open.

Failed attempts are retained. The initial factory incorrectly used the
NPC-only draw-state persistence helper for Player and rejected contact; the
corrected factory reads the authoritative live Player draw state. Earlier
1-FPS courses saved after the captured-pose fade had ended; both reactions were
present, but strict active-pose acceptance failed. The corrected 60-FPS course
polls the committed-contact log every 5ms and requests F5 immediately. Initial
third-person courses requested POV during held BlockIdle; the queued view
change left actual images in first-person. Their semantic results are retained,
but they do not count as third-person evidence. The corrected course switches
before blocking and requires a passive third-person observation. The sanitizer NPC regression also needed the immediate-save correction. A
preflight inspection caught a retry waiting on telemetry without enabling its
event stream; that owned process was stopped before contact and the corrected
fresh retry enables the stream explicitly. Final manifest comparison also
caught an extra queued POV action in the sanitizer third-person preparation;
its semantic/camera results are retained, and a fresh exact-public-manifest
course replaces it. No failed expectation was relaxed.

[M15-PLAYER-BLOCKHIT-VIEWS-VERIFICATION.json](M15-PLAYER-BLOCKHIT-VIEWS-VERIFICATION.json)
pins source/manifests, binaries, exact check reports, independent selector scope,
process epochs, actual save/live chains, audio, image review and NPC regression.
Cached/state2/3/5 replacements, outgoing/crossfade/parallel layers, independent
native first-person strikes, normal Recoil/Stagger/BlockAttack factories,
creature reactions, mastery/effects, directional powers and the full
opponent/input/view/FPS/cancellation and in-flight restart matrix remain open.
**M15S4 remains incomplete.**

## Repeated Player BlockHit replaces both cached owners

Paired Player contacts can now reset the same cached BlockHit/Recoil resource
with fresh captured poses in both views. Both old owners must have the same
cause, with matching incoming kind/group in each view; the old body must still
own its process action. No strike, detached retiring layer or outgoing copy is
admitted. Resident reaction retirement is supported independently of detached
layers. Normal blocked-contact generation admits matching Player BlockHit pairs;
normal Recoil generation and distinct-resource/parallel replacement remain open.
Only the body owns gameplay action. Each view keeps its clock, replaces its own
pose and binds the new consumed cause. Runtime schema remains **57**.

The new World test covers initial captured state, direct easing, steady,
retiring and inactive representations, independent body/FP clocks, rejection
without mutation, restoration, consumed-action replay and first-person-only
cleanup invalidating a prepared replacement while the body remains unchanged.
Normal and ASan/UBSan checks pass **1,766 tests / 125 suites**, with exact
inventory/XML agreement, no failures or skips. Their engine-source fingerprint
is `39cba6dce48abc8c88b0c3ea77952eb5cce5fbe56002fdd444cb215e745f270b`
(parent `e1cf006080`). Subsequent fixture-only authoring passes **338 Python
tests** and actual structural/semantic readback. ASan disables leak detection;
UBSan halts on errors. No leak coverage is claimed.

The independent original selector/pose probe executes `474530`, real
manager/sequence/controller/blend/interpolator constructors, `6C5C70`/`6C9F10`,
`6CA950` and `6CC3A0`. **288 contexts / 2,016 samples** cover both views,
BlockHit/Recoil/Stagger native group keys, four clocks including100000, both
027F/037F words and old native states0/1/2/3/5. State0 includes both missing and
resident inactive targets. Each same-cached reset enters state5 with two
registered sequences, and all following timing/pose samples match fresh start.
Raw pre-first-update last/weighted/output fields can retain zero for old
resident0/1/2/3, unlike the fresh sentinel; byte equality of that unsampled raw
prefix is not claimed. The fixture supplies one prescribed transform per view,
heap/OS boundaries and false scene predicates. It proves no full original
helper/input/gameplay or distinct/parallel-layer acceptance.

The editable runtime fixture keeps `M15StationaryOpponent.esp` unchanged and
adds a two-record overlay: the same local-layout script stops combat after real
Fatigue spending crosses145, and the stock rusty shortsword's speed is20. All
other weapon fields, record flags, NPC stats and style are retained. The existing
zero-regeneration fixture makes both contacts' resource channels exact. No
script supplies damage, contacts, condition or Fatigue writes. A typed input
helper promotes the pinned idle save46->57 and changes only its one idle
`fighting=0` script instance's winning origin to the overlay. Exhaustive decoded
comparison permits only that identity, version and enumerated empty schema
fields; every other value, inventory, action and RNG entry is preserved.
Structural writing, readback, fork compilation and runtime acceptance are
recorded separately. No TES4 authoring support is attributed to the TES3 MCP.

The normal held-B / Space course performs two production NPC contacts2/3.
Cause3's factory reports previous cause2; the old reaction is not recovered
before replacement. Both view bind records occur once per cause. The first
actual save contains the new cause3 captured poses (64 body / 66 FP tracks),
with no detached old owner. A fresh process loads that exact output, rebinds
both poses, recovers unheld-input action-1 and removes both reaction owners.
RNG stays505908858 and next action4, with no pending owners or replayed contacts,
selections or impact requests. Configured cap60 is not measured60FPS coverage.

Expected cumulative Fatigue Damage is **-40.04821014404297** for Player and
**-15.600000381469727** for the NPC. Player/opponent sword conditions become
**55.826988220214844 / 55.400001525878906**. These persist without regeneration
through continuation. Other native AV channels and original inventories retain
only the declared Health/Fatigue/condition changes. Added cached references are
independently checked against original master identity/base/CELL. Whole-world
cache counts are not fixed outcomes.

The composed original arithmetic probe exposes a separate unresolved precision
question: `5FF8F3` Health-store results differ by one float bit between007F and
027F/037F. Actual weapon-entry007F observations do not identify the control word
at the later blocked-contact Health store. Both executed alternatives are
preregistered explicitly: cumulative Player HealthDamage
**-1.9427038431167603 / -1.9427037239074707**. Every other selected numeric result
agrees across the three words. The current engine matches the wider alternative;
this does not close original runtime block-Health precision. No tolerance is
tuned to an observed engine result, and that fidelity gate remains open.

Audio requires two stock blade-block temporal clusters and four independently
logged played requests before restart, then none. The inherited0.75 waveform
threshold and pinned stock files are unchanged. The pilot's inherited
one-cluster assumption failed: actual audio/wall contact spacing was3.485s,
although the native-clock interval stayed short. Its two stock matches are
retained as a failed original criterion, and fresh final courses preregister
two clusters. Correlation does not establish coincident voice count. Direct
image review supplies only the observed views/guard/recovery frames; clean
held-block skin rendering and original animation interpolation remain open.

Retained failures also include a stale-test fixture with an Offset but missing
paired timing fields; only its fixture initialization was corrected, preserving
validation. A sanitizer build was stopped before source edits and is not a
pass. Changing the base plugin first correctly failed its saved fingerprint;
an overlay then correctly failed the old saved script-unit origin. The explicit
idle fixture migration resolves those authoring inputs without changing engine
admission. The first migration comparison omitted empty schema fields; exhaustive
reinspection identifies only the declared defaults and idle unit change.

All four normal/sanitizer × first/third-person courses pass the strict save,
fresh-process continuation and original-master reference audits. Both normal
courses and the sanitizer third-person course pass the preregistered two/zero
PCM cluster criterion. Sanitizer first-person matches only one of the required
two clusters before restart (zero afterward), despite four played requests and
two accepted gameplay contacts. Its original PCM report remains **failed**.
A diagnostic finds a stock02 correlation0.71579 near the unmatched impact;
this does not pass the0.75 criterion. No threshold is lowered and this course's
audio acceptance remains open. All eight actual screenshots were inspected,
showing the intended camera and guard/ready frames with the stated limits.

The hash-linked receipt is
[`M15-PLAYER-CACHED-POSE-VERIFICATION.json`](M15-PLAYER-CACHED-POSE-VERIFICATION.json).
It distinguishes passing runtime state checks from full course acceptance.

Full M15S4 remains incomplete: normal Recoil/Stagger/BlockAttack generation,
distinct/parallel Player layers, independent native FP strikes, creature and
mastery/effect branches, directional powers, actual block-Health precision,
the failed sanitizer audio criterion, clean held-block rendering and the complete view/input/FPS/cancellation and
in-flight restart matrix remain open.

## Original blocked-contact Health precision

The formerly open block-Health control-word question is resolved for the
observed original normal Player-block path. A separate cloned prefix loaded the
pristine save, established equipment/placement/combat through logged console
setup and held normal mouse block against Dreth's ordinary attacks. Read-only
GDB observations at `5FF8F3` and `5FF914` recorded **007F** for two actual Player
blocked contacts, with actual Player/attacker identity and input/output bits.
The second live Health store distinguishes precision: PC24 gives1075113456,
while027F/037F give1075113455. Both actual outputs exactly match independent
execution of the original instructions. No damage/contact calls or resource/CW
writes were injected. Software breakpoint machinery is explicitly declared.
The game and owned Steam wrapper exited0, the pristine save hash was retained,
and all private inputs were restored with source-prefix hashes unchanged.

`blockedHealthDamage` now rounds both subtraction and multiplication to24
significant bits with extended exponent range, using the existing exact binary
arithmetic primitives before the final float store. Both armed and unarmed
ordinary block preparation call it. Invalid supported-domain inputs are
rejected. The editable native probe regenerates **1,100 exact expected rows**,
including both live contacts, composed two-contact inputs, boundary floats and
seeded finite pairs. A standalone run reproduces the committed corpus byte for
byte; a mismatched-image negative control produces no output directory.

Normal components pass **2,963 / 280 suites**; the sanitizer ESM4 component
selection passes **816 / 64 suites**. Both engine profiles pass **1,766 / 125
suites**, with exact inventories/XML and no failures or skips. The tested C++
fingerprint is `b483577b82cc48029c6a125ddb85790b14875f5abe40127dba393b1a0762f127`
(parent `37758da630`). After both helpers terminated, only the standalone
Python/JSON probe and expectation metadata changed; final Python checks pass
**338 tests**. ASan leak detection is disabled; this is not leak coverage.
The first environment monitor incorrectly expected engine options for the
component selection and failed before retaining a snapshot. Its failure is
preserved. Component options are the helper's configured values; actual engine
ASan/UBSan options have a separate successful live-process capture.

Fresh normal/sanitizer × first/third-person two-contact courses now require
only cumulative Player HealthDamage **-1.9427038431167603** (bits3220744837).
The wider alternative is removed from the public expectation. All four strict
state/restart audits pass, including unchanged Fatigue/condition expectations,
inventories, RNG505908858, next action4, paired reaction replacement and fresh
unheld recovery without replay. All four fresh courses pass the original two/zero PCM criterion, with
four played native block requests before restart and none after. All eight
actual images were directly reviewed for the intended guard/ready camera
frames; interpolation and the held-block skin issue remain outside that
acceptance. These outcomes are recorded separately per course. The earlier failed sanitizer PCM criterion is
retained; a diagnostic matches its two stock02 impacts separated by20ms, which
does not retroactively change its original failed criterion.

The evidence receipt is
[`M15-BLOCK-HEALTH-PRECISION-VERIFICATION.json`](M15-BLOCK-HEALTH-PRECISION-VERIFICATION.json).
Reproduce the arithmetic corpus with the Unicorn environment and
`scripts/tes4_m15_block_health_oracle.py ORIGINAL_EXE NEW_EVIDENCE_DIRECTORY`.
This closes the identified Health arithmetic discrepancy, not complete native
cost/wear/mastery arithmetic, full original contact dispatch or full M15S4.
All other unresolved implementation and view/input/FPS/cancellation/restart
requirements above remain open.

## Independent Player reaction completion and replacement

The Player BlockHit factory and captured reaction transaction now accept either
old view having completed independently. A surviving owner must still match the
new kind/group in its view, and both incoming owned-root captured poses are
required. No strike, detached outgoing layer or distinct-resource replacement
is silently omitted. Each view retains its own clock; only the body owns the
process action. Runtime schema remains57 and paired incoming causes remain
validated. First-person inactive cleanup now emits a passive diagnostic, and
the factory records both previous causes.

A new World test covers **16 contexts**: BlockHit/Recoil, either view completed,
and surviving captured5/direct easing2/steady1/retiring3 states. Supplied inactive
state is removed through the real cleanup method. Atomic preparation/rejection,
new independent pose snapshots/clocks, binary restore, replay rejection and
resource/RNG invariants are checked before zero-duration frame anchoring.
This is service-state verification, distinct from the normal-input courses.

The independent original-helper probe executes **576 contexts**, including
**192** absent/same-cached per-view cases,96 distinct first-person crossfades
and288 missing-first-cache cases. Both camera bytes, three x87 words and four
first-person clocks are supplied. Original complete BlockHit/Recoil helpers
admit absent/same-cached old owners independently when both caches contain the
key. These supplied foreign roots use direct activation; the separate earlier
288-context owned-root probe establishes captured-pose reset. Empty controller,
cache hash/equality and scene boundaries are explicit. No original normal-input
asymmetric-clock/contact acceptance is claimed.

The missing-cache cases expose additional work: selection consults the active
view. In first person, missing cache prevents selection; BlockHit preserves old
owners while Recoil resets both channel3 slots without changing prior action.
In third person the body can start while the first-person owner remains.
General missing-resource integration therefore needs active-view selection and
independent causal ownership; removing current paired validation alone would
be wrong. That domain and distinct/parallel owners remain open.

The editable fixture extends only controller StopTime and End text-key time
in one stock KF from0.3 to4 seconds. All transform tracks, priorities and other
bytes are preserved. The actual NIF reader reinspects both variants. The
reviewed two-record opponent overlay changes weapon speed to0.5; NPC/style,
script local layout, damage, weight and other weapon fields stay intact.
Canonical idle input and zero-regeneration plugin remain hash-identical.
Structural writing, NIF readback, script execution/world-reference audits and
normal runtime are recorded as separate layers. No script injects contacts,
Health/Fatigue/condition/RNG writes or playback owners.

Final normal and ASan/UBSan suites pass **1,767 engine tests/125 suites**, with
exact inventory/XML agreement and no failures/skips. All **341 Python tests**
pass, including three byte-preservation/invalid-input fixture tests. Final
fingerprint: `a6d4ba27d0cade59962c8c34b2e2b260c9831fcc66cae2a490ae23da692ddce4`
(parent `ead4d076b6`). Every final source file matches the checked fingerprint;
only this receipt/audit follows those checks. Actual sanitizer test/runtime
options are recorded; leak detection is disabled and no leak coverage is claimed.
Unchanged components use the preceding PC24 checkpoint rather than a repeated run.

**Eight** normal/sanitizer × long-body/long-first × first/third-person courses
pass strict normal-contact, exact-resource, save/restart and original-master
reference audits. The second production contact observes old causes **[2,0]**
or **[0,2]**, then publishes fresh64/66-track poses with cause3. Fresh processes
load the exact saved output, recover unheld action-1, clear both owners and
advance the retained first-person clock without replay. HealthDamage stays
**-1.9427038431167603**, Fatigue/weapon conditions keep the prior exact expected
bits, RNG remains505908858 and next action4 has no pending owner. Actual saved
NPC playback speed0.5, loaded overlay fingerprint and unchanged fixture hashes
are rechecked. Configured cap60 is not measured60FPS acceptance.

Six courses pass the unchanged two/zero PCM-cluster criterion; normal
first-long/third-person and sanitizer body-long/first-person match only one of
two required first-phase clusters. All continuations match zero. Four played
native block requests occur before restart and none after. Both failed PCM
reports remain failed. Diagnostic sums of two identical stock voices separated
by20ms match the unmatched impacts at0.99881/0.99877 correlation; this does not
retroactively alter the single-wave criterion or establish coincident voice count.

All16 images were directly reviewed and show the intended cameras. Seven
continuations show ready poses. The sanitizer first-long/first-person
continuation still shows a raised sword/forearm before its later save. A
subsequent chronology check found that the screenshot preceded first-person
cleanup (see the following correction); this capture does not establish
settled ready-pose acceptance.
Static frames do not establish original motion/camera parity, and the held-block
skin issue remains open. Full course acceptance is separated from state audits.

The first speed2 fixture produced four correctly failed branch attempts:
both old owners were still present at contact3. Its normal launch/save/restart
phases completed0, but the required asymmetric branch was absent. Only the
owned queued orchestration/audio parents were stopped before launching that
fixture under sanitizer; build/tests continued and passed. Corrected fresh
courses use speed0.5 and explicitly wait for old-view cleanup. Continuations
wait for actual first-person cleanup before saving. Original attempts and
criteria remain retained.

[`M15-PLAYER-ASYMMETRIC-OWNERS-VERIFICATION.json`](M15-PLAYER-ASYMMETRIC-OWNERS-VERIFICATION.json)
pins the source, instruction probes, authored inputs/readbacks, checks, actual
executables/epochs, full matrix, images and retained failures. M15S4 remains
incomplete: general missing resources, normal remaining reaction factories,
distinct/parallel owners, native FP strikes, mastery/effects/creatures and
directional powers, audio/settled-pose criteria and the full input/FPS/cancellation/
in-flight restart matrix still require delivery.

### Settled Player recovery capture correction

The preceding raised-sword image was captured 33.998 seconds **before** the
first-person cleanup event, as the original process log confirms. The later
save establishes cleared owners after that image; it cannot establish their
state at capture time. The prior inference of a persistent after-cleanup pose
was incorrect. Historical reports and images remain preserved.

The continuation manifest now waits for both cleanup events before its existing
60-second settling interval, screenshot and save. Eight fresh processes loaded
the eight preserved actual contact saves without changing or rerunning their
first phases: normal/sanitizer × body/first-person long owner × first/third
camera. All eight pass scenario/epoch checks, exact state/resource/RNG/inventory
verification, master-reference audit and zero-replay PCM checks. Actual process
logs put both cleanup events before each capture. Direct inspection of all
eight new images shows the intended camera and a ready sword pose.

The Python suite passes 341 tests for this manifest revision. No engine source
or executable changed; the prior 1767-test normal and sanitizer inventories are
reused evidence, not fresh builds. Four new continuations use ASan/UBSan with
halt-on-error and leak detection disabled.

[`M15-PLAYER-SETTLED-RECOVERY-VERIFICATION.json`](M15-PLAYER-SETTLED-RECOVERY-VERIFICATION.json)
pins the correction, readonly inputs, actual process epochs, checks and reviewed
images. The two historical positive-contact PCM failures retain their original
failed status. Static recovery images do not establish original motion/camera
parity or close the separate held-block skin issue. The broad M15S4 gaps listed
above remain open.

### Native sequence variant selection and contact reaction dispatch

The immutable `selectNativeSequenceVariant` helper now models original Single
and Multiple collections using explicit collection identity, requested byte and
CRT state. It returns the index, next state and draw count without consuming
live RNG. Single ignores the request and draws nothing. Multiple uses an
in-range nonnegative signed-byte index directly; negative or out-of-range
requests consume one draw and select by unsigned remainder. A one-item Multiple
collection still draws. Malformed kinds/counts reject; counts above INT_MAX
remain outside this helper's supported domain.

The hash-identified original selectors and actual initialized CRT update pass
38,400 cases across all request bytes, ten collection/count contexts, five
seeds and three x87 control words. Only CRT thread storage is supplied. A
standalone production-C++ comparison matches every emitted row. Two new unit
tests retain 23 original goldens and reject malformed collections. Full normal
component checks pass 2,965 tests; selected ESM4* sanitizer checks pass 818 tests
in 64 suites. Inventories match XML with no failures or skips. Existing compiler
warnings remain; sanitizer leak detection is disabled and UBSan halts.

A separate 960-case original contact-dispatch probe establishes that positive
absorbed fraction requests defender BlockHit/BlockAttack. With no counter,
nonprojectile contacts also request attacker Recoil regardless of the disarm
helper's result. At zero fraction, this block-reaction branch requires an
unarmed defender with Journeyman-or-higher HandToHand. Supplied mastery,
counter/disarm results and reaction-call boundaries are explicit; this probe
establishes dispatch rather than actual drops or rendered playback. Another
168 drawn/standing selector cases confirm that shields override BlockHit family,
while Recoil and Stagger retain the weapon family.

The preserved full NPC helper matrix shows two Multiple draws: group preflight
and playback. The second selects the played resource. Player preflight uses its
active camera cache; later first-person matching uses resource basenames and
can require another draw. The new primitive represents one collection operation;
it must not substitute for that complete caller order.

[`M15-SEQUENCE-VARIANT-VERIFICATION.json`](M15-SEQUENCE-VARIANT-VERIFICATION.json)
pins source, checks, probes and comparison evidence. This chunk does not wire
normal contacts to Recoil and has no new engine/runtime acceptance. That wiring
must join both selection draws to the existing atomic contact RNG transaction
and preserve interrupted sequence ownership. Full M15S4 remains incomplete.

### Intact NPC attacker Recoil on blocked contact

Ordinary nonlethal, positive-absorption blocks now prepare an intact NPC
attacker's Recoil together with the defender's BlockHit. The factory keeps the
native drawn/movement family, does not apply BlockHit's shield override to
Recoil, and uses the verified ordered stock-resource catalog. Both original
Multiple operations are represented: discarded preflight and playback. Their
RNG draws follow existing numerical/armor draws in the same immutable contact
transition. A failed or stale preparation publishes neither reaction nor RNG.
The final contact reports the aggregate draw count.

The new Recoil retains the interrupted strike as a retiring sequence with its
causal action, group, animation time and timing offsets. Its incoming body
reaction owns action 7. The existing service publishes both reactions, resources,
equipment and shared RNG once. Own projected equipment withdrawal is excluded
from this new factory path so pre-break equipment cannot select a Recoil group.
Player attacker Recoil, zero-absorption unarmed reactions and fatal blocked-contact
parity remain open.

Two new World tests cover paired publication, invalidation by another actor's
shared RNG commit, retry/once-only publication and rejection of Recoil selected
from equipment scheduled for withdrawal. Full engine inventories pass 1,769
cases each in normal and ASan/UBSan configurations, with no failures/skips.
The four engine source hashes are unchanged through the later fixture/input
corrections. The final Python suite passes 345 cases, including real SDL relative
motion delivery, native queue priority, held-key preservation and malformed
motion rejection. The SDL helper is separately compiled without sanitizer
instrumentation; engine sanitizer leak detection is disabled.

The editable synthetic opponent script now stops combat on a second normal
activation, allowing the restored NPC Recoil to bind and recover naturally.
A negative-Fatigue safety stop remains unused. The builder changes only SCTX
and weapon speed 0.5, preserves saved local layout, and is independently reread;
actual fork compilation/world use is checked separately in every runtime phase.
Stock body/FP BlockHit and Recoil resources are used without loose KFs.

The final normal/sanitizer by first/third camera matrix records eight phases:
two preserved normal first02 captures reused read-only, two fresh normal05
continuations and four fresh sanitized05 phases. Six new 05 process epochs are
verified against actual executables and environments. Every state/epoch/master
reference check passes. The source RNG 505908858 advances by two selections per
blocked contact to 773150046. Both first saves contain real Recoil cause 3 and the
retiring cause 3 strike; reloaded reactions recover, a fresh action 4 starts, and
normal activation cancels it before any further contact. All Health/Fatigue
channels, conditions, original-reference inventories and RNG remain exact after
reload. No hit/miss/sound or new reaction selection replays.

Direct inspection covers all eight contact/recovery images. First-person
contact images show the raised Player blade and bent/raised NPC pose; the NPC
is mostly occluded in third person. Final images show ready poses after both
Player view owners and the NPC have recovered. Stills do not establish original
motion/camera parity or resolve the separate held-block skin gate. The configured
60 FPS cap is not measured 60 FPS; controller and FPS sweeps remain open.

The preceding failures remain failed: runtime01 auto-StopCombat canceled the
NPC before restored binding; runtime02 activation missed the NPC; runtime03's
first relative event was suppressed while establishing the pointer reference;
runtime04 recovered and saved but contained contradictory required-action and
forbidden-selection checks. Fresh05 removes only that contradiction, retaining
the strict one-canceled-action/zero-contact verifier. The initial build01
interruption for the intact-equipment guard also retains no test acceptance.
Unused inherited fixture annotations are corrected in an explicit errata;
sealed protocols and original reports are preserved.

Both normal first-phase PCM recordings pass the unchanged two-cluster rule.
Sanitized first-person finds only one required cluster and third-person finds
none; both positive-contact audio checks remain failed. All four continuations
pass the zero-cluster replay check. Each first phase separately logs four
successfully played block requests. Neither those requests nor successful state
checks override the failed PCM criterion; these recordings are not rerun until
a chance pass.

[`M15-NPC-BLOCKED-RECOIL-VERIFICATION.json`](M15-NPC-BLOCKED-RECOIL-VERIFICATION.json)
pins each independent validation layer and its audio outcomes. Full M15S4 remains incomplete: Player FP strike
and distinct/parallel ownership, Stagger/BlockAttack, mastery/effects, remaining
creature/power branches, input/FPS/cancellation/restart matrices and audio gates
are still separate required work.

### Stock block-audio mixture assay

The original single-template PCM failures remain failed. Each blocked contact
requests two equal-gain voices from the same emitter, and the mixer can start
them together or one update period apart. A sound compared only with a single
clip can therefore fail the existing correlation threshold even when the two
voices are present. The new assay declares a finite bank before fresh captures:
six aligned unordered pairs of three pinned stock clips and nine ordered pairs
separated by exactly one 960-frame/48 kHz mixer period. It keeps correlation 0.75
and one-second cluster separation, fits no amplitude/offset/time stretch, and
measures stereo channels separately. Coincident same-clip voices remain
indistinguishable from a louder single clip under normalized correlation;
native voice requests and temporal audio events are separate observations.

Six independent control tests construct capture-rate PCM without the bank or
correlation implementation. They cover all 15 pair/offset combinations, silence,
DC, unrelated noise, antiphase stereo, wrong event counts, truncated/unsupported
formats, malformed recipes/hash mismatches and refusal to overwrite a report.
The initial onset assertion used the earliest above-threshold window; an
alternate finite hypothesis could start 20 ms earlier. That failure is retained.
Reports now expose the strongest offset separately, which matches known source
onsets. Cluster counts, threshold and finite hypotheses are unchanged. The full
Python inventory passes 351 tests without skips.

The model is frozen before historical diagnostics and fresh runtime capture.
Diagnostics identify two clusters in each preceding failed sanitized recording,
with strongest correlations near 0.999 and one-period delayed pairs. Those known
recordings are diagnostics, not held-out acceptance, and their original failed
reports are preserved.

The fresh matrix uses eight new engine epochs: normal/sanitizer by first/third
camera, positive contacts and fresh-process recovery. Every actual process pins
its executable, environment, on-disk audio configuration and mapped OpenAL
library. Both phases explicitly configure period_size 960 and periods 3. Every
state/epoch/master-reference check passes, as do all four two-cluster positive
and four zero-cluster continuation checks under the frozen mixture criterion.
Original single-asset reports are also retained independently: three fresh
positive cases pass and sanitized first-person fails with only one cluster.
All four original single-asset continuation checks also pass. The new assay
does not rewrite that failed criterion. Direct inspection
covers all eight new contact/ready images, with the same static-pose limitations
as the prior Recoil course. No engine source or binary changes; prior 1,769-test
normal/sanitizer inventories are reused, with exact source/binary identity.

The three local asset bytes reproduce exactly from the pinned Sounds BSA using
`build/bsatool extract`. The editable recipe is
[`m15_block_mixture_assay.json`](../../scripts/data/oblivion_compat/m15_block_mixture_assay.json);
assets remain local. From the repository root, with NumPy installed, run:

```sh
python3 scripts/tes4_m15_block_audio.py --wave CAPTURE.wav --recipe scripts/data/oblivion_compat/m15_block_mixture_assay.json --clusters 2 --output NEW-REPORT.json
```

Use `--clusters 0` for continuation/absence checks. The tool refuses to replace
an existing report. The receipt records reproducible archive member names,
extraction commands and hashes, controls, diagnostics, source fingerprints,
fresh process/config identities and both PCM criteria:
[`M15-BLOCK-MIXTURE-AUDIO-VERIFICATION.json`](M15-BLOCK-MIXTURE-AUDIO-VERIFICATION.json).
This closes only the declared finite block-audio course. Full M15S4 still needs
Player strike/parallel-view ownership, remaining reactions/mastery/effects and
full input/FPS/cancellation/restart acceptance. The configured 60 FPS cap is not
measured 60 FPS, and sanitizer leak detection remains disabled.

### Observed PC24 melee-frame arithmetic

A widened original-instruction matrix exposed a precision-domain mistake in the
shared timing kernel. The previous implementation matches all 9,720 027F/037F
control updates, but fails the newly sampled 007F path. For example, with speed
and frequency 0.7 and duration 0.1, its second offset store is -0.1300000101327896;
the original 007F result is -0.12999999523162842. Historical wider-control passes
remain valid for their declared context; they do not establish the game-frame
control word.

An isolated original-game run now supplies that missing observation. Four
normal-input phases cover unarmed and vanilla Rusty Iron Shortsword swings in
first/third camera modes. Each phase records four ordinary-frame and four
sequence-manager entries for both actual Player body and first-person owners:
64 entries, all with control word 007F. These are four gameplay phases, not 64
independent trials. Player/form/group/pointer identity and executable instruction
bytes are checked. The first attempt assumed the wrong camera-byte mapping and
recorded no samples; its interrupted/detached debugger artifacts remain failed.
A live byte inspection and displayed camera establish 0 first/1 third before the
corrected observations. Normal camera switching uses a short R press.

The weapon cases use logged console add/equip setup before normal mouse swings;
no Health/Fatigue/condition/RNG/clock/CW values or contact calls are forced.
Debugger breakpoints affect scheduling, so the observations do not establish
uninterrupted timing. Every debugger detaches, the game and owned Steam exit 0,
the private relevant inputs restore exactly, and source prefix/pristine save
hashes remain unchanged. Original observations are separate from the fork's
structural/compiler/runtime layers.

The timing helper now rounds speed*duration, its subtraction, and manager
frequency*delta before subsequent operations, matching PC24 register arithmetic.
It changes neither host FPU state nor save schema. Signed-zero initialization,
anchor stores, one-phase ordering and validation remain in place. The compiled
production implementation matches 4,860 ordinary and 4,860 power frame updates,
including clock/phase/state and every timing store, plus 2,048 scalar offset
cases. The composed oracle supplies isolated slot 3/null-sync clamp metadata and
empty controllers; original activation/frame/manager instructions execute without
called-function stubs. Full actor/contact/culling/geometry scheduling is separate.

Two component tests retain 30 independently emitted frame-history goldens. Four
prior speed-offset goldens are replaced by directly executed 007F values after
observing the actual control word. Full normal component checks pass 2,967 tests;
selected ESM4* sanitizer checks pass 820, and full engine checks pass 1,769 in each
profile, with inventories matching and no skips/failures. Actual sanitizer unit
epochs/options are captured separately: the component helper sets leak0 and
UBSANhalt, while engine tests inherit explicit halt/stacktrace settings. Leak
checking remains disabled.

A fresh normal/sanitizer by first/third matrix exercises nonbinary speed 0.7,
stored as 0.699999988079071, using the same activation-controlled opponent and
zero Fatigue regeneration. The editable fixture recipe is preserved as source;
only SCTX and WEAP speed are rebuilt/reinspected, with saved local layout and
NPC/style preserved. Actual SCTX compilation/world use and all eight fresh
process/scene/save/reference/resource/RNG checks pass independently. Both real
blocked contacts retain Player body/FP BlockHit, NPC Recoil and its retiring
strike. Reload recovers naturally and cancels new action 4 before another
contact. No prior hit, sound or selection replays. The frozen two-voice PCM
assay passes two positive clusters and zero continuation clusters in every
case. The original single-asset method also passes two positive and zero
continuation clusters in each case; its earlier failed runs remain failed.

All eight images are directly reviewed, including ready poses after both Player
owners and the NPC recover. Static views do not establish original motion/camera
or held-block skin parity. The configured 60 FPS cap is not measured 60 FPS. The
fixture source is added after frozen build checks; compiled source bytes remain
identical to the checked checkpoint. The unchanged 351-test Python suite and assay
implementation are reused evidence rather than a new Python run.

[`M15-MELEE-PC24-TIMING-VERIFICATION.json`](M15-MELEE-PC24-TIMING-VERIFICATION.json)
pins original observations/cleanup, retained failures, primary/wider oracle
domains, source/build/actual sanitizer identities, editable inputs and every
fresh runtime layer. This is a shared timing correction, not Player FP strike or
parallel-owner implementation. Remaining reactions/mastery/effects, creature and
power branches, and full input/FPS/cancellation/restart acceptance keep M15S4
incomplete.

### Normal Player action routing and body phase authority

The generic actor dispatcher skips Player camera byte 0, but that isolated
prefix does not establish first-person contact authority. Four original-game
normal-input phases now identify the actual normal caller: return address
`672E12`. The caller temporarily writes camera byte 1 around `5FCAB0`, then
restores the saved byte. Read-only mapped-code verification and a 48-context
original-instruction caller matrix confirm that sequence. The full generic
method is an explicit boundary in this caller matrix; the separate 48-context
generic-prefix probe executes the real vtables/getters to select body data.

Unarmed and vanilla sword swings in first/third cameras produce four ordinary
attack-cost entries, all selecting body animation data with body phase 1.
Two entries have first-person phase 0 at the same point. Separate normal frame
observations identify both owners and the actual outside camera. The 76 total
debugger rows are four reloaded gameplay phases in one process, not independent
trials. Console add/equip is setup only; no AV, condition, clock, RNG, CW or
register writes force an outcome. These air/near-wall swings do not establish
native target damage, hit acquisition or complete contact/replay parity.

All debuggers detach, the game and owned Steam exit zero, private inputs restore
exactly, and source inputs/pristine save remain unchanged. The initial Steam
startup error and unavailable `xset` input utility remain recorded failures.
No new compiler or fork-runtime acceptance is claimed for this evidence-only
change. [`M15-PLAYER-ACTION-ROUTING-VERIFICATION.json`](M15-PLAYER-ACTION-ROUTING-VERIFICATION.json)
pins the editable probe, observations, instructions, boundaries and cleanup.
This resolves the normal body-authority question before separate first-person
strike ownership; it does not implement that ownership or complete M15S4.

### Independent Player strike owners and captured activation

Schema 58 retains separate body and first-person strike phase, raw time,
sequence timing, captured pose and retiring sequences around one body-owned
action/contact. Fresh captured activation stores offset positive zero before
anchoring; distinct steady outgoing owners crossfade independently. Queued
replacement consumes an abandoned pending action once. A first-person End marker
lasts until body release, and body-first completion permits independent
first-person recovery. Version 57 migration invents no strike or capture.
Renderer event tracks and sequence layers bind each view at its durable time;
prepare/commit rejects stale or mismatched paired histories atomically.

Actual original owned-root activation and frame/manager instructions establish
captured initialization independently. Two 162-context matrices cover 3,240
ordinary and 3,240 power updates, across both views, three clock origins, speeds,
frequencies and durations at CW 007F. Compiled production agrees exactly with
these and the two prior 4,860-update controls: 16,200 frame updates. Supplied key
arrays, slot 3, constant transforms and declared heap/import/scene boundaries
limit this proof; full actor/contact/culling and original visual parity remain
separate. The initial signed-zero counterexample is retained, rather than
relabelled a pass. Component tests retain 120 independently emitted rows.

Frozen normal checks pass 2,971 component tests, 1,772 engine tests and 353
Python tests. Sanitized ESM4* checks pass 824 tests and the full sanitized engine
inventory passes 1,772. Exact inventories match with no skips or failures. Actual
sanitizer process identities and options are recorded; leak checking is disabled.
Twelve fresh C++/Python payloads agree in complete binary and canonical JSON,
including old reactions, paired heads, partial captured activation and outgoing
history. The interrupted first build and second attempt's incorrect test
aggregate initializer remain failed evidence.

Two exploratory normal-input pilots remain explicitly separate from acceptance.
The first failed before input because its schema 49 save lacked required native
process state. The second loads known idle schema 57 state, unequips the Player's
sword and restrains three nearby NPCs without resource/inventory-content/RNG
writes. It saves both power action 3 heads with outgoing ordinary action 2 in
each view. It fails an inherited expectation that ordinary 2 should contact:
held input replaces that action before its hit. A fresh-process continuation
passes its declared restore/one-contact/recovery checks, retains RNG 505908858,
and empties the pending ledger. Both images are directly reviewed. Sound is
disabled; there is no final same-process save comparison or asymmetric runtime
matrix yet. These pilots do not close the runtime gate.

[`M15-PLAYER-OWNED-STRIKE-VERIFICATION.json`](M15-PLAYER-OWNED-STRIKE-VERIFICATION.json)
pins sources, exact checks, independent probes, failures and exploratory runtime
layers. The editable probe is retained under `scripts/data/oblivion_compat`.
Asymmetric runtime restarts, Player attacker Recoil, parallel Stagger,
mastery/effect/creature branches and the wider input/FPS/cancellation matrix keep
M15S4 incomplete.

The editable Player strike fixture now reproduces a selected view's four-second
left/right/power holds and a zero-time `blend:30` key, making independent recovery
and outgoing retirement observable. The generator rejects foreign identities,
paths, hashes and malformed layouts before creating output, and refuses existing
output. Three synthetic tests exercise both views and failure paths without game
bytes in Git; all 356 Python tests pass. Independent byte comparisons of all six
actual generated files preserve everything outside the declared two time stores,
key count and inserted key. Strict NIF reading accepts all six files with zero
failures. These are structural/readback checks; runtime acceptance remains open.
[`M15-PLAYER-STRIKE-DURATION-FIXTURE-VERIFICATION.json`](M15-PLAYER-STRIKE-DURATION-FIXTURE-VERIFICATION.json)
pins the source, extraction, Python inventory and byte/NIF evidence. Engine source
and the prior tested runtime binaries are unchanged.

### Normal asymmetric strike-tail restart acceptance

Four fresh normal courses now exercise both ordinary strike finish orders in
first/third cameras. Each baseline uses 16 fixed normal quicksave observations
and a final settled save. The first completed save satisfying the preregistered
tail predicate is selected, without choosing by damage or retrying numeric
outcomes. Its exact disk hash is preserved alongside independently serialized
live state; a fresh process resumes that save without attack input.

When the body is longer, the first-person head reaches End while body contact 2
is pending. The restart commits exactly one body contact. When first-person is
longer, the body has completed and consumed contact 2 while the captured
first-person head remains active. That restart commits no contact. In both
cases, final native actor values, life state, bases, ledger, RNG, bow state and
loose weapons match the same-process continuation exactly. All eight processes
exit successfully; all 72 completed save/live pairs match. Action history settles
to next3/pending[], RNG505908858, and both view heads recover. Actual ObScript
compilation has zero failures, and observation streams end without errors or
unsupported commands. Eight settled images are directly reviewed. Third-person
uses normal `r` input with a read-only Lua camera observer.

An editable input recipe and staging helper reproduce the whole runtime input
save byte for byte (SHA2566182819ef8cbbeaa96f83d920def92ff2432aa093bc15605f965c81f7e3a02fd).
Only the declared weapon equipped slot and three restrained flags change from
the pinned idle schema57 source; all other decoded state is preserved through
schema58 promotion. A fresh full Python check passes356 tests. Canonical public
manifests parameterize fixture paths; concrete executed manifests remain pinned.
The initial normal first-camera courses predate the extra read-only camera Lua
observer included in the public manifests.

[`M15-PLAYER-STRIKE-TAIL-NORMAL-VERIFICATION.json`](M15-PLAYER-STRIKE-TAIL-NORMAL-VERIFICATION.json)
pins every process, input, selection, save/live verification, compiler layer and
image. The baseline's scheduling includes its normal saves; this is not original
uninterrupted motion parity. Sound is disabled, and the configured60 FPS cap is
not measured60 FPS. Sanitized runtime courses remain a separate running matrix.
Queued power, mixed captured/outgoing runtime transitions, Player Recoil,
parallel Stagger and the broader milestone gates remain open. M15S4 is incomplete.

### Resolved Player reaction resource selection

A new immutable selector models the Player caller's separate resource choices:
active-camera preflight, body playback, then first-person Multiple basename
matching or fallback selection. It preserves explicit Single/Multiple identity,
including Multiple with one entry, and returns indices, match status, final CRT
state and draw count. Single first-person collections take their direct fallback;
Multiple matches consume no extra RNG. Both resource groups must already be
resolved and nonempty; group/family/movement fallback and loading remain outside
this helper. No World/contact factory uses the new helper yet.

Compiled production agrees with840 preserved original full-helper observations
across both cameras, mixed cache kinds, partial first-person variant lists, five
seeds and CW007F/027F/037F. The280 CW007F contexts are fresh executions of the
pinned original helper harnesses. Actual Player getters, helpers, filename matcher
and initialized CRT execute within the declared supplied cache/hash/list,
scene/TLS/empty-controller/atomic boundaries. The initial interpreter lacking
Unicorn remains a failed import attempt. Sixty distinct original input/output
rows become component goldens. Normal component checks pass2973 tests; sanitized
ESM4* checks pass826, with exact inventories and no failures/skips. The actual
sanitized process options are recorded; leak checking is disabled.

[`M15-PLAYER-REACTION-RESOURCE-VERIFICATION.json`](M15-PLAYER-REACTION-RESOURCE-VERIFICATION.json)
pins these sources, original corpora, independent comparison and component checks.
The existing strike runtime matrix remains pinned to its earlier unchanged engine
binaries. Player Recoil integration, paired reaction replacement and normal
blocked-contact acceptance remain open, as do the wider M15S4 requirements.

### Sanitized asymmetric strike-tail restart acceptance

The sanitized counterpart now passes all four declared ordinary strike-tail
courses: body/first-person finish orders in first/third cameras. Eight fresh
engine processes complete, and all72 preserved save/live pairs agree exactly.
Final native values, life, bases, ledger, RNG, bow and loose-weapon state match
the respective same-process baseline. An additional audit verifies complete
Player and target inventories, including condition, charge and equipped slots,
across all eight normal/sanitized courses. All16 settled images are directly
reviewed. Actual instrumented process environments record ASan halt-on-error,
disabled leak checking and UBSan halt-on-error with stack traces. Each original
process epoch has exited; the tested sanitized binary is separately preserved.

The first sanitized attempt exceeded startup/input deadlines before attack and
remains failed evidence. Successful02 courses increase those deadlines without
changing input predicates or numerical expectations. Across both profiles,
16processes supply144 exact completed save/live pairs. The configured60 cap is
not measured FPS; sound is disabled. These historical binaries precede the
subsequent paired contact-reaction integration.

[`M15-PLAYER-STRIKE-TAIL-SANITIZED-VERIFICATION.json`](M15-PLAYER-STRIKE-TAIL-SANITIZED-VERIFICATION.json)
pins every sanitized course, actual process environment, preserved binary,
selection, restart, image audit and the retained timeout failure. This closes
the declared ordinary asymmetric strike-tail runtime matrix. Queued power,
mixed capture/outgoing transitions, paired reactions and the wider M15S4 gates
remain separate requirements. M15S4 remains incomplete.

### Paired Player contact reaction integration

Player BlockHit/Recoil now replace the prior body and first-person owners
independently. A steady distinct source retains its exact outgoing timing and
cause; an easing, captured, same-cached or inactive source uses pose capture.
Existing detached history survives. Stale clocks, foreign causes and colliding
history reject before mutation. The body owns process6/7 and contact consumption;
first-person recovery stays independent. Fresh captured reactions retain requested
offset+0 before manager anchors, including NPC captures; legacy absent-offset
saves remain valid in both C++ and Python. The World factory now uses the native
Player resource selector and actual active camera. The melee caller also admits
Player attacker Recoil for intact, positively absorbed blocks.

Original instruction evidence supplies840 resource-selection observations and
separate96-context/672-sample steady and easing owned-root replacement corpora.
Those supplied scene/cache/controller boundaries do not establish full original
gameplay parity. New engine tests cover independently captured/steady/ended
heads, prior history, opposite finish orders, exact source preservation, stale
packet atomicity, aggregate RNG and saved recovery. Full normal and sanitized
engine inventories each pass1775 tests. Normal components pass2974, sanitized
ESM4*827, and Python357; unchanged passing component/Python sources are reused.
Fifteen C++/Python wire payloads agree exactly. Actual sanitizer options are
recorded; leak checking is disabled.

Two normal runtime courses now pass first/third-camera blocked Player contacts
and in-flight fresh-process recovery. All six completed disk/live pairs agree;
final values, life, bases, full inventories/conditions, ledger, RNG, bow and loose
weapons match same-process continuation exactly. Both Player Recoil roots and
the NPC captured BlockHit recover, with one baseline contact and zero replayed
contacts or selections. Six images are directly reviewed. The opponent fixture
sets normal stance/movement/block controls in Lua; production blocking AI is not
covered. Contact numbers are a pinned fork regression baseline, not an original
numerical oracle. Sound is disabled.

The retained failed pilot exposed a second Player exclusion upstream of the
factory. The corrected normal runtime reaches process7 without changing damage,
fatigue, absorption or RNG expectations. Earlier fixture/API mistakes and two
interrupted sanitizer builds remain failed evidence. Editable baseline,
third-camera and continuation manifests plus course metadata are versioned.

[`M15-PLAYER-PAIRED-REACTION-VERIFICATION.json`](M15-PLAYER-PAIRED-REACTION-VERIFICATION.json)
pins sources, checks, native boundaries, wire proof, normal courses and failures.
Sanitized runtime remains a separate running course. Missing-resource/zero-blend
contracts, legacy body-only history, breaking/fatal contacts, blocking AI,
queued-power/mixed runtime transitions, parallel Stagger, mastery/effects,
creatures and the wider input/FPS/cancellation matrix keep M15S4 incomplete.

### Sanitized paired Player Recoil runtime acceptance

The same ordinary blocked Player contact now passes both sanitizer camera
courses and their in-flight fresh-process continuations. All four instrumented
engine processes exit, all six completed save/live pairs agree exactly, and both
Player Recoil views plus the NPC captured BlockHit recover without replaying
contact2. Actual process environments record ASan/UBSan halt-on-error with leak
checking disabled. Six images are directly reviewed.

An additional audit matches preregistered executable hashes, preserved restart
input hashes, event PID/epochs, actual environment epochs and unchanged production
source files for all four normal/sanitized courses. Final native actor values,
life, bases, ledger, RNG, bow and loose-weapon state plus complete Player/target
inventories also agree exactly across all four camera/profile baselines. The
combined matrix has eight engine processes and twelve exact saved/live pairs.

[`M15-PLAYER-RECOIL-SANITIZED-VERIFICATION.json`](M15-PLAYER-RECOIL-SANITIZED-VERIFICATION.json)
pins these additional layers. Sound remains disabled for this matrix; separate
sound-enabled courses are being checked. The stationary scripted block controls
do not establish production blocking AI. Missing-resource/zero-blend contracts,
other contact branches, queued-power/mixed transitions, parallel Stagger and the
wider M15S4 requirements remain open.

### Paired Player Recoil contact audio acceptance

Sound-enabled normal first/third-camera counterparts now pass the same exact
state and restart checks. Each baseline plays one WPNBlockHand and one
WPNBlockBlade request. Captured OpenAL mixer PCM contains exactly one joint stock
Blade+Hand mixture, using the previously accepted method and unchanged 0.75
correlation, 0.2 positive component-gain fraction and 25 ms relative-start limits.
Each fresh-process continuation has zero contact requests and zero mixtures.
No audio-based save selection or numerical retry is used. Four processes exit,
six completed save/live pairs match, and six images are directly reviewed.

The new editable audio recipe transforms the existing three public recoil
manifests into valid sound-enabled scenarios and records stock asset identities,
static gains, method and exact criteria. Concrete executions and analyzer source
remain pinned under ignored evidence. This accepts actual mixer output for this
contact; it does not claim hardware listening, original pitch/reverb parity or
the entire milestone sound matrix.

[`M15-PLAYER-RECOIL-AUDIO-VERIFICATION.json`](M15-PLAYER-RECOIL-AUDIO-VERIFICATION.json)
pins those layers. Production sources and previously accepted normal/sanitizer
checks are unchanged. Other contacts, parallel Stagger, mastery/effects,
creatures and the wider input/FPS/cancellation gates keep M15S4 incomplete.

### Fresh Player BlockHit regression after paired factory integration

A normal first-camera regression now passes two fresh paired Player BlockHits,
an in-flight save and fresh-process recovery through the new factory. Production
NPC Recoil separates the attacks; the fixture's explicit StopCombat cancels its
second reaction. All three completed disk/live states agree exactly. Final
native arrays and complete inventories agree with the same-process baseline,
including exact PC24 Health damage -1.9427038431167603, Player/NPC weapon
conditions 55.826988220214844/55.400001525878906, empty next4 ledger and RNG773150046.
All 36,289 original reference inventories remain unchanged except declared target
weapon wear. Player process action returns to -1 after release and recovery.
Two baseline contacts become zero replayed contacts/selections; both processes
exit and three images are directly reviewed.

The first reused harness expected action-1 during held block; the second added
an impossible natural-NPC-recovery wait after StopCombat. Both are manually
interrupted through their owned scenario deadline handler and retain failed
reports. A third structural pass used an obsolete pre-PC24 Health expectation;
that numerical criterion remains failed. The fourth course declares the already
independently proven original007F PC24 result before launch. No production
arithmetic or tolerance changes are made. New NPC Recoil also adds two native
resource draws per contact; its final state is independently preregistered from
the CRT recurrence, rather than reusing the obsolete no-Recoil RNG baseline.

[`M15-PLAYER-FRESH-BLOCK-REGRESSION-VERIFICATION.json`](M15-PLAYER-FRESH-BLOCK-REGRESSION-VERIFICATION.json)
pins source input, strict state/inventory checks, independent Health proof,
editable manifests, images and failures. This is fresh activation, not runtime
same-cached replacement. Historical source57 staging is identified explicitly;
source regeneration with the current schema requires a separate fixture update.
Sound is disabled. Other reaction branches and the wider M15S4 gates remain open.

### Current-schema cached-block input regeneration

The narrow cached-block idle staging helper had become unusable after schema58:
it required both CURRENT_VERSION and the literal57. The recipe and explicit
supported target now agree on58. The original46 source, pinned hash, script
identity/locals, output-existence checks and full semantic reinspection remain
strict. No general script/save rebasing is added.

Three synthetic tests use actual binary saves to verify opaque-record/source
preservation and unchanged values/inventory/RNG, plus stale hash, foreign
schema/identity, nonidle locals and existing-output rejection. The first success
assertion omitted the declared empty inventory extra_scale default; that failed
assertion is retained and corrected in the test. The full Python suite passes
360 tests with frozen sources. Actual staging of the pinned source succeeds;
independent comparison to the historical57 idle input differs only at the
normalized schema tag. This proves structural writing and semantic reinspection;
compiler/world and current-input runtime acceptance remain separate layers.

[`M15-CACHED-BLOCK-INPUT58-VERIFICATION.json`](M15-CACHED-BLOCK-INPUT58-VERIFICATION.json)
pins those sources and checks. C++ production is unchanged. Two generated
four-second BlockHit holds separately pass strict NIF reading and independent
byte checks; runtime cached replacement with those assets is the next case.

### Join renderer threads before Engine teardown

A cached-contact course completed its actions and saves but crashed during
shutdown. Its raw process log and preserved crash stack show signal11 at nil
inside osg::GLBufferObject::compileBuffer on an active graphics draw thread while
the main thread destroys World/OblivionScriptManager. The crash catcher's later
SIGKILL explains the initially recorded exit-9; the earlier unknown-kill diagnosis
is superseded. Cgroup counters did not establish an OOM event.

Engine destruction now stops viewer threading before releasing screen-capture
and game managers. Both normal and ASan/UBSan applications rebuild with frozen
source fingerprints. Headless test binaries do not link the changed destructor;
their unchanged complete 1775-test inventories are reused for unchanged code.
Actual applications verify the new lifecycle behavior.

An explicit DrawThreadPerContext normal first-camera cached replacement and
in-flight continuation pass exact native state, inventories, PC24 Health,
RNG773150046 and no-replay checks. Both processes record final thread stop after
last thread start before peaceful exit. Three further Morrowind boot/save/quit
courses pass normal SingleThreaded, normal DrawThreadPerContext and sanitized
DrawThreadPerContext. Each produces one actual TES3 save without native state
markers. All five accepted engine processes exit0; three native save/live pairs
match. Actual environment epochs are retained and six images directly reviewed.
The legacy images show the existing terrain/sky/HUD view; this is narrow profile
isolation, not city traversal or full Morrowind gameplay. The OSG model and stop
semantics are identified against its installed3.6.5
[ViewerBase source](https://raw.githubusercontent.com/openscenegraph/OpenSceneGraph/OpenSceneGraph-3.6.5/src/osgViewer/ViewerBase.cpp).

[`M15-RENDER-THREAD-SHUTDOWN-VERIFICATION.json`](M15-RENDER-THREAD-SHUTDOWN-VERIFICATION.json)
pins the fault, preserved failed binary, builds, runtime checks and limitations.
A separate corrected third-camera baseline exits0 but its first saved captured
poses have already finished the stock0.1s fade. That capture-restart criterion
remains failed; a longer declared fixture blend is being prepared. The full
cached replacement camera/sanitizer matrix and wider M15S4 gates remain open.

### Observable cached BlockHit capture fixture

A new narrow generator reproduces both stock BlockHit clips with four-second
StopTime/End holds and the existing Blend modifier changed from0 to30 frames.
The modifier's original timestamp bits0x3d088880, key count, spelling, byte length,
controller/transform data and all opaque bytes stay intact. Independent comparison
finds exactly nine changed bytes per view: eight in the two float stores and one
in the existing modifier text. Strict NIF reading accepts both actual files with
zero failures and an explicit validator exit0.

Two synthetic tests verify reversible whole-file preservation, both-view
prevalidation and rejection of foreign fields, hashes, offsets, forged-hash
modifier shapes and existing output. The full Python suite passes362 tests with
frozen sources. The first wrapper tried the attack-key inserter, whose existing
Blend guard correctly refused actual BlockHit inputs before output creation.
That failure is retained; the synthetic layout now models the actual existing
modifier rather than an unrelated middle key.

[`M15-CACHED-BLOCK-HOLD-FIXTURE-VERIFICATION.json`](M15-CACHED-BLOCK-HOLD-FIXTURE-VERIFICATION.json)
pins editable recipe/helper, exact checks, raw-byte proof and strict reading.
This closes structural writing and readback only. Compiler/world and captured
restart acceptance remain separate. The one-second blend provides the declared
observation window for the next cached normal/sanitized camera courses without
changing contact, damage, wear, inventory or RNG expectations.

### Reproduce strike-tail input through current-schema staging

The unarmed strike-tail stage now accepts the two reviewed idle source layouts,
57 and58, while retaining its exact recipe/hash, idle action/owner, equipped
weapon and three restrained-actor guards. The default recipe consumes the current
cached idle58 output, and the report records the actual declared source schema.
The initial current58-source rejection remains retained evidence.

Both the current58 and historical57 paths now produce exactly the same whole
file as the accepted runtime input, SHA2566182819ef8cbbeaa96f83d920def92ff2432aa093bc15605f965c81f7e3a02fd.
Every byte matches; all other decoded state remains preserved. Full Python checks
pass362 tests with frozen sources. Existing normal/sanitized strike-tail courses
remain the runtime evidence for those identical input bytes; no new gameplay
acceptance is inferred from regeneration alone.

[`M15-PLAYER-STRIKE-TAIL-INPUT58-VERIFICATION.json`](M15-PLAYER-STRIKE-TAIL-INPUT58-VERIFICATION.json)
pins the complete current46->58->unarmed58 chain, compatibility branch, exact
byte proof and checks. C++ production and animation assets are unchanged.

### Paired cached BlockHit matrix: state and lifecycle pass, visual gate fails

The reviewed four-second holds and existing one-second Blend modifier now admit
same-cached replacement and a captured restart in normal and ASan/UBSan builds,
in both cameras. All eight engine processes exit successfully. Twelve complete
save/live pairs match exactly; the first post-contact3 save retains both captured
heads (64 body and66 first-person tracks), with no retry or numeric/image-based
selection. Both cause2 heads remain until cause3 replacement. Two production NPC
contacts produce the independently fixed PC24 Health damage, declared weapon
wear and RNG773150046. All36289 original reference inventories and complete
Player inventories match their declared changes. Fresh processes recover both
Player heads without new selections or contacts; final resource/action arrays
match their own settled baseline exactly. Actual DrawThreadPerContext epochs
record final thread stop before peaceful exit, including both sanitized cameras.

Direct review of twelve images finds a separate failure: large orange polygons
obscure the first-person contact view in both builds. Final ready poses recover
in both processes and cameras; third-person contact stills show raised guards.
Finite saved locals and near-unit captured quaternions do not identify the
rendering cause. No arithmetic change is justified by these images. First-person
body attachment, skinning and camera investigation remains open; visual acceptance
is explicitly false. Sound is disabled,60FPS is only a configured cap and leak
checking is disabled.

[`M15-PLAYER-PAIRED-CACHED-BLOCK-RUNTIME-VERIFICATION.json`](M15-PLAYER-PAIRED-CACHED-BLOCK-RUNTIME-VERIFICATION.json)
pins every course, process, manifest, restart selection, verifier and image.
The editable acceptance contract records exact actions and separate state,
lifecycle and visual gates. Structural fixture reading, runtime state/restart
acceptance and failed visual acceptance remain distinct. Full M15S4 is incomplete.

### Native first-person torso exclusion

The large contact-view polygons came from the attached UpperBody torso shape.
A diagnostic overlay changes only that shape's native hidden flag, preserving
Arms and all animation data. Its two save/live pairs, exact native resource
arrays, complete Player inventory and all36289 original reference inventories
pass; the foreground polygons disappear. A broader cross-course comparison of
all final streamed reference keys failed and is retained: the courses loaded
different additional references, while shared inventories and declared original
inventories agree. That comparison is separate from the contact acceptance gate.

Independent execution of the hash-identified original47B090 geometry binding
function supplies the production rule. Its first-person branch compares geometry
names against the nine-byte UpperBody prefix case-insensitively, using the actual
original CRT implementation, and removes matching shapes. Twenty supplied
geometry cases across both view flags include case variants, longer prefixes,
short names, Arms/Hand and nonprefix/Tri names. Scene allocation/child removal,
empty skin-array maintenance, absent extra-data queries and reference operations
are declared boundaries; this is original rule evidence, not original gameplay.

NpcAnimation now applies that filter to attached first-person instances before
FaceGen registration. It removes ordinary and skinned drawables after traversal,
preserving bone nodes and transforms, source templates, other instances, Arms and
Hand geometry. Third-person bodies retain torso geometry. The passive first-person
peer uses the same first-person view flag while either camera is active.

Two tests exercise60 original-case/drawable-class comparisons and hidden nested
parents, bone preservation, idempotence and instance isolation. Full normal and
explicit ASan/UBSan engine checks each pass1777 tests from125 suites, with exact
inventories and no failures/skips. Leak checking is disabled. Initial focused
checks covered ordinary geometry; review caught the native skinned Drawable
wrappers before production acceptance, and the corrected full runs cover both.

The first stock-mesh normal baseline now shows the contact sword arm and opponent
without foreground torso polygons, followed by correct ready recovery. The full
fresh-process/camera/sanitizer runtime matrix is still running; those remaining
courses are not inferred from unit checks or the diagnostic overlay.
[`M15-FIRST-PERSON-TORSO-FILTER-VERIFICATION.json`](M15-FIRST-PERSON-TORSO-FILTER-VERIFICATION.json)
pins the native probe, diagnostic, implementation, full checks and bounded image
review. Full M15S4 remains incomplete.

### Stock-mesh torso-filter runtime matrix

The production filter now passes the complete bounded cached BlockHit matrix:
normal and ASan/UBSan, first and third cameras, baseline plus captured fresh-process
continuation. All eight processes exit0; twelve complete save/live pairs match.
Every first completed cause3 save retains64 body and66 first-person pose tracks.
Exact preregistered PC24 Health damage, weapon wear, all36289 original reference
inventories, complete Player inventory, RNG773150046 and ledger next4/pending[]
pass. Fresh loads recover both Player heads without new selections or contacts.
Final native resource/action arrays and complete Player inventories also agree
across all four profile/camera courses. Each actual threaded environment records
last thread stop before peaceful exit; sanitizer options are observed in-process.

Direct review of all twelve images confirms that the previous first-person
foreground torso polygons are absent in both builds, the sword arm/opponent are
visible, third-person torso/guard presentation remains present, and both baseline
and restart return to the ready pose. Stock body meshes are used: the diagnostic
hidden-torso overlay is absent from every manifest. Synthetic BlockHit timing
holds remain declared. This closes that specific foreground-occlusion defect,
not full original motion, lighting or character rendering parity. Audio, measured
FPS and leak coverage are absent.

[`M15-FIRST-PERSON-TORSO-RUNTIME-VERIFICATION.json`](M15-FIRST-PERSON-TORSO-RUNTIME-VERIFICATION.json)
pins the four courses, all process environments, byte-identical pristine inputs,
first-save selection, verifiers and image hashes. It supplements the earlier
implementation receipt's then-pending runtime matrix. The historical failed
visual matrix remains unchanged. Full M15S4 remains incomplete.

### Parallel Stagger ownership: native slot and dispatcher evidence

Stagger selects native slot0; ordinary strikes, BlockHit and Recoil use slot3.
An expanded original-helper corpus now covers128 contexts over prior actions
2/6/7/8, both cameras, two x87 controls, four distinct first-person clocks and
present/absent first-person slot3. The actual Stagger helper installs slot0,
changes process action to8 and retains each supplied old slot3 in state1 through
five actual manager updates. Prior slot3 protocols include ordinary strikes,
BlockHit and Recoil. Preserving only old strikes would miss required histories.
These are supplied foreign-root/empty-controller/cache contexts, not gameplay.

A separate32-context probe composes the actual Stagger helper with the original
5FCBEE action switch through its real process getter and jump table. Even with
old ordinary slot3/phase1 retained, process action8 reaches5FD7DE; a manual raw
action2 control reaches the ordinary branch5FCC05. The probe stops at those
branch entries. It proves the dispatcher distinction, not execution of an entire
frame, a damage callback, automatic recovery or complete cancellation semantics.

The service currently has one primary strike/reaction owner and no independent
active slot3 history beside Stagger. Both body and first-person state require a
representation that separates resident visual protocols from process/contact
ownership; retiring/ease-out history alone does not model a retained state1 slot.
[`M15-STAGGER-PARALLEL-SLOT-VERIFICATION.json`](M15-STAGGER-PARALLEL-SLOT-VERIFICATION.json)
pins the independent probes and concrete source gaps. No production schema or
parallel-Stagger acceptance is claimed by this checkpoint. M15S4 remains open.

### Passive slot3 schema59 contract

The save contract now separates a resident slot3 protocol from its former
process/contact authority. Each body and first-person view can retain an optional
passive strike or BlockHit/Recoil record, including its complete timing, phase,
sampled time and captured pose. Stagger remains the primary slot0 reaction;
passive slot3 can remain after that primary owner recovers. A passive cause must
already be consumed/canceled and cannot be pending. It grants no new action.

Schema59 appends explicit absent/strike/reaction tags after the existing melee
record, with the first-person tag present only when that view exists. Earlier
versions retain their exact layout and JSON key set. Migration adds absent
passive defaults without inventing history. Validation rejects future/pending
causes, absent clocks, inactive/dead owners, untimed strikes, Stagger in slot3,
primary slot3 conflicts, premature inactive retirement, bow conflicts, bad tags
and unsupported legacy fields.

Full normal checks pass2976 component and364 Python tests; the separate ASan/UBSan
ESM4 selection passes829 cases, all with exact inventories and frozen sources.
Thirty complete C++ payloads and JSON states independently decode, validate and
reencode byte for byte in Python. They cover ordinary, BlockHit, Recoil, captured
strike and captured reaction, either/both views, and active/recovered Stagger.
Initial helper-promotion and binary32 fixture errors are retained; the final
checks include the corrected migration boundaries and captured variants.

The editable cached-idle and strike-tail recipes now target59. Actual current
46->59->unarmed59 and historical57/58->unarmed59 paths produce byte-identical
strike-tail saves, preserving all other decoded resources/inventories apart
from the declared fixture changes and empty defaults. This is structural write
and semantic reinspection, not compiler/world or runtime acceptance.

[`M15-PASSIVE-SLOT3-SCHEMA59-VERIFICATION.json`](M15-PASSIVE-SLOT3-SCHEMA59-VERIFICATION.json)
pins the contract, checks, wire proof, regenerated inputs and remaining work.
Service contact publication, passive clock/renderer advancement, replacement,
cancellation and actual schema59 engine/runtime acceptance remain to be wired.
The successful torso-filter matrix used schema58 applications. Full M15S4 is
still incomplete; this checkpoint establishes its needed persisted state model.

### Stagger contact publication and passive service frames

The service now publishes a fresh Stagger slot0/action8 while preserving the
previous slot3 protocol in each retained view. It consumes the contacting
strike once or cancels a different pending prior body strike once. Body and
first-person clocks, timing, sampled times and captured poses remain independent.
Passive records cannot reacquire contact authority. Their frames run controller
manager timing without ordinary phase/speed correction or reaction Start/End
dispatch. Resident state1 records remain after primary Stagger recovery; an
explicit actor cancellation removes them. Service capture requires schema59.

The new typed world/service matrix covers16 contexts: contacting/interrupted
ordinary strikes, prior BlockHit/Recoil, body-only/paired Player views and
steady/captured playback. It rejects malformed and stale requests atomically,
checks immediate binary restart plus128 frame/restart checkpoints, keeps passive
phases and flags unchanged beyond End, and activates a fresh recovered slot0
without erasing slot3. The existing NPC transaction fixture now retains native
victim timing and asserts passive ownership. Stagger no longer uses the old
slot3 outgoing-fade fixture.

Full normal and ASan/UBSan engine runs each pass1778 tests across125 suites,
with exact inventories, no failures and no skips, at source fingerprint
`b59c76e255b1a73f39c9a63c297cb13571a8869acc4ef22aaf987daf2ce0a9d8`.
Leak detection is disabled. The initial test-macro compile failure, legacy
fixture clock-order failure and deliberately stopped sanitizer build remain
recorded; the final03 runs use the corrected frozen source.

[`M15-STAGGER-PASSIVE-SERVICE-VERIFICATION.json`](M15-STAGGER-PASSIVE-SERVICE-VERIFICATION.json)
pins these checks and rebuilt schema59 applications. These are headless engine
and semantic restart checks, not actual game sessions. Character rendering
still needs passive group/pose projection; the world resource factory still
rejects Stagger. Active/cached slot0 replacement, passive slot3 re-admission and
the full input/runtime/E2E matrix remain open. M15S4 is not complete.

### Passive slot3 projection and explicit visual cancellation

Character frames now collect and project passive body/first-person groups,
including after primary Stagger recovery. The new renderer adapter uses the
retained sampled time, each view's resource weight and bone priorities, easing
or retirement interval, and captured pose. It creates no event playback owner.
Captured pose bindings remain stable during easing and can be rebuilt from the
saved values. Explicit combat-input cancellation releases passive and outgoing
history through the renamed `clearMeleeVisualHistory` operation, then clears
both renderer pose bindings so the next frame cannot resurrect those slots.

Three new renderer tests cover ordinary/power strikes and BlockHit/Recoil,
independent view times/priorities, Stagger coexistence and removal, suppression
of playback/text-key callbacks, exact original captured-pose interpolation
goldens,20 fresh renderer rebindings and stable controller identities. Negative
cases cover missing resources, invalid clocks/timing, unanchored activation or
retirement, and inactive binding release. The16-context service matrix also
checks the complete binary result of explicit visual-history cancellation.

Full normal and ASan/UBSan engine suites each pass1781 tests across125 suites,
with exact inventories, no failures and no skips, at source fingerprint
`efab9c1b9dbe680ba7842b6f4d99b02bcaef468f96a178a2dd440edca4749727`.
Leak detection is disabled. An initial negative fixture omitted its required
native timing offset; its1780/1781 diagnostic result and deliberately stopped
sanitizer build remain recorded. The corrected final02 checks use one frozen
source and require no production change to satisfy that fixture.

[`M15-STAGGER-PASSIVE-PROJECTION-VERIFICATION.json`](M15-STAGGER-PASSIVE-PROJECTION-VERIFICATION.json)
pins the source, applications and verification. These are headless bone/scene
and service checks. Actual rendered save/restart courses remain open, as do
slot0-aware world factory/resource handling, active/cached Stagger replacement,
passive slot3 re-admission and the full normal-input/E2E matrix. M15S4 remains
incomplete.

### Rendered passive slot3 save/restart matrix

Four actual application courses now retain ordinary slot3 beside an already
retiring Stagger slot0, save while both views own their respective records,
quit, load that exact save in a fresh process, and recover both Stagger views
while retaining passive slot3. Normal and ASan/UBSan builds each cover first
and third person: eight processes, eight complete save/live state comparisons,
and eight exact loaded melee-state/clock comparisons. All 36,762 reference
inventories in the pinned drawn source and the complete Player inventory remain
unchanged, as do native values/bases, RNG and the consumed action ledger apart
from the declared primary process-action recovery. Final native values/bases
and Player inventories agree across all four courses; whole final reference-set
equality is not claimed. Each process verifies actual DrawThreadPerContext and
stops its final render threads before peaceful exit.

The editable recipe, staging helper and scenario manifests live in `scripts/`.
The helper requires shared Player `DRAW=1`, preserves the source and all other
decoded state, and changes only the declared Player melee/action state and
camera bool variant. Camera variants differ by exactly one shared `CAM_/FIRS`
byte and produce identical staged files across builds. Five fixture tests cover
complete wire/opaque preservation, absent/idle Player entries, foreign recipes,
overwrite/replay rejection, holstered-source rejection and the camera byte.
The full Python suite passes 369 cases at frozen source fingerprint
`f7043356131aaa7b167870625151cd0d2282e60bc3a1a2e05b631741e812bbcd`.
The applications match the prior 1,781-case normal/instrumented engine checkpoint;
no production C++ changed during this fixture work.

Two setup diagnostics remain explicit. The first prototype was labelled third
person but actually first person, and its holstered source triggered the
existing cancellation guard after recovery. A normal F-draw/F5 course supplied
a drawn idle source instead; production cancellation was not changed. That
seed's original all-inventory-unchanged check found one NPC weapon condition
materialized from unset(-1) to56. Its failed comparison remains retained. The
final recipe pins the resulting source, and all four final courses preserve
every inventory in that source without exceptions.

All 16 pre/post-save active/recovered images were directly inspected. The
first-person foreground remains free of the torso obstruction, and the
third-person actor remains visible without exploding geometry. This is a
qualitative scene check, not original motion/lighting/appearance equivalence.
Sound is disabled, the 1-FPS cap is configured rather than measured, and leak
detection is disabled. The eight-second retirement is deliberately synthetic;
these courses exercise injected saved state, not resource selection, a native
retirement default, a normal contact or fresh Stagger activation.

[`M15-STAGGER-PASSIVE-RUNTIME-VERIFICATION.json`](M15-STAGGER-PASSIVE-RUNTIME-VERIFICATION.json)
pins the matrix, source recipe, binaries, helpers, retained diagnostics and
remaining gates. Structural write, semantic reinspection and actual runtime
world admission are recorded separately; no new plugin compilation is claimed.
Slot0-aware world factory/activation, active/cached Stagger replacement, passive
slot3 replacement by subsequent input, captured/other-protocol runtime crosses
and the complete normal-input/audio/FPS/cancellation/E2E matrix remain open.
M15S4 is still incomplete.

### Paired strike replacement of recovered passive slot3

Player strike preparation now queries the actual slot3 predecessor in each
view, including passive ordinary/power strikes and BlockHit/Recoil. Stagger
slot0 is excluded. Consumed/canceled passive causes retain visual residence
without pending contact authority, even when a former strike phase is End.
Primary first-person End and inactive/retiring reaction sources keep their
existing capture rules. Both the controller and service use this shared query.

Paired strike admission validates outgoing/captured activation against that
history, clears passive fields in its candidate, and allocates one new action
after validation. The old cause remains consumed; outgoing spans retain its
timing and sampled time. A body-only call without an activation packet rejects
a passive predecessor before allocating an action.

A 32-context typed world/service matrix covers four predecessor protocols,
four per-view capture masks and incoming ordinary/power strikes, including
same-cached power capture. It recovers both Stagger views through the service,
checks a fixed activation-state table and original saved predecessor history,
rejects stale/altered requests without binary changes, and verifies 128
frame/restart checkpoints, one new pending action, unchanged resources/RNG and
completion of the outgoing spans. A separate source-query test distinguishes
primary first-person End, explicit passive residence, retiring Recoil and
Stagger slot0.

Full normal and ASan/UBSan engine suites each pass 1,783 cases across 125 suites,
with exact inventories, no failures and no skips. Leak detection is disabled.
[`M15-PASSIVE-SLOT3-READMISSION-VERIFICATION.json`](M15-PASSIVE-SLOT3-READMISSION-VERIFICATION.json)
pins the frozen source, rebuilt applications, checks and warning triage. This
is service/engine verification, not yet actual normal-input re-admission.
Body-only activation, reaction re-admission, Stagger slot0 factory/activation
and the full M15S4 runtime/input/audio/FPS/cancellation/E2E gates remain open.

### Mouse admission after recovered passive slot3

The opt-in SDL replay bridge now delivers mouse button down/up/click events
and exposes their held mask to the ordinary input controller. It preserves
native coordinates and opt-out behavior; relative motion carries the held
mask. Real-SDL tests cover button/keyboard chords, queue ordering, partial
lines, invalid domains and exact receipts. All 374 Python tests pass at the
frozen source recorded in
[`M15-PASSIVE-SLOT3-MOUSE-VERIFICATION.json`](M15-PASSIVE-SLOT3-MOUSE-VERIFICATION.json).

One normal-build first-person rendered course starts from the pinned prior
recovered passive Left history. A real SDL left press selects one new ordinary
Left strike. Its first completed save matches the whole live export: both
views have strike2 at windup, both passive fields are cleared, cause1 remains
consumed and only action2 is pending (next3). Native resources and bases,
combat RNG, complete Player inventory and all 39,111 source reference
inventories match the source exactly. Load-owned states and clocks match the
input. Draw threads join before peaceful exit and the process epoch is gone.
The post-save image was inspected; arms are visible without foreground torso
obstruction or exploding geometry. It is later than the saved windup.

The two failed setup courses remain retained: missing mouse binding, then an
incorrect Right wait despite the source's Left preference. Numeric and action
assertions were unchanged for the accepted course. The later hit after the
accepted save is outside this windup checkpoint. This is one same-cached Left
capture through normal input, from injected/recovered history. Sanitized and
third-person mouse courses, other protocols/crossfades, a restart of the new
windup, normal Stagger activation and the complete M15S4 input/audio/FPS/E2E
matrix remain open. Sound is disabled and FPS is configured, not measured.
M15S4 remains incomplete.

### Restart of a mouse-admitted windup

A second normal first-person course now compares uninterrupted completion
against a fresh process loaded from the exact first completed windup save.
The initial process uses one SDL left press and saves paired strike2 before
contact, then completes and saves again. The second process loads that windup,
receives no attack input, completes strike2 and saves. Each process commits
one hit by action2, consumes it and recovers both views without passive history
or an extra allocated action. Cause1 never contacts again.

All three complete saved/live pairs match. Both load-owned state and clock
comparisons match their inputs. Completed native actor values/bases, RNG,
ledger, Player inventory and all 39,111 original reference inventories match
between the uninterrupted and restarted paths exactly. Observed Damage slots
are NPC Health -1.4199999570846558, NPC Fatigue -1.7100000381469727 and Player
Fatigue -7 in both continuations. These are observed reference values for
restart equivalence, not an independent original-game numerical oracle.
Both fresh process epochs are gone, with draw threads stopped before peaceful
exit. Both completed images were directly inspected; arms and opponent remain
visible without foreground torso obstruction or exploding geometry.

Editable first/continuation manifests and a saved/live comparison verifier are
versioned. All 374 Python tests pass at the recorded frozen source.
[`M15-PASSIVE-SLOT3-MOUSE-RESTART-VERIFICATION.json`](M15-PASSIVE-SLOT3-MOUSE-RESTART-VERIFICATION.json)
pins the recipes, binary, captures and observed deltas. The initial passive
history remains injected/recovered, and this checkpoint covers one normal
first-person same-cached Left attack. Sanitized/third-person/gamepad crosses,
other predecessor protocols, normal Stagger activation, audio, measured FPS
and the full M15S4 acceptance matrix remain open. M15S4 remains incomplete.

### Paired contact reaction replacement of recovered passive slot3

BlockHit/Recoil resource preparation and the contact transaction now consult
the shared slot3 source in each view. A recovered passive strike or reaction
supplies its actual group, sampled time, timing, pose and consumed cause.
Candidates replace passive history only after validating capture/crossfade;
outgoing layers preserve the saved predecessor. The source descriptor also
retains reaction kind, preserving rejection of conflicting protocols using
the same resource. A remaining Stagger in either view still rejects slot3
replacement, including body recovery ahead of first-person slot0.

A new 32-context world/service test covers four passive protocols, independent
body/first capture masks and incoming BlockHit/Recoil. It runs actual typed
Stagger frame/prefix recovery, checks a fixed activation table against raw
saved history, rejects malformed/stale/missing-mirror requests without binary
changes, tests stale prepared transactions and first-person Stagger skew,
and verifies exactly one contact publication. All 128 frame/restart
checkpoints preserve resources, RNG and the ledger; outgoing layers finish.
Full normal and ASan/UBSan engine inventories each pass 1,784 tests across
125 suites, with no failures or skips. Leak detection is disabled. Warnings
are triaged explicitly in
[`M15-PASSIVE-SLOT3-REACTION-READMISSION-VERIFICATION.json`](M15-PASSIVE-SLOT3-REACTION-READMISSION-VERIFICATION.json).

This rebuilds the resource factory wiring but does not yet provide a rendered
normal-input passive-to-reaction contact course. Body-only activation,
Stagger slot0 factory/activation and the complete M15S4 input, audio, measured
FPS, cancellation and E2E acceptance matrix remain open. M15S4 is incomplete.

### Third-person mouse windup continuation

A normal third-person course with the reaction-readmission tested application
now passes the same uninterrupted-versus-restarted ordinary Left comparison.
All three saved/live pairs and both load-owned state/clock comparisons match.
Each process commits one action2 hit and recovers both views. Completed
resources, bases, RNG, ledger, Player inventory and all 39,111 source reference
inventories match the uninterrupted reference exactly. All three saved camera
flags remain third person, DRAW remains1, and receipts show exactly one mouse
down/up in the initial process and none in the restarted process. Both process
epochs are gone after threaded peaceful shutdown. Both completed images were
directly inspected; Player and opponent are visible without exploding geometry.

[`M15-PASSIVE-SLOT3-MOUSE-THIRD-RESTART-VERIFICATION.json`](M15-PASSIVE-SLOT3-MOUSE-THIRD-RESTART-VERIFICATION.json)
pins this course's distinct source, later binary, preparation and comparisons.
The earlier normal first-person checkpoint pins its own earlier binary; these
are not a uniform-revision complete camera/build matrix. This ordinary attack
course does not exercise the new passive-to-reaction path. The initial passive
history remains injected/recovered, sound is disabled and FPS is unmeasured.
Normal Stagger activation and the full M15S4 runtime/input gates remain open.

### Instrumented first-person mouse windup continuation

The ASan/UBSan first-person course now passes uninterrupted-versus-fresh-process
continuation with the reaction-readmission tested application. Its first save
has paired Left strike2 at windup with only action2 pending. Each process
commits exactly one action2 hit, consumes it, and recovers both views.
All three whole saved/live pairs and both input-owned state/clock comparisons
match. Completed native resources/bases, combat RNG, ledger, Player inventory
and all 39,111 initial source reference inventories match the uninterrupted
reference exactly. An additional audit includes all 39,286 inventories in the
actual saved windup, including newly captured references; all match in both
completed paths too.

Both live process environments explicitly record ASan halt-on-error with
leak detection disabled, UBSan halt-on-error/stacktrace, libasan first in the
preload chain and DrawThreadPerContext. Both process epochs are gone after
threaded peaceful shutdown, with no reported sanitizer error. All three saved
camera flags remain first person and DRAW remains1. Input receipts show one
mouse down/up in the initial process and none on restart. Both completed images
were directly reviewed: arms and opponent remain visible without foreground
torso obstruction or exploding geometry. No original appearance parity is
claimed.

The first attempted course passed its game scenario but failed the process
monitor, which was mistakenly hardcoded to the normal executable. It remains
rejected and retained. A fresh course corrected the monitor's executable with
unchanged gameplay/numeric gates. An additional inventory-audit script initially
used the wrong epoch filename; its failed script is retained, and the corrected
audit reads the immutable captured event paths without changing comparisons.
[`M15-PASSIVE-SLOT3-MOUSE-SANITIZED-RESTART-VERIFICATION.json`](M15-PASSIVE-SLOT3-MOUSE-SANITIZED-RESTART-VERIFICATION.json)
pins the final course and both diagnostics.

This is one instrumented first-person same-cached ordinary attack from
injected/recovered history. The earlier normal first-person checkpoint used
an earlier binary; a uniform-revision full camera/build matrix is not claimed.
Sound remains disabled and FPS is configured rather than measured. This does
not accept normal passive-to-reaction contact, Stagger activation or the full
M15S4 gamepad/audio/FPS/cancellation/E2E gates. M15S4 remains incomplete.

### Body-only strike activation from resident slot3

NPC strike activation can now submit the same captured/outgoing slot3 history
as Player activation, with an absent first-person view. Explicit Player packets
still require a mirror; NPC packets reject one and reject FP outgoing history.
The controller prepares history from its actual primary/passive predecessor
and preserves the prepared incoming timing. Body-only ordinary/power frame
adapters carry the captured destination through activation and clear the
snapshot when easing completes. Their zero-additional-duration projection now
includes the body strike and its captured layer without a second clock step.
No schema or native wire-format change is involved.

A 24-context engine/service matrix covers16 passive predecessors (ordinary,
power, BlockHit, Recoil; steady/captured; ordinary/power incoming) and8 pending
primary strike predecessors. It runs actual typed Stagger recovery for passive
sources, compares outgoing timing against raw saved history, rejects malformed
and foreign-view packets without any binary change, and checks96 frame/restart
checkpoints with one new action, consumed old cause, independent clock
advancement, snapshot/outgoing completion and cancellation. The paired ownership
test additionally rejects a Player packet lacking its mirror. Full normal and
ASan/UBSan inventories each pass1,785 tests across125 suites with no failures or
skips. Leak detection is disabled.

The first builds were deliberately stopped after review found a fixture timing
comparison used the seed before manager recovery. A later normal run passed
1,784/1,785 cases: the new test incorrectly demanded invalid_argument where
existing typed validators throw runtime_error. All38 failures were exception
subtype expectations; whole-state and positive assertions passed. The corrected
matrix uses the established std::exception rejection contract while retaining
all state, action, timing and resource comparisons. Both attempts remain
retained in
[`M15-BODY-SLOT3-STRIKE-ACTIVATION-VERIFICATION.json`](M15-BODY-SLOT3-STRIKE-ACTIVATION-VERIFICATION.json).

Rendered NPC admission/restart is being pursued separately; this checkpoint
claims engine/service verification only. Normal Stagger slot0 activation,
active/cached replacement, the complete input/gamepad/audio/FPS/cancellation
matrix and full M15S4 E2E acceptance remain open. M15S4 is incomplete.

### NPC body-only admission through normal activation and windup restart

A normal first-person course now exercises body-only admission in the actual
controller. The pinned source declares consumed passive NPC Left history and
drawn state. Normal activation runs an authored script that releases the
fixture restraint and calls StartCombat. The existing deterministic stationary
production policy selects Left strike2; no script selects an attack or writes
hits, damage or resources. The first completed save has only action2 pending,
no NPC FP/passive fields and an easing captured pose with64 tracks.

The initial process continues to two real contacts (actions2 and3), then its
script stops combat after Fatigue spending. The production intent cleanup
cancels remaining playback. A fresh process loads the exact first windup save,
receives no activation input, produces the same two contacts and cancels in
the same way. Both end with next4, no pending actions, no owned NPC strike,
reaction, FP, passive or AI intent. The old cause1 never contacts.

All three complete saved/live pairs and both load-owned state/clock comparisons
match. Windup native resources/bases, combat RNG, Player inventory and all
39,111 initial reference inventories are unchanged. Completed native values,
bases, combat RNG, ledger, Player inventory and all39,286 inventories captured
in the actual windup match the uninterrupted reference exactly. The observed
NPC Fatigue Damage is -15.600000381469727 and Player Health Damage is
-1.9907171726226807 in both paths; NPC weapon condition is55.400001525878906.
These are observed continuation values, not a new original-game damage oracle.
The saved script fighting local is1 at windup and2 at completion in both paths.
Both process epochs are gone after threaded peaceful shutdown. Both completed
images were directly reviewed: NPC sword/body and Player arms are visible
without torso obstruction or exploding geometry.

Editable overlay/script, saved-state and runtime manifests plus a strict staging
helper are versioned. Its tests cover whole-wire/opaque preservation, input and
view rejection, source/target plugin hashes, stale clocks, cause replay and
existing destination preservation. All378 Python tests pass. The inspected
replacement SCPT compiles with zero fork diagnostics and keeps its saved local
layout. The helper reproduces the accepted input byte-for-byte. This is TES4
fixture generation/readback, fork SCTX compilation, semantic save reinspection
and actual two-process runtime admission; it is not TES3 OpenMW-CS MCP support
or a general compiler/world-validator claim.

Setup failures remain explicit: invalid raw phase4 rejected before writing;
inherited restraint prevented attack policy; an unused walking seed failed its
declared geometry gate; changed plugin bytes correctly triggered saved content
fingerprint rejection; and one driver raced incomplete staging before launching
an engine. The final recipe declares the inspected old/new content identity,
retains original geometry, releases restraint through normal activation and
verifies staged bytes before launching. No engine guard or gameplay/numeric
comparison was relaxed. Sources, final gates and diagnostics are pinned in
[`M15-NPC-PASSIVE-STRIKE-RUNTIME-VERIFICATION.json`](M15-NPC-PASSIVE-STRIKE-RUNTIME-VERIFICATION.json).

Initial passive history remains injected/recovered. This is one normal-build,
first-person same-cached source with authored speed20. Instrumented/third-person
NPC courses, normal Stagger activation, other protocols and the complete
input/gamepad/FPS/audio/cancellation/E2E matrix remain open. FPS is configured,
not measured; sound is disabled. M15S4 remains incomplete.


### Native counter dispatch and correction to the passive-phase assumption

Independent execution of the pinned original executable now covers1,024 counter
branch contexts and240 paired Stagger/slot3 phase-prefix contexts. The counter
branch uses BlockAttack29/action2, attempts its target at AnimData phase1, calls
disarm then Stagger when a target exists regardless of the disarm result, and
immediately recovers action6 with the same sequence, including on acquisition
miss. The next action6 switch takes its generic default. Range-factor, target,
disarm, Stagger activation and sound remain explicit supplied boundaries in
that branch probe. Two harness failures are retained: an incorrectly guessed
action6 table entry and an incorrect armor-getter calling convention.

The phase-prefix probe exposes an earlier interpretation error. The previous
manager-only evidence never executed the AnimData prefix and cannot justify
freezing ordinary/power phases, speed correction or reaction Start/End. Actual
Stagger activation leaves the freeze selector FF and speed unchanged; all120
activated contexts match the120 baseline slot3 histories through phase changes
and End retirement, while process action stays8. Actual metadata lookup, slot
reset, retirement/deactivation and controller updates execute on supplied
fixtures. No complete original actor frame or live normal-input result is
claimed.

Consequently, earlier statements in the passive service/projection/runtime
receipts about frozen phases or retention beyond End describe the implemented
behavior at those historical revisions; they are **not native correctness
acceptance**. Those engine/runtime passes remain evidence for their stated
state ownership, restart and input comparisons. The passive animation path
requires correction and revalidation before normal Stagger activation proceeds.
The editable manifest, exact instruction scope, hashes and retained failures
are recorded in
[`M15-STAGGER-NATIVE-DISPATCH-VERIFICATION.json`](M15-STAGGER-NATIVE-DISPATCH-VERIFICATION.json).
M15S4 remains incomplete.


The phase-prefix corpus was rerun with an explicit group/subtype/key-count gate
and x87 control words007F,027F and037F. An initial fixture incorrectly labeled
ordinary right21 as power and supplied three keys; that160-case attempt is
retained and excluded from final acceptance. The corrected corpus uses standing
power22/subtype5. Its240 contexts and120 activation/baseline pairs all pass.
Seven007F cases supply fixed bit observations for engine regressions, covering
ordinary speeds.7/1/2/20 and power/BlockHit/Recoil, eight frames each.


### Correct passive slot3 phases, speed and End retirement

The passive path now follows the actual AnimData prefix: ordinary/power speed
correction and one visual phase step, or BlockHit/Recoil Start/End processing,
then controller timing. End immediately detaches slot3 into the existing
retirement layers; the manager advances that layer once in the same frame and
removes it on completion. Gameplay process action and the consumed cause remain
unchanged. Each renderer view supplies the correct phase metadata. Missing
metadata rejects atomically. Independent clocks, captured destinations and
schema59 restart remain supported; no wire-format change is involved.

Seven fixed original007F cases cover ordinary speeds.7/1/2/20 plus standing
power, BlockHit and Recoil over eight frames in both views. The engine test
compares112 view samples for exact phase/residency, retirement state, offset and
output bits, and checks56 binary restart checkpoints with unchanged resources,
RNG, action8 and consumed ledger. The16-context contact publication matrix now
requires visual phase progress and eventual removal. Requests after every
history has expired return no frame and preserve the whole state. Readmission
fixtures supply actual phase metadata. Normal and ASan/UBSan full inventories
each pass1,786 tests across125 suites with no failures or skips on the same
source fingerprint. Leak detection is disabled.

The first normal attempt passed1,785/1,786 tests, including the new original-bit
comparison; the legacy contact fixture still demanded a frame after all history
expired. That expectation was corrected without weakening binary/resource or
phase checks. Its paired sanitizer build was deliberately stopped. Both failed
attempts and the earlier wrong-group native fixture remain retained.

A rendered normal-build, first-person course loads the existing injected Left
history and independently clocked Player views beside an authored eight-second
Stagger retirement. At the first normalF5 save, both passive slots and their
retirement layers are absent while both Stagger reactions and action8 remain
active. The process then recovers Stagger automatically. A fresh process loads
the exact early save and reaches the same completed state. Three whole saved/
live pairs and both load-owned-state/clock comparisons match. Resources, bases,
combat RNG, ledger and all36,762 initial inventories are unchanged; all37,365
early-save inventories and every saved Player field match the uninterrupted
completion exactly. No strike selection/contact occurs. All three captures
were directly inspected: Player arms and the NPC body are visible without torso
obstruction or exploding geometry. Both epochs exit with draw workers joined.

Editable manifests, original observations, source/binary identities, exact
inventories, runtime comparisons and retained failures are pinned in
[`M15-PASSIVE-SLOT3-PHASE-CORRECTION-VERIFICATION.json`](M15-PASSIVE-SLOT3-PHASE-CORRECTION-VERIFICATION.json).
This accepts the bounded passive recovery correction. Initial history and the
eight-second Stagger interval are injected, sound is disabled and FPS is
configured rather than measured. Normal Stagger/counter triggering, outgoing
slot0/cached replacement and the broader mastery/input/cancellation/E2E gates
remain open. M15S4 remains incomplete.


### Looping outgoing slot0 timing and restart

Outgoing locomotion can now retain native loop0 timing during a crossfade.
The updater defaults to the previous clamp2 behavior, preserves unwrapped
weighted time and normalizes only the output. Original6C5FC0 preserves the
first exact End sample; later whole laps wrap to Begin. It handles zero spans
and negative remainders explicitly and rejects unsupported cycles. Retirement
metadata retains the KF cycle, so valid loop wraparound no longer raises a
renderer rewind error. Reactions and phase protocols still require clamp
sequences. No save-schema change is needed because cycle is immutable KF data.

Independent original execution covers630 contexts and6,300 samples across
seven intervals, five frequencies, three lifecycle states and three x87 words.
The intervals include the actual installed idle End2.1999998092651367.
A standalone C++ comparison matches all six timing fields in2,030 live007F
samples exactly. The initial540-context/1,740-comparison corpus also passes;
the extension adds the installed idle period without changing any gate.
A separate18-context original-helper composition confirms that steady idles in
both Player slot0 views retire as state3 while incomingStaggers ease as state2,
publish action8 and preserve slot3 pointers at activation. That probe does not
execute the full actor phase loop or establish frozen slot3 behavior.

The normal full component inventory passes2,977 tests across280 suites.
The instrumented native component selection ESM4* passes830 tests across64
suites. Full engine inventories pass1,787 tests across125 suites in each build
with no failures or skips, on the same source fingerprint. New engine coverage
checks body-only and independently clocked paired loop retirement, exact
0/1/0/0 outputs, ten binary restart checkpoints, atomic metadata rejection and
unchanged resource/RNG/action ownership through completion.

Normal and ASan/UBSan rendered courses each load an injected ongoing eight-second
idle-toStagger crossfade, save after both idle samples have wrapped, continue
to recovery, then restart a fresh process from that exact early save. The loop
outputs match original normalization; all six whole saved/live pairs and four
load-state/clock comparisons match. Every completed Player field, native
resource/base, combat RNG and ledger matches its uninterrupted reference.
All36,762 initial inventories are unchanged and all37,365 early-save inventories
match each reference. No strike/contact occurs. All six captures were directly
inspected: Player arms and NPC body are visible without torso obstruction or
exploding geometry. All four epochs are gone after draw-thread joins and
peaceful shutdown. Leak detection is disabled.

The first fixture recipe failed strict reinspection because two clock+8 values
had not been rounded to binary32. No engine ran on that attempt. The corrected
editable recipe preserves every existing guard and comparison. Original
observations, source/binary hashes, exact inventories, manifests, staging
failure and rendered comparisons are pinned in
[`M15-SLOT0-LOOP-TIMING-VERIFICATION.json`](M15-SLOT0-LOOP-TIMING-VERIFICATION.json).
This accepts loop retirement and its bounded restart path, including actual
rendering in both builds. It does not admit fresh Stagger through the normal
resource factory or contact path. Live slot0 source adoption, active/cached
replacement, normal counter/Stagger triggering and the remaining mastery,
input/cancellation/FPS/audio/E2E gates remain open. M15S4 is incomplete.


## Fresh Stagger slot0 adoption checkpoint

Fresh NPC/Player Stagger resource preparation now adopts the legacy renderer's
sampled locomotion phase and publishes its outgoing NI timing atomically with
the incoming reaction. Player views retain independent clocks and coordinates.
Legacy locomotion background layers stay suppressed while Stagger owns slot0;
parallel slot3 visual phases continue after their logical contact is consumed.
The original executable's 24-case source-presence probe distinguishes immediate
state1 activation for an empty slot from state2 crossfade for a steady source,
independently in each view. A canonical coordinate roundtrip avoids a one-float-
step false clamp rewind on frame zero. Schema59 storage is unchanged.

The normal check passes all 2,978 component and 1,791 engine tests. ASan/UBSan
passes the 831-case ESM4* component selection and all 1,791 engine tests, without
failures or skips; leak checks are disabled. Typed tests cover 36 source/prior-
slot3 contexts, four independent view-presence cases, malformed/stale packet
rejection, one-time consumption, and binary restart. Both current binaries also
pass rendered two-process outgoing-loop recovery courses with exact saved/live,
resource, RNG, ledger and inventory comparisons; all six captures were reviewed.
Those runtime courses inject an ongoing eight-second crossfade and do not prove
fresh World factory selection or normal-input Stagger triggering.

The source adoption baseline is the actual renderer sample, not reconstructed
private NI lap history. Active/cached Stagger replacement, Creature reaction
selection, normal counter/Stagger triggering, and the wider S4 input/mastery/
effect/E2E gates remain open. M15S4 remains incomplete. See the
[verification record](M15-STAGGER-SLOT0-ADOPTION-VERIFICATION.json) and
[editable oracle manifest](../../scripts/data/oblivion_compat/m15_stagger_slot0_adoption_oracle.json).

## Counter phase, owned fade and schema 60 checkpoint

Counter is now a distinct strike protocol (kind 7), with pending action 2 and
consumed action 6. Its raw keys are Start/Attack/End; ordinary powers continue
to use Start/Hit/End. It skips weapon-speed correction, preserving negative
zero. End detaches inclusively even when one frame skips Attack, so no late
contact can fire. Its state 3 fade retains logical action ownership until NI
state 0. A consumed Counter can then transfer action 6 to a resident native
BlockIdle 27 in slot 1; an undispatched Counter clears to -1. Explicit cancellation
consumes and clears immediately. Body and first-person timing, retirement and
inactive flags persist independently; the first-person tail can outlive body
recovery. Schema 60 appends the new fields only for Counter. Ordinary/power wire
packets and legacy 1..59 decoding retain their existing layouts.

Independent original execution covers 42 raw-key cases, 288 phase contexts with
624 samples, and 36 action/state/BlockIdle recovery contexts. The production
helper matches all 624 samples, including all six NI timing fields in 480
nonzero-state samples; raw-key comparison matches all 42 cases. A separate
24-context original-helper reference establishes empty-slot state 1 versus
steady-source state 2 activation independently in the two views, for future
factory work. These probes supply synthetic object/cache/scene boundaries and
do not execute a complete original actor frame.

The normal check passes 2,984 component tests, all 1,794 engine tests and 380
Python tests. ASan/UBSan passes 837 ESM4* component tests and all 1,794 engine
tests, on the same source fingerprint, without failures or skips. Leak checks
are disabled. New tests cover body-only and paired Counter packets, malformed
wire flags in both views, phase/action binding, inclusive End and state 0,
24 owner/contact/cancel/recovery combinations, three pending cancellation
stages, stale/replayed consequence rejection, and binary restart checkpoints.

Normal and instrumented rendered courses inject an already consumed Counter
at End with synthetic eight-second body and ten-second first-person fades.
Each saves while both tails own their state, finishes normally, then starts a
fresh process from that exact early save. All six saved/live pairs and four
load-state/clock comparisons match. Resources, bases, combat RNG, ledger,
all 36,762 initial inventories and all 37,365 early-save inventories are
unchanged or match the uninterrupted completion exactly. Completed Player
fields match. All six captures were directly reviewed; arms and NPC body remain
visible without torso obstruction or exploding geometry. All four epochs exit
with draw workers joined. The instrumented course uses a longer wall-clock
observation deadline; its fixture and semantic requirements are unchanged.

Retained attempts include a wrong two-key native fixture, the renderer's wrong
Hit-key parser, an injected prior-NI-input error, and the body-only validator's
missing actor-value index. The first instrumented course's 180-second observer
deadline expired before FP recovery; its incomplete reference save is excluded.
All failures, corrections, binary/source identities and evidence pins are in
[the verification record](M15-COUNTER-PROTOCOL-VERIFICATION.json). Editable
oracle and runtime inputs are preserved under scripts/data/oblivion_compat.

This accepts a bounded persisted Counter protocol and End/fade restart path.
Normal Counter resource preparation/triggering, atomic mastery-proc action
allocation, target acquisition/disarm consequences, failed mirror playback,
complete first-person action dispatch and cached reactivation remain open.
The wider Stagger/Creature/mastery/effect/input/gamepad/FPS/audio/E2E gates
remain open. These courses disable sound, configure 1 FPS without claiming an
FPS matrix, and inject history rather than trigger a Counter through gameplay.
M15S4 remains incomplete.


## Atomic Counter startup preparation checkpoint

Counter startup can now be prepared inside a physical-contact transaction.
A defensive request selects Counter instead of immediate BlockHit/Recoil;
missing playback consumes no new action. A complete packet privately allocates
one Counter identity and publishes it with the original contact, resource/RNG
changes and independent body/FP histories. Preparation is read-only. A full
ledger/owner lease rejects unrelated action issuance or consumption before
commit, preventing a prepared allocation from replacing newer live ownership.
The World wrapper also guards the selected animation instances and roots.

The group29 catalog shares native family/movement fallback and established
archive ordering with reaction catalogs while retaining its own namespace.
The factory handles drawn NPCs and paired Players with empty slot3 or a reviewed
steady different source. Counter timing uses raw Start/Attack/End and unit
weapon-speed correction. Steady BlockHit sources retain exact outgoing timing.
Stationary AI leaves Counter ownership alone, contacts bypass prior AI target
restriction, and a held resident BlockIdle remains while Counter owns slot3.
These controller changes have regression coverage, not new rendered acceptance.

Independent original activation evidence covers24 source contexts and192
availability contexts. Active-camera availability controls startup, but each
available view can play independently and the process pointer remains body
slot3, including old/null body sources. Current paired storage rejects partial
playback; this reference does not settle complete first-person action dispatch.
A separate48-case original600528..60056A probe confirms one playback-helper
call with the query result, and no immediate disarm/Recoil when the Counter
proc is true even if playback fails. Query, resource, disarm and Recoil effects
are declared boundaries. The first caller probe omitted a required emulator
return value; the failed script is retained and corrected probe02 passes.

New typed World/service tests cover NPC/Player, empty/steady BlockHit sources,
read-only preparation, one-time issuance, binary restart, malformed packets,
missing AnimData without phantom actions, stale unrelated allocations or
consumption, and contact independent of a prior AI target. All2,986 normal
component tests,1,798 normal engine tests,380 Python tests,839 ESM4* sanitizer
component tests and1,798 sanitizer engine tests pass with matching C++
inventories and no failed/skipped accepted cases. Both final engine runs use
source fingerprint `caf81457930762bcc101a82c0ae26e9c96fbcd918e5dbfa26b1227fb594bfa75`
on parent `c6988a6a71a8c4c3e4e5d45f36b22306668e9fe9`.
Component/Python checks precede only unrelated World edits or exception
expectation corrections, as detailed in the verification record. ASan leak
checks are disabled and UBSan halts. Existing compiler warnings remain.
Interrupted builds, the strict-key test correction, and the failed exception
expectations remain retained rather than being counted as passing attempts.

This is a startup preparation foundation. New integration tests supply typed
packets in headless fixtures; no normal input/resource-factory or rendered
Counter activation is claimed. Normal defensive proc triggering, Master disarm
and Counter phase1 target/consequence dispatch remain open. Partial mirror
playback, complete first-person action dispatch, cached/captured reactivation,
undrawn paths and the wider mastery/effect/input/gamepad/FPS/audio/E2E gates
also remain open. M15S4 remains incomplete. See
[verification](M15-COUNTER-STARTUP-VERIFICATION.json) and editable protocol
`scripts/data/oblivion_compat/m15_counter_startup_native_probe.json`.


## Disarm target eligibility and RNG correction

Complete native caller verification exposed an earlier partial-path error:
quest and undrawn targets are rejected by5E4A80 before either disarm caller
rolls. Source drawn remains a post-roll guard. The side-power and defensive
rule APIs now take a target snapshot that distinguishes weapon presence,
drawn state, quest status and caller-resolved lookup suppression. Tests retain
the inclusive thresholds and distinguish early target rejection from late
source rejection. No live engine caller uses these two decision APIs yet.

The original full defensive helper passes7,878 final contexts; the side-power
suffix passes576. The production C++ comparison matches all8,454 draw/trigger
observations. Target kinds include NPC/Player/Creature; source Creature exits
and impossible dual-Player contexts are excluded. The actual target lookup,
quest/drawn predicates and rank selector execute, with base AV reads, equipment
entries, RNG and real consequences explicitly supplied. A separate request
trace proves drop arguments, relation handling even for a null drop result,
process-action clear/block-release ordering and combat-controller notification.
These requests are prerequisites for integration, not proof of real drops.

All2,987 normal component tests and840 ESM4* ASan/UBSan tests pass with matching
inventories and no failed/skipped cases. Tested parent is
`50a1c0182cda519b1558b07ce84d74733c56115d`, dirty fingerprint
`2412196e510a73fd0a5caf5bc7af0282c7604fb689427dd789545bf26c717e10`.
Leak checks are disabled; UBSan halts. World/engine/Python sources did not change
and those suites were not rerun for this rule-only correction. Existing compiler
warnings remain. The initial wrong Player drop-vtable assertion and broader
exploratory identity matrix are retained and excluded from final counts.

This corrects the earlier S2 whole-caller RNG implication. Real disarm inventory,
action/controller publication, Counter triggering/dispatch and normal-input
restart/E2E acceptance remain open. M15S4 remains incomplete. See
[verification](M15-DISARM-TARGET-VERIFICATION.json) and the editable
`m15_disarm_target_native_probe.json` protocol under scripts/data/oblivion_compat.


## Atomic intact NPC disarm contact checkpoint

Physical contacts now carry a selected disarm separately from condition wear.
World matches each exact NPC carried weapon to one equipped-drop transaction,
rejects winning quest items, duplicate/break conflicts and wrong placement or
wear contexts, and uses native default placement. Publication includes the
original contact, intact or final fractional condition/charge, dynamic native
reference and dropper relation, scene/physics body, source inventory debit and
prepared RNG/resource changes. Preparation leaves live simulation unchanged;
scene failures and stale equipment/pose leases leave no partial drop or spent
contact. Unknown saved process action rejects rather than assuming a result.

Disarm withdraws Alive attack actions0..5 and weapon block6 without a remaining
shield. An existing shield retains6. Bound slot3 history becomes passive;
unbound intent is consumed without inventing a sequence. Recoil replaces the
original slot3, whereas Stagger preserves the staged passive source. Canceled
IDs are deduplicated. Eight owned-asset cases combine participant, wear and
binding states, including phase1 Counter-origin zero-damage contacts that
publish victim disarm/Stagger and consume the Counter into6. Typed packets,
synthetic phase keys and selected RNG transitions are supplied; this is not
normal-input, collision acquisition or mastery-proc acceptance.

Independent original default-placement arithmetic passes13,024 contexts with
no callee stubs; production C++ matches all3,256 wholly default observations
exactly across007F/027F. Disarm uses actor position plus Euler-rotated(0,50,30)
and unchanged angles; breakage uses its supplied carried pose. The pending
implementation initially reused that carried pose and was corrected before
acceptance. New unit goldens include precision-sensitive cases; invalid
trigonometry/nonfinite inputs reject. A default drop guards even actor pitch
zero-sign changes. No original full inventory/scene creation is inferred from
the placement-prefix probe.

All2,989 normal component tests and842 ESM4* sanitizer component tests pass.
Both final engine runs pass all1,802 tests in125 suites with exact inventories
and no failures/skips. Tested parent
`3166a98b3e39769aeb424e314eb34b20a21c5e88`, final dirty fingerprint
`f2f5fe1be71420c35fc3b0f6ec1520684ff4d39d2df2f519a3cb5e032ae8caea`.
Component checks precede only World/service and World-test changes; component
code/tests remain identical. ASan leak checks are disabled; UBSan halts.
Python sources did not change and Python was not rerun. Existing compiler
warnings remain. Early source-order/placement builds were interrupted and
retained. Fixture failures included a missing winning clone reference during
restore and a no-shield setup path restacking charged synthetic metadata whose
ENCH definition is intentionally absent. The fixture now registers its cloned
reference and authors native shield worn state initially. Passing intermediate
engine runs precede the final unknown-action guard/Counter cases; final06
reruns both complete engine inventories.

This checkpoint uses existing schema60 publication fields and adds no new
persistent request format. Semantic checks include binary RuntimeState and
service restore, not fresh-process World load. Current drop authority remains
NPC-only, one drop per owner, with owned bow playback rejected. Player/Creature
drops, multi-drop composition on one owner, live mastery/Counter dispatch,
projected post-disarm resource selection, replacement/controller/audio behavior
and the wider input/effect/gamepad/FPS/E2E gates remain open. M15S4 remains
incomplete. See [verification](M15-DISARM-CONTACT-VERIFICATION.json) and editable
`m15_disarm_contact_native_probe.json` under scripts/data/oblivion_compat.


## Counter phase1 contact dispatch checkpoint

Counter now enters a dedicated nondamaging contact branch from the production
controller/dispatcher. Reach is resolved actor scale times fCombatDistance;
weapon reach and prior AI target are not used. A physical miss consumes once
into6 without attack Fatigue, damage, wear, knockback, RNG or swish. On an
acquired target the helper prepares the Master disarm roll before Stagger
resource selection and publishes through the existing atomic World transaction.
The Stagger family query observes the projected removed right-hand weapon.

New tests cover inclusive chance40/41 with native CRT seed1/draw41, Expert
versus Master, source drawn-after-roll and target quest/undrawn-before-roll
ordering, missing physical drop backend, exact resources/inventory/RNG/serial
publication, one-shot consumption and binary/service restore. A real capsule
plus the production acquisition/dispatch path proves a physical miss with zero
attack Fatigue despite a large supplied weapon weight. The owned scene test
supplies its reviewed Stagger packet; it does not prove live post-disarm
animation discovery. Another-weapon inventory rejects a selected proc without
spending its RNG/contact, while a failed roll remains preparable.

Both final full engine runs pass **1,806 tests /125 suites**, exact inventory,
zero failed/skipped: `native-counter-contact-normal-03` and
`native-counter-contact-sanitized-03` under build/oblivion-compat/m15/S4.
The sanitizer environment disables leak detection and halts on ASan/UBSan;
no diagnostics were observed. Tested parent is
`3bb48ea33f934833c23570ef31d451f8f78f2938`, dirty source fingerprint
`3c0bf0279849bb7d67b7bb18257eb7c9112a84041d6b4e1aabe0bd28e9697670`.
Components/Python are unchanged and were not rerun. Both01 compile failures
and passing intermediate02 runs remain retained. Exact hashes/scopes are in
`M15-COUNTER-CONTACT-VERIFICATION.json`.

S4 remains incomplete. Ordinary defensive proc selection/normal activation is
still guarded. Player/Creature drops, Creature Stagger, NPC replacement and
combat-controller policy, complete native first-person action dispatch, partial
mirrors/cached sources, normal-input/save-restart and the camera/input/audio/FPS
matrix remain open. The other canonical S4 gates are unchanged.


## Expert defensive Counter activation checkpoint

The surviving blocked-contact World preview now appends the Expert query after
staged armor-wear RNG. A true proc selects Counter instead of immediate
BlockHit/Recoil, including failed-playback cases. The final numerical token is
rebuilt even when no animation was prepared, so query RNG cannot be discarded.
Ordinary numeric blocking admits Expert; immediate Master disarm remains guarded.
Projected worn equipment controls rank selection. Creature Counter targets and
selected equipment-change startup remain guarded.

Both final full engine runs pass **1,810 /125 suites**, exact inventory, no
failures/skips: native-expert-counter-normal-02 and -sanitized-02. Tested parent
`9b04a38167e25ba1a637ee6a926a79c6bb67a3b8`, dirty fingerprint
`d613396922ca4b48bcf3114a59fceada1cca131c8886ed9c0b951297668171ab`. Components/Python are unchanged.
The retained01 failures were two test fixture expectations/capture setup issues.

A normal keyboard Player attack now causes NPC Expert H2H Counter3, whose Attack
key acquires Player and requests paired Stagger3 with zero damage/attack Fatigue.
The declared idle input contains no active melee sequence/action; it unequips
the NPC weapon, sets base H2H75 and selects a declared native CRT seed (draw12,
installed chance25). Actual saved checkpoints show pending3, consumed3 and full
recovery. A fresh process loads the real pending windup save with no attack
input, fires Counter once and reaches the same values/life/bases, ledger/RNG and
all2,348 nonempty reference inventories as the uninterrupted process. Both
views recover. Direct captures were inspected. This is first-person/configured
10FPS software-rendered coverage with sound disabled, not the full S4 matrix.

The strict whole-payload diagnostic comparison failed: position JSON emits
some negative Euler zeros as integer -0. Numeric state equality passes, but
whole wire equality is not claimed. Authoritative binary gameplay core and
restart inventories match. This diagnostic issue remains to fix. NPC held
BlockIdle also rebinds at Counter recovery; original slot1 cache/clock
continuity remains open. Exact checks, source/data/epoch/save hashes, retained
failures and bounds are in M15-EXPERT-COUNTER-VERIFICATION.json. S4 stays open.


## Lossless runtime diagnostic JSON checkpoint

Canonical JSON now retains integer/double variant tags and signed zero for
runtime/script values, plus negative position/rotation zeros. This changes no
binary schema or gameplay. Three component cases exercise these distinctions.
Both final checks pass: **2,992 components /280 suites**, **845 ESM4 sanitizer
cases /64 suites**, and **1,810 engine cases /125 suites** in each build, with
exact inventories and no failures/skips. Source parent is
`dfad5b3b5df94f2ccaa5add9300b6f76e8e688cb`, dirty fingerprint
`4f2906f6e31f85282e4de2f0aed5c541caf37811c519909a2329ece8b619ed75`.
No sanitizer diagnostics were observed; leak checks were disabled.

The normal three-process course passes six exact binary/live T4ST payload
comparisons, including real pending and consumed Counter saves loaded into
fresh processes without attack input. Final combat core and all 2,348 nonempty
reference inventories agree. The corresponding sanitized scenarios also finish
and all six binary/live payloads agree exactly. Its windup continuation matches
all 2,251 inventories, but the consumed-contact continuation has six additional
inventories from cell `content:oblivion.esm:02c1b3` and fails full inventory-set
acceptance. Existing inventories, Player inventory and combat core match.
This failure is retained rather than excluded or normalized. Materialized-cell
capture and lazy reference discovery require investigation. The sanitized root's
older wire-verification.json covers only its first epoch; the failed full-course
wire-diagnostic-verification.json is authoritative for the broader comparison.
All ten final captures were directly inspected. The course remains first-person,
keyboard, configured 10 FPS, software-rendered, sound disabled.

Editable manifests include the consumed-contact and sanitizer continuations.
Exact source/build/epoch/save/dump/tool hashes, retained intermediate failures,
and bounds are in `M15-STATE-JSON-FIDELITY-VERIFICATION.json`. The earlier
Expert checkpoint's diagnostic failure is fixed by this new checkpoint; its
historical report is unchanged. Native held-slot continuity and all other
listed S4 gaps remain open. Full M15S4 is not accepted.


## Isolated Expert Counter three-process restart checkpoint

The isolated input changes only `actor_ai.restrained` to true for all saved
actors (3,660 additional changes among 3,663 entries). It retains the named
NPC's existing stance/block controls and all combat state; no contacts or
outcomes are injected. This prevents distant AI package progression from
changing the materialized inventory set between event-aligned snapshots at
different simulation times. The original unrestrained sanitizer failure remains
retained and broader background-AI snapshot behavior remains unresolved.

Both normal and instrumented three-process courses now pass all six strict
binary/live payload pairs each, without normalization. Real pending save3 and
consumed save4 start fresh processes: one Counter3 contact from pending and
zero from consumed. Final combat core, Player inventory and all **2,151**
nonempty reference inventories match within each course. Counter changes none
of the nine native actor-value arrays or life states, spends no RNG, and requests
paired Player Stagger3. The initial ordinary-hit regression values match the
previous declared values. All ten captures were inspected, all six scenarios
and snapshot monitors completed, and no sanitizer diagnostics were observed
(leak checks disabled).

These are the already-verified binaries at C++ source equivalent to
`d294be7bd1`; uncommitted renderer retention changes are excluded. The new
recipe, process/save/dump/binary hashes, checks and retained failure are in
`M15-COUNTER-ISOLATED-RESTART-VERIFICATION.json`. Coverage remains first-person,
keyboard, configured 10 FPS, software renderer, sound disabled. Held-slot
persistence and the remaining S4 gates are still open.


## Resident held BlockIdle retention through Counter

Counter body/FP playback now asks the renderer to retain the existing held
BlockIdle group when replacing legacy equal-priority states. Other states at
the same priority are still removed, and ordinary calls keep their replacement behavior.
Two renderer cases verify source/clock retention, replacement/default behavior,
independent peer clocks and view scope. Both full engine runs pass **1,812 /125
suites**, exact inventories, no failures/skips. Tested parent is
`da5784f2ac6615ffcc18268aced505a6200f2233`, dirty fingerprint
`ed422ec54020576b0b580213e6a0bb2e4b2a93011eca96f76ab1261388e226c5`.
Components/Python are unchanged. No sanitizer diagnostics were observed; leak
checks were disabled. Existing scripted-animation/test aggregate compiler
warnings remain in the build logs.

A hash-verified original Counter activation probe preserves both supplied
steady body/FP slot1 sequences and metadata byte-for-byte in 24 cases. Real NI
updates then advance 120 samples per view without reactivation. This supplies
cache/scene/notification boundaries and is not a full original-game process or
block activation proof. Both actual keyboard held-input runtime courses now
recover Counter to **action6** with exactly one NPC BlockIdle start and no
rebind. Each has three strict binary/live payload pairs, unchanged Counter AV,
life/RNG checks and directly inspected Stagger/recovery captures. Scope remains
first-person/configured 10 FPS/software renderer/sound disabled.

A separate original5F4AE0(false) probe returns false without removing steady
slot1 while action2 owns Counter. A retained normal-input red case releases only
Lua block controls when real Counter playback appears; the engine loses held
idle before Counter contact. That release discrepancy remains open. Its
provisional final action6 expectation is not accepted: the post-contact input
caller/release policy still needs tracing. Full native slot1 activation,
release fade and cache/clock persistence through restart are also open.
Exact checks, source/binary/epoch/save/tool hashes, editable courses and the
retained red case are in `M15-COUNTER-HELD-RETENTION-VERIFICATION.json`.
Full M15S4 remains incomplete.


## Block-release caller and view-policy instruction checkpoint

The original Player input slice6727DD..672821 gates release on process action6;
action2 skips the block helper. With supplied device query results, the full
caller/helper/action-setter composition passes **72 cases**. A separate **96-case**
NPC/Player, camera, source and x87 matrix starts real Counter playback, advances
its NI state, publishes supplied postcontact action6 through original5EFFD0 and
executes full5F4AE0(false). Release clears process action/pointer and held-slot
pointers while cached Counter slot3 survives in NIstate1. NPC held body retires
in3. Player camera0 ends with held body0/FP3, whereas camera1 has body3/FP3;
active-view virtual+164 and nested action-clear resets account for the asymmetry.
The INI value .25 and cache/scene/notification/device query boundaries are supplied.
This is not a full original-game input/contact/process observation.

Several retained attempts exposed missing group-code fixtures, an omitted
activation-clock manager update, and incorrect assumptions that both Player
held views end in the same NI state. The accepted matrix is explicit rather
than normalizing those differences. The raw08 report's inherited scope sentence
is corrected by verification08-reviewed.json without altering its source,
assertions or corpus. No C++/Python implementation changed and no tests were
rerun for this evidence-only checkpoint.

Production still releases held idle too early during action2 and lacks the
postcontact separation of process ownership, cached Counter and held NI fade.
Full held-slot persistence is also open. The earlier red course's provisional
final action6 expectation remains unaccepted; a corrected release course must
check action2 retention, postcontact action release and independent cached
recovery separately. Editable cases, hashes, boundaries and retained failures
are in `M15-BLOCK-RELEASE-CALLER-VERIFICATION.json`. Full S4 remains incomplete.


The block-release evidence initially mislabeled B06538 as a GMST. Its registered
name is `fAnimationDefaultBlend:General`, an INI global with compiled default
float bits3DCCCCCD (.1). The probes supply .25. Reviewed helper/caller reports
and the editable cases correct that provenance; raw execution artifacts and
corpora remain unchanged. No numerical result or production code changed.

## Counter block release and released-cache restart checkpoint

Counter now retains held BlockIdle while action2 owns startup. After committed
contact, release clears process action to -1 while Counter slot3 continues as a
passive cache and held slot1 retires independently. Resource capture checks the
resident source, authored metadata and view identities before atomic service
publication. Native expiry also removes the corresponding legacy animation
track. NPC uses body retirement; Player first-person-active releases body
immediately and retires FP, while body-active retires both. Both Player paths
use the active view's unsigned blend-frame byte/30, or the reviewed General INI
compiled default .1. Importing custom original INI values remains open.

Independent original-instruction evidence adds **288+96 authored-blend cases**
to the prior helper96/caller72 matrix, including actual x87 integer load/divide
and float store. Supplied scene/cache/notification, device-query and postcontact
boundaries remain explicit. This is not an original-game process observation.

Final normal and ASan/UBSan engine runs each pass **1814 tests/125 suites**, exact
inventories, zero failures/skips. Tested parent is
`540a19d8853239ac8a7df7dbec0ba4f24aa0780b`, dirty fingerprint
`706e4dbae0b14356b2a76eb48dfa86d8aef823a4e53477128554ff8527d74c09`.
Earlier normal components **2993/280** and sanitizer ESM4 **846/64** pass; their
component code is unchanged by subsequent fixes. Those overall01 attempts
failed engine compilation on private method access; overall02 failed on two
SCOPED_TRACE declarations sharing a source line. All attempts are retained;
final03 engine checks pass. Python is unchanged and was not repeated. No
sanitizer diagnostics were observed; leak checks were disabled. A test macro
style warning and existing aggregate warnings remain in build logs.

The accepted normal and sanitizer runtime courses each run two actual process
epochs: a keyboard attack with Lua block intent released during Counter startup,
and a fresh process loading the actual postcontact released save. An authored
Blend:30 key in the hash-pinned held KF supplies a one-second retirement window;
the editable structural recipe preserves every other source byte, and no
proprietary asset is committed. Actual parser/world and runtime acceptance are
recorded separately from that recipe's structural preflight status.

Each course has **four strict binary/live payload pairs**, unchanged Counter
AV/life/RNG checks, nine matching combat actors and **2151 matching inventories**.
The saved process-1/passive Counter3/held retirement state restores with exact
initial cache and clock bytes, then naturally retires without contact replay or
block rebind. All six Stagger/recovery captures were directly inspected.
Configured10 FPS, software/offscreen rendering, sound disabled and Player first
person bound this course. Actual Player Counter view release has unit coverage,
not an original-dispatch or complete runtime camera matrix.

Held-source adoption uses the actual legacy sampled phase as a canonical
baseline; it does not recover past unwrapped NI loop count. Full resident held
activation/windup persistence, reactivation during a prior held retirement,
BlockHit release, partial mirrors and full first-person Counter source dispatch
remain open. The isolated fixture's distant-AI restraint also does not close
the broader background materialization snapshot issue. Exact artifacts, failed
attempts, source and binary hashes, tool pins and editable manifests are in
`M15-COUNTER-BLOCK-RELEASE-VERIFICATION.json`. Full M15S4 remains incomplete.

## Held BlockIdle cache reactivation instruction checkpoint

Full original5F4AE0(true) now executes after Counter release, real NI3 anchoring,
and elapsed0/.125/.25/.5 updates in **96 cases**. The same held object is supplied
under matching cache/sequence group11b. Before fade expiry it is zero-deactivated;
after expiry it is already NI0. Activation publishes NI1 and process6 targeting
the active held view while cached Counter stays byte-identical in sampled NI
fields. A repeated true helper returns false and preserves complete selected
sequence bytes. NPC selects body; Player camera0 selects FP only, camera1 also
activates FP through its notification path. The scene-update boundary remains
supplied, so this does not prove the complete pose mixer or camera transitions.

**720 selected-view NI samples** establish a new clock origin and loop phase:
the first update uses negative current independent view clock and output0;
weighted time then follows0,.125,.25,1,2,2.125 while output preserves2 at exact
End and wraps to.125 afterwards. Zero-duration activation resets the offset
sentinel but retains prior sampled/ease fields until the first NI update.

Exploratory attempts initially delayed retirement anchoring, used the wrong
sentinel sign, assumed easeStart reset, and shadowed the hook-name map with a
sample variable. Retained failures and instruction review exposed those
assumptions; accepted07 corrects them and asserts the full matrix. Cache hash,
equality/variant, notifications, scene updates and INI .25 are declared supplied
boundaries. Postcontact state and clocks are fixtures, not an actual original
game process/input/contact course.

Production still lacks resident slot1 history and reactivation ownership. It
must replace the old fade with the new native lifecycle without losing cached
Counter or peer asymmetry; simply deleting retirement and restarting a legacy
track would not satisfy persistence. Editable cases and exact tool/corpus hashes
are in `M15-HELD-BLOCK-REACTIVATION-VERIFICATION.json`. No production code changed
for this checkpoint, and prior implementation tests were not repeated. Full
M15S4 remains incomplete.

A retained actual normal runtime reblock course now reproduces the production
failure. It loads the unchanged released save4 with exact initial cache/clock,
then Lua requests continuous block through ordinary controls. Save3 accepts
action6 but still contains the old held retirement. Expiry disables the newly
played same-group legacy track, and the controller logs a second block begin
without a new input edge. The late capture shows blocking again; that visual
alone would conceal the intervening lifecycle error. The driver passed, but
semantic regression acceptance is **false**, expected-red reproduction **true**.
Two save/live payload pairs are exact and nine AV arrays/life/RNG plus2151
inventories remain unchanged. Editable manifest and pinned artifacts are in the
reactivation receipt. This supplies a concrete before-case for resident slot1
ownership/persistence work.

Follow-up instruction review narrows the activation result above: supplied
47F7B0=false, mismatching660110 model roots and578F60=false select direct
zero-duration activation. The same-object reuse/clock-reset matrix proves that
branch; it does not prove activation blend timing for eligible/root-matching
actors. Selection474852 also directly reads compiled B06538 .1/B06530 1,
whereas the release getter supplies .25. Raw07 results are retained unchanged;
the reviewed receipt and editable cases now state these gates explicitly.
Eligible manager activation is a required next proof before a universal slot1
activation implementation.

The eligible/root-matching branch is now separately exercised in **288 cases**
with **3240 selected-view NI samples**, held blend frames0/9/30 and the prior
actor/camera/clock/x87/elapsed matrix. Full helper, manager6C5C70 and pose transfer
6C9D30 execute with a supplied preallocated6C5860 pose proxy. The selected held
sequence enters **NI5**, while the proxy enters **NI4**. Activation duration uses
authored frames/30 or direct compiled General INI .1 at0. First NI update anchors
the independent view's ease span and freezes destination0 through blending.
The first update reaching easeEnd changes held toNI1 and sets its loop origin
to that update's clock; later samples preserve strict loop End equality.

Attempts08/09 expected ordinary NI2/NI1 respectively and were falsified by
NI5. Exploratory10 records the actual protocol; accepted11 asserts all288 cases
without deriving expected values from production code. Counter cache and
repeated-held invariants remain intact. Proxy allocation/pool lookup, scene
controllers and rendering gates are supplied; actual scene pose capture,
production persistence, differing Player source blends and runtime reactivation
acceptance remain open. Updated editable cases and pins are in the same
reactivation receipt. No production code or test selection changed.

## Held-block schema61 persistence checkpoint

Schema61 now stores a per-view resource cache for held slot1 separately from
contact IDs, Counter slot3 and process action. Each entry retains sampled time,
unwrapped NI timing, optional retirement/inactivity and optional captured pose.
Inactive resources survive; one resident slot and case-insensitive unique aliases
are enforced per view. A resource cannot simultaneously appear in its old generic
retiring vector and new held cache. Actor life/process/clock ownership, independent
Player FP ownership, finite fields, pose tracks, retirement clocks and binary
boolean flags are validated before admission. Resource metadata/slot reinspection
remains a world-layer responsibility.

Eleven lifecycle cases include normal loops, requested/anchored poses, pending/
anchored/expired retirement and cached preupdate activation. Native activation
can reset offset/easeStart while retaining prior lastInput/weighted/output fields;
those explicit masks are preserved rather than cleared. Other partial timing is
rejected. Legacy migration adds empty caches only, and does not infer history from
action6. Headless service capture rejects older output versions before modifying
the destination and restores both view histories through the binary codec.

Final normal checks pass **382 Python tests**, **2995 component tests/280 suites**
and **1815 engine tests/125 suites**. Final ASan/UBSan checks pass **848 ESM4 tests/
64 suites** and **1815 engine tests/125 suites**, with exact inventories and no
failures/skips. Parent is `6919f23035801330036da5c737c08a0a4fa31e81`, tested dirty
fingerprint `8dd3c3bc95fb8dff0a1b59224005443eff15dc0fb183ffe548abd31f9172d0ef`.
All eleven C++ whole binary/canonical JSON cases reproduce byte-for-byte through
Python in both configurations, including signed zeros and independent clocks.
No sanitizer diagnostics observed; leak checks were disabled. Existing aggregate
warnings and the earlier Counter-test macro warning remain.

The first full Python run retained seven fixture helper failures at their old
version60 guards. Four reviewed helpers and six editable recipes now explicitly
declare61 and empty held caches; final full Python passes. A pinned real schema59
strike-tail source stages to61 with complete semantic reinspection and preserved
undeclared state. This is structural/semantic staging, not compiler/world or
runtime acceptance. Earlier receipts retain their original recipe hashes at the
tested revisions; prior gameplay evidence has not been relabeled as61 evidence.
Baseline C++ runs passed before native preupdate-mask review; final02 reruns cover
the corrected implementation. Exact pins are in
`M15-HELD-BLOCK-SCHEMA61-VERIFICATION.json`.

An additional original-instruction call across the same288 eligible matrix
verifies full5F4AE0(false) during fresh NI5: it returns false, retains process6
and preserves complete held/proxy/Counter blocks. The release intent must defer
through that transition. The reactivation receipt includes this check without
counting repeated timing samples as new independent parameter cases.

This checkpoint supplies the saved-state contract. Controller activation,
advancement/projection, actual held windup/blocking restart and the recorded
runtime double-begin regression remain open. Full M15S4 remains incomplete.


## Held-block service lifecycle checkpoint

The held slot now has contact-free service activation, per-view manager updates,
release and cached reactivation. Reactivation replaces the same resource's old
fade and preserves last-input, weighted-time and output history; case-insensitive
resource aliases cannot create a second cache. NPC body, Player body/FP and
Player FP-only selection retain their independent clocks. Consumed Counter and
BlockHit slot3 sequences become passive caches on release and survive reblock.
Bad source metadata and stale frame preparations cannot publish authority.

Schema61 admits the additional native preupdate layouts: release before the
first input, immediate inactive caches, and sub-ULP retirement expiry before
lastInput assignment. Nineteen whole C++ binary/JSON witnesses reproduce exactly
through Python in both configurations. Versions1..60 still migrate with empty
held caches; no prior resident lap history is invented. Legacy retiring history
moves only on an explicit activation and retains its existing bias.

Original executable SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`
passed the 1152-case nonzero-bound/frequency reactivation matrix with 12960 selected
view samples. NI5 starts with offset=source Begin and requested destination0;
weighted bias survives fresh-loop origin selection. A 144-case immediate-release
matrix and its second-reactivation extension verify missing timing history,
including expiry before first input. Probe 13 remains exploratory; probe 15 is a
subset of 16. Supplied rendering, cache, INI, scene and proxy boundaries remain
explicit. These are instruction probes, not an original-game process.

Normal checks passed 382 Python, 2995 components and 1820 engine tests. Sanitizer
checks passed 848 ESM4* components and 1820 engine tests, with no ASan/UBSan
diagnostic observed and leaks disabled. After both full runs, only world-test
braces and stronger ambiguous-source/peer-pose rejection assertions changed;
the final focused normal run passed all five affected lifecycle tests. Production
and codec/component-test files remain byte-identical to the full-suite sources.

Full-suite source fingerprint: `bc82319bb1ac1776a20a153df21a0c36d169c0b5f2235903bec5218f722513c0`.
Final focused-test fingerprint: `253bbd0d3a94f99444c978dfeb00d237bc377bc435deedb24c784f28e9955db6`.

The hash-linked receipt is
[M15-HELD-BLOCK-SERVICE-VERIFICATION.json](M15-HELD-BLOCK-SERVICE-VERIFICATION.json).
Editable native and wire cases remain in `scripts/data/oblivion_compat/`.
Generated oracle corpora, logs and saves remain ignored.

**M15S4 remains incomplete.** World/controller still uses legacy begin/end
paths; the actual double-begin runtime regression is open. Release while the
peer retains NI5 is rejected atomically: full support needs an independently
owned NI4 source-pose lifetime. Renderer binding/projection, interruption and
camera/source policies, custom INI import, actual in-flight restart acceptance
and the broad S4 matrix remain open. The power-of-two numerical probes do not
settle arbitrary-frequency deactivation under every original x87 control word.
This checkpoint is service and persistence validation, with no new compiler/world
authoring validation or actual runtime acceptance.


## Asymmetric held peer release protocol

Following service commit `0ec1d3fc9d`, the original full helper passed a
288-case asymmetric Player matrix. With body frames 9/FP30 and body active,
the completed body NI1 can release while FP held is NI5 and its source proxy
is NI4. The full reset changes both live FP objects to NI3 and gives them the
active body's new 9/30 fade. It does not retain the old NI4 interval. With the
reverse frame pattern and FP active at release, body held and its live proxy
are immediately deactivated; FP held receives the positive fade.

Two independent clocks, three x87 control words, four prior retirement phases,
two explicit individual update orders and three release times are covered.
Five subsequent held/proxy states per tuple match an independent binary32 NI3
model, including retained destination frame0 and frozen timing during the peer
fade. Complete body/FP Counter objects remain byte-identical throughout.
The 48-case exploratory 18 observations remain separate from accepted 19.

This refines the earlier model inference: the bounded peer-release path shares
one new fade between captured source and destination. The old NI4 lifetime need
not remain independent. Existing service rejection is still in place; next work
is to admit a held pose snapshot during NI3, preserve its requested destination
frame through retirement, verify save/restore, and wire renderer/controller use.

[M15-HELD-BLOCK-PEER-RELEASE-VERIFICATION.json](M15-HELD-BLOCK-PEER-RELEASE-VERIFICATION.json)
pins the original executable, preregistration, probe, corpus and editable cases.
All extracted instructions and bulky corpora remain ignored. Renderer/root,
scene, cache, INI and proxy allocation are supplied. Explicit source/destination
updates do not prove actual original manager traversal, and no game process or
production runtime acceptance is claimed. M15S4 remains incomplete.


## Shared peer-pose retirement implementation

The service now implements the asymmetric release handoff established by the
288-case original-instruction matrix. A peer's unfinished NI5 destination and
captured NI4 source share the new NI3 fade. The existing schema61 pose field
remains present through retirement, retaining requested destination frame0 and
its frozen sample. Immediate body deactivation in FP-active release and normal
fade expiry clear the live pose while keeping cached timing history. No wire
fields or version numbers changed.

Six headless cases cover both body/FP frame patterns and three release times,
with independent clocks and whole binary service restore before the fade anchor,
after every update and after expiry. They assert frozen peer input/output,
advancing completed-view time and unchanged contact ledger, owners and RNG.
The earlier rejection test is replaced by the now-supported lifecycle.
Twenty-three C++ binary/JSON witnesses reproduce byte-exactly through Python in
both configurations, including shared pose retirement before first input and
its sub-ULP expiry.

Normal checks passed 382 Python, 2995 components and 1821 engine tests. Sanitizer
checks passed 848 ESM4* components and 1821 engine tests, with no ASan/UBSan
diagnostic observed; leaks remain disabled.

Tested source fingerprint: `58057df07e33239e7618b502111f76804469ee173592f37d71de1f1e580121f0`.

[M15-HELD-BLOCK-PEER-LIFECYCLE-VERIFICATION.json](M15-HELD-BLOCK-PEER-LIFECYCLE-VERIFICATION.json)
links exact inventories, configurations, hashes, native proof and wire checks.
This is service/persistence coverage, without new compiler/world authoring or
actual runtime acceptance. World/controller wiring must project both source and
destination through the shared NI3 span; interruption/camera policy and the
recorded double-begin runtime regression remain open. M15S4 remains incomplete.


## Held native projection and public sample clocks

Held caches now participate in CharacterController's native frame routing,
metadata collection and per-view projection. Resident held event tracks bind at
zero legacy speed; native destination layers expose their stored time to public
animation queries, including Loop End and wrap. They do not seek or replay the
shared text-key track. Resource exclusion follows case-insensitive cache identity.
Inactive held entries disable their old event tracks without erasing the cached
history; advancing ordinary NPC strikes reuse the already-advanced actor clock.

Four renderer tests cover captured interpolation with literal native blend
goldens, stable bindings/callbacks, fresh-root reconstruction, both sides of the
shared peer fade, independent body/FP times, wrapped public queries, no extra
text-key calls, alias exclusion and rejection without replacing published time.
Full normal and sanitizer engine inventories pass 1825 tests each. Sanitizer
leaks remain disabled, with no ASan/UBSan diagnostic observed.

Tested source fingerprint: `49bf2593a43b4b6b792a5ba7066b901bedf7269145f21c3adb60977adf61cd78`.

[M15-HELD-BLOCK-PROJECTION-VERIFICATION.json](M15-HELD-BLOCK-PROJECTION-VERIFICATION.json)
pins configurations, exact inventories, retained baseline runs and editable cases.
World activation/release and atomic interruption adapters remain to be connected.
New native interruption evidence also requires inactive preupdate NI5 timing
masks100/101 in save validation. This is headless renderer/controller-source
verification, with no new actual runtime, visual, authoring/compiler/world or
fresh-process acceptance. The double-begin runtime regression remains open,
and M15S4 remains incomplete.


## Original held-slot interruption and pending-pose cancellation

The original full helpers passed 96 steady held NI1 cases: BlockHit/Counter retain
both held objects byte-for-byte, while Recoil/Stagger clear NPC body or Player
both slot1 pointers and apply the exact zero-deactivation field changes.
A further 192 cases execute full eligible held activation asNI5, then incoming
Recoil/Stagger before any manager update. They assert complete 0x70-byte held
sequence changes, including the one-store offset correction and clearing state,
link and requested destination. Incoming and old Counter resources are distinct
in every accepted tuple. Unselected NPC FP/Player body caches remain unchanged.

The inactive cache may retain a known offset with no easeStart and either a
retained or absent lastInput: masks101/100. These values must be represented,
not filled in or discarded. A further 96 detached-cache cases show that full Recoil/Stagger leaves already
detached NI3/inactive0 bytes unchanged; the fade continues on its own NI clock.
Production save validation and atomic cancellation need the pending masks and
resident-only policy before normal-input typed held activation is enabled.
[M15-HELD-BLOCK-INTERRUPTION-VERIFICATION.json](M15-HELD-BLOCK-INTERRUPTION-VERIFICATION.json)
pins the original image, accepted02/04/05 tools, preregistrations, corpora and
editable cases; exploratory01 and the weaker repeated-resource03 are retained.
Scene/root/render/cache/INI/proxy boundaries remain supplied. No actual manager
traversal, game process, production cancellation or full M15S4 acceptance is
claimed by these instruction probes.


## Resident held cancellation and atomic contact publication

The service now zero-deactivates only resident held resources using prepared
cyclic source metadata. It clears pose/easing ownership and retains native
input, weighted/output, ease and resource history. Detached NI3 and inactive
cache entries survive untouched; the following frame continues their own view
clocks. Standalone slot reset leaves the process-action write to its caller.

Recoil/Stagger prepares the held reset in the same melee-state transaction as
the reaction/contact consequence. World collects source metadata for resident
held caches when preparing those requests. Invalid source packets fail before
publication, and a later held cancellation makes an old prepared contact stale.
BlockHit/Counter retain held ownership. A legacy mirror replacement also retains
the old FP held cache rather than replacing it with an empty incoming packet.

Schema61 now preserves the inactive 100/101 masks from original pending-NI5
cancellation. Twenty-five C++ binary/JSON witnesses reproduce byte-exactly through
Python under both configurations. 24 standalone cases cover view/history/pose/
anchor combinations; another case retains independent detached/inactive caches;
16 contact cases cover both participants, owner roles and Recoil/Stagger.

Completed checks: 382 Python, 2995 normal components, 848 ESM4* sanitizer components,
and 1828 engine tests in each configuration. The first engine builds were
intentionally stopped to separate same-line SCOPED_TRACE declarations; their
completed parser/component results are retained, without calling those overall
runs passing. Both final engine inventories pass. Virtual-index reconstruction
reproduces the exact initial fingerprint using only the two trace formatting
reversals, linking the unchanged production/codec sources to the prior checks.
No ASan/UBSan diagnostic observed; leaks disabled.

Final engine source fingerprint: `7709cc9b30aa708b712f5ee3a7096362882fc4556663d1355f635c70cd0c9f6b`.

[M15-HELD-BLOCK-CANCELLATION-VERIFICATION.json](M15-HELD-BLOCK-CANCELLATION-VERIFICATION.json)
links exact scopes, configurations, inventories, source relationship, native
basis and editable cases. Headless contact tests supply prepared metadata and
fresh reaction packets; automatic normal-input resource selection is not claimed.
Character hard interruption and World held begin/release wiring remain to be
connected. No new authoring/compiler/world plugin validation, windowed gameplay
or typed in-flight restart acceptance. The double-begin regression and full
M15S4 remain open.


## Typed held World/controller integration and actual cache restart

World now prepares held activation/release/cancellation from actual animation
resources, selected view clocks and root identities. Controller input uses the
typed cache, restores its resident tracks and leaves fading/inactive resources
to the independent native frame lifecycle. A reblock reuses the resource and
removes its former generic held fade, so expiry cannot disable a newly accepted
same-group held track and manufacture a second begin. Legacy action6 without
recoverable history keeps its prior playback until release.

Actual restart testing exposed a separate startup edge: the first mechanics
frame precedes Lua's queued OnActive event. Reading default controls there
released the saved block before its continuous policy supplied input. The
controller now binds saved ownership while local scripts await activation;
zero-duration renderer refreshes also defer input interpretation. It uses the
existing activation boundary, without adding saved readiness state or inventing
held loop history. Adding onLoad/onActive policy alone and a zero-duration-only
guard each failed the positive-duration startup case; those runs remain intact.

Both final engine inventories pass all 1,829 tests in 125 suites. The new headless
NPC/Player test rejects missing scene resources with the whole runtime snapshot
unchanged. All 382 Python tests pass after adding the editable runtime manifests.
Normal and ASan/UBSan resident courses each accept exactly one first begin and
zero restart releases/begins. Every epoch saves twice with exact binary payload,
decode/re-encode and live JSON equality. Initial NPC melee/cache and clock are
exact under declared schema60-to61 empty-cache migration or direct schema61
restore; the continuation loads the actual prior final save in a distinct PID/
start-tick epoch. Nine AV arrays, life, RNG, consumed action ledger/owners and
2,151 original nonempty reference inventories plus Player inventory stay exact.

A separate normal course starts from an actual resident typed save and supplies
block=false intent. One release produces a live NI3 duration 1 checkpoint and a
late inactive cache. A fresh process preserves every inactive cache field through
two more saves without another release or begin. Late animation API queries
report playing for residence and stopped for release; all six final screenshots
were inspected directly for guard-up versus lowered stance. These are scoped
stance observations, not image equivalence or original-game skin interpolation.

The first preregistration overreached by requiring the consumed Counter cache
to remain throughout both saved checkpoints. It naturally reaches End under
the already independently verified passive slot3 rule and retires; that failed
assertion is preserved, and production was not changed to suppress End. One
exploratory sanitizer first epoch also remains unaccepted because a subsequent
rebuild changed its binary path and the launcher's final pin check rejected it.
The final accepted sanitizer binary remains pinned across both processes.

Final engine tested fingerprint:
`7ac35dad74577bc00e9566965b1054c6b71df18de9af5a826c2ebe9286ba268e`.
The receipt separately hashes the exact four tested C++ files, later-added
manifests, Python report, engine inventories, runtime executables, inputs,
preregistrations, semantic verifiers, saved/live pairs and retained failures.
No ASan/UBSan diagnostic observed; leak detection disabled.

[M15-HELD-BLOCK-WORLD-VERIFICATION.json](M15-HELD-BLOCK-WORLD-VERIFICATION.json)
pins these layers and editable cases. This checkpoint accepts NPC body steady
residence and normal release/inactive restart at configured 10 FPS with normal
Lua intent and keyboard saves. Saved resident phases are NI1; no actual NI5
held windup/release-deferral, Player asymmetric view/camera, equipment break/
disarm, contact-driven typed reaction, full input/FPS/mastery/effects or original
visual parity acceptance follows. Custom INI and initial FP clock/root policy
remain open. Existing TES4 fixtures were reused: no new plugin/compiler/world
validation or TES4 MCP capability claim. Full M15S4 remains incomplete.


## Actual held NI5 persistence and release deferral

Delayed normal block input now exercises the captured-pose World branch after
camera setup. Both normal and ASan/UBSan courses actually save resident NI5 with
64 captured tracks, requested destination zero and a one-second native ease
span. Neither script supplies pose or animation outcomes. A distinct process
loads that exact early save: initial NPC melee/cache and clock are exact, the
early continuation save retains every original captured track, and the late
save reaches steady NI1 with the pose cleared and no new begin or release.

A normal false-input course starts from the actual saved NI5. After at least
two Lua updates supplying block=false, the early keyboard save still has
action6/NI5 and the original captured pose. One subsequent release anchors its
one-second retirement at or after the saved NI5 ease end, then expires inactive.
A separate process preserves every inactive held field through two further
saves, without another release or begin. This extends the previous steady
release course to the pending-transition input gate.

The three two-process courses provide ten exact actual binary/live pairs.
Independent decode/re-encode, initial cache/clock comparisons, process epochs,
actual prior-save hashes and executable pins pass. Combat action ledger/owners,
combat RNG, all nine actor AV arrays/life and 2,151 original nonempty reference
inventories plus Player inventory stay exact. All six final screenshots were
inspected directly for transition/guard-up versus lowered stance. The authored
`Blend: 30` held KF and its editable asset source are pinned in the new receipt.

No C++ or save-format change was needed after `5710d7ded2`. The four exact C++
hashes and both runtime executable pins match the preceding full 1,829-test
engine inventories; the preceding 382 Python tests remain their recorded scope.
New editable JSON parses and its exact Lua/input content ran in the recorded
courses. Builds and unchanged unit tests were not repeated. No ASan/UBSan
diagnostic observed; leak detection disabled.

[M15-HELD-BLOCK-WINDUP-RUNTIME-VERIFICATION.json](M15-HELD-BLOCK-WINDUP-RUNTIME-VERIFICATION.json)
pins the preregistrations, semantic verifiers, inputs, saves/live data, reviewed
images and editable cases. The schema60 source lacks persistent slot1 history;
its old generic held fade expires during the delay, and lost cached laps are
not reconstructed. These are NPC body persistence, phase completion and release
deferral observations at configured 10 FPS, not a matched uninterrupted
trajectory comparison, Player asymmetric view/input matrix or original-game
skin interpolation proof. Equipment/contact/mastery/effects and the remaining
S4 gates stay open. Existing fixtures were reused; no new plugin/compiler/world
authoring validation or TES4 MCP capability claim. Full M15S4 remains incomplete.


## Native held equipment callers and callback distinctions

The pinned original image now has bounded composed proofs for the equipment
paths that the typed held controller exposed. The blocking-wear caller executes
full5F4AE0(false), not a recorded release stub. Across 2,304 cases, absent-item
and false-condition-return controls do nothing; a reported break with active
NI5 or action2 leaves complete sequence bytes, action and pointers unchanged.
Steady NI1/action6 uses the active view's positive span. Player FP selection
immediately deactivates body; body selection fades both views. Counter cache
bytes remain exact. Release INI .25 is supplied; frame0 activation still uses
the separately identified compiled .1 selection default.

Disarm differs. Its post-drop caller first tries the full held release helper,
then falls back to actual5EFFD0(-1,0) on failure. The 1,152-case composition
shows that old6 without a shield uses positive release when steady, but zero-
deactivates resident body/Player FP when NI5 rejects release. Complete 0x70 held
bytes match the independent native offset/state/link/destination model. Old2
Counter action withdrawal preserves held/proxy/Counter bytes; old6 with a shield
keeps its posture. The action setter is not a plain process-field assignment.

The actual shield-unequip cleanup prefix has 576 cases: old6 zero-resets selected
slot1, then the full action setter clears the remaining body/Player FP resident
slot regardless of NI5. Old2 stays unchanged. There is no positive fade or NI5
gate in this prefix. Separate weapon callback/caller composition covers 4,608
cases with distinct Actor Biped and AnimData objects and both drawn states.
Actual4DC5B0/478CE0 preserve every held/proxy/Counter byte and ownership field;
original drawn predicates gate asserted pending actor notifications in body
AnimData or both Player views. The following wear helper retains its NI5 gate
or NI1 positive release. Empty Biped weapon attachments, an empty scene-root
child getter and a suppressed UI repair branch are explicit boundaries.

[M15-HELD-BLOCK-EQUIPMENT-ORACLE-VERIFICATION.json](M15-HELD-BLOCK-EQUIPMENT-ORACLE-VERIFICATION.json)
pins the image, preregistrations, accepted01/04/06/08 probes, corpora and editable
cases. Drawn-true07 is a subset of08. Attempt02 failed unsigned packing before
the tested caller; attempt03 revealed native setup cleanup that invalidated the
assumed plain setter. Weaker05 had no Biped pointers and skipped the claimed
Biped model/notification branch; its original report and limiting review remain
intact. Corrected07/08 require the actual body/FP notification fields.

These are original instruction observations with supplied condition returns,
post-drop admission, cache/proxy/scene/INI and other stated boundaries. Inventory
mutation, real weapon model removal, the complete shield callback, subsequent
actor update and normal-input gameplay are not accepted by them. No C++/codec
change, engine build or runtime course in this checkpoint. Production still
needs atomic held equipment effects and slot3 cache retention, followed by real
break/disarm/restart coverage; the existing unconditional break withdrawal and
generic hard equipment cancellation do not establish those native paths.
Full M15S4 remains incomplete.

## Atomic held equipment publication and weapon-break windup restart

Physical-contact preparation now derives typed slot1 effects from the actual
broken item or disarm, rather than unconditionally assigning action-1. Blocking
weapon wear retains NI5/action6 until the selected view can release; steady NI1
uses its native positive span and Player FP selection immediately resets body.
Shield cleanup zero-resets resident body/Player FP even during NI5. NPC disarm
without a remaining shield tries positive release, then zero-resets if NI5
rejects it. Old Counter2 withdrawal preserves the independent held history;
old6 with a remaining shield keeps its posture. Bound slot3 histories become
passive without changing cached sequence bytes. Recoil/Stagger preparation
preserves equipment effects already staged in either Player view and cancels
only remaining residents, so a detached positive fade is not resurrected or
zeroed by a later reaction in the same transaction.

World prepares renderer metadata and selected-camera/root leases before
publication. Missing, malformed, foreign or stale resources reject without
partial wear, inventory or action changes. Suppressed Player god-mode wear
requires no unused renderer sources. Unrelated legacy contacts do not query
an unknown process action. The shared held-release implementation and new
source packet add no persistent fields or save-schema change.

A real normal-input course exposed a remaining controller defect: contact
correctly retained action6 but the next equipment frame hard-canceled NI5 and
began a new hand-to-hand block. The controller now attempts the native action6
release and waits when the gate rejects it, retaining the saved group across
a fresh-process load. This is tested for the blocking-weapon break below;
complete original subsequent model-update behavior and every ordinary equip
change are not inferred from the earlier bounded callback oracle.

Both final full engine checks pass **1,835 tests / 125 suites**, with matching
inventories/XML, zero failures and zero skips. Six new tests contain102 table
contexts across NPC, body Player and FP-only Player, five held phases, shield
and weapon break, god mode, old BlockHit/Counter, actual prepared NPC drops,
Stagger and paired Player Recoil. They inspect exact caches, slot3 history,
conditions, AV arrays, ledger/RNG and binary service restore. Tested parent is
`6522b5fcb7bb4898d5fca230f3eda5889009e464`; both builds retain fingerprint
`f139f9221b4b7c1f3aa06a60fc03ab441912a535fa1d28c54fd5c2c2fe98c65a`.
All five changed C++ file hashes remain identical after those checks. Editable
manifests/metadata were added afterwards; the full Python suite passes382 tests.
Later documentation/status metadata does not change executable behavior.
ASan/UBSan halt on errors; leak detection is disabled, not covered.

Normal and instrumented two-process courses each pass three independent
binary/live save comparisons: immediately after the contact during NI5, early
after a fresh load still during NI5, then after deferred release during NI3.
The existing pinned near-broken sword input is unchanged. Only two authored
`Blend:240` KF keys create an eight-second observation window. Normal B block
and Space opponent activation produce one production-policy NPC strike, id2,
Fatigue cost7.8. The Player retains a broken, unequipped sword without a drop,
serial allocation or new reference. The66-track first-person captured pose and
its clock/cache load exactly, then release starts after the saved windup end.
There is one begin before restart, no new begin after restart, one later
release and no replayed contact. Exact Health damage is bits1065087140 from the
existing original PC24 corpus row `{3f813333,3ccccccc,3f7bf0a4}`; RNG becomes
159719620 after the two declared contact draws. Original populated inventories
are preserved apart from the declared weapon wear; added reference identities,
bases and CELLs independently match the pinned master. No fixed world-cache
size or newly materialized inventory parity claim. Actual runtime sanitizer
environments and executable hashes are retained. All four screenshots were
directly reviewed: sword absent with prior grip, then unarmed release. Configured
cap60 is not a measured60FPS or frame-rate matrix. No new PCM correlation or
original skin-interpolation acceptance is claimed.

The first normal/sanitizer suites retain two failures: the unnecessary legacy
action query and an assertion that omitted consumption of the current contact.
The exploratory runtime retains the controller failure and missing telemetry
settings; its waiting monitor was terminated after scenario exit, and it has
no accepted restart evidence. Fixture01 failed a footer-size assertion; fixture02
used Blend900, whose low byte132 yields4.4 seconds. Corrected03 uses240. Failed
verifier01 copied obsolete wider Health bits; the strict correction uses the
preexisting original PC24 corpus, without tolerance. Verifier02 assumed every
lazy inventory already existed in the source; its failure is retained with the
explicit populated-inventory and original-reference audit correction.

[M15-HELD-BLOCK-EQUIPMENT-VERIFICATION.json](M15-HELD-BLOCK-EQUIPMENT-VERIFICATION.json)
pins checks, sources, tools, actual processes, saves/live payloads, images and
failures. Editable cases and asset/scenario manifests are under
`scripts/data/oblivion_compat/m15_held_block_equipment_cases.json`. Structural
KF writes, engine parser/resource loading, semantic reinspection and actual
runtime/restart acceptance are separate layers. Existing declarative stationary
plugins are reused; no TES4 MCP authoring capability is implied. Normal-input
shield/disarm controller paths, Player disarm, custom blend INI import,
asymmetric views/camera, matched uninterrupted trajectories and the broader
mastery/effects/input/FPS/interrupt gates remain open. **M15S4 remains incomplete.**

## Actual NPC shield break during held windup

The unchanged758138f20b implementation now has normal and instrumented
normal-input NPC shield-break courses. Both use the existing pinned near-broken
shield save, an ordinary Player keyboard unarmed strike and public NPC block
intent with AI disabled. Only the original body BlockIdle Blend text changes
from6 to240; the clip's transforms, End, StopTime and key count are unchanged.
The eight-second captured interval supplies a precontact observation window,
not stock timing or autonomous defensive-AI acceptance. Identical Lua sources
are retained across each pair of process epochs.

Each first process saves a64-track `blockidle` NI5 pose, commits contact id2,
then saves the same cache inactive with its pose cleared and no retirement.
Every timing field matches the independent native zero-history expectation,
including unchanged old ease start/end. This happens before the saved NI5 end:
normal clock10.516003 and sanitizer10.682003 versus end15.361998. The complete
cache and NPC clock load exactly in a fresh process; inactive history remains
unchanged on resave, with no new block begin or contact. Each course supplies
three independently decoded binary/live pairs, six total. Exact Health damage
bits1065809137, block fraction.25, zero block Fatigue, Player attack debit7,
unchanged RNG505908858, next action3 and empty pending owners agree with the
existing independent shield consequence contract. Saved Fatigue permits its
declared real regeneration ranges; remaining raw AV channels and life entries
are exact.

One condition-zero Iron Shield is removed from NPC inventory and published as
one native reference/body, serial2 and one dropper relation. The right sword
stays equipped at condition55.875. Original populated inventories are compared
with exact item fields/counts/slots, normalizing stack order only. Added content
references independently match the pinned master identity, base and CELL.
The finite moving shield remains inside the existing placement bounds; there
is no settled-body, fixed cache-size or matched uninterrupted-trajectory claim.
All six screenshots were directly reviewed. A low shield beside the actor can
be a falling body, so those images alone are not attachment-removal proof;
inventory, reference and physical-body checks establish the stated semantics.
Original skin/model-update parity and new PCM correlation remain unclaimed.

The loader preserves exact native packed velocity, pose, root and reference.
Its redundant world-unit velocity is reconstructed by the existing packed
restore contract: `float32(packedLinear * 6.999040126800537)`, with packed angular
entries copied. One normal derived float changes by one ULP at load. The final
verifier requires that exact conversion, without tolerance, together with
unchanged packed data and held history. It does not call the redundant velocity
byte-identical. The prior full engine suites already cover
`PackedNativeVelocitySnapshotRestartsAndContinuesExactPhases`.

The first scenario retained an obsolete historical `melee finished` wait;
current owned Recoil replaces the strike and actually recovers the same cause
into action-1. It times out with only a precontact save and is not accepted.
Corrected02 waits for that actual same-cause recovery, with no gameplay change.
Failed verifier01 assumed source inventory stack order survived native class
storage; failed verifier02 assumed redundant world-unit velocity was the native
authority. Both sources and their strict contract corrections are preserved.
No production code changed in this checkpoint.

[M15-HELD-SHIELD-BREAK-RUNTIME-VERIFICATION.json](M15-HELD-SHIELD-BREAK-RUNTIME-VERIFICATION.json)
pins the four process epochs, six saves/live pairs, images, reusable tools,
contracts and failures. Editable scenarios, asset and case declarations are
under `scripts/data/oblivion_compat/m15_held_shield_break_cases.json`. Exact
executables/source hashes match758138f20b's accepted1,835-test/125-suite normal
and sanitizer checks and382-test Python run; those unchanged suites were reused,
not rerun. Actual runtime sanitizer options and first libasan preload are
verified; leak detection remains disabled. Structural KF edits, engine parser
admission, semantic audits and actual runtime/restart evidence are separately
recorded. Existing stationary plugins are reused, with no compiler/plugin
change or TES4 MCP capability claim. Player/asymmetric shield views, other
equipment/disarm, mastery/effects, input/FPS/interrupt matrices and full S4
remain open. **M15S4 remains incomplete.**

## Original composed Master blocking caller

A separate original-instruction composition now covers31,356 contexts of the
blocked-contact caller with actual Counter query, native rank and full defensive
disarm target admission. It confirms that Master retains the Expert Counter
roll first. Successful Counter selection skips immediate disarm/Recoil even
when playback fails. After a miss, playback(false) precedes the Master disarm
query; a second draw requires target eligibility and source gear/rank, while
source drawn is checked afterwards. Nonranged Recoil follows independently of
disarm success. Ranged, target action/shield, null drop and raw Player-byte
controls are included, with intact stack/register conventions.

The30,720 main cases plus636 controls produce8,664 Counter selections,
681 immediate disarms and945 two-draw cases. Original SHA256 is
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`;
the corpus is `ce14c7a521a7cc3aa4523310eb33cc687b7f24d736aa1776770e785718146f65`.
Actual base AV/equipment returns, RNG stream, playback, drop/relation/action/
release/controller callbacks and Recoil are declared boundaries. This is not
inventory, animation/resource, damage or normal-input acceptance. Supplied
thresholds25/5 reuse installed-record evidence, not a fresh live-setting probe.

[M15-MASTER-BLOCK-DISPATCH-ORACLE-VERIFICATION.json](M15-MASTER-BLOCK-DISPATCH-ORACLE-VERIFICATION.json)
and editable `m15_master_block_dispatch_native_cases.json` preserve the protocol,
source and results. No failures or production changes in this checkpoint, and
no engine/component/Python suite rerun. Production still rejects Master ordinary
blocking; integrating Counter-first and immediate disarm remains required.
**M15S4 remains incomplete.**

## Master defensive contact integration

The Master ordinary-block guard is replaced by Counter-first preparation after
armor-wear RNG. On a miss, eligible target equipment/drawn/quest admission
precedes the separate disarm roll; source drawn is checked afterwards. Selected
NPC disarms without spare weapons reach the outer World drop factory with final
contact wear before values, actions, reactions or RNG publish. Both disarm and
incoming weapon break project an empty Right slot for Recoil resource discovery.

Five new tests cover 24 paired defender contexts, six target/source admission
modes, selected Player/spare rejection without staging leaks, mandatory drop
factory ownership and owned final-wear scene/physics publication/binary restore.
Full normal and ASan/UBSan engine inventories each pass 1,840 tests / 125 suites,
with no failures or skips; this is engine integration evidence.

The selected Player/spare/Creature policies, zero-absorption unarmed defense,
projected Counter activation after defender break and complete S4 input/view/
recovery matrices remain open. **M15S4 remains incomplete.**

The intact-weapon Master shield course passes normally and under ASan/UBSan:
one contact, Counter miss, immediate NPC disarm, one shortsword body at final
condition 55.70000076293945, and exact RNG 1802518722 after six draws. Two
processes and three live/save wire pairs per build verify persistence without
replayed contact. Player Health has a bounded positive debit and exact retained
Damage; this is not a new exact numeric damage oracle. The authored StopCombat
command cancels Recoil, so uninterrupted recovery remains a separate gate.

The original armor selector independently verifies seed 4: four shield-only
selection draws, Counter 45 miss and Master disarm 4 hit. The failed seed-62
recipe, recovery-wait failure, debugger diagnosis and verifier corrections are
retained. The incoming-weapon-break course additionally exposes an existing
ordinary-dispatch Recoil suppression; World projected equipment support alone
does not close that branch. Normal NPC breakage must use its zero-condition
drop policy. The next chunk must fix and verify that dispatcher path.

[M15-MASTER-BLOCK-DISARM-VERIFICATION.json](M15-MASTER-BLOCK-DISARM-VERIFICATION.json)
pins the two accepted courses, full suites, source and 92 evidence artifacts.
Editable scenarios and `m15_master_block_disarm_cases.json` retain inputs,
expected values and boundaries. **M15S4 remains incomplete.**

## Incoming break and Recoil admission

The ordinary dispatcher and World preparation now both admit Recoil after the
incoming weapon breaks. Resource discovery uses the projected empty Right slot.
Counter-first selection sees no remaining weapon and consumes no additional
Master disarm roll. The NPC break policy creates one zero-condition drop.

Two new tests cover owned break/drop/Recoil publication with prior RNG and
binary restore, plus automatic World admission for retained Player and quest
NPC breaks. The normal and ASan/UBSan full engine suites each pass 1,842 tests
in 125 suites with no failures or skips. Automatic Player admission is not a
new paired Player broken-weapon rendering course. The current scope also leaves
uninterrupted natural Recoil and the complete S4 matrix open.

Normal and instrumented runtime03 each complete two processes and three exact
live/save wire pairs. The contact logs action 7, H2H Recoil, condition 0,
block fraction .75, block Fatigue 0 and five random draws. The save contains
one zero-condition NPC shortsword drop and final RNG 1136761003. Fresh load
preserves equipment, native body state, consumed contact and RNG; the second
process emits no repeated contact. Four screenshots were reviewed, with the
instrumented post-contact Player shield occluding the NPC body. The inventory,
sole native drop/body and exact persistence checks establish these semantics;
original attachment/skin update and matched/settled trajectory parity are not
claimed. Player Health remains a bounded single debit with exact persisted
Damage, not a new numeric damage oracle. StopCombat interrupts Recoil.

[M15-BROKEN-INCOMING-RECOIL-VERIFICATION.json](M15-BROKEN-INCOMING-RECOIL-VERIFICATION.json)
pins the tested source, full inventories and 85 evidence artifacts. Editable
`m15_broken_incoming_recoil_cases.json` and the incoming-break scenarios retain
normal-input expectations. **Full M15S4 remains incomplete.**

## Advanced unarmed zero-absorption route

A separate 46,080-case original-instruction composition verifies zero/positive
absorption routing, actual rank queries and Counter-first immediate disarm.
Unarmed Hand to Hand at least Journeyman routes into blocked dispatch even at
zero absorbed fraction. Below that rank, the separate Block Recoil query leads
into general-hit processing, which remains outside this campaign's boundary.
All asserted route/call/draw and stack/register conventions pass. Counts and
source are pinned in
[M15-ZERO-ABSORPTION-MASTER-DISPATCH-ORACLE-VERIFICATION.json](M15-ZERO-ABSORPTION-MASTER-DISPATCH-ORACLE-VERIFICATION.json).
The oracle itself adds no production admission or runtime acceptance. Its
advanced-route integration is recorded below; lower-rank general hit remains open.


## Advanced unarmed block integration

Bare defenders with base Hand to Hand at least Journeyman now retain full
Health damage at zero absorption while entering blocked Counter-first dispatch.
A separate ephemeral Recoil request keeps reaction admission independent of
absorption. Original Block-derived Fatigue costs remain intact. Below Journeyman
Block, the caller uses ordinary flesh/incoming-weapon hit sounds; at or above
that rank it uses two Hand block layers. A separate 216-case original instruction
composition verifies this sound split. General-hit force is skipped on the
advanced blocked route. No schema or default changes.

The normal and ASan/UBSan full engine suites each pass 1,845 tests in 125 suites,
with no failures/skips; leak detection is disabled. Three new tests cover 54
base-skill/modifier/cost/sound contexts, atomic full-damage Master disarm and
owned reactions, and ordinary selected-target dispatch without general-hit
force. Headless unit fixtures do not establish renderer resource acceptance.

Normal and instrumented runtime courses each pass two processes and three
exact live/save wire pairs. Actual bare Player HTH 100/Block 25 defends against
one stationary NPC sword attack: fraction 0, block Fatigue 0, two draws, ordinary
flesh/blade sounds and H2H Recoil. Original CRT seed 15 supplies Counter 87 miss
and inclusive Master disarm 5 hit, final RNG 334604033. One final-wear shortsword
drop retains condition 55.70000076293945. Fresh load preserves native actor and
body state, equipment, consumed contact and RNG without replay. Supplemental
exact-token/sound verifiers pass both courses. All four captures were reviewed;
the sword lies below the viewport, so the native drop/body and inventory checks
establish its consequence.

The bounded real-content Health debit and exact persisted Damage do not supply
a new exact numeric damage oracle. StopCombat interrupts Recoil, leaving natural
recovery and in-reaction restart open. Earlier facing/charge fixture failures,
const-accessor compile failures and intentional sanitizer interruptions remain
retained; they are distinct from the passing checks in attempt 04.

[M15-ADVANCED-UNARMED-BLOCK-VERIFICATION.json](M15-ADVANCED-UNARMED-BLOCK-VERIFICATION.json)
pins tested parent d61c3d755e and C++ fingerprint
0ed2a5395cae77c1f51fd62a15fe29e37fa3c28168eb516eefa52743718f9710,
full checks and 105 runtime/input artifacts. The independent sound receipt and
editable native/runtime case protocols accompany it. Lower HTH general-hit
processing, selected Player/Creature disarms, spare replacement, directional
power/effect paths, and the complete input/view/FPS/interruption matrix remain
open. **Full M15S4 remains incomplete.**


## Reference-root drift invariant corrected

A fresh 2,112-case original-instruction campaign verifies 64 sequences of
32 subthreshold position steps and one larger jump. Full scene traversal and
reference callback execute. Every sequence has masks [0 x 32, 1]; small local
movement accumulates beyond .1 while the published reference stays unchanged,
then the larger step publishes the current root. The current engine already
uses those per-frame masks. An unconditional .01 reference/root closeness
assertion in the natural-Recoil verifier was therefore invalid; its failure is
retained and exact reference restoration replaces it. No production change or
new runtime acceptance is claimed here.

[M15-REFERENCE-ROOT-DRIFT-ORACLE-VERIFICATION.json](M15-REFERENCE-ROOT-DRIFT-ORACLE-VERIFICATION.json)
and `m15_reference_root_subthreshold_drift_native_cases.json` retain the evidence
and editable protocol. **M15S4 remains incomplete.**


## Advanced unarmed natural recovery and post-recovery restart

Normal and ASan/UBSan courses now retain combat through natural Player BlockHit
and disarmed NPC H2H Recoil recovery. The Player recovers into held block/action
6; the NPC recovers into action -1. Second normal activation then stops combat
and cancels the automatically allocated action 3 windup before another contact.
Final ledger next 4 is empty, both reactions are absent, actor actions are -1,
and two combat draws/RNG 334604033 plus the sole condition-55.7 drop persist.

Each course passes two processes, three exact live/save wire pairs, native
state/body/packed-velocity and full reference restoration, no contact replay,
and exact contact/sound checks. All four captures were reviewed. Sanitizer
post-contact has an inactive retiring first-person release fade; both restart
captures show fists. The sword remains below the viewport and is established
by native inventory/drop/body checks. No new exact numeric real-content damage,
PCM, original skin or matched physical trajectory acceptance is claimed.

The initial content-fingerprint rejection, input-hash reader mismatch and false
absolute reference/root bound remain retained. Original callback evidence
controls the corrected invariant. This chunk changes no engine code and reuses
the unchanged full normal/sanitizer 1,845-test checks. Leak detection is disabled.
Editable fixture recipe, existing public script and tested scenario manifests
accompany
[M15-ADVANCED-UNARMED-NATURAL-RECOVERY-VERIFICATION.json](M15-ADVANCED-UNARMED-NATURAL-RECOVERY-VERIFICATION.json).
The receipt pins 106 artifacts. Active-Recoil save/restart and its uninterrupted
comparison remain open, along with the other recorded S4 gaps. **M15S4 remains
incomplete.**


## Active-Recoil logical restart

Normal and instrumented progressed Recoil snapshots at .1660003662109375 now
resume in fresh processes, alongside an instrumented time-0/easing boundary.
Each control saves before contact, during actual owned reactions and after
natural recovery; continuation uses the real middle save. Both Player reaction
views and NPC Recoil restore their exact native state and naturally recover.
A second focused activation cancels newly allocated windup 3; contact 2, its
RNG and final-wear drop never replay. Final ledger next 4 is empty and RNG
334604033, Player Damage, equipment and the single condition-55.7 drop match
logical uninterrupted outcomes.

All three forks pass four exact wire pairs each: six accepted processes and
twelve comparisons. Whole Player/NPC/drop facades, values/life, clocks, owned
caches and body/native packed velocity restore exactly at load. Actual game
compiler reports 9,993 units and no failures in each process. Nine captures
were reviewed; last-rendered save JPEGs are not exact phase/image evidence.

The empty-focus failures and strict-positive selector's rejection of a genuine
start-phase checkpoint are retained. Explicit mouse aim solves the activation
course; the zero-time and progressed profiles are reported separately. No
engine code changed; existing normal/sanitizer full 1,845-test checks are reused,
with leak detection disabled.

[M15-ADVANCED-UNARMED-RECOIL-RESTART-VERIFICATION.json](M15-ADVANCED-UNARMED-RECOIL-RESTART-VERIFICATION.json)
pins 176 artifacts and the retained failures; editable actual scenarios and
`m15_advanced_unarmed_recoil_fork_cases.json` provide the source. Camera and actor
placements differ under uncontrolled frame/wall timing. This closes a bounded
logical in-reaction restart case, **not matched physical continuation or full
M15S4**. Remaining lower-rank/general-hit, power/effect, selected Player/Creature/
spare and input/view/FPS/interruption gates stay open.

Supplemental checks match the entire encoded native load payload bit for bit
in all three fork profiles, including signed zeros. Only the documented loose
linear/angular velocity projection from authoritative packed state is applied
to the expected view. This strengthens load-state fidelity without asserting
identical subsequent motion. Receipts and replayable checks are pinned in the
active-Recoil restart verification index.


## Native save movement publication

Native saving now publishes completed movement/contact results before building
the save profile, screenshot or records. The scheduler waits for the submitted
frame and synchronizes it once; World moves actor references and loose poses.
It submits no new frame, advances no clock and preserves movement history.
Morrowind save preparation is unchanged. Native wire schema remains 61.

The new scheduler regression covers zero, one and two workers, two submitted
frames, fractional interpolation, preserved endpoints and repeated calls with
no extra body step. Full normal and ASan/UBSan engine inventories each pass
1,848 tests in 125 suites, without failures/skips, at source fingerprint
`6075b46dbb71d86a020fdab0b2f36c917bd2f837fdd523ea40b306c2af8be13b`.
Leak checking is disabled; UBSan halts on errors.

Normal and instrumented actual active-Recoil forks each pass four exact native
live/save pairs and an entire encoded load-payload check. Both select the real
middle save at NPC Recoil time .1660003662109375. Player/NPC reactions recover
naturally once; action next 4 has no pending contact, RNG 334604033 and the sole
condition 55.7 drop survive without replay. Four post images were reviewed.
The first normal control's second activation misses focus; its later new
contacts and Counter are retained as failed-course evidence. The instrumented
camera course waits actual save completion before its normal mouse input.

Three separate read-only save-return observations match Player/NPC reference,
simulation and decoded binary positions exactly. A separate first-five-entry
load trace observes desired movement during World discard and initial Actor
skip, before positions start advancing. These debugger runs are diagnostic,
with timing altered by pauses. They establish no original Havok parity.

Physical restart is still incomplete: both forks end NPC Y=66.3976058959961
versus uninterrupted Y=70.79055786132812. The active save now records Y=64.8759079
instead of the earlier Y=60. Living movement history/inertia/standing identity
and first-load priming remain open. The process clock is not serialized.
[M15-NATIVE-SAVE-MOVEMENT-VERIFICATION.json](M15-NATIVE-SAVE-MOVEMENT-VERIFICATION.json)
pins source, fresh checks, four accepted processes, eight wire pairs, the
retained failure and both read-only diagnostics. Editable manifests and case
protocol accompany it. The remaining S4 mastery/effect/input/view/FPS and
interruption campaigns are still required.


## Actor movement restart continuation

Schema63 retains backend capsule movement endpoints/history, force/inertia,
standing identity, ground/collision/skip flags and World movement discard, plus
optional mechanics initial-grounding bookkeeping. The scene-complete load hook
validates the whole batch before mutation and restores mechanics before
publishing discard. No process clock is serialized. Older absent snapshots keep
compatibility initialization; schema62 still decodes with unknown mechanics flags.

The first schema62 real fork restored its entire encoded payload but ended
NPCY67.78781127929688 versus70.79055786132812. A read-only hardware watch traced
the subsequent skip reset to Actors::update initial grounding. Preserving that
separate mechanics flag fixes the bounded case without inferring it from poses.
Debugger runs remain diagnostic because their pauses alter timing.

Full normal and ASan/UBSan engine inventories each pass1,850 tests in125 suites
at dirty-source fingerprint
`215bbc80872c4916c5b21bc925f4b60c682e51248839f86581cb803538445234`
(parent61f7acf6bf). Unchanged component checks pass2,997 full normal tests and
850 instrumented ESM4 tests; Python passes384 tests. The receipt distinguishes
completed component modes from their later failed/stopped engine attempts and
pins the audit proving the final correction changed only World test setup.
Leak checking is disabled; UBSan halts on errors.

Four accepted actual processes provide eight exact live/save wire pairs. Both
profiles load their actual middle Recoil output at time.1660003662109375 with
the entire encoded payload exact, including nine movement snapshots. Both
NPC post-recovery XYZ values exactly match their uninterrupted controls:
[672,70.79055786132812,-107.74292755126953]. Natural paired recovery happens once;
contact/RNG/drop are not replayed. Four post images were viewed.

This closes the bounded NPC course only. Player arbitrary final Z differs by
-.00151824951171875 normal and+.0509796142578125 instrumented; the wall/frame
courses do not establish matched-time Player trajectories. Original Havok,
skin/update, PCM and remaining input/view/FPS/mastery/interruption campaigns
remain open. Full M15S4 is incomplete.
[M15-ACTOR-MOVEMENT-RESTART-VERIFICATION.json](M15-ACTOR-MOVEMENT-RESTART-VERIFICATION.json)
pins the tested source, selected checks, actual saves, reports, editable course
manifests, retained failed attempts and diagnostic scripts.


## General-hit continuation boundary

A separate512-case original-instruction campaign executes5FFF52..6000B4.
A supplied nonzero byte4F suppresses both the visual callback and force. With
zero, the actual caller forwards five verified lanes through defender virtual398
after attacker vector and process310 queries, then runs the existing force path.
NPC/Creature, Speed, controller, damage, FatigueRatio, directions, both x87 words
and CPU conversion branches are crossed. The full force suffix agrees with the
retained independent force probe, including actual8907A0 acceleration/time stores.
The rendering callbacks and primitive getters remain explicit boundaries.

Private instruction review places general-hit reaction decisions before the
lower-skill block Recoil roll at5FFEDF, rather than after5FFF52. Suppression4F
initializes0 at5FECAD and can become1 after the facing predicate6131D0 at6004E0.
The earlier general reaction/effect decisions still require independent
composition and integration before lower-Hand-to-Hand admission. This campaign
changes no production behavior and closes no runtime gate.

[M15-GENERAL-HIT-CONTINUATION-ORACLE-VERIFICATION.json](M15-GENERAL-HIT-CONTINUATION-ORACLE-VERIFICATION.json)
pins the hash-identified executable,512-case corpus, successful script,
preregistrations and retained fixture failures; the editable case matrix is
`m15_general_hit_continuation_oracle_cases.json`. Full M15S4 remains incomplete.


## Ordinary-hit common reaction composition

A new immutable numeric decision layer composes the original ordinary-hit
caller5FFEB5..5FFF52: damage flag, otherwise-unused Expert mastery draw, hit
speech, Stagger/visual-force suppression, then lower-HTH Block Recoil. It
consumes an explicit prefix of raw CRT draws and returns separate decisions;
no live RNG, resource, renderer or contact admission is changed here.

All4,808 independently executed original cases match normal and ASan/UBSan
production-object comparisons, including negative controls for a wrong expected
Stagger, missing row and invalid raw draw. Four new component regressions cover
ordering, threshold/raw speech and winning settings, invalid data, and post-hit
Health/entry identity. Full normal components pass3,001 cases; instrumented
ESM4 components pass854, with no failures/skips. Source fingerprint:
`5a67b20804a195d5c0f42e0afbf0c4bd29aaccc875d221efc37da420c6eaddcd`
(parent5f3d997f5f). Leak checks are off. Engine/runtime checks are not repeated
for this numeric-only chunk; no new E2E gate is claimed.

Speech uses post-contact current integer Health, not base Health. Threshold
equality consumes a draw; speech uses raw/32767, while percentile rolls use
raw%100. Compiled chance.35 differs from winning Oblivion.esm00088FFE chance.2;
threshold00088FFF is.1. A pre-scaling damage path can reach the resource sink
with both arguments zero and still perform its reaction roll. Zero and negative
post-integer Health preserve original ordered comparison behavior.

The common stage precedes both blocked and lower-skill routing. Current runtime
omits it: earlier bounded advanced-block/Counter/disarm courses do not prove
whole-caller RNG parity. Integration must prepare this stage ahead of both
paths, preserve independent Stagger slot0 and later slot3 ownership, stage
post-damage queries and speech/notification consequences, then revalidate the
full ordinary/block courses. Removing only the lower-HTH rejection is insufficient.

[M15-ORDINARY-HIT-REACTION-VERIFICATION.json](M15-ORDINARY-HIT-REACTION-VERIFICATION.json)
pins source, final checks, original observations, all retained fixture failures,
content/settings audit and both comparison profiles. Editable
`m15_ordinary_hit_reaction_oracle_cases.json` records the case groups and
remaining integration requirements. Full M15S4 remains incomplete.


## Blocked playback after common Stagger

Full original5F4FD0 activation followed by actual5F4E10 verifies128 paired
Player contexts and256 BlockHit/Counter requests. High-process virtual2D0
returns action8; the blocked-playback helper exits before resource lookup,
playback, RNG or notification. Actor/process/view/sequence bytes remain exact
for both request arguments and the Stagger sequences continue their own clocks.
Prior actions2/6/7/8, both cameras, four first-person clocks, prior first-person
slot3 presence and both x87 words are crossed. Scene and notification boundaries
remain supplied; no gameplay/render/PCM acceptance is added.

This refines the common-hit integration: retain Stagger slot0 and existing
passive slot3 when later blocked playback is suppressed by action8. Keep the
Counter query outcome separate from actual Counter activation, since the
blocked caller skips subsequent disarm/Recoil after a successful query even
when playback returns early. The guard campaign does not itself execute that
Counter query or defensive disarm. No new slot representation is needed solely
to manufacture a BlockHit over this guarded action.

[M15-STAGGER-BLOCKED-PLAYBACK-GUARD-VERIFICATION.json](M15-STAGGER-BLOCKED-PLAYBACK-GUARD-VERIFICATION.json)
pins the original executable identity, complete probe and corpus. The editable
case protocol accompanies it. Production common-hit integration and full M15S4
acceptance remain open.


## Prepared post-contact hit queries and blocked continuation

Prepared contacts now expose a copied, validated snapshot of post-resource
current integer Health/Fatigue/Agility/Luck and the post-Fatigue ratio. The
query uses staged block spending and hostile resource changes without publishing
them. Exact NPC form Health above binary32 precision remains intact; Player
composition, god mode and Creature Fatigue policy match the commit path. Stale,
missed, committed and destroyed-owner plans have no queryable victim. No RNG,
callback, wire change or new automatic contact admission is added.

Full normal and ASan/UBSan engine inventories each pass1,851 tests in125 suites
with no failures/skips at fingerprint
`f87b68d24317a8502b79faac7d2a7e95845cbe3a4e5d36fa15b5f323db0442c9`
(parentad624d679b). Tests establish fractional pre-commit visibility, unchanged
live/serialized state, exact integerHealth16777213 after fractional damage to
formHealth16777217, post-commit equality and expired lease rejection. The five
numeric helper/component-test source pins from the prior checkpoint remain
unchanged. Earlier original fixtures supplied decimal14/15 while naming
ordinary groups0x14/0x15. A fresh corrected4,808-case native run explicitly
uses decimal20/21 and yields a byte-identical numeric table; this preserves
the normal/SAN C++ comparison evidence. The old receipt now records the
correction and retains its earlier artifacts. Leak checking is off; UBSan halts on errors.

A separate512-case original instruction composition now joins the common hit
resource prefix and actual paired Stagger activation to full Counter and Master
queries. Counter is selected256 times but activates zero times: action8 guards
blocked playback. Counter success skips Master/Recoil; misses make256 Master
queries and256 attacker Recoil requests. Eligible Master paths take16 draws and
make8 drop requests, with4 null and4 nonnull supplied sink returns. These are
request/order observations, not actual dropped-item acceptance. Player action8,
Stagger slot0 and existing slot3 bindings persist. Earlier combined-fixture
errors are retained. Primitive/resource/speech/target-drop/target-action/scene
boundaries exclude real damage, inventory publication, rendering and gameplay.

[M15-PREPARED-HIT-VALUES-VERIFICATION.json](M15-PREPARED-HIT-VALUES-VERIFICATION.json)
pins code, final engine checks, the original composition and editable case
protocol. The next integration must use staged values, keep Counter selection
separate from playback admission, and respect speech/sequence-selection RNG
between the common prefix and later defensive rolls. Full common-hit runtime
publication and M15S4 E2E acceptance remain open.


## Staged Stagger through defensive Counter selection

World preparation now retains a prepared victim Stagger when the defensive
Counter query succeeds. The query returns selection separately from activation:
action8 suppresses BlockHit/Counter playback, while a selected Counter still
skips Master disarm and attacker Recoil. A Counter miss preserves Master drop
and attacker Recoil preparation. Existing random transitions remain one packet;
no new action owner or save format is introduced.

Full normal and ASan/UBSan engine inventories each pass 1,853 tests in 125 suites
without failures or skips at fingerprint
`20bed3fd57a4762186bf9eaa46fc9d61711ecdae79dc3f4c71894fc80d47d4df`
(parent b0bf624392). New tests cover paired Player Stagger, prior draws, selection
and miss, stale-plan rollback, exact RNG/save persistence, and an NPC physical
drop with attacker Recoil. The paired Player fixture has a headless NPC Recoil
boundary; the drop transaction uses owned renderer/physics fixtures. An initial
test macro compile error and the deliberately stopped matching sanitizer attempt
are retained. Leak checks remain off.

[M15-STAGGER-BLOCKED-WORLD-VERIFICATION.json](M15-STAGGER-BLOCKED-WORLD-VERIFICATION.json)
pins this bounded integration and the earlier 512-case native composition.
Callers still supply the common Stagger. Automatic common-hit/speech publication
and normal-input/fresh-process M15S4 acceptance remain open.

A separate full original INFO selector probe passes 10,240 cases. Accumulated
raw-bit2 candidates prepend, reversing acceptance order; a nonempty set consumes
one modulo draw even when it contains one entry. The first forward-order
expectation was falsified and retained before registering the correction.
[M15-HIT-INFO-SELECTION-VERIFICATION.json](M15-HIT-INFO-SELECTION-VERIFICATION.json)
pins that probe and its editable case protocol. Conditions, raw RNG and node
allocation are supplied boundaries. Serialized flag mapping, actual voice/audio,
result scripts and production integration are not established by this probe.


## Live dialogue candidate selection and shared native RNG

Say/SayTo voice lookup now accumulates random INFO candidates, honors accepted
RandomEnd/nonrandom stop boundaries, and selects in reverse acceptance order
using one native modulo draw. A singleton random candidate still draws; an
immediate nonrandom match and an empty set do not. Exact voice-file resolution
follows selection. The scalar draw shares the saved physical-contact authority.
An engine test verifies seed-1 values 41/18467/6334, agreement between staged and
immediate cursors, and binary save/restore replay.

The production selector matches all 10,240 recorded original cases in normal
and ASan/UBSan comparisons: selected INFO, draw count and condition visitation
order. Three component tests cover lazy traversal and invalid-draw boundaries.
Normal components pass 3,004 tests; the sanitized ESM4 selection passes 857.
Both full engine inventories pass 1,854 tests in 126 suites. Passing component
results are reused after correcting only the new engine fixture's required
Player reference, cell, race and class fields. The failed fixture attempt and
matching deliberately stopped sanitizer build are retained. Earlier wrappers
were interrupted while separate CMake groups remained live; the normal build
finished before its fresh wrapper started. No sanitizer defect was found.

A separate 1,536-case original DATA dispatch slice confirms that three bytes
load into INFO offsets 0x23/0x24/0x25: serialized flag byte 3 is native byte 0x25.
The file-read boundary is supplied; full record registration is not exercised.
An installed-plugin inventory finds 105 Hit INFOs, all in Oblivion.esm, without
nonempty result bytecode. This is not an active winning-load-order audit.
Quest ordering, SayOnce/history/link policies, real conditions and audio remain
separate from the supplied-admission selector comparison. One installed Hit
condition uses GetRandomPercent, whose existing AI resolver still uses
stableChoice. A 131,072-case original instruction run verifies both condition
and script entry paths across every raw draw and both x87 words: each consumes
one draw and stores exact double raw modulo 100. Raw RNG is supplied and console
printing disabled; condition-expression evaluation is not exercised. That
condition must join the native cursor for implicit combat speech parity.

[M15-DIALOGUE-INFO-SELECTOR-VERIFICATION.json](M15-DIALOGUE-INFO-SELECTOR-VERIFICATION.json)
pins code and evidence. The attempted real-content scripted audio regression
is not accepted: its original total deadline was 15 seconds before a 31-second
first sleep; a retained retry with a startup wait and 150-second deadline logged
uninitialized native Player values and returned to the title loop. Neither
process reached voice lookup. Fatal shutdown errors and failed reports remain
in the evidence; owned leftover crash reporters were closed. Native Player
setup/the older recipe must be reconciled before this regression can pass.
Automatic common-hit speech/resource publication and full normal-input and
fresh-process M15S4 acceptance remain open.


## Dialogue condition order, RNG and prepared resource projection

Original condition-list execution exposed two errors in the portable evaluator:
the current row's OR flag joins the following row, and a failed AND group must
stop before evaluating any subsequent predicate. True OR groups skip remaining
predicates. The corrected evaluator matches all 682 original Boolean results
and visitation sequences in normal and ASan/UBSan comparisons. Full original
56A510 runs over lengths 0–4, all OR/truth masks and both x87 words, with Boolean
predicate evaluation supplied. Diagnostic statuses and secondary-list
composition remain outside this probe; portable status policy has separate
tests. Earlier fixtures encoded the wrong previous-row rule and are corrected.

Dialogue conditions and script-command GetRandomPercent now use a shared native
raw sample modulo 100, replacing stableChoice and World PRNG calls respectively.
A preparation context supplies its own raw draw provider and actor-value
projections. A reached random condition cannot silently use live RNG when that
provider is missing. Prepared contact queries expose enabled participants'
staged Health/Fatigue float getters after attacker cost, block spending and
damage, without publication. Stale, missing and committed plans have no
projection. Other AVs and disabled raw-form queries retain their existing paths.
Dialogue binds the supplied listener Ptr directly, preserving valid draft and
dynamic targets that cannot yet be resolved through the AI registry.

Normal components pass 3,006 tests; the sanitized ESM4 selection passes 859.
Both full engine inventories pass 1,856 tests in 126 suites. New cases establish
immediate condition results 41/67 followed by script result 34, staged modulo
values, missing/invalid provider rejection, skipped random predicates, subject
and target post-hit conditions, unchanged live/save bytes during preparation,
same-packet RNG/resource publication, and stale-plan rejection without draws.
Existing Player god-mode and Creature Fatigue matrices inspect these resource
projections too. Leak checks remain disabled. No new rendered, audio or
normal-input gameplay course was run.

Engine builds were deliberately stopped after passing component checks to
include the script caller and its test before committing. Unchanged component
results are reused. The first full normal engine run then exposed registry
re-resolution losing the staged test's listener and returning MissingContext.
That failure and the matching deliberately stopped sanitizer build are retained;
both final engines pass after binding the supplied target.

[M15-DIALOGUE-CONDITION-INPUTS-VERIFICATION.json](M15-DIALOGUE-CONDITION-INPUTS-VERIFICATION.json)
pins source, full checks, original list execution, comparisons and the editable
case protocol. The preceding receipt supplies the original 131,072-case random
condition/script proof. Generic package random policy remains separate.
Automatic common-hit speech/publication, quest conditions, INFO history/results
and full M15S4 normal-input and fresh-process acceptance remain open.


## Quest dialogue condition decoder and composition

Native QUST root CTDA/CTDT rows now have a canonical condition vector with
stable FormKeys. INDX, QSDT and QSTA end the root scope, keeping stage, log and
target conditions out of dialogue eligibility. The existing raw sequence
remains available. Native CTDA must contain 24 bytes and legacy CTDT 20 bytes.
The typed esmtool dump exposes ordered numeric fields and resolved keys.
An actual ESMStore regression covers compressed records, reordered masters,
winning overrides, scalar/global/reference preservation and deletion.

Original INFO admission at 530830 and list evaluator at 56A510 establish quest
conditions before INFO conditions. A quest OR group can cross into INFO, so
separate evaluations joined with AND are incorrect. The new composition helper
plus production evaluator matches all 14,450 Boolean cases and exact predicate
visitation in normal and ASan/UBSan comparisons. The initial INFO-first
hypothesis was falsified by visitation despite matching Boolean output; the
failed expectation and corrected preregistration are retained. Predicate
results are supplied at the declared native boundary; this does not prove
actual predicates, INFO history, result scripts or audio.

Independent raw decoding of base Oblivion.esm agrees with the final typed dump
on 390 quest keys and 719 root conditions, including count, order, byte size and
every raw numeric field. The raw walk classified 3,784 total quest condition
rows. This is a base-file comparison, not a winning installed-override claim.
The complete official M14 and M15 inventories/count locks pass separately.
These inventory checks do not establish native predicate semantics.
The typed comparison also extends to every quest record in all 11 official
plugins: 416 quest records, 726 root conditions and 4,080 total quest condition
rows match independent raw decoding, with header-master-based identities.
This is a per-file comparison, not a winning-override semantic merge.

Normal components pass 3,009 tests; sanitized ESM4 suites pass 862. Both full
engine inventories pass 1,857 tests. These are decoder, store and automated
engine validation layers. No compiler/world gameplay, rendered audio, normal
input or fresh-process acceptance is added. Leak detection is disabled.

[M15-QUEST-DIALOGUE-CODEC-VERIFICATION.json](M15-QUEST-DIALOGUE-CODEC-VERIFICATION.json)
pins source, tests, native composition, retained falsification, comparisons and
the editable case protocol. Live Say/SayTo admission remains pending predicate
authority fixes; this chunk adds the canonical data and composition helper.
Automatic common-hit speech/resource/condition preparation and publication,
INFO history/results and complete M15S4 gameplay/restart acceptance remain open.


## Native Player admission before first-person construction

A fresh debugger trace identifies the new-game failure before any audio course:
first-person NpcAnimation construction queries the native process action, which
reads Player values before Actors::addActor admits them. World::startNewGame now
uses normal native admission immediately after setupPlayer and before rendering.
Initial native construction already supplies action -1 and knocked state 0.

Both full engine inventories pass 1,857 tests after this change. The instrumented
new-game frontend and a distinct normal process loading its save each reach five
actual Player Lua frames, save through F5, render the prison/HUD and exit cleanly
with zero engine errors. The observation module only counts Player frames;
official record plugins are unchanged. Independent Python save decoding and the
actual load-complete live snapshot establish exact native Player storage bits
and shared RNG cursor at load, plus selected Player identity, inventory, base
slots, form contributions, passive ledger and process metadata on resave. This
is bounded startup/persistence acceptance, not a full S4 combat trajectory.

The full scripted audio regression still fails. The first wall-clock wait ended
before later stages; a retained retry waits for the final actual stage without
removing expected command checks. SayTo, sound and weather execute, but Say has
an unresolved speaker and stops on missing INFO condition context. The scripted
course also reports vanilla VM numeric and actor-initialization diagnostics.
Those failures are retained separately from the positive clean boot/load course.
A first restart readiness check incorrectly waited for music with sound
disabled; the corrected manifest waits for five actual Player frames.

[M15-PLAYER-ADMISSION-VERIFICATION.json](M15-PLAYER-ADMISSION-VERIFICATION.json)
pins source, both engine checks, debugger evidence, captures, saved/live states,
semantic comparisons, editable manifests and retained failures. Leak checks
remain disabled. Automatic common-hit integration, quest predicate authority,
INFO history/results, general Say context and complete M15S4 normal-input and
fresh-process combat acceptance remain open.


## Native Say process admission

The original Say wrapper returns zero duration before voice selection for an
actor with no process or a Low, MiddleLow or MiddleHigh process. Full wrapper
execution with actual CRT RTTI and native process virtuals passes 384 cases
across Character, Player and Creature, optional flags/voice override and both
x87 words. Only argument extraction is supplied. No INFO, condition or RNG
entry occurs; callee registers, stack, SEH head, fixture bytes and global
B36798 are preserved. Its broader identity is not assigned here. High process
speech dispatch remains outside this oracle. The initial missing Windows
thread fixture and later parameter-pointer correction are retained.

Independent record inspection establishes that MS48 stage 70 addresses remote
actor 028A7A and topic GOODBYE (0000D4). The prison course has no resident Ptr for
that actor. Say now returns zero for this known native actor case before
INFO/condition/RNG queries, instead of throwing MissingContext or selecting an
arbitrary voice. It does not blanket-skip nonactors or SayTo. An actual-store
regression verifies cold NPC/Creature zero results without actor-state/RNG
changes, while resident actors still select the loaded Random candidate and
consume both expected draws before the empty voice VFS stops playback.

Both full engine inventories pass 1,858 tests. The actual shipped command
course passes all existing SayTo, sound, weather, save and exit checks, plus
the explicit remote Say zero trace and absence of a null-speaker INFO lookup.
Mixed audio is valid non-silent 48kHz stereo float WAVE, and the prison/HUD
capture was visually inspected. This establishes remote Say skipping and
resident SayTo playback, not actual resident Say playback or isolated audio
equivalence. The course still reports vanilla numeric and actor-initialization
diagnostics; they are enumerated and remain open. Earlier failed courses are
retained. The implementation does not invent a missing speaker or force it
into an active process.

[M15-SAY-PROCESS-GATE-VERIFICATION.json](M15-SAY-PROCESS-GATE-VERIFICATION.json)
pins source, full engine checks, native oracle, immutable retained runtime
binaries, course/capture/audio/save evidence and editable protocols. Component
code is unchanged from the preceding decoder checks. Leak checks are disabled.
Root quest predicate authority, automatic common-hit speech/resource/condition
preparation/publication, INFO history/results and complete M15S4 normal-input
and fresh-process combat acceptance remain open.

### Native dialogue realm and playable race prerequisite

The script and dialogue condition for `GetPlayerInSEWorld` now share saved
Player realm authority with bounty routing. `GetIsPlayableRace` conditions read
the native race flag and the Player's current race proxy. Both engine profiles
pass 1,860 tests; normal components pass 3,009, instrumented ESM4 components 862,
and Python 384. The rendered command course passes both no-argument realm
queries and its existing audio, weather, and save checks; the resulting save
retains the observed false realm flag. See
[M15-NATIVE-DIALOGUE-REALM-VERIFICATION.json](M15-NATIVE-DIALOGUE-REALM-VERIFICATION.json)
for source, independent native observations, content lock correction, retained
fixture failures, and runtime limits. Quest root admission and full common hit
speech and reaction integration remain incomplete; this does not close M15S4.

### Live quest eligibility and native NPC/creature faction queries

Voice selection now evaluates quest roots before INFO conditions. Native NPC
and creature faction queries use their base memberships with the original first
signed-rank rule. The Player faction adapter is unchanged. The pure rule matches
196,608 original observations in both comparison profiles; focused and full
engine/component checks pass. Both engine profiles execute 1,862 tests; normal
components execute 3,010 and instrumented ESM4 components 863.

The rendered command regression passes. Independent raw record reinspection
shows quest terms 365/254 preceding INFO term 72 for the actually voiced
CharacterGen response, matching runtime visitation and the expected audio clip.
The save and two realm queries agree. See
[M15-QUEST-LIVE-ADMISSION-VERIFICATION.json](M15-QUEST-LIVE-ADMISSION-VERIFICATION.json)
for exact source, comparisons, diagnostics, captures, and scope limits. Automatic
common hit speech/reactions and the remaining M15S4 acceptance cases stay open.

### Ordinary-hit phase contract

The numeric API separates damage/speech decisions, current-query Stagger
selection and the lower block tail, allowing caller-owned speech/resource RNG
between them. Fresh normal and sanitizer objects match 4,808 contiguous and
768 composed original cases; premature-tail and stale-knocked controls fail.
All 3,013 normal components and 866 instrumented ESM4 components pass. See
[M15-COMMON-HIT-PHASES-VERIFICATION.json](M15-COMMON-HIT-PHASES-VERIFICATION.json).
These APIs have no production caller yet. The live hit pipeline and full M15S4
acceptance remain open; this numeric contract adds no gameplay acceptance claim.

### Ordinary-hit caller argument preservation

The ordinary melee producer now carries pre-difficulty Health, normalized
Fatigue, pre-armor damage, the used skill index and pre-scaling damage-path
selection in an ephemeral contact packet. Raw active-block and bare-hand flags
are captured before facing and preserved across condition/equipment changes.
Original instructions pass 2,016 cache cases, ten NPC/weapon skill-index
observations and ten Creature bypass cases. Creature skill metadata stays
explicitly unavailable until its later native query is established.
Both normal and ASan/UBSan full engine inventories pass (1,862 tests each).
See [M15-COMMON-HIT-ARGUMENTS-VERIFICATION.json](M15-COMMON-HIT-ARGUMENTS-VERIFICATION.json).
The common phase APIs still have no live consumer; no new gameplay/restart gate
is claimed. Unsupported contact domains and remaining M15S4 gates stay open.

### Native dialogue class conditions

GetIsClass68 now reads native NPC class identity and the Player's current class
proxy in live dialogue; unloaded package conditions use the same predicate.
A native CLAS parameter is required. Both full engine profiles pass 1,864 tests;
388 Python tests pass, including the bounded fixture writer. The independent
original predicate passes 288 cases with actual base getters and no callee stubs.

A rendered course loads one INFO override with an added class condition while
retaining its original response data. Independent record reinspection and live
visits agree on `365 → 254 → 72 → 68`, class `0x836` and original response `0x150C7`.
The audio/weather/save course passes in 131.029146 seconds; the reviewed capture,
save schema 63 and 107 existing runtime diagnostics are retained. Compilation reports
10,470 units with zero compilation failures. The two failed cold-fixture checks
and two pre-content configuration failures remain recorded. See
[M15-NATIVE-DIALOGUE-CLASS-VERIFICATION.json](M15-NATIVE-DIALOGUE-CLASS-VERIFICATION.json).
This validates the class/voice path, not automatic melee hit speech or the
remaining normal-input/restart gates. Full M15S4 remains open.


### Persisted native dialogue history prerequisite

Runtime schema 64 now retains a global set of spoken INFO identities. C++ and
Python use the same counted, canonical wire tail; older schemas begin empty and
cannot silently discard nonempty history on downgrade. ScriptManager capture,
clear, retained-runtime exchange and prepared restore share this authority.
Unknown INFO definitions reject restore before publication.

The original executable storage probe passes 6,186 cases: 6,144 empty-condition
admission observations, 36 save/load/revert calls and six constructor slices.
History is represented by change-flag presence; its native save method writes no
additional history bytes. This does not establish the full native ESS container.
Normal components pass 3,015 tests, sanitizer ESM4 components pass 868, both full
engine profiles pass 1,865 and Python passes 390. Initial fixture-offset failures
remain retained. See
[M15-DIALOGUE-HISTORY-STORAGE-VERIFICATION.json](M15-DIALOGUE-HISTORY-STORAGE-VERIFICATION.json).

Speech eligibility and result callbacks do not consume this authority yet.
No rendered speech/history restart course or additional M15S4 gameplay gate is
claimed. All 105 winning installed Hit INFOs have no NAME additional-topic
entries; this inventory does not implement generic AddTopics effects.


### Scripted speech response and spoken-history integration

Loaded Say/SayTo now selects INFO identity before audio resolution. Deleted,
null-quest and spoken SayOnce entries are rejected before condition/RNG visits.
The response callback precedes voice resolution/playback, so missing audio still
runs result/history effects. RunImmediately suppresses that callback; Refusal
suppresses marking. Disabled/missing/failed result code still permits history
marking after the VM attempt. The public VM-only result utility is unchanged.

Production helpers match 38,912 original instruction observations, with both
false-policy negative controls rejected. Full normal components pass 3,016 tests,
sanitizer ESM4 components 869, both full engine profiles 1,866 and Python 393.
Retained compile/predicate fixture failures and a deliberately aborted stale
sanitizer run are distinguished from the final matching-source passes.

An editable fixture preserves the already-SayOnce INFO `0x150C7` response bytes
and flags while assigning an isolated topic/quest. Independent raw reinspection
and typed parsing pass. A rendered 63.142601-second process speaks its original
2.2205-second line once, then filters it. An 81.698547-second fresh process loads
a protected copied save, returns zero for both Say calls and resaves the same
schema-64 history without replay. The PIDs/epochs differ; binary/live histories,
pristine save hashes, loaded archives, audio and reviewed captures are retained.
Both processes compile 10,470 units without failures and have zero ScriptManager
diagnostics. Separately, startup observation records 31 deferred commands in
unrelated quests (AddTopic, SetUnconscious, RemoveSpell, ModDisposition); the
continuation has none. These remain visible limitations, not speech failures.

See [M15-SCRIPTED-SPEECH-HISTORY-VERIFICATION.json](M15-SCRIPTED-SPEECH-HISTORY-VERIFICATION.json).
INFO AddTopics effects remain outside this change; the 105 winning installed Hit
INFOs and this fixture have no NAME entries. Automatic common-hit speech wiring
and the remaining normal-input/opponent/mastery/restart gates are still open.


### Transient scene complexity and frame ownership

`SetSceneIsComplex` now writes the active native combat authority. The next
physical frame consumes the previous value, limiting fixed physics substeps to
two rather than three; the engine clears it before script/actor processing,
including frames with scripts disabled. Command statements now pass signed
arguments to their callee; set/if arithmetic keeps expression subtraction. The
independent Python frontend follows this grammar. The flag follows retained
simulation exchange and is absent from save state; schema 64 is unchanged.

The hash-identified original executable passes 270 full setter cases and 1,536
three-frame caller/reset cycles. Production results match all 4,608 frame
observations exactly. Missing-reset and ignored-limit controls fail 1,032 and 296
frames respectively. Both full engine profiles pass 1,869 tests; sanitizer leak
detection is disabled. Normal components pass 3,017 tests, sanitizer ESM4
components pass 869, and all 21 ObScript sanitizer cases and 394 Python
tests pass. All 11,098 official units compile with zero frontend failures; the
independent Python AST/IR comparison agrees. The changed grammar affects 401
portable units while original source/SCDA identity remains unchanged. Compile,
parser and build-launch failures are retained.

Rendered 5/60 FPS courses issue signed values 1, -1, 0 through the loaded command
path. Each has exactly two following-frame limit events, no later flag leakage,
and a normal F5 schema 64 save. Both reviewed captures show the prison scene,
HUD and minimap. Both compile 10,470 units without failures or ScriptManager
diagnostics; each separately records 31 unrelated deferred startup commands
(AddTopic 24, SetUnconscious 5, RemoveSpell 1, ModDisposition 1).

See [M15-SCENE-COMPLEX-VERIFICATION.json](M15-SCENE-COMPLEX-VERIFICATION.json).
The original rendering byte BA7A04 remains outside this mechanical implementation.
These courses do not close normal-input melee or fresh-restart gates. Automatic
common-hit speech and the remaining M15S4 delivery gates are still open.


### Native quest groups and INFO predecessor order

Dialogue queries now consume a content-load index that replays INFO revisions
in file order. Quest groups retain first-seen order on equal priority and sort
by the winning unsigned priority. Missing PNAM appends only a new entry; explicit
zero inserts first. Linked overrides clear the stored ordinal before resolving
the predecessor and compact at the native point. Deleted INFO memberships stay
in the index while live lookup rejects their missing definitions; deleted
topics return before visiting arrays. Each eligibility visit uses its owning
quest group, including duplicate INFO identities.

Stable QSTI/PNAM keys are decoded before global index remapping, preserving null
and absent-field distinctions. The original executable passes 781 priority
cases and 1,503 insertion/loader sequences. All 18,799 production comparisons
match; three false-policy controls are rejected. Independent raw/typed audits
match all 20,338 INFO revisions across the official files and authored fixtures.
Both full engine profiles pass 1,871 tests; normal components pass 3,021 and
sanitizer ESM4 components pass 873 with unchanged component inputs. All 397
Python tests pass. Sanitizer leak detection is disabled.

An editable fixture authors four separately identified voiced INFOs and a
priority/INFO override patch. The rendered course selects 812, 811, 813, 810,
plays four original 2.2205-second clips with their LIP companions, filters the
next call and saves schema 64 history. A fresh process loads a protected copy,
filters every repeat and resaves the same history. The existing original-INFO
history fixture now explicitly supplies PNAM zero for native head insertion;
its rendered save/restart course also passes. Both fixture families retain
compiler results, actual playback and wave captures, reviewed prison/HUD images
and unrelated startup diagnostics separately. These captures do not show the
speaking actor's face.

See [M15-DIALOGUE-ORDER-VERIFICATION.json](M15-DIALOGUE-ORDER-VERIFICATION.json).
This closes the ordered candidate input for scripted speech. Automatic common-hit
callback wiring, GetFriendHit actor binding/storage, Player audio identity and
the remaining normal-input/opponent/mastery/restart M15S4 gates remain open.


### Native reference condition binding and friendly-hit ownership

The native condition evaluator substitutes the opposite context for a null
first reference/Actor argument after swapping both contexts for run-on-target.
Explicit first arguments and raw second arguments remain unchanged. The full
original binding path passes 1,792 cases, including 256 GetFriendHit cases:
192 actual getter calls and 64 missing-parent rejections. GetIsReference
and GetInSameCell also execute their original evaluators; four other predicate
evaluators are declared argument-observation boundaries.

Loaded and unloaded condition adapters now apply that binding to distance,
line-of-sight, detection, reference identity and same-cell queries. Previously
target-run distance could compare the target to itself, detection ignored an
explicit actor argument, and same-cell ignored its comparison reference. Both
full engine configurations pass 1,873 tests. The two new integration tests cover
loaded contexts and unloaded selection without materializing cold actors. All
398 Python tests pass; the writer exactly reproduces 20 old/new fixture files.
Sanitizer leak detection is disabled.

A guarded head response requires target-run GetDistance > 0 with argument zero.
The previous committed executable skips it and speaks 811/813/810. The corrected
executable, with identical content, actions and configuration, speaks
812/811/813/810 and saves all four histories. A fresh process loads a protected
copy and filters every repeat. Exact voice/LIP records, nonzero wave capture,
compiler results and reviewed prison/HUD images are retained. The images do
not show the speaking face. First-process unrelated startup diagnostics remain
separate from the successful custom course.

Additional original storage probes pass 38,880 insertion/removal snapshots and
155,520 queries. Full extra-owner creation, native lookup/cache, insertion and
removal pass another 16,384 snapshots/65,536 queries: counters belong to each
victim/target pair, first increment creates type4E, queries/removals do not
create it, and an empty counter list retains its allocated owner extra. These
probes do not establish native hit classification, scheduling or persistence.

See [M15-REFERENCE-BINDING-VERIFICATION.json](M15-REFERENCE-BINDING-VERIFICATION.json).
GetFriendHit production state/producer/expiry integration, Player audio identity,
automatic common-hit callbacks and the remaining M15S4 gameplay gates remain open.


### Per-reference IgnoreFriendlyHits flags and save authority

SetIgnoreFriendlyHits and GetIgnoreFriendlyHits now use the addressed reference
instead of a ScriptManager global. Loaded and unloaded condition 338 queries
share the World authority. Authored flags supply the default; explicit true and
false overrides use canonical reference keys and survive World retention and
schema 65 saves. Legacy saves add an empty override map. Invalid/dangling owners,
noncanonical keys and non-boolean wire values fail before publication. Native
argument -1 leaves the reference and save state unchanged.

The hash-identified original setter, bit writer and getter pass 504 cases across
Player, Character and Creature contexts. The actual C++ helper matches 1,512
fields; treating -1 as ordinary nonzero produces 36 mismatches. Argument extraction
and MarkModified are declared observation boundaries; native save serialization
is not emulated. Full normal engine 1,876/components 3,024 and sanitizer engine 1,876/
ESM4 components 876 pass, as do all 399 Python tests. Leak detection is disabled.
All four affected pinned fixture generators reproduce schema 65 outputs with
complete decoded-state semantic reinspection.

A rendered original-content course sets Player and creature true and NPC false,
checks independent values and -1 no-ops, and saves. A fresh process loads a
protected copy, repeats the queries/no-ops and resaves all three flags, including
explicit false. Each course compiles 10,470 units with zero failures/diagnostics.
First-course 31 unrelated startup deferred commands and continuation 0 remain
separate. Reviewed prison/HUD images do not establish flag values; command and
save records do. Sound is disabled and this is scheduled script acceptance.

See [M15-IGNORE-FRIENDLY-HITS-VERIFICATION.json](M15-IGNORE-FRIENDLY-HITS-VERIFICATION.json).
GetFriendHit production counters/expiry/lifecycle/persistence, automatic common-hit
callbacks, Player audio identity and the remaining full M15S4 gameplay gates
remain open.
