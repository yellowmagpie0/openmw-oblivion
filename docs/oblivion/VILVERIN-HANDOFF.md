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

Next chunk: audit the observed original CREA inventory warnings and choose a
fresh original leveled creature whose LVLI loot exercises the separate creature
constructor. Reuse the shared list resolver; preserve authoritative saved empty
inventories instead of retrofilling old actors. Finish its actual gameplay and
fresh-process acceptance before expanding into grouped spawns or other work.

Reusable genuine inputs: exterior exit SHA256
cb89fc17ac333a5c4cbd03c0fa1445302e41ac3fd73102111494bf1a2e55f4b8;
ordinary shore and genuine encounter F5 paths/hashes are pinned in the receipt.
Current C++ source is `b71bb621ab`; later acceptance/fixture commits change no C++.

ASan SDL replay requires the actual ELF `/usr/lib64/libasan.so.8.0.0` before the
input shim. The compiler's libasan path is a linker script on this host. Actual
ordering is verified in each accepted instrumented course.
