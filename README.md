<div align="center">
  <img src="docs/brand/jellyfin-ps3-mark.png" alt="JellyFin [PS3]" width="150" height="150">

  <h1>JellyFin [PS3]</h1>

  A Jellyfin homebrew application for the PlayStation 3.

  <p>
    <a href="https://github.com/jw-notes/JellyFin-PS3/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/jw-notes/JellyFin-PS3?label=release&color=00a4dc"></a>
    <img alt="Platform" src="https://img.shields.io/badge/PS3-Evilnat%20CFW%204.93-003791">
    <img alt="Jellyfin" src="https://img.shields.io/badge/Jellyfin-10.11%20%7C%2012-aa5cc3">
    <a href="LICENSE"><img alt="Licence" src="https://img.shields.io/badge/licence-GPLv3-blue"></a>
  </p>

  <b><a href="https://github.com/jw-notes/JellyFin-PS3/releases/latest">Download</a></b> ·
  <a href="docs/MANUAL.md">Manual</a> ·
  <a href="CUSTOM-CHANGES.md">What's changed</a> ·
  <a href="CREDITS.md">Credits</a>
</div>

<br>

<p align="center">
  <img src="docs/screenshots/3.3.6/home.png" alt="The Classic home screen" width="100%">
</p>

Watch your Jellyfin films, shows, music and Live TV on a PS3, with hardware
H.264 decoding up to 1080p and lossless surround sound. Browse in the
**Classic** layout, laid out like Jellyfin in a web browser, or switch to the
**XMB** layout, which looks and feels like the console's own menu.

This is my continuation of
[vortigauntlet's LosslessAudio fork](https://github.com/vortigauntlet/JellyFin-PS3-LosslessAudio),
which itself started from [MontyMcK's JellyFin-PS3](https://github.com/MontyMcK/JellyFin-PS3).
All of their work is still here; I fixed and added things on top.

---

## Highlights

| | |
|---|---|
| 🎬 **Films and shows** | Hardware H.264 up to 1080p at 25 Mbps, true 24 Hz output, resume and Continue Watching, skip intro / recap / credits |
| 🔊 **Lossless sound** | TrueHD, Atmos and DTS-HD MA decoded on the console in 5.1 or 7.1, or Dolby Digital passthrough for a soundbar |
| 📺 **Live TV** | A channel grid with genre tabs, a guide, and remote-style controls; dead channels are skipped and dropped streams reconnect |
| 🔍 **Search** | Results as you type, recent searches, and Movies / Shows / Episodes filters |
| 🎵 **Music** | Albums, artists, playlists and lossless FLAC, with audio-reactive visualisers |
| 💾 **Offline and USB** | Download films and episodes to the PS3, or play MKV, .m2ts and .ts files from USB drives with no server |
| 🎨 **Make it yours** | Two layouts, nine themes that each recolour the whole app, an artwork screensaver, six languages |
| 🔐 **Easy sign-in** | Quick Connect: approve a code from your phone or computer instead of typing |
| ⬆️ **Updates built in** | Settings → System → Software Update downloads and installs new releases |

---

## Screenshots

<table>
<tr>
<td width="50%" align="center">
  <img src="docs/screenshots/3.3.6/livetv.png" alt="Live TV channel grid with genre tabs"><br>
  <sub><b>Live TV</b>: channel cards with genre tabs worked out from the channel names</sub>
</td>
<td width="50%" align="center">
  <img src="docs/screenshots/3.3.6/search.png" alt="Search results filtered to movies"><br>
  <sub><b>Search</b>: results as you type, filtered with L2 / R2</sub>
</td>
</tr>
<tr>
<td width="50%" align="center">
  <img src="docs/screenshots/3.3.6/screensaver.png" alt="Artwork screensaver"><br>
  <sub><b>Artwork screensaver</b>: backdrops from your library, with what each one is about</sub>
</td>
<td width="50%" align="center">
  <img src="docs/screenshots/3.3.6/settings.png" alt="Settings"><br>
  <sub><b>Settings</b>: a few short pages, each explained on screen</sub>
</td>
</tr>
<tr>
<td colspan="2" align="center">
  <img src="docs/screenshots/3.3/themes.png" alt="Synthwave theme in the XMB layout" width="70%"><br>
  <sub><b>Themes and the XMB layout</b>: every theme recolours the whole app, the wave and the logo included (shown: Synthwave)</sub>
</td>
</tr>
</table>

---

## Install

**You need:** a PS3 running **custom firmware** and a **Jellyfin 10.11 or 12**
server the console can reach on your network. Tested on **Evilnat CFW 4.93**.

1. Download **`JellyFin-PS3.pkg`** from the [latest release](https://github.com/jw-notes/JellyFin-PS3/releases/latest).
2. Copy it to the root of a USB stick, or to `/dev_hdd0/packages/` over FTP.
3. Install it from **Package Manager**. It appears under **TV / Video Services**.
4. Open it, pick your server, and sign in. With Quick Connect on (Jellyfin
   Dashboard → General), press `X` and approve the code on another device.

Installing over an older version keeps your sign-in and settings. Later
updates install from inside the app.

---

## Recommended settings

| For | Setting | Value |
|---|---|---|
| Sharpest films | Settings → Playback → **1080p Playback** | On |
| Best film quality | `X` on a title → **Quality** | Max (drop to Very High if it stutters) |
| Surround sound | Settings → Audio → **Audio Output** | 5.1 or 7.1 (Dolby Digital for an ARC soundbar) |
| Smooth film motion | Settings → Playback → **24Hz Output** | Auto (the default) |
| Live TV on Wi-Fi | Settings → Live TV → **Live TV Quality** / **Buffer** | Auto or 480p / 15 seconds |

A wired connection gives the best results for high-bitrate films. On Wi-Fi the
app automatically uses safer network settings. The [manual](docs/MANUAL.md)
explains every setting and button.

---

## Controls at a glance

| Where | Buttons |
|---|---|
| **Menus (Classic)** | D-pad / left stick move · `X` open · `△` quick-peek · `O` back · `L1` / `R1` Home, Search, Settings |
| **Film page** | `X` play / resume · `Down` to the quality row, then onto the cast · `Left` / `Right` scroll the cast |
| **Playing a film** | Any button shows the bar · `L2` / `R2` skip (hold to scrub) · `△` audio and subtitles · `L1` picture size · `O` stop |
| **Live TV** | `Left` / `Right` change channel · `Up` / `Down` volume · `L2` / `R2` change tab · `□` guide |
| **Music** | `L1` / `R1` previous / next track · `△` shuffle · `□` change visualiser |

Works with the DualShock 3, the official Blu-ray remote, and USB or Bluetooth keyboards.

---

## Live TV and your server

Live TV works with any Jellyfin tuner, including M3U / IPTV playlists. A few
server settings make a big difference:

- **Turn on hardware encoding** (Dashboard → Playback → Transcoding) if your
  server has an Intel, AMD or NVIDIA GPU. Live channels are always re-encoded
  for the PS3, and a low-power CPU can't keep up on its own.
- **Turn off "Read input at native frame rate"** on an M3U tuner (Dashboard →
  Live TV). With it on, channels can stall forever at ad breaks.
- A channel that shows a **server error** usually has an out-of-date address in
  the M3U list or a stream the server can't read; the app skips it when you
  change channel.

---

<details>
<summary><b>Supported formats</b></summary>

<br>

| | Supported |
|---|---|
| **Video** | H.264 up to 1080p (High profile, level 4.2), hardware-decoded. Other codecs are transcoded by the server. |
| **Surround audio** | TrueHD / Atmos (as its 7.1 bed), DTS-HD MA, DTS, Dolby Digital; AC-3 5.1 transcode as a fallback |
| **Music** | FLAC up to 24-bit / 192 kHz (lossless), MP3 and WAV; other formats are converted by the server |
| **USB and HDD files** | MKV, .m2ts and .ts from NTFS, exFAT or FAT32 drives (H.264 video) |
| **Subtitles** | Text and PGS drawn by the console, in 3 fonts and 3 colours |
| **Output** | 480p to 1080p, 24 Hz for films, overscan adjustment |

</details>

<details>
<summary><b>Troubleshooting</b></summary>

<br>

- **Something went wrong?** Settings → System → **Send Log to Server** uploads
  the log; it appears in Jellyfin under Dashboard → Logs as `upload_PS3_….log`.
  Over FTP it is at `/dev_hdd0/tmp/player_log.txt`.
- **Films buffer:** lower Quality on the film's page, or use a cable instead of Wi-Fi.
- **Live TV stutters:** set Live TV Quality to 480p and the buffer to 15 seconds,
  and check the server tips above.
- **Can't find the server:** make sure the PS3 and the server are on the same
  network, and enter the address by hand (for example `http://192.168.1.10:8096`).

</details>

<details>
<summary><b>Building from source</b></summary>

<br>

It builds with the ps3dev SDK inside Docker. Use the **nightly-2026-07-26** SDK
(GCC 7.2): newer SDKs use GCC 13, and their builds freeze on real hardware.

```powershell
# once: put ps3dev-linux-X64.tar.gz from ps3dev nightly-2026-07-26 next to tools/Dockerfile.ps3
docker build -f tools/Dockerfile.ps3 -t jellyfin-ps3-build:2026-07-26 tools

# build and check the package
docker run --rm --mount "type=bind,source=$PWD,target=/build" jellyfin-ps3-build:2026-07-26 make -j8 pkg
docker run --rm --mount "type=bind,source=$PWD,target=/build,readonly" jellyfin-ps3-build:2026-07-26 python3 tools/verify_pkg.py
```

The result is `JellyFin-PS3.gnpdrm.pkg`. Host tests run with
`make -f Makefile.host check` in `tests/`. The
[developer guide](docs/DEVELOPMENT.md) covers testing on a console, logs,
screenshots and how releases are made.

</details>

---

## Credits

- **vortigauntlet**: lossless audio, the XMB interface, JellyWave and most of the app
- **MontyMcK**: the original JellyFin-PS3
- **Ethan Hammer**, **Lucian** and **orbisai0security** for their contributions
- [Movian](https://github.com/andoma/showtime), FFmpeg, liba52, libdca, minimp3, stb,
  [PSL1GHT](https://github.com/ps3dev/PSL1GHT), mohasi's NTFS/exFAT drivers, and Jellyfin

Every library, font and icon set, with its licence, is listed in [CREDITS.md](CREDITS.md).

## Licence

GPLv3, see [LICENSE](LICENSE). Copyright (C) 2026 vortigauntlet and MontyMcK;
changes since 3.2.3 copyright (C) 2026 jw-notes.

<sub>JellyFin [PS3] is an independent project and is not affiliated with Jellyfin or Sony.
PlayStation is a trademark of Sony Interactive Entertainment.</sub>
