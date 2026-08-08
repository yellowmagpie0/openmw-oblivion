# Seyda Neen ring quest implementation

This walkthrough was produced from the vanilla `Morrowind.esm`, `Tribunal.esm`,
and `Bloodmoon.esm` load order with `openmw-cs-mcp`.

There are two connected quests behind the apparent “hidden ring” story:

- `MS_FargothRing` — **Fargoth's Ring**, the ring found in the Census and
  Excise courtyard barrel and optionally returned to Fargoth.
- `MS_Lookout` — **Fargoth's Hiding Place**, the later job to watch Fargoth and
  loot his hollow treestump. If the first quest was completed, the returned
  ring becomes part of this stash.

## Fargoth's Ring (`MS_FargothRing`)

The object is `ring_keley`, a `CLOT` record named **Engraved Ring of Healing**.
It uses enchantment `Heal_minor` and script `CharGen_ring_keley`. That script is
character-generation tutorial logic which unlocks/explains the magic menu; it
does not advance the quest.

The `CONT` record `chargen barrel fatigue` contains one `ring_keley`. A reference
to that container is present in the exterior `CELL` record `Seyda Neen`.
`CharGenDoorEnterCaptain` prevents the player from continuing until the ring is
in their inventory, which makes finding it part of the opening tutorial.

The quest itself begins through Fargoth's dialogue:

1. INFO `31191105114705102` in `Greeting 5` is selected for actor `fargoth`
   while journal `MS_FargothRing == 0`. It introduces the missing ring.
2. INFO `2507536232473510507` in topic `ring` writes journal stage 10 and offers
   choices 1 (deny having it) and 2 (return it).
3. Choice 2 with `ring_keley > 0` selects INFO `3245423813243151497`. Its result
   script writes journal stage 100, transfers the ring from the player to
   Fargoth, sets Fargoth's local `state` to -1, raises his disposition by 50,
   and raises Arrille's by 40.
4. Choice 2 without the item selects INFO `158791326429969956`, which lowers
   Fargoth's disposition by 5. Choice 1 selects INFO `156171272864323892` and
   leaves the quest open.

The journal records are:

| Index | Status | Meaning |
| ---: | --- | --- |
| 0 | name | `Fargoth's Ring` |
| 10 | active | Fargoth asks for the stolen ring. |
| 100 | finished | The ring was returned and Fargoth promises to tell Arrille. |

The title and finished marker are effective expansion overrides; the original
Morrowind records supply the stage text.

## Fargoth's Hiding Place (`MS_Lookout`)

Hrisskar's topic `Fargoth's hiding place` advances the quest from stage 10 to
20 when the player accepts. The core behavior is split between two scripts:

- `lookoutScript`, attached to activator `active_lookout_unique` in the `Seyda
  Neen` cell, waits for journal stage 20, nighttime (after 22:00 or before
  04:00), and the player within 512 units. It then drives Fargoth through a
  sequence of AI travel points.
- At the stump, `lookoutScript` adds 300 gold and a journeyman lockpick to
  container `flora_treestump_unique`. If `MS_FargothRing == 100` and Fargoth
  still has `ring_keley`, it also moves the ring from Fargoth into the stump.
- `treestumpScript`, attached to `flora_treestump_unique` (**Hollow
  Treestump**), advances `MS_Lookout` to stage 40 when the player activates it
  after Fargoth has deposited the stash.

Returning to Hrisskar with at least 300 gold selects INFO
`13504245661527210384`: it removes 300 gold, gives 100 back to the player, gives
200 to Hrisskar, and finishes the quest at stage 100. Without the full 300,
INFO `2916020299178068693` refuses completion and lowers disposition.

The important consequence is that returning the ring does not permanently
remove it from the world. Completing `MS_Lookout` can let the player recover it
from Fargoth's stash later.

## Reproducing the inspection

Use these MCP calls in order:

1. `get_quest({"quest_id":"MS_FargothRing"})`
2. `search_records({"query":"ring_keley"})`
3. `get_record({"record_type":"CLOT","record_id":"ring_keley"})`
4. `get_record({"record_type":"CONT","record_id":"chargen barrel fatigue"})`
5. `get_quest({"quest_id":"MS_Lookout"})`
6. `get_record({"record_type":"SCPT","record_id":"lookoutScript"})`
7. `get_record({"record_type":"SCPT","record_id":"treestumpScript"})`
