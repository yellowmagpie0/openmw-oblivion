# M13 items, inventory, equipment, locks, and economy

Status: accepted on 2026-08-31. Implementation revision
`85d35873eebe12999690bdb7886e48330c8e12be`.

M13 completes the native Oblivion item backend. Released TES4 item records now
populate live inventory and container stores; mutable instances retain their
count, condition, enchantment charge, equipment slots, quick-key binding,
ownership, and remaining light time; and those values survive a normal
quicksave/reload. Locks, keys, lockpicks, harvesting, barter, repair, recharge,
and training use profile-specific paths rather than TES3 formulas or the
earlier interaction placeholders.

## Delivered implementation

- Native `AMMO`, `APPA`, `ARMO`, `BOOK`, `CLOT`, `INGR`, `KEYM`, carryable
  `LIGH`, `MISC`, `ALCH`, `SGST`, `SLGM`, and `WEAP` definitions are projected
  into reviewed shared item classes. The base-master runtime count-lock is
  5,436 native items. `MISC` additionally distinguishes gold, lockpicks, and
  repair hammers, while `BOOK` distinguishes normal books and scrolls.
- Player and reference inventories use live `ContainerStore` and
  `InventoryStore` instances. Add, remove, transfer, consume, equip, unequip,
  and quick-key operations update the same state used by scripts, rendering,
  interaction, encumbrance, and saves. Transfers conserve counts, preserve
  instance metadata, and never retain source-only equipment or quick-key
  bindings.
- Stacks merge only when base identity and all mutable instance fields match.
  Equipped items are split from stacks when required, unequipped copies merge
  again, integer counts saturate safely, and removal prefers ordinary copies
  before equipped or quick-key-bound copies. Oblivion-owned items remain
  distinct; Morrowind retains its established owner-clearing stack behavior.
- The 16 native biped bits are honored for armor and clothing, including
  conflicts and the two selectable ring slots. Dedicated state bits represent
  weapon, ammunition, and carried-light equipment. Live changes rebuild the
  M11 actor attachments using the actual Oblivion `Weapon` and `Torch` nodes,
  without stale body parts or FaceGen state.
- Eight Oblivion quick-key positions are written back to native inventory
  state. Reassigning a position clears its former item and activating an item
  continues through the normal inventory-use/equipment path.
- Item condition and enchantment charge are initialized only for definitions
  that actually support them. Repair and recharge restore the definition
  maximum, carryable lights track remaining use time, ammunition stays as one
  equipped stack, and consumables remove exactly one instance. Magic-effect
  application remains correctly bounded to M16/M17.
- Containers expose their actual native contents. Harvesting and world pickup
  use the same player store, preserve ownership on acquired instances, and
  persist depleted references. Locked containers accept their native key or a
  bounded Security/Agility/Luck auto-lockpick attempt; failed attempts consume
  one lockpick. Existing keyed doors and trap activation use the same stable
  reference state.
- Native values, actor Mercantile/Luck, derived disposition, and released GMST
  defaults drive barter offers and haggle acceptance. Merchant repair uses the
  missing-condition fraction and Oblivion's repair multiplier. Recharge and
  training have native cost backends, and the training window uses the fixed
  Oblivion service price instead of applying the Morrowind barter adjustment.
- Oblivion menu icons resolve through `textures/menus/icons/...` before the
  inherited fallback. Inventory, quick-key, merchant-repair, and training
  hooks consume the native item and economy backends; construction of the full
  Oblivion menu presentation remains M19 work.
- Native runtime state is version 4. Every inventory entry serializes a stable
  base `FormKey`, signed count, condition, charge, equipment mask, quick-key,
  owner `FormKey`, and remaining usage time. Versions 1--3 migrate with safe
  defaults, and invalid identities, fields, duplicate references, corruption,
  truncation, and non-finite values are rejected.

## Item and operation matrix

The deterministic market/tutorial matrix uses released base-game records for
all 14 native item families plus the two special `MISC` service/currency
families:

| Runtime category | Representative released item | Operations exercised |
| --- | --- | --- |
| Ammunition | Iron Arrow | stack, transfer, equip, save/load |
| Apparatus | Mortar and Pestle | transfer, sell/reacquire, save/load |
| Armor | Iron Cuirass | condition, slot conflict, equip/unequip, save/load |
| Book | *The Armorer's Challenge* | acquire, transfer, save/load |
| Clothing | Middle Class Shirt | multipart conflict, equip/unequip |
| Ingredient | Potato | stack, consume, sell/reacquire |
| Key | Imperial Prison Key | inventory lookup and keyed transition |
| Light | Torch | equip, remaining time, visual attachment, save/load |
| Miscellaneous | Lockpick | stack and failed-attempt consumption backend |
| Potion | Restore Health | stack and consume |
| Scroll | Fire Damage scroll | stack and consume |
| Sigil stone | released Sigil Stone | transfer and persistence |
| Soul gem | Petty Soul Gem | transfer and persistence |
| Weapon | Iron Longsword | condition, charge eligibility, equip, repair |
| Repair tool | Repair Hammer | consume and restore |
| Currency | Gold | sale/reacquisition balance round trip |

The component market workflow transfers each native family to and from a
merchant, checks total-count conservation, and binary-serializes, validates,
and reloads the state after every transfer, repair, recharge, consumption,
quick-key, and equipment operation. The real-data runtime course repeats the
complete item-family sale/reacquisition round trip, finishes with exactly 16
expected stacks, and verifies the post-reload equipment mask `458756`, cuirass
condition `300`, sword condition `140`, and valid torch time.

## Economy and lock formula verification

The standalone mechanics layer makes the Oblivion rules independently
testable. Effective barter skill is
`clamp(Mercantile + 0.4 * (Luck - 50), 0, 100)`. Buying and selling apply the
Oblivion base/multiplier pairs to the player/merchant skill difference and
disposition, enforce the native minimums, and round to integral gold. Haggle
acceptance uses the released `fHaggleDispositionMult = 0.5`,
`fHaggleBase = 0.55`, and a slider clamped to 0--40.

Merchant repair is
`ceil(value * missing-condition-fraction * 0.9)`. Recharge is
`ceil(missing-charge * fRechargeGoldMult)`; the released base master overrides
that multiplier to 1. Training is `current-skill * 10` for skills below 100
and is not haggled. Boundary tests cover zero-value and complete items,
invalid/maxed skills, both ends of the haggle slider, expert/novice price
ordering, and lockpick chance saturation and monotonicity.

## Automated and end-to-end verification

The completed revision passed:

| Suite or gate | Result |
| --- | ---: |
| Component tests | 1,542 / 1,542 |
| Engine tests | 511 / 511 |
| Python compatibility/state tests | 31 / 31 |
| Focused inventory/state/icon tests | 24 / 24 |
| Focused native-profile service test | 1 / 1 |
| Market and tutorial item-matrix scenarios | 2 / 2 |
| Keyed lock and trap regressions | 2 / 2 |
| Morrowind runtime isolation regression | 1 / 1 |

Durable unit logs are in `build/oblivion-compat/m13-final-tests/`. Runtime
evidence uses the OpenMW runtime and released game records/assets:

- `build/oblivion-compat/m13-market-final/` runs inside A Fighting Chance in
  the Imperial City Market District. It loads the count-locked 5,436-item
  catalog, performs the complete 16-category mutation sequence, equips the
  cuirass, sword, arrows, and torch, quicksaves, reloads, and emits both the
  native runtime report and strict `m13-state-verification.json`.
- `build/oblivion-compat/m13-tutorial-accepted/` repeats the matrix in the
  tutorial world, proving that the result is not tied to the market cell.
- `build/oblivion-compat/m13-key-route-final/` loots the real Imperial Prison
  Key, unlocks and traverses its keyed prison door, and saves the result.
- `build/oblivion-compat/m13-trap-regression/` executes the real tutorial trap
  gallery without frame errors or interaction regressions.
- `build/oblivion-compat/m13-morrowind-final/` boots Balmora, changes weather,
  captures two valid frames, records a 12.8 MB audio stream, and quicksaves.
  The save contains none of the TES4 profile or runtime markers.

The market and tutorial before-save/after-reload captures were inspected
directly. In both, the cuirass remains on the correct upper-body slot, the
longsword and torch remain on the correct hands, and actor geometry and scene
placement remain stable after reload. The market pair additionally shows the
same shop interior and Rohssan placement. State inspection independently
confirms every exact item count and equipment field, so the visual check is
not standing in for persistence verification. No original Oblivion
executable, Wine, or Proton was used.

## Reproduce and inspect

Build and run the real-data market matrix with a local Oblivion installation:

```sh
cmake --build build --target openmw components-tests openmw-tests -j2
python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_m13_item_matrix.json \
  --output build/oblivion-compat/m13-market \
  --variable "source=$PWD" \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/path/to/Oblivion/Data" \
  --variable "scene=market" \
  --variable "start=ICMarketDistrictAFightingChance::ref=0x1d15a::side=south"
```

Then validate its save independently:

```sh
save_path=$(find build/oblivion-compat/m13-market/userdata/saves \
  -name '*.omwsave' -print -quit)
python3 scripts/oblivion_compat.py runtime-state m13-verify "$save_path" \
  --report build/oblivion-compat/m13-market/m13-state-verification.json
```

To inspect the saved result interactively, load the generated quicksave with
the scenario's isolated config:

```sh
save_path=$(find build/oblivion-compat/m13-market/userdata/saves \
  -name '*.omwsave' -print -quit)
build/openmw --replace=config \
  --config build/oblivion-compat/m13-market/config \
  --load-savegame "$save_path" --no-grab
```

Use `Tab` for the current shared inventory presentation, `F` to ready the
equipped weapon, `R` to switch camera, `Space` to activate containers/items,
and `F5`/`F9` to check persistence. Full TES4 menu layout and interaction
polish is intentionally reserved for M19.

## Scope boundary

M13 owns item definitions and instances, stacks, equipment and quick keys,
condition/charge/time, inventory and container transfer, harvesting,
locks/keys/lockpicks/traps, ownership, economy service backends, and their
persistence. It does not borrow placeholder creature loot or TES3 pricing as
acceptance substitutes. M14 owns navigation, detection, and AI packages; M15
owns combat, theft/crime consequences, and jail; M16/M17 own magic-effect,
alchemy, enchanting, and soul-gem semantics; and M19 owns the complete native
Oblivion UI. Those later systems were not implemented as part of M13.
