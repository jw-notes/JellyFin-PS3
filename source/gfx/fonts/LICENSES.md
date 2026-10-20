# Fonts

Every font the app draws with, where it comes from, and under what licence.
Every font compiled into the app is free to redistribute.

| Font | Used for | Licence | Licence text |
|---|---|---|---|
| **SCE-PS3 Rodin LATIN** | all interface text, and the fallback for every other role | Sony; **not shipped** | read from the console's own firmware at runtime |
| Archivo Expanded Bold | media titles (`--font-display`) | SIL OFL 1.1 | [OFL-Archivo.txt](OFL-Archivo.txt) |
| Archivo Expanded ExtraBold | the "JELLYFIN" wordmark in the lockup | SIL OFL 1.1 | [OFL-Archivo.txt](OFL-Archivo.txt) |
| Michroma | codec / quality values, eyebrows and section labels | SIL OFL 1.1 | upstream (Vernon Adams, Google Fonts) |
| Satoshi (Regular, Bold) | tab labels, clock, cast | ITF Free Font License | upstream (Fontshare) |
| Open Sans (Regular, Bold) | fallback when Rodin is unavailable, and the default subtitle face | Apache 2.0 | upstream |
| Noto Sans Bold | optional subtitle face | SIL OFL 1.1 | [OFL-NotoSans.txt](OFL-NotoSans.txt) |
| Noto Sans JP Regular | kana and kanji: the last link of every face chain | SIL OFL 1.1 | [OFL-NotoSansJP.txt](OFL-NotoSansJP.txt) |
| Roboto Condensed Bold | optional subtitle face | Apache 2.0 | [Apache-2.0-RobotoCondensed.txt](Apache-2.0-RobotoCondensed.txt) |
| Tabler Icons | UI glyphs | MIT | upstream |
| Material Icons | UI glyphs | Apache 2.0 | upstream |

## Rodin comes from the PS3, not from this repo

Rodin is the PS3's own system font and is what makes the interface look like it
belongs on the console. Sony's licence does not allow it to be redistributed, so
the app reads it from every console's firmware at startup:
`/dev_flash/data/font/SCE-PS3-RD-R-LATIN.TTF` (`rodin_load()` in
`source/ui/render/ui_text.cpp`). If it cannot be read, the interface falls back to
Open Sans.

The host tests and `tools/ui_preview` read a copy through the `JF_RODIN_TTF`
environment variable. Copy the file from your own console into `tests/local/`
(git-ignored) and point the variable at it. Without it the tests use Open Sans.

## Archivo is a Latin instance of the variable font

`Archivo-ExpandedBold.ttf` and `Archivo-ExpandedExtraBold.ttf` are fixed instances
(width 125, weight 700 and 800) of Archivo's variable font from
[google/fonts](https://github.com/google/fonts/tree/main/ofl/archivo), subset to
Latin-1, Latin Extended-A and common punctuation. stb_truetype cannot read
variable fonts, hence the instances. Anything outside the subset comes from Rodin
through `chain_of()`.

## Why these three for subtitles

The typefaces people associate with subtitles — Arial, Helvetica, Netflix
Sans, Tiresias Screenfont — are all proprietary and cannot ship in a GPLv3
package. These are the open faces closest to them:

- **Open Sans** — closest to Helvetica/Arial in colour and width, and already
  bundled for the UI, so it costs nothing. The default.
- **Noto Sans** — Open Sans's sibling; a shade more open, slightly more even
  in rhythm.
- **Roboto Condensed** — narrower, so a long line of dialogue fits on one row
  instead of wrapping to two. Worth having on a 16:9 screen.

All three are used **bold**. Every broadcaster and streaming service sets
subtitles semibold or heavier, and weight does more for legibility across a
room than the choice between one humanist sans and another.

## Why the added faces are small

The full faces are 631 KB (Noto Sans Bold) and 502 KB (Roboto Condensed Bold)
— together roughly the size of the entire package. They are subset to what a
subtitle actually needs:

- ASCII, Latin-1 Supplement and Latin Extended-A
- the punctuation SubRip carries: curly quotes, en and em dashes, ellipsis,
  guillemets, inverted `?` and `!`, the OE ligature, arrows

That is 328 glyphs each, **20.8 KB and 20.2 KB** — 97% and 96% smaller, and
about 3.5% of the package between them.

Regenerate with `fonttools`:

```
pyftsubset NotoSans-Bold.ttf --output-file=NotoSans-Bold-subset.ttf \
  --unicodes=U+0020-007E,U+00A0-017F,U+2013,U+2014,U+2018,U+2019,U+201C,U+201D,U+2026,U+00AB,U+00BB,U+2039,U+203A,U+00A1,U+00BF,U+0152,U+0153,U+0178,U+2190,U+2192 \
  --layout-features= --no-hinting --desubroutinize \
  --drop-tables+=GSUB,GPOS,GDEF,DSIG,LTSH,VDMX,hdmx,kern
```

then convert to a C array the way `notosans_bold.h` was made. The music note
`U+266A`, which lyric subtitles sometimes use, is absent from both upstream
faces and so cannot be subset in.

## Noto Sans JP is a subset, and the last link of every chain

 is Noto Sans JP Regular (the notofonts/noto-cjk
, sha256
) cut down
to 3,719 characters: every character the interface catalog uses, the kana, the
CJK punctuation and full-width forms, and the 2,965 kanji of JIS X 0208 level 1
(what a Japanese title or name is written in nearly always). 685 KB instead of
4.4 MB. It carries no layout tables and no hinting.

Regenerate with 3719 characters requested (needs
); it reads the characters the catalog needs from
, so it must be re-run when a translation brings in a
character the subset lacks.  fails when it does.

## History

Before 3.3.0 the build embedded Rodin itself, plus Microgramma, GT America Expanded
Bold and Mata Bold, none of which may be redistributed. Microgramma was replaced by
Michroma (drawn after it), GT America Expanded by Archivo Expanded Bold, and Mata by
Archivo Expanded ExtraBold.
