<div align="center">
  <img src="docs/brand/jellyfin-ps3-lockup.png" alt="JellyFin [PS3]" width="480">

  A Jellyfin client for the PlayStation 3.

  [Download](https://github.com/jw-notes/JellyFin-PS3/releases/latest) · [What's changed](CUSTOM-CHANGES.md) · [Full manual](docs/MANUAL.md)
</div>

<p align="center">
  <img src="docs/screenshots/3.3/home.png" width="49%">
  <img src="docs/screenshots/3.3/movies.png" width="49%">
</p>

Watch your Jellyfin films, shows and music on a PS3, with a menu that looks and
feels like the console's own.

This is my continuation of
[vortigauntlet's LosslessAudio fork](https://github.com/vortigauntlet/JellyFin-PS3-LosslessAudio),
which itself started from [MontyMcK's JellyFin-PS3](https://github.com/MontyMcK/JellyFin-PS3).
All of their work is still here, I just fixed and added things on top.

## Features

- Lossless TrueHD, Atmos and DTS-HD MA in 5.1 or 7.1, plus 1080p and 24 Hz output for films
- Search as you type, with recent searches and Movies / Shows / Episodes filters
- A full-screen artwork screensaver that shows what each title is about
- Nine themes; each one recolours the whole app, the wave and the logo included
- Sign in with a Quick Connect code from your phone or computer, no typing needed
- Music player with visualisers, offline downloads, Live TV, and files from USB drives

<p align="center">
  <img src="docs/screenshots/3.3/search.png" width="32%">
  <img src="docs/screenshots/3.3/themes.png" width="32%">
  <img src="docs/screenshots/3.3/music.png" width="32%">
</p>

## Install

You need a PS3 on CFW or HEN and a Jellyfin server on your network.

1. Download **JellyFin-PS3.pkg** from the [latest release](https://github.com/jw-notes/JellyFin-PS3/releases/latest).
2. Copy it to a USB stick, or to `/dev_hdd0/packages/` over FTP.
3. Install it from Package Manager. It shows up under **TV / Video Services**.

Installing over an older version keeps your sign-in and settings.

For the best picture and sound, turn on **Settings → Playback → 1080p Playback**,
set **Settings → Audio → Audio Output** to 5.1, and pick **Max** under Quality on a
film's page. If something stutters, drop Quality to **Very High**.
The [manual](docs/MANUAL.md) explains every setting and button.

## Building

It builds with the ps3dev SDK, inside Docker. Use the **nightly-2026-07-26** SDK
(GCC 7.2): newer ones use GCC 13, and their builds freeze on real hardware.
The steps are at the end of [CUSTOM-CHANGES.md](CUSTOM-CHANGES.md#building).

## Credits

- **vortigauntlet**: lossless audio, the XMB interface, JellyWave and most of the app
- **MontyMcK**: the original JellyFin-PS3
- **Ethan Hammer**, **Lucian** and **orbisai0security** for their contributions
- [Movian](https://github.com/andoma/showtime), FFmpeg, liba52, libdca, minimp3, stb,
  [PSL1GHT](https://github.com/ps3dev/PSL1GHT), mohasi's NTFS/exFAT drivers, and Jellyfin

Every library, font and icon set, with its licence, is listed in [CREDITS.md](CREDITS.md).

## License

GPLv3, see [LICENSE](LICENSE). Copyright (C) 2026 vortigauntlet and MontyMcK;
changes since 3.2.3 copyright (C) 2026 jw-notes.
