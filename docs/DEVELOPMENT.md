# Developing JellyFin [PS]

How to build, test, try out and release the app. For what the app does, see the
[manual](MANUAL.md); for what changed in each version, see
[CHANGELOG.md](../CHANGELOG.md).

## The short version

```powershell
docker run --rm --mount "type=bind,source=$PWD,target=/build" jellyfin-ps3-build:2026-07-26 make -j8 pkg
docker run --rm --mount "type=bind,source=$PWD,target=/build,readonly" jellyfin-ps3-build:2026-07-26 python3 tools/verify_pkg.py
```

That builds `JellyFin-PS3.gnpdrm.pkg`, the package to install on a console.

---

## Toolchain

The app is PS3 homebrew written against **PSL1GHT** with the **ps3dev** SDK:
32-bit, big-endian PowerPC (the Cell's PPU), plus a little SPU code.

**Use the ps3dev `nightly-2026-07-26` SDK (GCC 7.2).** From September 2026 ps3dev
ships GCC 13, and the same code built with it hangs the GPU about a second after
the Home screen appears on real hardware. The pinned SDK is wrapped in a Docker
image:

```powershell
# once: put ps3dev-linux-X64.tar.gz from ps3dev nightly-2026-07-26 next to tools/Dockerfile.ps3
docker build -f tools/Dockerfile.ps3 -t jellyfin-ps3-build:2026-07-26 tools
```

### Fonts

The PS3's own Rodin font is not in the repo (it is Sony's). The app reads it from
the console's firmware at run time. For builds that need it on the PC, put
`SCE-PS3-RD-R-LATIN.TTF` in `tests/local/` (git-ignored) and pass
`-e JF_RODIN_TTF=/build/tests/local/SCE-PS3-RD-R-LATIN.TTF` to `docker run`.
Every other font is free and in `source/gfx/fonts/`.

### Output

| File | What it is |
|---|---|
| `JellyFin-PS3.gnpdrm.pkg` | The package to install (copy to `/dev_hdd0/packages/`) |
| `obj/pkg/USRDIR/EBOOT.BIN` | The signed executable, if you replace it in place over FTP |
| `release/JellyFin-PS3.pkg` | The released package (committed; see [Releasing](#releasing)) |

`data/jf_spu_kernel.bin` is rebuilt by every build; don't commit changes to it.

---

## Tests

Logic that can run on a PC lives behind host-tested headers and is checked in
`tests/`:

```powershell
docker build -f tools/Dockerfile.tests -t jellyfin-ps3-tests tools
docker run --rm --mount "type=bind,source=$PWD,target=/build" jellyfin-ps3-tests make -f Makefile.host -j8 check
```

The tests have their own image (`tools/Dockerfile.tests`): they build with the
host compiler and need libfaad (`test_aac`), ffmpeg (the tone files the audio
tests decode) and ntfs-3g (the drive images `test_lfs` makes), none of which
the PS3 build image has. The PowerPC byte-order check at the end is skipped
unless a ppc64 cross compiler and qemu are installed.

Run a single test with `make -f Makefile.host test_livetv && ./test_livetv`.
Delete a test binary first (or use `make -B`) if you changed a header it uses,
or an old binary can report a pass.

Tests worth knowing about:

| Test | Covers |
|---|---|
| `test_stream_request` | Every stream URL the player and downloads can ask for |
| `test_live_watch`, `test_livetv`, `test_livetv_prefs`, `test_livetv_quality` | Live TV buffering, channels, genre tabs, saved pages, quality |
| `test_classic_library`, `test_layout` | Classic layout geometry at every resolution |
| `test_settings_model` | The Settings rows, sections and help text |
| `test_i18n_text` | Every translation fits its box and every glyph is in the fonts |
| `test_offline` | Downloads |
| `test_release_version` | Version comparison for in-app updates |

### Translations

All on-screen text is English in the source (`TR("...")` / `TRN("...")`), with
Japanese, Brazilian Portuguese, German, French and Spanish in
`source/i18n/catalog/*.json`. After changing text:

```powershell
python tools/i18n.py gen     # regenerate source/i18n/i18n_tables.c
python tools/i18n.py check   # every string translated and used
```

---

## Trying it on a console

The app is developed on a retail PS3 with **Evilnat CFW 4.93** and
**webMAN MOD** (FTP on port 21, no login).

1. Build, then upload `JellyFin-PS3.gnpdrm.pkg` to `/dev_hdd0/packages/` over
   FTP and check its SHA-256 after the upload.
2. Install it from Package Manager (it installs over the old copy and keeps
   settings).

### Logs

| File (in `/dev_hdd0/tmp/`) | What's in it |
|---|---|
| `player_log.txt` | Everything: playback heartbeats every few seconds (`hb:` lines: frames, buffer, network rate), stream URLs, errors. `player_log.prev.txt` is the run before. |
| `crash_log.txt` | Step markers, written synchronously; the watchdog's report when the main loop stops |
| `update_detection.txt` | The launch update check |

Settings → System → **Send Log to Server** uploads the log to Jellyfin
(Dashboard → Logs, `upload_PS3_….log`).

### Screenshots

Write a request to `/dev_hdd0/tmp/jf_shot_req.txt` over FTP and the app saves
the screen to `/dev_hdd0/tmp/jfshot/`:

| Request | Result |
|---|---|
| `home` | One shot, `home.bmp` |
| `burst tour 30 6` | 30 shots, one every 6 seconds: `tour-01.bmp` … `tour-30.bmp` |

The screen holds still for about a second while each shot is taken.

---

## Things learnt the hard way

- **GCC 13 builds hang on hardware.** Use the pinned GCC 7.2 image (above).
- **Memory is tight.** Free RAM after start-up is about 20–30 MB at 1080p. The
  video decoder, jitter buffer and read-ahead ring are sized carefully; check
  `free=` in `crash_log.txt` after changes that allocate.
- **The PS3's network memory is small.** On Wi-Fi a large receive window lets
  in more than libnet's pool can hold, and *every* socket on the console fails
  ("no buffer space", `net_errno=55`) until a restart. Over Wi-Fi every stream
  uses a 128 KB window; a wired console uses 512 KB for high-bitrate films.
- **Servers don't always send what was asked for.** With hardware encoding,
  Jellyfin sends a live channel at its source 1080p whatever MaxWidth says, and
  a VA-API Baseline encode froze the console. Live TV asks for High profile and
  always sizes the decoder for 1080p; a picture bigger than the buffers ends
  playback instead of overrunning them.
- **Jellyfin 10.11 has live TV bugs.** It answers a live stream copy with HTTP
  500, and crashes outright when a transcode tied to a live stream with no
  PlaySessionId is cleaned up (jellyfin/jellyfin#18035). Live TV never asks for
  a copy, and leaves `LiveStreamId` out of its start / progress reports.
- **The update download avoids `github.com`.** The PS3's TLS lacks the ciphers
  GitHub's download servers require, so updates come through `api.github.com`
  (see [Releasing](#releasing)).
- **Every screen has to keep drawing.** The PS button's menu draws over the
  app's frames; a screen that draws once and then waits for a button freezes
  the whole console when the menu opens. Wait loops redraw and flip every frame.
- **Each PS3 user's data is save data.** The sign-in and the per-user settings
  (the list is in `source/util/jf_paths.cpp`) live in the save
  `JFPS30000-LOGIN` (`source/api/login_store.cpp`, `source/util/user_prefs.cpp`):
  the system picks the signed-in PS3 user's folder, so the app never has to
  work out who that is. At start-up the settings are unpacked to
  `/dev_hdd0/tmp/jf_user/` and `jf_data_path()` points there.
- **Escaping.** Writing C source through shell here-docs has turned `\n` into
  real line breaks and `\0` into NUL bytes. Write patch scripts to files, and
  scan edited files for stray control characters before building.

---

## Releasing

1. Bump the version everywhere it appears:
   - `APP_VERSION` in `source/net/update_check.h`
   - `APP_VER` in `sfo.xml`
   - `APP_VER` in `tools/verify_pkg.py`
   - the expected version in `tests/test_release_version.cpp`
2. Add a `## a.b.c` section at the top of [CHANGELOG.md](../CHANGELOG.md)
   (`make -f Makefile.host check-app-version` in `tests/` checks it matches),
   and update the manual and README if behaviour changed.
3. Build, run the host tests and `tools/verify_pkg.py`, then copy
   `JellyFin-PS3.gnpdrm.pkg` to `release/JellyFin-PS3.pkg`.
4. Commit, tag (`v3.3.9`, …), push, and publish a GitHub release with the same
   file attached as `JellyFin-PS3.pkg`.

The app finds new versions through the GitHub releases API, downloads
`release/JellyFin-PS3.pkg` at the release's tag through `api.github.com`, and
checks it against the release asset's SHA-256, so the committed file and the
attached one must be identical.

---

## Where things are

```
source/
  api/      Jellyfin API: items, PlaybackInfo, Live TV, play state
  audio/    Audio output, AC-3 / DTS / TrueHD / FLAC / MP3 decoders
  cache/    Poster and artwork cache
  gfx/      RSX setup, shaders, fonts, text rendering
  i18n/     Translations
  local/    USB and hard-drive files (NTFS / exFAT / FAT32, MKV / TS)
  music/    Music player and visualisers
  net/      HTTP / HTTPS, server discovery, update check and install
  offline/  Downloads
  player/   Playback: main loop, HUD, stream reader, threads
  spu/      SPU kernels for the visualisers
  ui/       Interface: Classic and XMB layouts, Live TV, search, settings, rendering
  util/     Settings, timing, logging
  video/    TS demux, hardware decoder, jitter buffer
tests/      Host tests (Makefile.host)
tools/      Docker image, package checker, translation and font tools
themes/     Theme .ini files (themes/jw-notes/ for the newer ones)
docs/       Manual, design notes; docs/history/ for finished plans
```
