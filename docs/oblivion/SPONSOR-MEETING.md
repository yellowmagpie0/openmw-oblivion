# The Sponsor meeting

Start at the [Lower Hall checkpoint](BAURUS-LOWER-HALL.md). Tell Baurus he can meet the Sponsor, take the upper gallery door and cover him from above. Once Raven discovers the backup, help Baurus defeat the three cultists. Search Raven Camoran for the fourth volume of the Commentaries and the Sunken Sewers key.

## Continue playing

Run `build-windows/Continue-After-the-Sponsor.cmd`. The save is `build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Fourth-Commentary.omwsave`. The player has 100 health, all four books and the key, and faces the exit from the meeting room. Baurus survived and has started his travel package to Cloud Ruler Temple. The journal now points to deciphering the Commentaries with Tar-Meena.

## Runtime checks

The installed Oblivion master has SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`. Its quest records assign Baurus the chair at stage 78 and wait for him to sit before enabling Raven. Raven's travel completion starts his speech at stage 80. The fourth book's OnAdd script sets stage 90.

`build-windows/sponsor-gallery-final08` records the uninterrupted trip from the Lower Hall on executable SHA-256 `a166ce60af452c8c93755094e782273c955b178c2430e2f3d1d8a86550f0a0e5`. The player used the gallery entrance. Baurus sat down, Raven delivered his opening line, and the player broke cover. Baurus stood and drew his sword when native combat interrupted his chair package. The player fought alongside him, heard his post-fight line, and took the book and key through Raven's loot window. The player then healed and walked to the exit.

A separate concealed replay in `sponsor-gallery-quiet10` kept the player crouched and stationary. Raven continued his voiced speech through `022da8` before a patrolling guard spotted the player, warned him and triggered the ambush.

An earlier approach raised the alarm before Raven crossed the locked grate. `sponsor-gallery04/At-the-locked-grate.omwsave` preserves the obstruction. In `sponsor-gallery-grate06/ai-events.log`, Raven used his passage rights to open the blocking door (`060352`) during pursuit. Baurus and the player finished that fight and recovered the book. `sponsor-gallery-full07/Ambush.omwsave` records Raven's timed armor, helm and bound sword during the fight. The final checkpoint retains the ordinary robe and removes the temporary gear on death.

```powershell
python scripts/verify_sponsor_meeting.py build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Fourth-Commentary.omwsave --gallery-save build-windows/sponsor-gallery-final08/Gallery.omwsave --ambush-save build-windows/sponsor-gallery-full07/Ambush.omwsave --route-log build-windows/sponsor-gallery-final08/openmw.log --restart-log build-windows/sponsor-gallery-stock09/openmw.log
```

The stock-controls restart in `sponsor-gallery-stock09` displayed Tar-Meena as the objective and accepted normal activation of the exit to the Elven Garden Sewers. With a 30 FPS cap at 1280 × 720, the stationary exit view averaged 28.8 FPS across 721 frames during seconds 15–40. The 95th-percentile frame took 39.7 ms. The full Windows test inventories passed with 2,353 engine tests and 3,327 component tests. Inventories, results and source fingerprints are in `build-windows/checks-4d3fd3c00c084828861f10046476746a`.

Continue with [Tar-Meena's cipher](TAR-MEENAS-CIPHER.md).
