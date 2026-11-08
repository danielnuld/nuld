Covers for apps, and a log line for the save icons of server games.

## New

- **Covers for apps.** An app (APPS/) has no serial: its covers are now `covers/<title>.c16` and `_s.c16`, named
  after the title the launcher shows (tools/covers.py makes them from a PNG or JPG with that name).

## Changed

- `launcher.txt` logs each request for a home-server game's memory card ("nuld vmc remote"), to find why its save
  icon does not show yet.

## Updating

With the network cable in, START at the update notice. Otherwise copy `launcher.elf` **and** `neutrino/` from the
zip over the ones on your drive.
