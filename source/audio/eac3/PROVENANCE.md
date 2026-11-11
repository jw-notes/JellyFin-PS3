# Vendored AC-3 / E-AC-3 decoder (from FFmpeg)

- **Upstream project:** FFmpeg — https://ffmpeg.org/
- **Obtained from:** `https://github.com/FFmpeg/FFmpeg`, `master`, commit
  `68845e2` (the same commit as `../dcahd/`).
- **Licence:** LGPL-2.1-or-later (every file taken carries the LGPL header) —
  compatible with this application's GPL-3.0 licence. No GPL-only FFmpeg file
  is used.
- **Why FFmpeg's:** E-AC-3 (Dolby Digital Plus) is not AC-3. liba52 (`../a52/`)
  stops at AC-3 and there is no small standalone free E-AC-3 decoder, so the
  decoder is taken from libavcodec with the minimum of its framework, as the
  TrueHD (`../mlp/`) and DTS-HD (`../dcahd/`) decoders were.
- **Where it is used:** files on a drive (a server converts E-AC-3 to AC-3 for
  this player). AC-3 keeps going through liba52 and its passthrough path; this
  decoder takes E-AC-3 only (`adec_eac3.cpp`).

## What was taken

From `libavcodec/`, here: `ac3dec.c`, `ac3dec.h`, `ac3dec_float.c`,
`ac3dec_data.c`, `ac3dec_data.h`, `eac3dec.c`, `ac3.c`, `ac3.h`,
`ac3_parser.c`, `ac3_parser.h`, `ac3_parser_internal.h`, `ac3defs.h`,
`ac3tab.c`, `ac3tab.h`, `eac3_data.c`, `eac3_data.h`,
`ac3_channel_layout_tab.h`, `kbdwin.c`, `kbdwin.h`, `ac3dsp.c`, `ac3dsp.h`,
`bswapdsp.c`, `bswapdsp.h`.

Added to the shared `../mlp/ff/libavutil/`: `tx.c`, `tx_priv.h`,
`tx_template.c`, `tx_float.c` (the MDCT the decoder's IMDCTs run on),
`float_dsp.c`, `float_scalarproduct.c`, `lfg.c`, `lfg.h`, `md5.c`, `md5.h`,
`cpu.h`, `bprint.h`, `avstring.h`. (`tx.h` and `float_dsp.h` were already there
from the DTS-HD port, identical to this commit's.)

**No upstream file's contents were modified.**

## How it is compiled

Not through `SOURCES`: this directory is not in it. The wrappers in
`source/audio/` compile it:

- `eac3_api.c` — the decoder as one translation unit (`ac3dec_float.c`
  #includes `eac3dec.c` and `ac3dec.c`), with the tables, header parser and DSP
  it uses, and the entry points `eac3_api.h` declares.
- `eac3_tx.c`, `eac3_txf.c` — `tx.c` and `tx_float.c`, each its own unit as in
  FFmpeg. The double and int32 transform lists are empty: only float is used.
- `eac3_util.c` — the float DSP, the lagged Fibonacci generator (and the MD5 its
  seeding uses) that dithers zero-bit mantissas, `log2_tab.c`, and two
  libavutil functions (`av_log2`, `av_bessel_i0`). The DTS-HD decoder links
  these too; it used to stub `av_tx_*` and the float DSP.

## Local additions (not upstream)

- `../mlp/ff/libavcodec/parser.h`, `parser_internal.h`, `aac_ac3_parser.h` —
  stand-ins so `ac3_parser.c` compiles. It keeps the header parser the decoder
  needs (`ff_ac3_parse_header`) behind the same `#if CONFIG_AC3_PARSER` as
  libavcodec's AC-3 parser object; that object is compiled but never
  registered or called.
- `../mlp/ff/libavcodec/avcodec.h`, `codec_internal.h`: the fields the decoder
  touches (`audio_service_type`, `AVFrame.decode_error_flags`) and the codec
  descriptor's `CODEC_SAMPLEFMTS`. `../mlp/ff/libavutil/downmix_info.h`:
  `AVDownmixInfo`, which the decoder fills and `eac3_api.c` gives somewhere to go.
- `../mlp/ff/config.h`, `config_components.h`: `HAVE_X86ASM 0`,
  `CONFIG_EAC3_DECODER 1`, `CONFIG_AC3_PARSER 1`.
- The decoder's options (dynamic range scale and so on) are set by
  `eac3_api.c` to the defaults libavcodec's option system would give them.

## Verified

`tests/test_eac3_decode.c` decodes ffmpeg-made E-AC-3 5.1 and AC-3 5.1 files
frame by frame, as the player feeds it, and compares every sample with
ffmpeg's own decode of the same file: worst difference about 1.3e-5 (float
evaluation order), every sample present. It also covers a flush and a restart
mid-stream, and damaged and truncated frames. A real Dolby Digital Plus 5.1
track (60 s, 640 kb/s) gave the same result.
