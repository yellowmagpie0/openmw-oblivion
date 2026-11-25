# Baurus at the Lower Hall

The course starts at the [Elven Garden sewer entrance](ELVEN-GARDEN-SEWER-ESCORT.md). Follow Baurus to the wheel, through the raised gate, across the second sewer interior and back into the southern section of the Elven Garden Sewers. Baurus stops near the two approaches to the Sponsor's meeting. Continue with [the Sponsor meeting and the fourth Commentary](SPONSOR-MEETING.md).

The installed `Oblivion.esm` has SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`. The wheel's activation script `09588b` plays `Forward` on wheel `0830ae` and linked gate `0830ac`. Baurus's escort package `022b8f` targets marker `022b89`. Its completion script checks that he is within 200 units of the marker before advancing *The Path of Dawn* to stage 76. The load doors along the route are `06af16` and `06af26`.

## Continue playing

Run `build-windows/Continue-in-the-Lower-Hall.cmd`. The checkpoint is `build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Lower-Hall.omwsave`. The player has full health, a fresh torch equipped, the first three Commentaries and the Sponsor's note. Baurus is within speaking distance. Activate him to choose who attends the meeting. The playthrough used “I'm not ready yet” at the first briefing and left both roles available.

## Route verification

The Lower Hall checkpoint also reproduces a FaceGen texture alignment issue.
On the stock human head, the nostrils sit below the eyes in the base DDS
but above them in the stored EGT color layer. The renderer now maps the two
layers to the same facial landmarks. Compare the same greeting in
`build-windows/baurus-face-before/Lower-Hall.png` and
`build-windows/baurus-face-after/Lower-Hall.png`. The rebuilt game also
rendered Phintias at First Edition in `build-windows/baurus-face-bookstore/Phintias.png`.
All 11 FaceGen tests passed, including the regression for both image origins
and mismatched texture sizes, in `build-windows/checks-d5eab9e06b7a4c4e84feaa49d55f9bd3`.

`build-windows/sponsor-full26/openmw.log` records an uninterrupted playthrough from `Path-of-Dawn-Elven-Garden-Sewers.omwsave` on the final executable. The player followed Baurus, fought at the wheel, crossed the open passage and used both load doors. Baurus defeated the rats and goblins and started the briefing at the meeting door. `sponsor-full26/Briefing.png` shows the three choices. `sponsor-full26/Uninterrupted-Lower-Hall.omwsave` preserves stage 76 after postponing. The separately prepared handoff restores the same decision with full health and a fresh torch.

```powershell
python scripts/verify_baurus_lower_hall.py build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Lower-Hall.omwsave build-windows/sponsor-full26/Uninterrupted-Lower-Hall.omwsave --route-log build-windows/sponsor-full26/openmw.log --restart-log build-windows/sponsor-confirm27/openmw.log
```

The verifier checks the two final animation poses, the wheel script's link to the gate, both characters' positions, the inventory, quest stage and ordered route events. The wheel and gate now play their authored animation and restore its final pose on load. The escort keeps solid obstacles in an opened gate's frame and replans the approach when the leaf moves. Other encounter fixes cover goblin archers' `AttackBow` animation, escort recovery from knockdown and the first-person torch attachment during player death. `sponsor-torch12` records an arrow hitting Baurus and `sponsor-recovery15` resumes from his knockdown.

The alternate spawn in `sponsor-passage13/After-06af16.omwsave` produced two rats. Their seven possible diseases previously caused melee damage to be discarded. `sponsor-rat20/openmw.log` records Baurus killing both and walking on. `sponsor-rat20/After-rats.omwsave` records distinct persistent selections for the two rats, one empty and one containing disease `008b74`.

The Windows RelWithDebInfo build passed the full inventories of 2,353 engine tests and 3,327 component tests. The inventories, XML results and tested source fingerprint are in `build-windows/checks-e18464314fb844a891a61f6ea6ee8c00`. In `sponsor-death22`, the low-health player's torch remained equipped through the death sequence, the game played `music/special/death.mp3`, and the reload prompt appeared. A different death in `sponsor-complete24` exposed an animated bone scale that prevented the transition to ragdoll physics. Native bone scale now contributes to joint positions while the physics bodies keep rigid orientations. Replaying the same fight in `sponsor-scale25` reached the reload prompt with no frame errors.

`sponsor-confirm27` cold-loaded the delivered checkpoint with the stock player script on executable SHA-256 `fb91ba205ec055eee270861cbaf924c3e403697aa86116baca72da688d8101f0`. Space reopened the three choices. At 1280 × 720 with a 30 FPS cap, seconds 15–40 of this torchlit view averaged 19.3 FPS across 483 frames. The 95th-percentile frame took 70.9 ms. Mechanics averaged 31.8 ms per frame. Frame pacing remains below target.
