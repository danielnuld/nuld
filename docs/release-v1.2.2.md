Achievements work again.

## Fixed

- **No achievements unlocked in v1.2.1.** v1.2.1's freeze fix checked the wrong DMA channel (SIF0, which is
  always running), so the console never sent the game's memory to the achievements client. It now checks SIF1,
  the channel it shares with the game.

## Updating

With the network cable in, START at the update notice. Otherwise copy `launcher.elf` **and** `neutrino/` from the
zip over the ones on your drive.
