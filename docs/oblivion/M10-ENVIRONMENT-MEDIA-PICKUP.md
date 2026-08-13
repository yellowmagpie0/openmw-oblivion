# M10 environment, weather, audio, and video pickup

Status: **in progress**, stabilized on 2026-08-13. Implementation revisions
`a2367f5a241e3ef53bf322c90c8d37475afb4cd2` and
`6d2b64d001cd4431f92854cab808b5e5ae235e7e`.

This is a pickup boundary, not an M10 acceptance report. The current slice is
buildable, tested, and committed, but the roadmap requires a broader real-data
weather and media matrix before M10 can be marked accepted.

## Implemented and usable

- Typed TES4 `CLMT`, `WTHR`, `WATR`, and regional-weather data feed the normal
  weather manager. Native climate timing, weather colors/fog, wind,
  precipitation classification, cloud textures, transition speed, and weather
  probabilities no longer depend on Morrowind weather records.
- Exterior sky selection supports native sun, glare, stars, moon flags and
  phase length. A worldspace with no explicit `CNAM`, including base-game
  Tamriel, deterministically uses `DefaultClimate` instead of the first climate
  in record order.
- Native weather exact fog reaches the camera fog manager. Native rain/snow,
  water textures/colors/opacity/fog/wind/waves/fresnel, ripples, and underwater
  fog have profile-specific render paths. Weather override state is serialized
  in `WTHR`/`OWTH`.
- Native ambient region sounds, music state selection, positional sound
  attenuation, `SOUN`/`SNDR` volume, voice lookup/playback, and decoder duration
  are wired into the sound runtime.
- ObScript `ForceWeather`/`FW`, `ReleaseWeatherOverride`, `PlaySound`,
  `PlaySound3D`, `Say`, `SayTo`, and `PlayBink` have real runtime handlers.
- Oblivion startup logos, title music, intro/outro video, main-menu loop,
  credits, loading images, level-up sound, and post-load logo are selected by
  the Oblivion profile. Scenario manifests for each unfinished verification
  area are checked in under `scripts/data/oblivion_compat/oblivion_m10_*.json`.

## Verified at this boundary

The clean `oblivion_m10_environment.json` run at Old Bridge passed. Its five
1280x720 captures are non-empty and visibly vary across the accelerated cycle;
the process selected `DefaultClimate`, native sky assets, both moons, Clear
weather, and `DefaultWater`. It wrote one 1.3 MiB quicksave containing `WTHR`
and `CWTH`, and produced no forbidden missing sky/weather/water resource,
shader, frame, assertion, abort, or crash diagnostics.

Evidence is below `build/oblivion-compat/m10-environment-acceptance/`:

- `scenario.json` (`passed: true`)
- `process.log`
- `oldbridge-initial.png`
- `oldbridge-cycle-{06h,12h,18h,24h}.png`
- `userdata/saves/Bendu_Olo/Quicksave.omwsave`

The stabilization pass also completed:

| Check | Result |
| --- | ---: |
| Build `openmw` and `openmw-tests` | passed |
| Focused Oblivion weather tests | 2 / 2 |
| Python compatibility tests | 19 / 19 |
| M10 manifest JSON validation | 5 / 5 |
| `git diff --check` | passed |

No Proton, Wine, or original executable was run.

## Remaining work before M10 acceptance

1. Run and iterate `oblivion_m10_weather_matrix.json`. Confirm every requested
   base weather becomes active, visually inspect its capture, verify clouds,
   precipitation, fog and transitions rather than only matching log lines, and
   add state/save-reload assertions for forced and released overrides.
2. Add representative interior and quasi-exterior 24-hour runs. Verify native
   cell lighting, fog and visibility indoors as required by the roadmap; the
   accepted exterior cycle does not prove this part.
3. Run and iterate `oblivion_m10_audio_scripts.json` with sound enabled. Verify
   ambient regions, public/dungeon/battle/explore music transitions,
   positional attenuation, `PlaySound`/`PlaySound3D`, cross-plugin `Say`/`SayTo`
   voice lookup, and capture decoded waveform/duration evidence. Audio code is
   implemented but has not received this real-playback acceptance pass.
4. Run and iterate `oblivion_m10_menu_media.json` and
   `oblivion_m10_outro_script.json`. Visually confirm logo order, main-menu
   loop, intro, credits, loading images and scripted outro, including skip/end
   behavior and return to the correct UI state. These manifests are prepared,
   not accepted evidence.
5. Audit all official base-content climate/weather/water/audio/video references
   against the combined VFS and prove that every used asset resolves. Review
   warnings for unsupported media codecs and missing resources, then add a
   count-locked report. M10's “every official asset” success criterion is not
   yet established.
6. Run the complete component and engine suites after the matrix fixes, plus
   targeted Morrowind weather/audio/media regressions. Only the focused M10 and
   Python tests were rerun for this stabilization boundary.
7. Inspect and compare representative frames and waveforms. An original-game
   montage was not produced, consistent with the instruction not to spend time
   running the original game under Proton; document whatever alternative oracle
   is chosen when M10 is finally accepted.

The current logs still show unrelated unsupported `GetButtonPressed` ObScript
diagnostics from a base-game object script during the exterior run. They do not
invalidate the environment probe, but they should not be confused with a clean
whole-game script result.

## M8 and M9 pickup status

No known implementation work remains in the defined M8 or M9 scope. Both are
committed and recorded as accepted in `IMPLEMENTATION-STATUS.json`, with full
unit and real-cell runtime evidence in their milestone reports. They were not
fully rerun during this stabilization pass. Their documented verification gap
is the roadmap's original/OpenMW montage: no original executable was invoked
for these deliveries. M9 also intentionally left water appearance to M10; that
path is implemented in the current M10 slice but still needs the broader matrix
above before it is accepted.
