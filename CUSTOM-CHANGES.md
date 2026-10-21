# JellyFin [PS3] 3.3.5: changes

By jw-notes, based on [vortigauntlet/JellyFin-PS3-LosslessAudio](https://github.com/vortigauntlet/JellyFin-PS3-LosslessAudio) `v3.2.3`.
Tested on a retail PS3 with CFW at 1080p.

## Unreleased

- **Faster Live TV start.** The live reserve is now measured in programme time
  from the stream's own clock (PCR), not in bytes at the quality ceiling. A
  2.5 Mbps channel under the 4 Mbps ceiling used to wait 11-16 s for "8 s";
  it now waits about 8 s, less when the server sends a burst. The byte target
  stays as the memory ceiling and for streams without a PCR.
- **Live TV Buffer** (Settings > Playback): 4, 8 (default) or 15 seconds.
- **Live channels are passed through when the PS3 can play them.** Buffering
  went on after the start because the server re-encoded each channel in
  software at about 0.85x real time, so the buffer always ran dry. A stream
  copy of the same channel ran at exactly 1.0x. Live requests now let the
  server copy H.264 channels under the quality ceiling (High listed first,
  level 4.1); anything else is still re-encoded. Direct Stream = Off restores
  the old always-re-encode request.
- **Live TV Quality: Auto** (new default): the 720p frame (1080p with the
  1080p toggle) and an 8 Mbps ceiling, so most channels are copied untouched.
- **Circle cancels "Connecting to server..."** while the server opens a
  channel (up to a minute). Before, the only way out was switching off.
- Server tip: with *Read input at native frame rate* on the M3U tuner, the
  server sends live channels at exactly real time, so the reserve fills no
  faster than it plays. Turning it off lets the server send what the source
  already has, which fills the buffer almost at once.

## New in 3.3.5

- **Live TV in Classic.** My Media includes the server's Live TV tab even though
  it has no library ID. The channel list and guide keep the Classic header;
  guide/search retain their own Back actions, while Back from the list returns
  Home. The tile uses a local antenna icon without requesting server artwork.
- **Separate Live TV quality** (Settings > Playback > Live TV Quality): 480p /
  1.5 Mbps, 720p / 4 Mbps (default), or 1080p / 10 Mbps. Channels no longer
  inherit a movie's higher bitrate. The same setting controls the decoder size
  and every live stream/reconnect request; audio and TS overhead fit the budget.
- **Live buffering reserve.** Startup, channel changes and reconnects target
  about eight seconds at the configured stream rate, capped by the existing
  ring allocation, audio queue and a 20-second deadline. Startup keeps Start now;
  cancelling a reconnect stops cleanly. This trades some channel-switch speed
  for more reserve against short Wi-Fi gaps and adds buffer diagnostics.
- Host checks cover live/header/guide routing, settings and translations, and
  reserve sizes across bitrates and constrained memory. The console test on
  Wi-Fi showed improved Live TV playback, with some buffering still remaining.

## New in 3.3.4

- **Classic film details.** The page before Play keeps the Classic header instead
  of drawing the XMB spine and category divider. Its poster and details sit below
  the compact header, with the existing Play/Resume, version, quality and other
  actions retained.
- **Triangle preview artwork in Classic.** Home and library grids record the
  selected card's artwork key, image type, cached size and screen position, so
  the preview can show its cover and animate from the selected card.
- **Ratings and finish time on details.** Available server metadata shows the
  community score out of 10, critic percentage and age certification. A separate
  row keeps scores visible alongside an Ends at estimate. The estimate uses the
  selected version's runtime when known, accounts for Resume versus starting
  over, and follows the PS3's time zone, summer-time and 12/24-hour settings.
  Missing scores and unknown finish times are omitted.
- **Ratings and finish time on Triangle previews.** The quick-peek card shows
  available community and critic scores above its synopsis, alongside the age
  certification already present. An Ends at line uses the item's runtime and
  Resume position in the console's local time. Series and unknown runtimes do
  not get a guessed finish time. Scores load with the existing background facts
  request; missing, null and out-of-range values are omitted.
- Host regression checks cover selected artwork and position, paged grids,
  episode thumbnails, music artwork and invalid/off-screen selections.
  Finish-time checks cover runtime fallbacks, Resume/restart, invalid saved
  positions, local time formats and midnight rollover (22 checks passed).
  Translation catalog and font coverage checks also pass. Console verification
  remains pending for the new details and preview changes.

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
own version (3.3.x) and checks this repo's releases for updates.

**Sign-in.**
- Usernames and passwords with quotes, backslashes or spaces are escaped properly in
  the login request (they used to break it or be trimmed).
- Saved sessions with spaces in the username load correctly.
- The password is no longer written to the debug log.

**Startup hang if the update check couldn't start.** It now finishes instead of leaving
the boot animation running forever.

**Music visualizers hidden in Classic mode.**
Classic's plain browsing background carried into Now Playing and hid JellyWave
and JellyDrop. Music playback now enables the selected visualizer and restores
Classic's plain background when you leave. The saved choice and Off option are kept.

## Changes

- **Classic layout** (Settings > Display > Layout): a web-style Home with My Media,
  Continue Watching, Next Up, Downloaded and latest-library rows. Home, Search and
  Settings share a header; L1/R1 move between its pages.
- **Classic library pages.** Movies, Shows, Collections and generic libraries open
  a full-width poster grid under the same header. Seasons and episodes keep their
  breadcrumbs, with wide stills for episodes. Existing fetching, paging, A-Z,
  details and playback actions are reused. Up from the first row focuses the
  header; Circle at a library root returns Home, while drill-down keeps Back.
- **Classic Music and Playlists.** Square artwork with room for artist credits;
  Music retains Albums, Artists, Playlists, Genres and Songs, plus artist/genre
  and playlist drill-down. Up from the grid focuses the Music sub-tabs, then the
  shared header. Small displays with large UI text use one row when needed.
- **Media in Classic My Media.** A local tile opens Downloads, internal storage
  and USB drives under the Classic header. Its icon needs no server artwork;
  opening a place uses the existing folder browser and playback screens.

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

- **Updates from inside the app.** When a new release is out (at launch, or Settings →
  System → Software Update), the app offers to download it, checks it against the
  SHA-256 GitHub publishes, and has webMAN MOD install it once the app closes. Without
  webMAN the package is left in `/dev_hdd0/packages` for Package Manager.
- **Settings → System → About**: the version, credits and the licence of everything
  in the app.
- **Only free fonts are shipped.** The PS3's Rodin font is read from the console's own
  firmware instead of being bundled, and three commercial fonts were replaced with
  free ones (Michroma, Archivo Expanded), so the app can be shared openly.

## Tests

New host tests: version comparison, sign-in escaping and saved sessions, and both
keyboards (typing a server address, passwords, limits and random input; also checked
under AddressSanitizer). Run with `make -f Makefile.host check` in `tests/`.

Classic checks: `test_classic_library` exercises the production geometry and input
adapter across 480p, 576p, 720p and 1080p, UI scales and overscan. It covers poster,
wide and square art, header and Music sub-tab navigation, Media entry actions,
Back/Cancel routing and selection alignment when changing layouts. It passed
normally and with AddressSanitizer/UndefinedBehaviorSanitizer; `test_layout` also
passed. See [Classic library notes](docs/classic-libraries.md).

The updated PS3 package builds with the verified GCC 7.2 image and passes
`tools/verify_pkg.py` (payloads, TV category, version, executable and themes).
The new Classic pages and music visualizer visibility fix still need console
verification; the retail-PS3 testing noted above covers the earlier changes.

## Building

```powershell
# once: put ps3dev-linux-X64.tar.gz from ps3dev nightly-2026-07-26 next to tools/Dockerfile.ps3
docker build -f tools/Dockerfile.ps3 -t jellyfin-ps3-build:2026-07-26 tools
docker run --rm --mount "type=bind,source=$PWD,target=/build" jellyfin-ps3-build:2026-07-26 make -j8 pkg
docker run --rm --mount "type=bind,source=$PWD,target=/build,readonly" jellyfin-ps3-build:2026-07-26 python3 tools/verify_pkg.py
```

Install `JellyFin-PS3.gnpdrm.pkg` with Package Manager.

## Releasing

1. Bump `APP_VERSION` in `source/net/update_check.h` and `APP_VER` in `sfo.xml`.
2. Build, then copy `JellyFin-PS3.gnpdrm.pkg` to `release/JellyFin-PS3.pkg` and commit it with
   the source.
3. Tag that commit (`v3.3.3`, ...) and publish a GitHub release with the same file attached as
   `JellyFin-PS3.pkg`.

The app downloads updates from `release/JellyFin-PS3.pkg` at the release's tag, through
api.github.com: GitHub's normal download server only accepts ciphers the PS3's firmware does
not have. The file is checked against the release asset's SHA-256, so the two must match.

