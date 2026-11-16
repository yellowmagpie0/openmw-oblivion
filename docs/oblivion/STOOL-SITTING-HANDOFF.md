# Wellspring Stool — accepted bounded chunk

Original Stool `06efc7` / FURN `02c46d` in Wellspring Cave `0352a9` now supports
ordinary Player sitting, seated interaction and getup. This advances M14
furniture use and M11 authored animation. Full M4/M11/M14 remain open.
[Contract](STOOL-SITTING-CONTRACT.json), [original oracle](STOOL-SITTING-ORACLE.json),
[verification receipt](STOOL-SITTING-VERIFICATION.json).

The original active forward marker selects the authored entry, seated idle and
exit KFs. One Player owner retains marker/resource identity, phase, clock and
independent look angle in schema106. Movement/Jump retain the seat; targeted
activation picks up the original Weight. Activate with no usable focus requests
getup. Body clips stay on the body, while ordinary hand animation clocks and
first/third-person switching continue. Existing bed rest remains separate.
Other entry directions and NPC furniture packages remain open.

Both final normal and ASan/UBSan binaries pass four fresh processes: ordinary
sit → seated cold load/one Weight pickup/nonzero look → cold getup → cold
standing/walk. Actual first/third-person modes and captures were reviewed.
Seated phase3 cold continuation is runtime verified; entering/exiting phases
have structural round-trip coverage only. Original pose comparisons use declared
reasonable tolerances, with exact identity/state ownership checks.

The source is the [Scales continuation](SCALES-DROP-HANDOFF.md), SHA256
`a247a6d78c80f9627d1eb75fdfd2520c3b1d3786c0e7a7f28b1b0a357be0372a`.
No fork grants, teleports, actor enabling or inventory merges. Accepted normal:
`build/oblivion-compat/stool/normal-stool-fp-standing-final-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `f7e7119df4b8ad92895bac3a7f5d724e0c10369ec6f3d55dc0ba5e572ff43621`.
SAN: same root `sanitized-stool-fp-standing-final-02`, SHA256
`8bc75c390574bc1c7289be2aa9df92b17f37ec85deb1c69913eced9e14274635`.
Health70.48, Club/Cuirass owners and condition, held Scales1 and allocator20
remain; exactly one genuine Weight is added. Independent inventories stay separate.

Full engine2295 and components3306 pass in each mode; Python621 passes.
SAN engine partitions form an exact disjoint union. Unit leak checks are enabled;
rendered game courses use leak0 and do not establish rendered leak coverage.
The receipt preserves original source fingerprints, exact binaries and reused
component provenance. Retained failures include legacy schema fixtures, a
malformed observer, rejected first-person deformation/clock candidates, a long
POV tap entering Preview, and disk/quota failures before execution. Corrected
final cases pass. Prior borrowed archives are restored exactly; only own
reproducible superseded binary caches were retired, retaining source/hash/logs.

Next action: at this clean local checkpoint, fetch and merge GitHub `windows`
as requested, verify affected builds and actual content continuation, then choose
the next real-content acceptance gap. Do not push or claim full milestones complete.
