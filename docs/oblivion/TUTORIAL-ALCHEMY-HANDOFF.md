# Tutorial alchemy — current handoff

Target completed: original Natural Caverns mortar, Wisp Stalk Caps and Stinkhorn
Cap → one Damage Health poison → new-process load/resave. This is a separate
genuine first-goblin continuation; the main Wellspring and shore saves remain
independent. Full M16/M19 and the frozen full sewer escape remain open.

[Contract](TUTORIAL-ALCHEMY-CONTRACT.json),
[original oracle](TUTORIAL-ALCHEMY-ORACLE.json) and
[verification](TUTORIAL-ALCHEMY-VERIFICATION.json) distinguish structural,
compiler, independent semantic and actual runtime evidence. Both normal and SAN
perform ordinary acquisition, inventory apparatus use, ingredient selection and
Create, gameplay save, cold inventory load and resave. Full2292 engine tests per
mode and618 Python checks pass. Runtime leak checks are disabled; actual SAN
environment/library identities are retained. Historical [component gates](CHARTER-BOOK-VERIFICATION.json)
retain their original provenance; this chunk does not claim fresh3301 tests per mode.

Result: Damage Health1 for5s, value3, weight0.1, exactly one persistent generated
poison. Both original ingredients remain consumed. Alchemy advances5→6 with one
native use0 award5XP (existing progression model leaves2.1062093XP). The original
mortar has inventory-script owner1/allocator2; OnAdd enters MQ01 stage64 once,
retains current stage80, and never replays on reload. Generated poison uses
shared-item1; it is not a native reference and does not consume the script/ref
allocator. Its save-only marker0x100 is not an original TES4 ALCH flag.

Accepted continuations:

- normal: `build/oblivion-compat/tutorial-alchemy/normal-final-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`, SHA256 `3daa7c67027e16b2209b831a85c8de82c38b3ae8039e02ad02c1f915fa8a1e86`.

- sanitized: `build/oblivion-compat/tutorial-alchemy/sanitized-final-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`, SHA256 `62a04d89b60303c34eabd168a8c3ccddad932b54a888be6c91c7322f94d845ce`.

Health80, Alchemy6, poison1, original mortar1, Wisp0/Stinkhorn0; keep the native
script owner and all unrelated inventory/equipment/progression intact.

The implementation is limited to the declared novice stats and mortar-only
first-DGHE recipe. Higher skill/Luck/tools and other recipes remain open.
Generated poison uses the existing shared Potion save representation. It is
currently declined by native potion consumption; applying poison to a weapon is
unimplemented. This adds no TES4 authoring/MCP API.

Editable acquisition/brew/reload manifests and
`scripts/verify_tutorial_alchemy.py` reproduce the acceptance course. World input
uses isolated SDL; UI input uses X11. Tab cycles keyboard focus; Escape exits
inventory. Failed preflight/aim/save attempts and the deliberately stopped
intermediate SAN build remain under `build/oblivion-compat/tutorial-alchemy`.
The missing legacy GMST baseline is retained. Original-game prefix files and
pristine save were restored exactly.

Next: select and verify another original content gap, likely the tutorial crate's
original poison or the crafted poison applied to a weapon. Finish that bounded
course before expanding into adjacent magic or combat requirements.
