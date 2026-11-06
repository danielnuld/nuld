ORBIT is now **NULD**: a new name, a new look, and nothing to redo on your USB drive.

**Website, screenshots, manual:** https://danielnuld.github.io/nuld/

## New

- **A new name.** ORBIT clashed with other PS2 projects; the launcher, its repository and its site are now NULD.
- **A new look.** Black, steel and one acid accent: a loading screen with the four NULD logos taking turns over a
  wire sphere, 1 px frames and HUD corners on the selection, and the Archivo, IBM Plex Mono and Michroma typefaces.
  The in-game menu follows the same palette.
- **A new website** with the loading screen running in the browser, every screenshot in a full-size viewer, and
  the setup guide as a manual of its own.

## Updating from ORBIT

Nothing to do by hand. On the first boot NULD:

- moves `orbit/` to `nuld/` (config.ini, per-game options, achievements data);
- installs `BOOT/NULD.ELF` on your memory card and keeps `BOOT/ORBIT.ELF` up to date, so a FreeMcBoot autoboot
  that points at the old name keeps working (the setup wizard, START, can point it at the new one);
- keeps using `VMC/ORBIT.bin` as the shared virtual memory card where you have one.

ORBIT v1.1 can update itself to this version: with the network cable in, START at the notice. Otherwise copy
`launcher.elf` **and** `neutrino/` from the zip over the ones on your drive.

If you run the home game server, the service is now `nuld-catalog` (`server/nuld_catalog.py`, `server/nuld_hash`);
the old `orbit-catalog` keeps working with this version, as the protocol did not change.

The companion apps read the new `nuld/config.ini` from their next versions on (`mass0:/orbit/` still works).

## Known issues

- "Native" video mode can give a black screen on some HDTV setups: there, keep the default 480p.
- Loading over udpfs needs a wired network on both ends.

## Checksums (SHA-256)

```
NULD-v1.2.zip     9B5268D18B9B6FC884A5DAFEF0F4054778CFA0494F86DF1D0D55115B44F62EF9
launcher.elf      C5E84A110D00D1779B941DAA628E96BA94D0C405E954512B6AADA2AC6AB29A78
```

GPL-3.0. Ships a fork of [Neutrino](https://github.com/rickgaiser/neutrino) (AFL-3.0) with the in-game menu and the
achievements agent. Developed with [Claude Code](https://claude.com/claude-code) as the coding assistant.
