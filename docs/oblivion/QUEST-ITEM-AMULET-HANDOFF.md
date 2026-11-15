# Earned Amulet — quest-item inventory protection

Accepted bounded M12/M13/M19 progress: the Amulet actually earned from the
Emperor now has zero effective carried weight, and ordinary cursor drop is
refused before inventory debit or dropped-reference allocation. Winning raw
records retain their nominal weight. No save-format change.
[Receipt](QUEST-ITEM-AMULET-VERIFICATION.json),
[contract](QUEST-ITEM-AMULET-CONTRACT.json),
[independent original oracle](QUEST-ITEM-AMULET-ORACLE.json).

Four actual normal phases and four instrumented phases pass: refused drop,
fresh-process repeat, original scripted avatar equip, then cold repeat with
retained script identity. Independent actual-save audits preserve surrounding
inventory, quests and Player values apart from the intended encumbrance
correction 102.3→101.3. Amulet count 1/unworn, script owner 7/allocator 8 and its Player
local survive the final cold course; no dropped reference or OnEquip replay.

All new refusal captions are visually verified. The original slips-off caption
is visible in the normal equip capture; the instrumented equip image lacks
that transient caption. Its actual VM message/unequip trace and saved identity
prove the script regression, without claiming instrumented caption visibility.
The shared tooltip omits zero weight, whereas the original row displays 0.
Original-only diagnostic grants are confined to the isolated oracle; fork
acceptance uses the genuinely earned item with no grants or teleports.

Normal and sanitizer regression each pass 3,304 components and 2,295 engine cases;
619 Python checks also pass. Sanitizer engine coverage is an exact disjoint union of
82+82+82+2049. Actual unit leak checks are enabled; rendered leak checks are
explicitly disabled. Failed attempts and corrections remain in the receipt.

The separate shore continuation is under `build/oblivion-compat/quest-items`:
normal `normal-earned-amulet-owned-cold-drop-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `0d820b5cb21300bb43344ce2c4c8db1c815578f21560b84580ee6aeae4a8d292`;
instrumented `sanitized-earned-amulet-owned-cold-drop-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `ab1a9d43741173406e8c0611a252c02b26994ded8cfaa118c76bf758cad79568`.
Keep it separate from the cave and alchemy inventories.

Other quest-item types, dynamic quest-object changes, controller drop, merchant
sale and container-transfer interactions require separate real-content coverage.
Full M12/M13/M19 remain open. Next: select another reached piece of original
content that exposes an unimplemented behavior; continue locally without push.
