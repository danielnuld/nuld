Faster covers and achievements in the launcher, covers for apps.

## Faster

- **Covers** are read from the USB in one request per file instead of 1 KB at a time.
- **Achievements**: the first game's query no longer waits for the update check (seconds of HTTPS at every boot),
  and the client's "still loading" answer is asked again 5 times a second instead of once.

## New

- **Covers for apps.** An app (APPS/) has no serial: its covers are `covers/<title>.c16` and `_s.c16`, named after
  the title the launcher shows (tools/covers.py makes them from a PNG or JPG with that name).

## Changed

- `launcher.txt` logs each request for a home-server game's memory card ("nuld vmc remote"), to find why its save
  icon does not show yet.

## Updating

With the network cable in, START at the update notice. Otherwise copy `launcher.elf` **and** `neutrino/` from the
zip over the ones on your drive.
