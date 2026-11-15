# Shady Sam — native unit trade

Target: [Lockpick trade contract](SHADY-SAM-TRADE-CONTRACT.json), bounded M13/M19.
The separate healthy-shore branch reached original Sam with normal movement and
defeated the two pursuing original Mud Crabs with learned Flare. No fork grants,
teleport, actor disables or edited stats. Main cave/tutorial branches stay separate.

Normal and sanitizer courses pass: original Barter and linked stock, stage/cancel, reject a
purchase with zero gold, sell six carried Lockpicks individually, cold-load the
funded state, buy one and cold-load the purchase. Final state: Player 3 Lockpicks/1 gold,
chest 35, Health 110, allocator 7, Mercantile progress 2.4. Inventory/chest metadata match
the actual source save on load. Purchase/cancel/reject award no sale XP.

[Original-game observations](SHADY-SAM-TRADE-ORACLE.json) independently establish
stock ownership and fixed 400 budget. Original prices of 6 to buy/2 to sell differ from fork
prices of 5/1; existing barter arithmetic remains approximate. Original inputs were
restored exactly. The [editable original protocol](SHADY-SAM-TRADE-ORACLE-PROBE.json)
claims no original cold-save gate.

Checks: normal 3,303 component/2,292 engine cases; sanitizer 3,303 component and
all 2,292 engine cases in four exact disjoint process partitions. The full sanitizer process hit
Bullet's global thread-counter assertion after 2,038 passing cases; the isolated
case and all partitions pass. The failed full run remains retained. Broad normal
engine precedes a one-line rejected-label refresh; rebuilt focused and actual
normal/SAN runtime checks cover that refresh. Sanitizer engine partitions use the final C++.

Private root: `build/oblivion-compat/shady-sam-purchase`.
Normal current binary SHA256:
`bf77adf50dae2d8fb0a31451645d0f09820bceb980dd3374c8d10ab0c18101d5`.
Sanitizer current binary SHA256:
`1ea5f8667214fbe3972f8eb364086a925c611ae5232a6037337f46d1b23a2a95`.
Normal purchased cold save: `normal-sam-purchased-cold-01`, SHA256
`8944665bd947e09018e1a0a1640889237c3fad5fded95f49cf6a48bcd5b0c5f2`.

All seven actual sanitizer phases and final 618 Python checks pass. The
[receipt](SHADY-SAM-TRADE-VERIFICATION.json) records structural validation,
independent content/original observations, actual state and loaded-library hashes,
compiler/test gates, exact save chains and retained failures. No full milestone,
exact pricing/autovalue, bulk/haggling, stateful equipment, vendor restock or TES4
MCP claim. Runtime item acceptance is Lockpick only; other MISC/ALCH profiles,
explicit sale ownership and inherited item tabs/tooltips remain open.

Normal purchased continuation: above. Sanitizer purchased cold save:
`sanitized-sam-purchased-cold-01`, SHA256
`7a1b321217019f164835b1156641102768106b15da4d3d2db18d80b35d0f1e07`.
Next: select genuinely carried or reached original content that exposes another
unimplemented acceptance gap. Keep independent branches separate. Do not push.
