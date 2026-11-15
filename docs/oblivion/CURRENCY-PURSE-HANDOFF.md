# Native Player purse — current handoff

Accepted bounded M19 [Gold001 presentation](CURRENCY-PURSE-CONTRACT.json), using
the Septim earned from [Shady Sam](SHADY-SAM-TRADE-HANDOFF.md). The
[original-game oracle](CURRENCY-PURSE-ORACLE.json) shows gold in a purse footer,
absent from the Player Misc list; footer controls preserve its 44 gold. Original
inputs were restored exactly. Its [editable protocol](CURRENCY-PURSE-ORACLE-PROBE.json)
claims Player UI behavior only.

The native Player item view now excludes currency while its actual inventory
retains canonical Gold001 for transactions and scripts. A live noninteractive
footer shows 0, 1 and 6 gold and follows an actual purchase from 6 to 1. Normal
and sanitizer purchased cold processes retain 1 gold, 3 Lockpicks, stock 35,
Health 110, allocator 7 and Mercantile progress 2.4. Footer controls do not debit
currency or create native coin references. Non-Player/Morrowind paths are guarded
separately; no added gameplay acceptance for those profiles is claimed.

[Receipt](CURRENCY-PURSE-VERIFICATION.json): exact inventories/load/save ownership,
UI PNGs and manual inspection, frozen GUI assets, loaded libraries, full normal
2,292 engine and exact disjoint sanitizer 2,292 engine cases, plus 618 Python
checks. Unchanged 3,303 component cases per mode retain the prior Sam provenance.
The initial overload/private-API compile failures are retained. Resizing uses the
parent item pane, with the footer outside it.

The old draggable-gold baseline is diagnostic: it debited 1 to 0 without a native
reference or allocator advance. No physical/console/script GoldDrop fix is
claimed; the direct native drop alias gap remains a separate domain. No full M19,
exact barter arithmetic, complete tabs/tooltips or TES4 MCP claim.

Private root: `build/oblivion-compat/earned-gold`. Normal purchased continuation:
`normal-purse-purchased-cold-01`, SHA256
`90baa6fdfc96832b8a9e6d66883632decbbc9a3d174d8822afff93267598d1ee`.
The instrumented continuation and binary/resource hashes are in the receipt.
Keep main cave, tutorial and healthy-shore inventories separate.

Next: select another genuinely carried or reached original content item that
exposes an unimplemented gameplay requirement. Finish that bounded course before
expanding. Commit locally; do not push.
