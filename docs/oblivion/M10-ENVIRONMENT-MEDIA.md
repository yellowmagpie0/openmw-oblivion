# M10 environment, weather, audio, and video

Status: accepted on 2026-08-20. Completion revision
`831d5025a90ee71031033e70d3fd50342f8ef015`, building on
`a2367f5a241e3ef53bf322c90c8d37475afb4cd2` and
`6d2b64d001cd4431f92854cab808b5e5ae235e7e`.

M10 completes the native Oblivion environment and media path. Climate,
weather, cell lighting, water, region ambience, music, sound, voice, and Bink
media now use typed TES4 data and the official archives without relying on
Morrowind weather records or placeholder menu buttons.

## Delivered implementation

- Native `CLMT`, `WTHR`, region, cell, worldspace, and `WATR` data select sky,
  sun and glare, both moons and their phases, cloud layers, fog and visibility,
  wind, precipitation, climate timing, weather probabilities, interior/quasi-
  exterior lighting, and water appearance. Weather override/current state is
  serialized in normal saves.
- All seven base weather classes transition through the normal weather manager.
  Script requests without an explicit region resolve the player's native
  environment, and snow model paths follow the combined VFS rules.
- Native regional sound weather masks and fixed-point probabilities are decoded
  correctly. Directory-valued `SOUN` records choose a real shipped variant;
  static attenuation, positional range, `SOUN`/`SNDR`, voice duration, and
  cross-plugin dialogue-topic lookup feed the normal sound runtime.
- Explore, public, dungeon, and battle music selection is explicit and tested;
  quasi-exteriors use explore music. `Say`, `SayTo`, `PlaySound`, `PlaySound3D`,
  `ForceWeather`/`FW`, `ReleaseWeatherOverride`, and `PlayBink` execute through
  their real runtime handlers.
- Startup logos, title music, the animated main-menu map, intro, credits,
  scripted outro, loading images, post-load logo, and level-up sound use the
  official Oblivion media. The Oblivion menu uses readable localized text
  controls instead of missing Morrowind image-button textures; normal New Game
  enters `ImperialDungeon01` when no startup script has placed the player.
- Two official water records with no shipped texture use the shipped dungeon
  water as an explicit engine fallback. This is count-locked separately from
  missing official resources.

## Official-content audit

The acceptance audit enumerates the merged VFS and every direct M10 record
reference from all eleven official plugins. The base master used here is
`Oblivion.esm` SHA-256
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.
Exact hashes and sizes for all official plugins are in
`scripts/data/oblivion_compat/oblivion_m10_asset_counts.json`.

| Audit item | Count |
| --- | ---: |
| Official archives / merged VFS entries | 17 / 147,535 |
| `CLMT` / `WTHR` / `WATR` / `SOUN` records | 19 / 40 / 24 / 1,191 |
| Direct / directory references | 1,340 / 528 |
| Resolved files reached by record references | 2,755 |
| Loading images / music / videos | 59 / 28 / 9 |
| Sky meshes / sky textures / water textures | 12 / 62 / 2 |
| Sound effects / voice audio / voice lips | 2,090 / 50,942 / 50,915 |

The reference fingerprint is
`sha256:b867d2fa28700b001c557dce5205ae4a65b6ef87a0e415c65ad2e34f761759b4`
and the inventory fingerprint is
`sha256:bfc91e3b8ed2fdc1f373a1f48592f0b5861344fbd8deb0a586eb945a26e1f7eb`.
Thirty-six references absent from the released PC archives are exact,
reviewed, count-locked exceptions (Xbox low-fidelity variants, intentionally
silent directories, unused editor/prototype records, and the two procedural
water definitions); no missing reference is unreviewed. The complete report is
`build/oblivion-compat/m10-assets-acceptance/m10-assets.json`.

## Automated and runtime verification

The completed tree passed:

| Suite | Result |
| --- | ---: |
| Component tests | 1,512 / 1,512 |
| Engine tests | 502 / 502 |
| Python compatibility/state tests | 25 / 25 |
| M10 OpenMW runtime scenarios | 18 / 18 |
| Official asset exception and count locks | pass |

The runtime set includes seven isolated Clear, Cloudy, Fog, Rain,
Thunderstorm, Snow, and Overcast transitions; accelerated exterior, interior,
and quasi-exterior 24-hour cycles; three real-audio routes; startup/menu,
credits/intro, and scripted-outro media routes; and two Morrowind regressions.
Every accepted scenario has exit code zero, all required state/log assertions,
and no forbidden frame, assertion, crash, M10 media-resource, or playback
diagnostics.

Representative evidence:

- Weather: `build/oblivion-compat/m10-weather-{clear,cloudy,fog,rain,thunderstorm,snow,overcast}-final/`
  and `build/oblivion-compat/m10-weather-matrix.png`.
- Lighting/cycles: `build/oblivion-compat/m10-environment-acceptance/`,
  `build/oblivion-compat/m10-interior-environment-probe/`, and
  `build/oblivion-compat/m10-quasi-final2/`. Their saves contain native current
  and override weather records (`CWTH`/`WTHR`).
- Audio/scripts: `build/oblivion-compat/m10-audio-final/`,
  `build/oblivion-compat/m10-exterior-audio-acceptance/`, and
  `build/oblivion-compat/m10-public-audio-final/`. The real quest route decodes
  41.46 seconds of 48 kHz stereo audio; exterior and public routes decode 36.30
  and 25.08 seconds. `m10-runtime-waveform.png` confirms non-silent voice,
  sound-effect, and music output.
- Media: `build/oblivion-compat/m10-menu-text-final/`,
  `build/oblivion-compat/m10-interactive-media-final4/`, and
  `build/oblivion-compat/m10-outro-final2/` contain the official animated menu,
  credits, intro, and outro captures plus their playback logs.
- Shared-engine regression: `build/oblivion-compat/m10-morrowind-menu-final/`
  preserves the Morrowind image-button menu, while
  `build/oblivion-compat/m10-morrowind-runtime-final2/` exercises exterior
  weather, region ambience, music, waveform output, and a non-TES4 save.

The captures were inspected directly. Weather frames have distinct clouds,
fog, precipitation, illumination, and visibility; the exterior day/night cycle
changes sky and scene light; interior/quasi-exterior frames retain their cell-
defined lighting; official videos contain decoded, non-empty frames; and both
Oblivion and Morrowind menus are readable and interactive. The battle-state
playlist precedence is covered deterministically by the native audio unit test;
a complete hostile Oblivion encounter belongs to the actor systems introduced
in M11.

Magenta actor/HUD/crosshair placeholders visible in some world captures belong
to later actor and UI milestones. They are not M10 sky, weather, water, audio,
or video resource failures and are excluded only by narrowly scoped M10
resource assertions. No original executable, Wine, or Proton was run.
