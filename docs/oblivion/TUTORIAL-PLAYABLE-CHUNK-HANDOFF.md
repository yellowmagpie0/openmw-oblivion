# Shortest path to a playable tutorial chunk

Prepared: 2026-10-07.

## Target

Deliver an early playable checkpoint: start in the Imperial Prison, follow the
Emperor and Blades, survive the first ambush, loot, and continue past it through
normal gameplay. This is a focused subset of M15, not full M15 acceptance or a
promise of a complete tutorial through the sewer exit.

This document records a read-only review of the existing plans and reports.
No build, test, or runtime scenario was run for that review. The user requested
this handoff document; implementation has not been requested by that action.

## Existing foundation

M0–M14 are marked accepted in the roadmap and implementation ledger. They
provide parsing, identity, startup, saves, scripts, rendering, actors, player
movement and character creation, inventory/equipment, navigation, and tutorial
escort behavior.

The M14 tutorial escort manifest demonstrates opening prison progression,
named Emperor/Blades movement and door transitions, and save/reload using
normal inputs. It does not establish successful tutorial combat. If the user
only wants to play the opening before combat, recheck that existing course on
the current build before assuming more implementation is necessary.

## Shortest reasonable work list

1. **Finish necessary services and persistence — M15 S3.** Close the state,
   restore, and action-continuation boundaries needed by this route. Actors,
   equipment, and ongoing actions must remain consistent across save/load.
2. **Finish melee, blocking, damage, and reactions — M15 S4.** Ordinary
   Player/NPC contacts already have bounded verified implementation. Complete
   the route's required actor, reaction, control, and restart paths. Include
   creature attacks if the chosen endpoint reaches them. S5 bow combat can be
   deferred for a melee-focused checkpoint if the actual route does not require
   it; this is not a waiver of S5 for full M15 acceptance.
3. **Implement death, essential recovery, and corpse loot — M15 S6.** Mortal
   enemies must die correctly, essential escorts must recover correctly, and
   loot must come from actual corpses. Preserve Renault's authored death as a
   distinct scripted requirement.
4. **Implement autonomous combat and escort resumption — M15 S7.** Assassins
   and Blades must acquire targets and fight through production combat paths;
   escort behavior and authored progression must continue after the encounter.
5. **Close tutorial dependencies and verify the route — tutorial portions of
   M15 S12/S13.** Execute original dialogue conditions/results and quest events.
   Implement the first assassins' real bound armor/helmet/mace ability, including
   creation, equipment, removal, save, and death behavior. This is an identified
   narrow magic prerequisite, not completion of M16. Verify normal-input entry,
   combat, loot, post-encounter progression, and save/reload continuation.

This focused dependency list is a recommendation for an earlier checkpoint.
The existing full-M15 implementation plan uses a linear S0–S14 execution order,
including S5 and S8–S11 before S12/S13. Do not silently revise that plan or mark
its stages accepted based on this smaller route. If implementation of this
checkpoint is later authorized, make the bounded scope and any departure from
that order explicit in its implementation handoff.

## Work that can wait for this checkpoint

- Bow/projectile gameplay when not required by the selected route.
- Stealth/pickpocket and general crime, arrest, and jail gameplay.
- Arena and representative dungeon acceptance campaigns.
- Full magic, alchemy, enchanting, disease, and progression (M16).
- General dialogue, persuasion, and services (M17), and full quest/journal/marker
  support (M18), beyond the real paths required by this tutorial chunk.
- Complete Oblivion UI presentation (M19). Functional input, dialogue choices,
  inventory/equipment, combat feedback, and save/load must still be accessible.

The roadmap's full fresh-launch/new-game/tutorial/sewer-exit checkpoint belongs
to M19. Reaching this smaller endpoint does not demonstrate that larger flow.

## Acceptance for the smaller checkpoint

Use the M15 E1 tutorial combat contract as the basis, scoped to an explicitly
named post-encounter endpoint:

- Start from normal prison progression, or a checkpoint with proven legitimate
  progression provenance; no fabricated quest completion or already-dead targets.
- Acquire and equip actual available equipment through normal interaction.
- Follow the authored escort/door sequence and execute real dialogue results.
- Demonstrate actual autonomous attacks, contacts, damage, death, escort
  survival/recovery, and exact corpse-loot changes.
- Reach the endpoint through original scripts/conditions without console stage
  injection, forced outcomes, or unsupported commands masquerading as success.
- Verify appropriate legal consequences, including no inappropriate bounty.
- Save/restart during combat or its nearest valid save boundary, and after loot;
  continue to the same endpoint and compare against uninterrupted progression.
- Report implementation/test results separately from actual runtime acceptance.
  Run relevant build, sanitizer, and Morrowind regression gates for changed code.

## Resume instructions and sources

First read the current M15 report and recheck its stage ledger. At this review,
S0–S2 were passed; S3, S4, and S5 were in progress; S6–S14 were pending. Later
reports and current source take precedence over this dated snapshot.

Read local AGENTS.md and the relevant repository skills before implementation
or verification. Keep proprietary game data and generated evidence local;
preserve unrelated changes and do not push or publish without authorization.

- [Compatibility roadmap](../OBLIVION-COMPATIBILITY-ROADMAP.md)
- [Implementation status](IMPLEMENTATION-STATUS.json)
- [Current M15 report and stage ledger](M15-COMBAT-STEALTH-CRIME.md)
- [M15 implementation plan, scope boundaries, S3–S7, S12–S13, and E1](M15-COMBAT-STEALTH-CRIME-IMPLEMENTATION-PLAN.md)
- [Selected campaign inputs and first-ambush bound equipment](M15-CAMPAIGN-COMBAT-POLICIES.md)
- [M14 navigation and tutorial escort acceptance](M14-NAVIGATION-DETECTION-AI.md)
- [Editable M14 tutorial escort manifest](../../scripts/data/oblivion_compat/oblivion_m14_tutorial_escort.json)
