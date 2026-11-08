Save icons for the games on your home server.

## New

- **The save icon of a game from the home server.** Its virtual memory card lives on the server, which the
  launcher never read: the catalog service now lists the card and hands over the newest save's icon.

## Updating

With the network cable in, START at the update notice. Otherwise copy `launcher.elf` **and** `neutrino/` from the
zip over the ones on your drive.

On the server, replace `nuld_catalog.py` and `nuld_hash` with the ones in the zip's `server/` and restart it:
`systemctl --user restart nuld-catalog`. An older catalog keeps working; the icons just do not show.
