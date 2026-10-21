# Changelog

What changed in each release of JellyFin [PS3], newest first.

Releases from 3.3 on are by jw-notes. They start from
[vortigauntlet's](https://github.com/vortigauntlet/JellyFin-PS3-LosslessAudio)
3.2.3, which jw-notes fixed (much of it by hand) before making this version
public: it froze on the
Home screen on real hardware, ran out of memory at 1080p, and had sign-in and
update-check bugs (see 3.3.0 below). vortigauntlet's own release notes for 3.2.3
and earlier are kept at the end, as he wrote them.

## 3.3.9

- **Films no longer lose the network over Wi-Fi.** A film at 1080p on the
  large receive window hit the same console-wide "no buffer space" failure as
  Live TV did, 13 seconds in. The app now asks at start-up whether the console
  is on Wi-Fi or a cable; over Wi-Fi every stream uses the small window, and a
  wired console keeps the large one for high-bitrate films.
- **Classic details page redesigned**, after Jellyfin's web page: a larger
  poster at the left with the title, details, scores and buttons beside it
  (Play / Resume right under the details), the synopsis, labelled Genres and
  Director rows, and the cast as round photos.
- **The cast scrolls.** `Down` from the Quality row moves onto the cast;
  `Left`/`Right` scroll through everyone, with arrows when there is more. Up to
  30 people per title are loaded (was 12).

## 3.3.8

- **Smoother Live TV on Wi-Fi.** When data trickled in, the player kept
  waiting on part of a packet instead of feeding the decoder, so pictures
  stalled with plenty of video already buffered and half the frames were
  dropped to keep up with the sound. It now goes back to feeding the decoder.
- **No more lost network after a few minutes of Live TV.** On a laggy Wi-Fi
  link the large receive window let in more than the console's network memory
  could hold, and the whole console lost its network until a restart. Live TV
  now uses a smaller window (128 KB, plenty for a channel); films are
  unchanged.
- **Reconnects for about a minute** after a channel drops, instead of giving
  up after a few seconds.
- **Picture with 1080p Playback off.** A server that encodes in hardware sends
  live channels at 1080p whatever was asked; the decoder is now always set up
  for that on Live TV (it showed sound and no picture before).
- **Live TV settings have their own section** (Settings > Live TV): Live TV
  Quality and Live TV Buffer. Auto is 720p at 4 Mbps and no longer follows the
  films' 1080p Playback setting.
- **Classic Settings fits on one screen**: the list starts under the header,
  so all seven sections show without scrolling.

## 3.3.7

- **Classic is the default layout.** It now opens on the web-style Home with
  rows of posters under a header. A layout picked in Settings > Display >
  Layout is kept, and XMB is still one press away there.

## 3.3.6

- **Faster Live TV start.** The live reserve is now measured in programme time
  from the stream's own clock (PCR), not in bytes at the quality ceiling. A
  2.5 Mbps channel under the 4 Mbps ceiling used to wait 11-16 s for "8 s";
  it now waits about 8 s, less when the server sends a burst. The byte target
  stays as the memory ceiling and for streams without a PCR.
- **Live TV Buffer** (Settings > Playback): 4, 8 (default) or 15 seconds.
- **Live channels stay re-encoded.** Jellyfin 10.11 answers a live stream
  copy with HTTP 500 (a NullReferenceException in GetStreamingState), so live
  requests never ask for one.
- **Live TV Quality: Auto** (new default): 720p, or 1080p with 1080p
  Playback on.
- If the server answers a channel with HTTP 500, the reopen asks for a
  re-encode for the rest of that viewing.
- **Circle cancels "Connecting to server..."** while the server opens a
  channel (up to a minute), and quitting from the PS menu works there too.
  Before, the only way out was switching off.
- **Fewer Live TV freezes with hardware-encoding servers.** Live channels are
  asked for in High profile at every quality, which every server encoder can
  produce and the PS3 always decodes.
- **No more freezes when a server sends a bigger picture than asked for.**
  Some servers send live channels at full 1080p whatever quality you pick.
  Live TV now always has room for a 1080p picture, and any picture too big
  for the player stops playback with an error instead of freezing the
  console.
- **No server crash on channel changes.** Start/progress reports leave the
  LiveStreamId out: with it, Jellyfin 10.11's transcode kill timer looks the
  stream up with a null PlaySessionId and takes the whole server down
  (jellyfin/jellyfin#18035). The player closes the stream itself.
- **Dead channels are skipped.** A channel the server cannot open (an M3U
  source answering 403) is marked for this run and the change carries on to
  the next one in the same direction, up to three; later presses step over
  it. Channel changes retry once instead of three times.
- **Remote-style controls while watching Live TV.** D-pad Left/Right change
  channel and Up/Down change the volume (a level readout shows for two
  seconds). The left stick moves around the control bar instead of changing
  channel.
- **Classic Live TV page redesigned.** A full-width grid of channel cards
  (large logo, number and name, the programme on now and its progress) under
  the same heading layout as the library pages. Categories are tabs in the
  Music page's style, switched with L2/R2. Square opens the guide.
- **Genre tabs.** Channels are sorted into genres (News, Sports, Movies,
  Kids, Crime & Mystery, Comedy, Food and more) from their names, since
  Jellyfin passes on no channel categories. A genre needs three channels for
  a tab; Customise (Start) hides any. The main tab is now All Channels and
  starred channels are under Favourites.
- **L1/R1 hints use button icons**, like L2/R2 (e.g. Picture size).
- Server tip: if channels stop at ad breaks, turn *Read input at native frame
  rate* off on the M3U tuner. At an HLS discontinuity ffmpeg's -re pacing
  waits on the jumped timestamps; the player reconnects after 15 s without a
  picture either way.

## 3.3.5

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

## 3.3.4

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

## 3.3.0 to 3.3.3

The first public releases. Before publishing, jw-notes took vortigauntlet's
3.2.3, which did not run reliably on a real console, and fixed it, with much of
the work written by hand; the new features followed on top.

### Fixed in vortigauntlet's 3.2.3

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

### New and changed

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

---

# Earlier releases (vortigauntlet)

vortigauntlet's release notes, unchanged. The problems fixed in 3.3.0 above
were present in these versions.

## 3.2.3

### Added
- **Search types on the PS3 system keyboard** instead of the drawn one. Select the search field with X (or Down) to open it; confirming runs the search at once and highlights the first result.

### Fixed
- **Stuck on "Finding versions..." for a film opened from Search.** The first
  time a debrid title is opened from Search, the server adds it to your library
  under a new id, and the old one stops working. The page kept asking about the
  old one, so the versions never arrived until you opened the film again from
  the Movies tab. The page now switches to the new id at once, and opening the
  same search result again goes straight to it.
- **Films playing in the wrong language.** Playback started on whichever
  soundtrack the file marked as its default, which on some releases is a dub
  (Avatar's Russian remuxes start on Russian). It now starts on a soundtrack in
  your PS3's system language when the film has one, skipping commentaries, and
  only otherwise uses the file's default. Up to 16 soundtracks per version are
  listed now, up from 8, so an English track further down the list is no
  longer cut off.
- **Home's posters glide at 3.1's speed again**, and the details under a
  poster fade in as it arrives instead of waiting for it to stop.
- **The wave no longer stutters behind the version picker or the music
  player.** The version list is drawn by the graphics chip now, and every list
  remembers how it shortened a long title instead of working it out again every
  frame.
- **Episode titles use the wider type 3.1 had**, so most fit under the still.
- **The player bar goes away when paused too**, after 8 seconds without a
  button press. Skip Intro stays up for 10 seconds and Next Episode for 20.
- **Titles with no streams say so** instead of offering a Play that ends in an HTTP 500: the Version row reads "No streams found" and Play shows a message.
- **Playback that dies before its first picture explains why** instead of a black screen and a silent return to the media page.
- **Posters are sharper** (higher JPEG quality).

## 3.2.2

### Fixed
- **Menus no longer stutter while you flick between tabs.** The cache of
  drawn text was being thrown away completely every few seconds, and each time
  every label on screen had to be drawn again from scratch. Now only the oldest
  text is dropped and what is on screen stays ready.
- **Fits TVs that crop the picture's edges.** Most TVs outside a "PC" or
  "Just Scan" mode cut off the edges of the picture, which hid the logo, the
  clock and the button hints. The app now keeps a small margin by default, like
  the PS3's own menus. Settings → Display → Screen Size still adjusts it (or
  turns it off).
- **"Back to Season" froze the screen for seconds.** Leaving an episode's
  details page waited up to 2 seconds per request behind the page's background
  stream lookup before even asking the server. It no longer waits.
- **Music player inside that margin too.** The album art and the Up Next list
  no longer run off the edges on those TVs.
- **The 24Hz/50Hz output prompt** is now a properly sized panel in the middle of
  the screen, instead of small text in the corner where a TV could cut it off.
- **Version picker missing on a film opened from Search.** The first time a
  debrid title is opened, the server is still finding its versions; the page
  now says "Finding versions..." and fills them in when they arrive, instead
  of showing only Quality until you back out and in again.
- **Freeze on "Preparing" when starting a film.** Starting playback freed the
  poster memory while a poster that was still downloading could be written into
  it, right where the video decoder was being set up. On a slow connection, or
  whenever posters were slow to arrive, that could lock up the whole console.
  The poster loader now stands down cleanly before the player starts.
- **Sluggish menus when posters are slow.** The menu wrote a debug line to the
  hard drive for every poster it asked for, on the drawing thread, and the
  poster lock could leave the menu waiting on the lowest-priority thread. Both
  are gone: the log is written in the background and the locks hand priority
  to whoever holds them.
- **"Update available" on the latest version.** 3.2 and 3.2.1 still reported
  themselves as 3.2 to the update check, so 3.2.1 announced itself as an
  update on every launch.
- **ffwrap is now a one-click install** for Windows servers that stall on
  debrid streams: `ffwrap-for-jellyfin.zip` on the release has
  `install-ffwrap.bat` and `uninstall-ffwrap.bat`, which say SUCCESS or FAILED
  when they finish. It now downloads at 1.75× the film's bitrate instead of
  1.25×, so the PS3 builds a buffer and stops micro-stuttering.
- The log now says why a poster failed to load (no connection, the server's
  answer, or an image too large), not just that it did.

### Changed
- **Smoother background.** The gradient no longer shows bands on high-end
  TVs.
- **Longer titles under posters.** Titles use a condensed face, so about
  about 10 characters fit under a poster instead of 8, at a larger size.
- **The Next Episode prompt comes up later** (in the last 75 seconds rather
  than 90, or when the credits start), so it no longer gives away that a scene
  is the episode's last. Skip Intro now goes away after 5 seconds, and both
  prompts are translucent.
- **Posters lose the white outline.** The selected poster is marked by a soft
  purple glow that fades outward instead. Buttons keep their outline.

## 3.2.1

### Fixed
- **4:3 TVs: overlapping text on the details screen.** The Resume, Mark as
  watched and Favourite buttons and the Quality / Offline selectors now size to
  their text instead of printing over each other.

### New
- **Picture size hint in the player.** The hints bar now shows `L1` for
  picture size on films and episodes.

## 3.2

The biggest update yet: **Live TV, downloads, playing straight from USB drives,
six languages, Quick Connect sign-in** and a new Settings screen, plus a long
list of fixes.

### New
- **Quick Connect sign-in.** No more typing passwords with a controller: the
  console shows a code, you approve it in the Jellyfin app on your phone
  (Settings → Quick Connect), and you're in. Username and password still work.
- **Live TV.** If your server has Live TV (a tuner or an IPTV playlist), a Live
  TV tab lists your channels with what's on now and a TV guide. `↑`/`↓` change
  channel while watching. *My Channels* and *Recently watched* pages come
  first, and `Start` lets you hide the categories you don't use.
- **Downloads.** Save a film, an episode or a whole season (`△` on a season) to
  the PS3's hard drive or an **NTFS USB drive**, and watch it with no server.
  Downloads pause while you stream, resume after a restart, and always leave at
  least 10 GB free on the hard drive.
- **Play from USB drives, no server needed.** The Media tab plays MKV, .m2ts
  and .ts films from NTFS, exFAT or FAT32 drives, with lossless TrueHD /
  DTS-HD MA audio and subtitles, plus **FLAC, MP3 and WAV music** as albums.
- **Six languages:** English, 日本語, Português (Brasil), Deutsch, Français and
  Español. It follows your PS3's language, or pick one in Settings → Display.
- **A new Settings screen** laid out like the PS3's own: six categories
  (Playback, Audio, Subtitles, Downloads, Display, System), each with a short
  description of every setting.
- **Fit, Fill or Stretch** (`Select` in the player): Fill zooms a 16:9 film to
  fill a 4:3 TV.
- **Chapters** (`R1` in the player), for films whose server has chapter data.
- **Picture size** (`L1` in the player), for CRTs that cut off the edges.
- **Favourites**, with their own row on Home.
- **Dolby Digital output** (Settings → Audio → Audio Output), for soundbars on
  a TV's ARC port that lose the centre channel in 5.1.
- **Wake the server** with Wake-on-LAN (Settings → System). The console also
  wakes it by itself when it can't reach it.
- **The app finds your server again** if its address changes (for example the
  PC moving from Wi-Fi to Ethernet).
- **Send Log to Server** (Settings → System) for bug reports, no FTP needed.
- **HTTPS server addresses** for sign-in and browsing (experimental).
- **Pinned to the top of the Game column** in the XMB, above your games, and
  the XMB now shows the real version (03.20).
- Smaller touches: `←`/`→` skip 10 s with the player bar hidden, and clicking
  a stick shows the player stats.

### Fixes
- **Films start straight after the 24 Hz switch** again, with no second
  buffering screen in between.
- **The version list showed the same version over and over** on episodes a
  debrid plugin had listed many times. Repeats are now shown once.
- **Random freezes and missing artwork** when opening pages quickly: two
  internal locks were never actually created, so parts of the app could read
  each other's network replies. Fixed, along with **Software Update**, which
  could not run in the previous build.
- Resuming a film from its page started it over instead of where you stopped.
- Pressing the PS button while paused could make the player look frozen.
- Some pages opened empty and hung on retry when the server's reply was framed
  unusually.
- The menu froze for about five seconds at a time while the server was
  unreachable, and startup waited on a server that wasn't there.
- Small text on standard-definition (480/576) TVs is now readable.
- Oversized cover images and failed video-memory allocations no longer crash
  the app; a failed seek could close the stream twice; JellyDrop jumped once an
  hour.

### Buffering on a wired server PC? Check your router
If your Jellyfin PC is plugged into the router with a cable and you stream
through a **debrid / Stremio-style plugin** (Gelato and similar), the server can
download a film from the internet as fast as your connection allows when it
starts. Some routers can't keep up with that and stop passing traffic between
your PC and the PS3 for a few seconds at a time, so the film buffers and stalls
over and over even though everything is on a fast network. Two ways out:

- **A cheap gigabit switch** between the router, the PC and the PS3, so their
  traffic never goes through the router. This also protects other streaming,
  such as Moonlight.
- **Make the server download at film speed.** This release includes
  `ffwrap`, a small stand-in for Jellyfin's ffmpeg that limits how fast it
  reads from the internet (1.75× the film's own bitrate since 3.2.2). See
  [`tools/ffwrap/README.txt`](tools/ffwrap/README.txt).

The app also notices when a stream arrives in bursts with long gaps and
reconnects by itself.

## 3.1

**The new interface is now what everyone gets.** In 3.0 (and the beta before it),
the layered interface, JellyWave, and the GPU-drawn posters and text were all
switched off unless a setting file existed on the console, and nothing created
those files. A fresh install showed the old tab strip, a flat three-band wave,
and slower CPU-drawn menus. They are all on by default now. If you installed 3.0,
installing 3.1 over it is all you need to do.

### New
- **Settings → Software Update.** Shows the result of the launch check (up to
  date, a newer version available, or couldn't check). `X` checks again, or
  reopens the update popup when there is a new version.
- The update popup now says where to get the new `.pkg`.

### Controls
- **`O` stops playback**, the same as `Start`, whether or not the player bar is
  showing. If a track menu or the volume slider is open, `O` closes that first.
- **`□` hides the player bar** without waiting for it to time out. The player
  stats overlay is still on the remote's `DISPLAY` key and in Settings.
- **Remotes without `L1`/`R1`:** the Blu-ray remote's `|<<` `>>|` and `<<` `>>`
  keys now switch tabs in the menus, so a Bluetooth remote can reach every tab.
  Unrecognised remote keys are written to the debug log so they can be mapped.

### Fixes
- **Home posters on the selected row.** The poster cache has a fixed number of
  slots, and when Home wanted more than fit, the row you were looking at could
  lose out: some of its posters stayed blank until you moved off it. The
  selected row now loads first.
- JellyWave moves at the speed it was tuned to on a TV (a little calmer than 3.0's
  default).

### Turning things back off
Each of these still has a file in `/dev_hdd0/tmp/`; put `0` in it to switch that
feature off: `jellyfin_spine.txt` (layered interface), `jellyfin_gpuwave.txt`
(JellyWave), `jellyfin_gpucards.txt` and `jellyfin_gputext.txt` (GPU posters and
text). `jellyfin_jwspeed.txt` sets the JellyWave speed (default 14).
