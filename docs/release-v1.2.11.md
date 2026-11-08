Fixes a black screen after the splash.

## Fixed

- **Black screen after the splash, at every boot.** The update check waits for GitHub over HTTPS; when a server
  went quiet the launcher waited forever (the network library here ignores the 15-second limit the code asked for),
  and the screen stayed black. HTTPS reads now give up after 8 seconds of silence, and `launcher.txt` says where
  the check stopped. If you are stuck on v1.2.8: on the USB, in `nuld/config.ini`, set `[actualizaciones] buscar = no`
  to boot, update by hand, then set it back.

## New since v1.2.5

- **Save icons for the games on your home server** (needs the server's `nuld_catalog.py` and `nuld_hash` from this
  zip's `server/`; restart it with `systemctl --user restart nuld-catalog`).
- **Covers for apps**: `covers/<title>.c16` and `_s.c16`, named after the title the launcher shows.
- **Faster covers and achievements**: covers read in one request per file; the first achievements query no longer
  waits for the update check; the client's "still loading" answer is asked again 5 times a second.

## Updating

With the network cable in, START at the update notice. Otherwise copy `launcher.elf` **and** `neutrino/` from the
zip over the ones on your drive.
