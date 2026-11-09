# Vilverin — current handoff

Target course: genuine sewer exit → independent Vilverin expedition → outside
alive with retained loot. The full expedition and full milestones remain open.

Accepted bounded slices:

- Original Player Flare, tutorial goblin defeat and continuation:
  [receipt](PLAYER-FLARE-VERIFICATION.json).
- Original single-actor LVLC bandits, concrete NPC gear and saved movement:
  [receipt](VILVERIN-LEVELLED-ACTORS-VERIFICATION.json).
- Original mudcrab CREA leveled microtreasure and ordinary corpse transfer:
  [receipt](MUDCRAB-LOOT-VERIFICATION.json).
- Original first-area food sack `06c2ad`, concrete LVLI transfer and empty stock:
  [receipt](VILVERIN-CONTAINER-LOOT-VERIFICATION.json).
- Actually earned Lettuce, first-effect REFA5 over1 second with independent
  timers and pending restart: [receipt](LETTUCE-INGREDIENT-VERIFICATION.json).
- Carried original Flash Bolt `0888bf` / ENCH `05c057`, FIDG20 instant Target:
  [receipt](FLASH-BOLT-SCROLL-VERIFICATION.json).
- Carried original Chameleon `084aeb` / ENCH `05b044`, CHML25 for30 seconds Self:
  [contract](CHAMELEON-SCROLL-CONTRACT.json),
  [original-game oracle](CHAMELEON-SCROLL-ORACLE.json),
  [receipt](CHAMELEON-SCROLL-VERIFICATION.json).

Latest closed chunk: Chameleon. Ordinary inventory/magic-menu use spends one
selected instance at cast admission without Magicka or learned-spell skill gates.
Schema93 owns the consumed BOOK cast, then the resolved ENCH/magnitude/elapsed
effect. Native visibility and NPC detection consume AV46. Active reloads preserve
the remaining duration; natural expiry removes only this source's contribution;
another reload stays expired. Same-ENCH refresh adds no second25. Legacy92 AV46
without an owned timer is preserved. Approximate alpha0.75 is verified visually;
exact Oblivion refraction and complete stealth outcome parity remain open.

Normal and instrumented courses each pass90 actual-save semantic checks across
use → pending restart → active restart/expiry → expired restart. Full3,278
component and2,264 engine tests pass per configuration; full602 Python tests pass.
The same real Flash Bolt course passes65 additional normal regression checks
with schema93. Native ENCH remains explicit; the generic TES3 getEnchantment
property is not used for native casting. Evidence/source/ELF identities and
retained development failures are in the receipt.

Reusable latest normal checkpoint:
`build/oblivion-compat/chameleon-scroll/chameleon-expired-restart-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `41b8e537b995738176c9568aeedd57a3e9a0044d44deae5e5c63f7ad18b041f4`.
Health67.679, Magicka80, Fatigue170; Flash Bolt0, Chameleon0, nativeAV46=0;
Bone1, Crab Meat1, Lettuce1, Rice1. Ambush `06c356` and sitting bandit `06c359`
remain dead with Player killer; food sack `06c2ad` stays opened and empty.
No pending ingredient timer, scroll effect, cast or projectile; next projectile19.

Current chunk: original Mead `0b1201`, two DRAT1 attribute drains for60 seconds
(Intelligence/Willpower) plus instant REFA2. [Contract](MEAD-POTION-CONTRACT.json).
Typed/raw audit passes all253 potions/330 effects. Ordinary-use baseline02 passes
its diagnostic/save checks but leaves stock1 and all native attributes/resources
unchanged: current potion admission supports only instant restoration.

Actual expedition progress: tripwire `06bf50` and short maces already work;
passage causes one11.5-point hit, retained without replay on fresh load. The lower
room is reached with unchanged inventory/Magicka (81 route +9 room checks).
Three original bandits `04bb72`/`04bb73`/`06bfba` are defeated using owned starter
healing and the carried healthy Club. A fresh process retains their Player-killer
deaths and collects actual bottle `06c362`. No combat/trap implementation change
is claimed. Retained route/menu/aim failures and the failed worn-axe battle stay
under `build/oblivion-compat/vilverin-trap-passage`.

Current actual Mead input:
`build/oblivion-compat/vilverin-trap-passage/mead-acquire-after-encounter-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `6677188fc1f68ec1142c994b9398492b3df0266a009b23cf43f117a4a29918b6`.
Health29.73584, Magicka80, Fatigue170, Intelligence40, Willpower30, Mead1.
Club equipped; prior expedition loot/deaths and expired Chameleon retained.

Next action: finish the isolated original Mead control in
`build/oblivion-compat/mead-use/original-native-02` (game currently active).
One drink gives Int39/Will29/MP78; repeated-source/expiry and real-deficit Fatigue
restoration remain under observation. Then implement the bounded mixed profile
with owned saved contributions and verify normal/instrumented use, active restart,
natural expiry and completed restart. The autonomous original-content iteration
remains active. No C++ change or build fingerprint is currently in progress.

Remaining acceptance gaps: expedition continuation/return; general M16 effects
and UI; native spawn-root/child identity, grouped actors, respawn, all Player
levels and full bow tactics. Restoration chest `04bb8b` remains deferred behind
the retained partial route/ledge gap. The pre-existing learned target-SPEL mastery
gate reads Alteration AV20 rather than Destruction AV22; use a future actual
learned-spell case to address it. Consumable coverage has increased, while the
route/return gap remains; prioritize forward gameplay at the next checkpoint.

Rendered ASan uses leak detection off; the full engine test process uses it on.
Current courses use ordinary keyboard/mouse input with fresh-frame and quicksave
acknowledgements. Historical SDL-shim/display recipes retain their original
provenance in earlier linked receipts.
