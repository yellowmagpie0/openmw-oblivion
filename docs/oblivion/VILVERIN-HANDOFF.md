# Vilverin — current handoff

The bounded Player Flare slice is accepted; see
[PLAYER-FLARE-VERIFICATION.json](PLAYER-FLARE-VERIFICATION.json). It is checkpoint
coverage, not full M16 or a complete campaign.

The original Vilverin single-actor leveled bandit slice is now accepted. Authored
references `06c2aa` / `06c2ab` select original NPCs at genuine Player level 1, receive
concrete nested LVLI gear, and retain reference/base/level, inventory and stats
through F5/fresh processes. Ordinary arrival visibly shows both armed bandits;
natural pursuit/hits provide real damage and weapon wear. The exterior-grid
repair preserves all 15 saved movement owners across encounter continuations.

Evidence: 132 normal and 78 instrumented semantic checks; 160 original-instruction
cases; 112 production-pool comparisons; 27 typed lists / 303 raw-decoded entries.
Full component suites 3,272 each and engine suites 2,257 each pass normal/ASan/UBSan.
Engine leak checking is enabled; rendered-game leak checking is disabled.
See [VILVERIN-LEVELLED-ACTORS-VERIFICATION.json](VILVERIN-LEVELLED-ACTORS-VERIFICATION.json)
for source/binary/input provenance, component reuse and retained failures, and
[the bounded contract](VILVERIN-LEVELLED-ACTORS-CONTRACT.json) for scope.

Single-actor runtime projection uses the authored placement key. Native scripted
spawn-root/child identity, grouped actors, cell respawn, all Player levels and
full bandit bow/combat/death behavior remain separate. Instrumented simulation
is slow; its probes use actual reference/package readiness. Normal camp imagery
is the visual layer. Exact original random streams are not claimed.

The bounded original mudcrab `06c927` creature-loot slice is accepted. Commit
`53b1f21ea7` resolves real CREA LVLI stock through the shared native resolver.
The first natural spawn rolled Crab Meat1 + Bone1. Ordinary arrival, two Flare
casts, visible corpse Take All and a fresh process preserve the dead, empty
corpse and exact Player quantities. The pre-fix Meat-only save remains Meat-only.
Instrumented fresh construction/F5 and continuation of the nonempty saved
inventory pass. 76 semantic checks, 112 focused constructor checks, both full
2,257 engine suites and 31 original-instruction loot cases pass. Unchanged full
component evidence is reused with an empty component-source/test diff.
See [MUDCRAB-LOOT-VERIFICATION.json](MUDCRAB-LOOT-VERIFICATION.json) and
[the contract](MUDCRAB-LOOT-CONTRACT.json). Rendered ASan uses leak detection off;
engine test-process leak checking is on. The timed-out instrumented initial
AI/input attempt is retained; only receipt timing changed, with all gameplay
assertions retained.

Current course remains sewer exit → independent Vilverin expedition → outside
alive with retained loot. Next action: select/reproduce the next original
container or dungeon-entry blocker, using the genuine post-loot character.
Container LVLI templates remain unresolved in original unopened saved state;
inspect/reproduce a bounded original chest before implementing that path.
No full M13/M16, respawn, grouped actors or full dungeon completion claim.

Reusable genuine inputs: exterior exit SHA256
cb89fc17ac333a5c4cbd03c0fa1445302e41ac3fd73102111494bf1a2e55f4b8;
ordinary shore and genuine encounter F5 paths/hashes are pinned in the receipt.
Current C++ source is `53b1f21ea7`; runtime evidence identifies tested dirty
`501d9e845a` source and exact executable/resource hashes.

ASan SDL replay requires the actual ELF `/usr/lib64/libasan.so.8.0.0` before the
input shim. The compiler's libasan path is a linker script on this host. Actual
ordering is verified in each accepted instrumented course.
