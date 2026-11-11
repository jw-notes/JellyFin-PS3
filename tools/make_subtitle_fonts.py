#!/usr/bin/env python3
"""Build the subtitle fonts in subtitle-fonts/.

JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
GPL-3.0-or-later, with the attribution terms in NOTICE. The fonts it makes keep
their own licence (SIL OFL 1.1; the texts are beside them in subtitle-fonts/).

The player draws subtitles in a Latin face and falls back, a character at a time,
to faces for the scripts that face lacks (ui_text.cpp, chain_of). These are the
fallbacks the package ships; each is read from USRDIR/fonts only while a subtitle
line needs it, so it costs memory only then. Korean comes from the console's own
firmware font and is not built here.

  JFSub-CJK.otf     Noto Sans SC Bold: Han (GB2312, Big5 level 1, JIS X 0208
                    level 1), kana, CJK punctuation, full-width forms
  JFSub-Hebrew.ttf  Noto Sans Hebrew Bold
  JFSub-Arabic.ttf  Noto Sans Arabic Bold, its Presentation Forms included: the
                    player has no shaping engine, so source/util/rtl_text.h maps
                    each letter to its joined form's code point, which must be in
                    the font's cmap

Layout tables, hinting and kerning are dropped: stb_truetype uses none of them.

Usage (needs fontTools: pip install fonttools):
  python tools/make_subtitle_fonts.py <folder with the Noto sources>
Sources: notofonts/noto-cjk Sans/SubsetOTF/SC/NotoSansSC-Bold.otf,
notofonts.github.io fonts/NotoSans{Hebrew,Arabic}/hinted/ttf/*-Bold.ttf
Then rebuild the package; tools/verify_pkg.py checks the three are in it.
"""
import sys
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "subtitle-fonts"


def codec_range(codec, lead, trail):
    out = set()
    for a in lead:
        for b in trail:
            try:
                ch = bytes([a, b]).decode(codec)
            except UnicodeDecodeError:
                continue
            if len(ch) == 1:
                out.add(ord(ch))
    return out


def han():
    """The Han characters of GB2312, Big5 level 1 and JIS X 0208 level 1: what Chinese and Japanese text uses."""
    gb2312 = codec_range("gb2312", range(0xB0, 0xF8), range(0xA1, 0xFF))
    big5_l1 = codec_range("big5", range(0xA4, 0xC7), list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF)))
    jis_l1 = codec_range("euc_jp", range(0xB0, 0xD0), range(0xA1, 0xFF))
    return {c for c in gb2312 | big5_l1 | jis_l1 if 0x4E00 <= c <= 0x9FFF}


def ranges(*pairs):
    return {c for lo, hi in pairs for c in range(lo, hi + 1)}


def build(src, out_name, wanted, family):
    font = TTFont(src)
    cmap = font.getBestCmap()
    keep = sorted(c for c in wanted if c in cmap)
    opts = subset.Options()
    opts.layout_features = []
    opts.hinting = False
    opts.drop_tables += ["GSUB", "GPOS", "GDEF", "DSIG", "LTSH", "VDMX", "hdmx", "kern", "vhea", "vmtx", "VORG",
                         "BASE", "JSTF", "MATH", "STAT"]
    opts.name_IDs = [0, 13, 14]  # copyright and licence; the names are set below
    opts.notdef_outline = True
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=keep)
    sub.subset(font)
    name = font["name"]
    for nid, value in ((1, family), (2, "Bold"), (4, family + " Bold"), (6, family.replace(" ", "") + "-Bold")):
        name.setName(value, nid, 3, 1, 0x409)
    path = OUT / out_name
    font.save(path)
    return path, len(keep)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    src = Path(sys.argv[1])
    OUT.mkdir(parents=True, exist_ok=True)
    jobs = [
        (src / "NotoSansSC-Bold.otf", "JFSub-CJK.otf",
         han() | ranges((0x3000, 0x303F), (0x3040, 0x30FF), (0x31F0, 0x31FF), (0xFF01, 0xFF60), (0xFFE0, 0xFFE6),
                        (0x2015, 0x2015), (0x2025, 0x2026), (0x2E80, 0x2EFF)),
         "JF Subtitle CJK"),
        (src / "NotoSansHebrew-Bold.ttf", "JFSub-Hebrew.ttf", ranges((0x0590, 0x05FF), (0xFB1D, 0xFB4F)),
         "JF Subtitle Hebrew"),
        (src / "NotoSansArabic-Bold.ttf", "JFSub-Arabic.ttf",
         ranges((0x0600, 0x06FF), (0x0750, 0x077F), (0x08A0, 0x08FF), (0xFB50, 0xFDFF), (0xFE70, 0xFEFF)),
         "JF Subtitle Arabic"),
    ]
    for src_path, out_name, wanted, family in jobs:
        path, n = build(src_path, out_name, wanted, family)
        print(f"{out_name:20s} {n:6d} characters {path.stat().st_size / 1024:8.1f} KB")


if __name__ == "__main__":
    main()
