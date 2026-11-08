Achievements inside games, for real this time, and a lighter agent.

## Fixed

- **In-game achievements never unlocked since v1.1.** The launcher handed Neutrino the agent's config file instead
  of the game's watch list, so the console never sent the game's memory to the achievements client. The list of
  achievements on ○ was not affected.
- **Games froze at scene loads with achievements on** (Tekken 5 from the home server). The agent kept about 25 KB of
  the IOP's 2 MB for snapshots it never sends; it now keeps 4 KB, one packet's worth.

## Updating

With the network cable in, START at the update notice. Otherwise copy `launcher.elf` **and** `neutrino/` from the
zip over the ones on your drive.
