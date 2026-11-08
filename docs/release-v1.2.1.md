Fix for a freeze with achievements on.

## Fixed

- **Games froze at scene loads with achievements on** (seen in Tekken 5 from the home server: black or frozen
  picture after a fight or a Story mode replay). The achievements telemetry sent its snapshot to the IOP in the
  middle of the game's own transfers; it now waits for a free frame. It also stops when the game reboots the IOP,
  until the agent says where its new buffer is.

## Updating

With the network cable in, START at the update notice. Otherwise copy `launcher.elf` **and** `neutrino/` from the
zip over the ones on your drive.
