# Credits

JellyFin [PS] is built on other people's work. Thank you to all of them.

## People

- **[vortigauntlet](https://github.com/vortigauntlet)**: the
  [LosslessAudio fork](https://github.com/vortigauntlet/JellyFin-PS3-LosslessAudio) this
  continues. Lossless TrueHD and DTS-HD, the XMB interface, JellyWave and JellyDrop, 24 Hz
  output, Live TV, offline downloads, USB media and most of the app as it is today.
- **[MontyMcK](https://github.com/MontyMcK)** (Monty McKeefry, also Tuggs-Bunny): wrote the
  original [JellyFin-PS3](https://github.com/MontyMcK/JellyFin-PS3) client.
- **Ethan Hammer**, **Lucian** and **orbisai0security**: contributions to the original and the fork.
- **jw-notes**: this version (see [CHANGELOG.md](CHANGELOG.md)) and the seven extra themes.
- Parts of the code across the projects were written with the help of Claude (Anthropic).

Every commit, and who made it, is in the git history.

## Reference

- **[Movian](https://github.com/andoma/showtime)** by Andreas Öman and contributors. The video
  display loop, frame blending and seek path follow how Movian does them.
- **[Jellyfin](https://jellyfin.org)**: the server and its API.

## Code included in the app

| What | Used for | Licence |
|---|---|---|
| [FFmpeg](https://ffmpeg.org) (MLP and DCA decoder files) | TrueHD / Atmos and DTS-HD MA | LGPL 2.1+ |
| [liba52](https://liba52.sourceforge.io) (Aaron Holtzman, Michel Lespinasse) | Dolby Digital | GPL 2+ |
| [libdca](https://www.videolan.org/developers/libdca.html) (Gildas Bazin and others) | DTS core | GPL 2+ |
| [minimp3](https://github.com/lieff/minimp3) (lieff) | MP3 | public domain (CC0) |
| [stb_image, stb_truetype](https://github.com/nothings/stb) (Sean Barrett) | images and fonts | public domain / MIT |
| [mohasi/ps3-dev](https://github.com/mohasi/ps3-dev) NTFS and exFAT drivers | USB drives ([`third_party/mohasi_fs`](third_party/mohasi_fs)) | Apache 2.0 |
| [PSL1GHT](https://github.com/ps3dev/PSL1GHT) and the [ps3dev](https://github.com/ps3dev) toolchain | the SDK it is built with | MIT |
| libFLAC, FAAD2 (ps3dev portlibs) | FLAC and AAC | BSD / GPL 2+ |

## Fonts, icons and art

| What | Licence |
|---|---|
| Open Sans, Roboto Condensed, Material Icons | Apache 2.0 |
| Archivo, Noto Sans, Noto Sans JP, Michroma | SIL OFL 1.1 |
| Tabler Icons | MIT |
| Satoshi (Fontshare) | ITF Free Font License |
| PlayStation button sprites ([Kenney](https://kenney.nl)) | CC0 |
| Jellyfin logo ([Jellyfin project](https://github.com/jellyfin/jellyfin-ux)) | CC BY-SA 4.0 |

The interface's main font, Rodin, is the PS3's own. It is not included: the app reads
it from the console's firmware. See [source/gfx/fonts/LICENSES.md](source/gfx/fonts/LICENSES.md).

## License

JellyFin [PS] is GPLv3, see [LICENSE](LICENSE). Each bundled component keeps its own licence.
