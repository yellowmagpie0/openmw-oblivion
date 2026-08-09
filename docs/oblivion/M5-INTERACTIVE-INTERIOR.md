# M5 first interactive Oblivion interior slice

Status: accepted on 2026-08-08. Acceptance revision:
`153a60668bd924cd844b6f024e969fbf43c43ea1`.

M5 turns the standalone TES4 renderer and M4 native state substrate into the
first bounded playable Oblivion slice. The executable can now enter the prison
with a placeholder player, collide with the world, focus and activate native
TES4 references, mutate them through gameplay actions, cross doors, and save
the resulting state. All behavior is selected by the Oblivion game profile;
the established Morrowind class, activation, Lua, and save paths remain in use
for Morrowind.

## Delivered source

### Native interactive classes

`apps/openmw/mwclass/esm4interactive.*` adds profile-native class adapters for
TES4 activators, books, containers, placeholder creatures, doors, flora, and
takeable ammunition, armor, clothing, ingredients, keys, miscellaneous items,
potions, and weapons. They provide names and tooltips, insert the appropriate
world or door collision, and dispatch through normal OpenMW activation
actions.

- Loose items enter the native player inventory and disappear from the cell.
- Containers transfer their native inventory once and retain opened state.
- Books display their title and bounded, tag-stripped text and retain read
  state.
- Flora yields its configured ingredient once and retains harvested state.
- Owned references display ownership and block the mutation while recording
  that ownership was enforced.
- Locked containers block access without losing their contents.
- Ordinary doors animate through the shared door machinery. Teleport doors
  resolve their destination cell and transform. A matching TES4 key unlocks a
  keyed door before transition.
- Activators, containers, flora, and doors select native TES4 action sounds
  when the record supplies one. Automated runs use a null audio sink but still
  reject load failures and assert the sound-selection path in their logs; the
  interactive launcher uses the normal audio device.

`apps/openmw/mwworld/oblivioninteraction.*` owns the bounded M5 mutations and
updates the authoritative `T4ST` runtime state at activation time. Quicksaves
therefore preserve exact item, inventory, lock, open, read, harvest,
ownership-check, activation-count, transform, and destination state. The
tutorial goblin remains a static creature placeholder but exposes its native
inventory, allowing the keyed tutorial transition to be exercised before M9
actor simulation.

### Traversal, collision, and focus

- TES4 interactive references now receive physics objects with the correct
  world/door collision type. The tutorial secret wall is physically closed
  despite its visual-only NIF flag, then disables both rendering and collision
  when opened.
- The unsupported detached TES4 player skeleton no longer prevents a usable
  first-person camera.
- Oblivion focus has a bounded tooltip-reference fallback for unsupported NIF
  ray mappings. Morrowind retains its existing ray behavior.
- Deterministic starts accept `Cell::ref=0xFORMID` with optional
  `::side=east|north|south`. This is a test/development selector layered over
  normal direct-cell startup and is not written into saves.
- The default Oblivion start uses `CGPlayerStartMarker`, so
  `scripts/run-oblivion.sh` enters the first prison interior without a console
  command.

The scripted tutorial wall is provisionally opened by its native activator in
M5. M7 replaces that narrow behavior with the real ObScript event and command
path; the resulting visible and persisted state is already represented in the
same native runtime structures.

### Activation bridge and data support

The shared Lua activation handler delegates ESM4 books and doors to their C++
classes while retaining the Morrowind handler and invisibility behavior. The
TES4 Key record is now present in the typed store and central record registry,
which allows key ownership to be resolved through stable `FormKey` values.
Reviewed activation and movement settings have explicit Oblivion profile
values rather than silently demand-created defaults.

### Reproducible developer tooling

- `scripts/run-oblivion.sh` builds a disposable configuration and launches the
  local executable with real input and audio, unsupported TES4 NIF loading, and
  persistent user data below `build/oblivion-userdata`.
- `scripts/oblivion_compat.py m5-acceptance` owns the complete M5 gate.
- Normal scenario input now includes key down/up, pollable held keys, atomic
  timed holds with a true zero-duration no-op, relative/absolute mouse motion,
  held mouse buttons, and focused-window actions. This was required because a
  one-frame synthetic transition can be missed by either original Oblivion or
  OpenMW.
- The original-game manifest loads a real save in a copied Proton prefix,
  accepts its reviewed missing-DLC warning, enters `ImperialDungeon01`, and
  positions a fixed camera using held console input. The acceptance gate
  proves menu-to-game and loaded-save-to-prison transitions, rejects a bright
  menu frame, and checks prison-viewpoint stability.
- `esmtool --type human` exposes the interaction-relevant TES4 reference data
  used to select and review fixtures.

## First interactive slice

From the repository root, with the accepted build and the original Oblivion
data installed, run:

```sh
cmake --build build --target openmw -j2
./scripts/run-oblivion.sh \
  "$HOME/.local/share/Steam/steamapps/common/Oblivion/Data"
```

Use the mouse to look, `W/A/S/D` to move, `Space` to activate, `F5` to
quicksave, and `Esc` to quit. Saves and screenshots are kept in
`build/oblivion-userdata`. Override that directory with
`OPENMW_OBLIVION_USER_DATA`, or omit the data argument and set
`OBLIVION_DATA`.

This is a deliberately bounded slice: walk and collision are interactive;
loose objects can be taken; containers and the placeholder tutorial creature
can be looted; books can be read; flora can be harvested; locks and ownership
are enforced; ordinary doors animate; and keyed teleport doors change cells.
NPC AI, combat, full ObScript, and Oblivion-native menus arrive in later
milestones.

## Acceptance command

The accepted campaign was run from an empty output directory:

```sh
python3 scripts/oblivion_compat.py m5-acceptance \
  --source "$PWD" \
  --build "$PWD/build" \
  --oblivion-data "$HOME/.local/share/Steam/steamapps/common/Oblivion/Data" \
  --morrowind-data "$HOME/.local/share/Steam/steamapps/common/Morrowind/Data Files" \
  --output "$PWD/build/oblivion-compat/m5-acceptance-certified-3" \
  --proton "$HOME/.local/share/Steam/steamapps/common/Proton - Experimental/proton" \
  --oblivion-install "$HOME/.local/share/Steam/steamapps/common/Oblivion" \
  --original-prefix "$HOME/.local/share/Steam/steamapps/compatdata/22330" \
  --steam-root "$HOME/.local/share/Steam"
```

It completed with exit code zero in 636.748565 seconds. Durable local evidence
is:

- `build/oblivion-compat/m5-acceptance-certified-3/acceptance.json`
- `build/oblivion-compat/m5-acceptance-certified-3/acceptance.html`
- `build/oblivion-compat/m5-acceptance-certified-3/visual-review-montage.png`
- `build/oblivion-compat/m5-acceptance-certified-3/image-metrics.txt`
- full test logs in the same directory

The data fingerprints used were:

- `Oblivion.esm`: 277,504,985 bytes, SHA-256
  `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`
- `Morrowind.esm`: 79,837,557 bytes, SHA-256
  `5c3c8c2cbd20e25901b59b3ece33d36b7ef0e3d60ad8d11828bcc61a5ead1647`

## Automated scenario and state results

All 13 graphical scenarios passed: closed wall, wall opening, item take,
container loot, book read, flora harvest, owned item, locked container,
animated door, teleport door, keyed route, Morrowind visual save/load, and the
original-game prison viewpoint.

All 11 independent native state checks passed:

| Check | Required persisted result |
| --- | --- |
| Closed wall | Player remains bounded by physical wall collision. |
| Wall open | Wall is disabled and marked opened; player crosses its former collision. |
| Take | Reference is deleted/marked taken and its base enters player inventory. |
| Container | Contents enter player inventory and the container is empty/opened. |
| Book | Read state is true. |
| Flora | Harvest state is true and the ingredient enters inventory. |
| Owned | Exact selected reference remains present and records `ownership_checked`. |
| Locked | Lock check is recorded and contents remain unchanged. |
| Animated door | Saved Z rotation reaches the open angle. |
| Teleport | Player reaches `ImperialDungeon01`. |
| Key route | Creature inventory is looted, Iron Key enters inventory, and the keyed door reaches `ImperialDungeon04`. |

The focused gate passed 12/12 runtime/save tests, 1/1 profile-service test,
and 17/17 Python harness tests. After acceptance, a clean rebuild of `openmw`,
`components-tests`, `openmw-tests`, and `esmtool` succeeded; the complete
component suite passed 1,483/1,483 and the complete engine suite passed
495/495.

The full official FormKey graph remained restart/reorder stable with 1,188,447
keys, 1,190,841 revisions, 2,949,394 references, zero unreviewed edges, and
fingerprint `fnv1a64:c36366457061760e`. The established Morrowind integration
campaign started and passed all 14 tests with no failures or incomplete cases.

## Visual inspection

All 35 generated PNGs passed automated nonblank inspection. Across the set,
mean luminance was 0.0399657-0.482318 and entropy was 0.602015-0.9228. The
original capture additionally passed the dark-prison ceiling, showed a 74.539%
changed-pixel transition from the loaded Weynon House save, and remained
perceptually stable between the initial and positioned prison frames
(`SSIM=0.99280773`, perceptual-hash distance 0).

The 18-frame review montage was inspected manually at source resolution. The
original prison is a real rendered gameplay frame rather than the menu. OpenMW
shows coherent prison geometry and lighting, a solid then absent secret wall,
the targeted take/container/book/flora/ownership objects, readable book text,
an ordinary door in distinct closed/open phases, the static tutorial loot
fixture, the keyed exit, and a different destination cell. No frame is empty,
all-black, all-white, or missing its required geometry.

Known magenta placeholder widgets and some material placeholders remain. They
are visible, bounded, and consistent across the captures; replacing them with
Oblivion UI identity and complete rendering is explicitly assigned to later
milestones rather than hidden by this gate.

## Backward compatibility and success assessment

Morrowind still uses its own record classes and historical save schema. Its
visual scenario created and reloaded a normal quicksave, and its integration
suite passed 14/14. The full shared component and engine suites also passed,
so no Morrowind regression is accepted by M5.

M5's deliver, verification, and success criteria are satisfied. The selected
prison slices are traversable without console intervention, required objects
are present and activatable, collision prevents falling or walking through the
closed wall, interactions produce exact native state, ordinary/keyed/teleport
doors work, paired original/OpenMW prison captures are valid, and Morrowind is
unchanged. There are no open M5 gates. M6 is the next bounded delivery.
