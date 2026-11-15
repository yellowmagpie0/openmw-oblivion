# A Shadow over Hackdirt — current handoff

The selected return/reward chunk is accepted through original MS45 stage 100 and
fresh-process persistence. This is checkpoint-based original-content evidence;
an uninterrupted whole-quest run and full M14/M15 acceptance are separate gates.
No original quest scripts or records were edited.

## Verified course and current checkpoint

Dar-Ma independently resumes original TravelHome (`0a2e59`) after the escape
conversation, reaches Chorrol and shop cell `000807` alive, and releases the Player
companion link. Normal stairs/approach lead to Seed-Neeus. Original Missing
Daughter (`028090`) → question (`0280c0`) → safe answer (`028210`) completes stage 100. Afterwards
Dar-Ma selects original Accompany Seed-Neeus (`0292f3`), targeting her mother rather
than the Player. This authored package transition is expected.

The original reward changes Mercantile 15 → 20 and Fame 0 → 1, records five per-skill,
Personality and Stealth advances, and leaves major progress 0 for the original
temporary class. Seed-Neeus's essential override becomes false, MS45FIN starts,
and Chorrol→Player faction reaction is set to 5. The authored disposition request
is +20; the existing native derived-score clamp stores modifier 40 → 58.

A fresh process preserves the quest, native Player values, inventory, reward
counters, faction reaction and disposition. Seed-Neeus gives original completed
acknowledgement (`028735`); the completion topic is absent and stage 100 is not replayed.
Dar-Ma remains alive in the original shop at health 34.2531.

Latest immutable normal save:
`build/oblivion-compat/hackdirt/return-completed-reload-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Its parent is `return-reward-03`; the independent matched pre-arrival continuation
is `return-arrival-matched-01`, from genuine `return-shop-diagnostic-04`.

## Implementation and evidence

The door timeout was an observer error: Lua reports editor ID
`ChorrolNorthernGoodsAndTrade`, not display name `Northern Goods and Trade`.
No shop activation engine change was needed. ModPCFame now uses the existing
native reputation authority. Positive/zero ModPCSkill publishes native skills,
experience and saved advancement bookkeeping. Negative counts and unsupported
base overflow are explicit errors; normal skill-use/level-up UI and original
training notifications are outside this bounded prerequisite.

[Return contract](HACKDIRT-RETURN-CONTRACT.json) and
[verification receipt](HACKDIRT-RETURN-REWARD-VERIFICATION.json) retain exact source,
binary/library/content/save identities, editable manifests, scoped assertions,
37 normal semantic checks, original instruction provenance and failed attempts.
Normal gates pass 3,272 component and 2,257 engine tests, exact inventories with no
failures/skips. ASan/UBSan gates pass the same complete inventories. Engine
leak checking is enabled; component/render leak checking is disabled. The
instrumented original turn-in passes with the same reward and 12 additional
semantic checks. Exact provenance remains in the receipt.

The accepted normal captures show original question, completion and post-rescue
acknowledgement. No-sound runs do not establish audio quality or pacing; broader
NPC/rendering fidelity is not certified by this chunk.

## Remaining gaps and next action

No acceptance work remains for the selected chunk. Broader milestone campaigns
and uninterrupted whole-quest assembly remain separate. The user has authorized continuing with another bounded original-content course
that exercises missing functionality, then repeating that implementation/testing/
commit cycle. Select that course separately from this accepted chunk.

[Historical handoff and earlier accepted prefixes](HACKDIRT-HISTORY-2026-11-08.md)
retain rescue/follower/overland provenance and earlier navigation investigations.
