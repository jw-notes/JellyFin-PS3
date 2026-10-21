# Classic library pages

With **Settings > Display > Layout > Classic**, opening Movies or Shows from
**Home > My Media** now keeps the Home / Search / Settings header and opens a
full-width poster grid, rather than entering the XMB column/spine transition.
Collections and generic media libraries use the same page layout. Music and
Playlists keep this header too, with square artwork and room for artist credits.
Music retains Albums, Artists, Playlists, Genres and Songs, plus its existing
artist/genre and playlist drill-down and playback actions. Small displays with
large text use one row when two rows would overflow. Live TV retains its existing screen.

My Media also includes **Media**, for Downloads, the internal disk and USB drives.
Its list of places keeps the Classic header; folders and file playback use the
existing local browser. The Media tile uses a local icon and requests no server artwork.

The library name and visible range stay above the grid. Series drill-down keeps
the header and shows season/episode breadcrumbs; episodes use wide stills.
The grid reuses the existing item fetching, thumbnails, watched progress,
alphabetical jump bar, pagination, item details and playback actions.

- D-pad moves through posters; Up from the first row focuses the header.
- Left/Right on the header selects Home, Search or Settings; Cross opens it.
- Down or Circle on the header returns focus to the content. In Music, Up
  from the grid focuses its sub-tabs; another Up focuses the shared header.
- Circle at the library root returns to Classic Home. Inside a series or
  collection it retains the existing Back action; the A-Z rail retains Cancel.
- L1 returns from a library to Home; R1 opens Search.
- The saved selection stays visible when changing layouts, even when the two
  layouts have different column counts.

Geometry adapts to framebuffer size, overscan and UI scale. The implementation
uses the existing three rendering phases and shared grid renderer, with a
plain selection ring and no XMB spine. Header focus also has a CPU fallback
when GPU card rendering is disabled.

Validation: `test_classic_library` exercises the production geometry and input
adapter across 480p, 576p, 720p and 1080p, UI scales and overscan, including
square Music/Playlist art, Music sub-tabs, Media/header navigation, Back/Cancel
routing and layout-switch selection alignment.
It passes normally and with AddressSanitizer/UndefinedBehaviorSanitizer.
The existing `test_layout` also passes. The PS3 package is built with the
verified `jellyfin-ps3-build:2026-07-26` (GCC 7.2) image.

Console verification remains: enter Movies and Shows from My Media, scroll
several rows, open a series and back out, and switch between the header pages.
Also enter Music, change sub-tabs, open an artist/playlist and play a track;
then open Media and browse Downloads or a connected drive.

Classic keeps a plain browsing background, but Now Playing temporarily enables
its selected visualizer. JellyWave and JellyDrop render during music playback;
returning to browsing restores the Classic background. The saved visualizer
selection and the Off option are preserved.

Changes in 3.3.4: film details use the compact Classic header and
omit the XMB spine/divider. Triangle previews use the selected card's exact
artwork reference and position in Home and library grids. Verify on console:
open a film from each page, check its Triangle cover, open full details, Play or
Resume and return; also check an episode thumbnail and the existing XMB layout.

Details also show available server-provided community (/10), critic (%) and
age ratings. Ends at is an estimate for starting now: it uses the selected
version's runtime, falling back to the item's runtime, and subtracts a valid
resume position unless Play from start is selected. It refreshes with the
console clock, using the same local time and 12/24-hour settings as the header.
No end time is guessed when runtime is unknown. Scores and finish time have
their own row, so a long genre list does not push them out of view.

Triangle previews also show available community/critic scores and a local
Ends at line above the synopsis. Preview estimates use the item's runtime and
Resume position; edition/start-over choices remain on full details. Series
have no single finish estimate. Scores come from the existing background facts
request, and invalid/missing scores are hidden. Verify a rated film, a resumed
film and an item without ratings/runtime in both Classic and XMB layouts.
