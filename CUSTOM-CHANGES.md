# JellyFin [PS3] 3.3.0: changes

By jw-notes, based on [vortigauntlet/JellyFin-PS3-LosslessAudio](https://github.com/vortigauntlet/JellyFin-PS3-LosslessAudio) `v3.2.3`.
Tested on a retail PS3 with CFW at 1080p.

## Fixes

**Freeze on the Home screen (the main one).**
Builds made with the current ps3dev SDK hung the GPU about a second after Home appeared.
The official release is compiled with GCC 7.2.0, but ps3dev moved to GCC 13.2 in
September 2026. The same code built with GCC 13 hangs; built with GCC 7.2 it runs fine.
The build now uses the last GCC 7.2 SDK (nightly-2026-07-26), via `tools/Dockerfile.ps3`.

**Out of memory at 1080p.**
After startup only ~3.7 MB of RAM was left, so posters failed to load and the app
eventually froze, especially while browsing a large library.
- The video decoder now reserves 64 MB instead of 96 MB at 720p (still 2x what the
  firmware asks for, and the same as 1080p already used). This frees 32 MB.
- The poster cache stops taking memory once only 8 MB would be left.

Free memory after startup goes from ~3.7 MB to ~32 MB.

**False "new version" popup.**
A suffixed version (`3.2.3-custom`) was treated as older than `v3.2.3`. The app now has its
own version (3.3.0) and checks this repo's releases for updates.

**Sign-in.**
- Usernames and passwords with quotes, backslashes or spaces are escaped properly in
  the login request (they used to break it or be trimmed).
- Saved sessions with spaces in the username load correctly.
- The password is no longer written to the debug log.

**Startup hang if the update check couldn't start.** It now finishes instead of leaving
the boot animation running forever.

## Changes

- **TV / Video column.** The app installs under TV / Video Services instead of Game, as
  "JellyFin [PS3]". Same title ID, so it installs over the original and keeps
  settings.
- **Drawn keyboard kept, with a fallback.** Put `1` in `/dev_hdd0/tmp/jellyfin_sysosk.txt`
  to use the PS3's system keyboard instead.
- **Search uses the drawn keyboard again, with live results.** 3.2.3 had moved Search
  to the PS3 system keyboard, which only searches when you confirm. Results now update
  half a second after each letter (2+ letters). The search runs in the background, so
  typing never stalls. Square deletes a letter.
- **Screensaver choice** (Settings → Display → Screensaver): *Classic* (the original
  posters over the wave), *Artwork* (still, full-screen backdrops that crossfade, with
  the title, episode, year, rating, runtime, video/audio format and synopsis at the
  bottom left, like a streaming app) or *Off*. Artwork starts loading a few seconds
  before the screensaver is due, so it opens straight on artwork; items without a
  backdrop are skipped. **Screensaver Delay** sets the idle time, 30 seconds to 5
  minutes.
- **Themes colour everything.** JellyWave, JellyDrop, the Jellyfin logo and the
  "Jellyfin" wordmark now follow the theme. Golden Age gets its gold logo and a
  gold-to-copper wave; custom `.ini` themes use their `lk_mark_a`/`lk_mark_b` pair
  (or their accents) and the logo is recoloured to match. XMB Wave looks exactly as
  before.
- **Seven new themes by jw-notes**, shipped in the package (`themes/jw-notes/`): Deep
  Ocean, Emerald, Crimson Night, Aurora, Arctic, Midnight Rose and Synthwave. Pick them
  under Settings → Display → Theme. New installs start on **Deep Ocean** with the
  **Artwork** screensaver; a theme or screensaver someone has already chosen is kept.
- **Loading feedback.** Posters still loading show a soft shimmer instead of a flat
  box (Home, library rows and full library grids), and fade in when they arrive. A
  poster that failed to load stops shimmering.
- **Search: recent searches and filters.** With nothing typed, your recent searches are
  listed (X searches again, Square removes one); a search is remembered when you open
  one of its results. L2/R2 filter the results to Movies, Shows or Episodes instantly,
  without asking the server again.
- **"Because you watched …" row on Home**: titles similar to the last film or episode
  you finished, from the server's own similarity data. Hidden when there is nothing.
- **Translations** for all the new text (Japanese, Portuguese, German, French, Spanish).
- **Sign-in screens redesigned.** Centred and scaled for the TV: a clear choice between
  Quick Connect and username/password, and the Quick Connect code shown large in
  boxes in the middle of the screen, with instructions for a phone, tablet or computer
  (including the server's web address for a browser).
- **Menu Sounds toggle** (Settings → Audio → Menu Sounds) for the PS3's own navigation
  sounds.
- **Freeze watchdog.** If the main loop stops for 2 seconds, `crash_log.txt` records the
  step it stopped in and the last step the GPU finished. This is what found the
  compiler problem. Every crash log line also shows free memory.

- **Only free fonts are shipped.** The PS3's Rodin font is read from the console's own
  firmware instead of being bundled, and three commercial fonts were replaced with
  free ones (Michroma, Archivo Expanded), so the app can be shared openly.

## Tests

New host tests: version comparison, sign-in escaping and saved sessions, and both
keyboards (typing a server address, passwords, limits and random input; also checked
under AddressSanitizer). Run with `make -f Makefile.host check` in `tests/`.

## Building

```powershell
# once: put ps3dev-linux-X64.tar.gz from ps3dev nightly-2026-07-26 next to tools/Dockerfile.ps3
docker build -f tools/Dockerfile.ps3 -t jellyfin-ps3-build:2026-07-26 tools
docker run --rm --mount "type=bind,source=$PWD,target=/build" jellyfin-ps3-build:2026-07-26 make -j8 pkg
docker run --rm --mount "type=bind,source=$PWD,target=/build,readonly" jellyfin-ps3-build:2026-07-26 python3 tools/verify_pkg.py
```

Install `JellyFin-PS3.gnpdrm.pkg` with Package Manager.
