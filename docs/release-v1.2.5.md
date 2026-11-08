Achievement unlocks show up in the game.

## New

- **Unlock notice in the game.** When the achievements client announces an unlock, the game pauses for about
  2.5 seconds under a box with "LOGRO DESBLOQUEADO  5 PTS" (or "ACHIEVEMENT UNLOCKED") and the achievement's title.
  The titles come from the client's list at launch (the ones still to earn, up to 128 per game).

## Fixed

- **The in-game menu did not show during gameplay** (Tekken 5 in a fight: the game froze for 10 s, no box). The
  menu, and now the notice, wait for a moment when the game is not sending anything to the GS (up to 2 s).
- **The achievements list (○) of games with subsets stayed empty** ("could not read the list"): Tekken 5's answer
  names the subset's achievement count first.

## Updating

With the network cable in, START at the update notice. Otherwise copy `launcher.elf` **and** `neutrino/` from the
zip over the ones on your drive.
