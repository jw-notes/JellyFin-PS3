"""Archivo Expanded instances for JellyFin [PS], subset to Latin and written as C headers.

    pip install fonttools; python tools/fonts/make.py "Archivo[wdth,wght].ttf" source/gfx

The variable font is from https://github.com/google/fonts/tree/main/ofl/archivo (SIL OFL 1.1).
"""
import os, sys
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from fontTools import subset

src, out_dir = sys.argv[1], sys.argv[2]
UNICODES = (list(range(0x20, 0x7F)) + list(range(0xA0, 0x180)) +
            list(range(0x2010, 0x2028)) + list(range(0x2030, 0x203B)) + [0x20AC, 0x2122])

def build(weight, name, symbol, header):
    f = TTFont(src)
    instantiateVariableFont(f, {"wdth": 125, "wght": weight}, inplace=True,
                            updateFontNames=False)
    opts = subset.Options()
    opts.layout_features = ['kern', 'liga']
    opts.name_IDs = ['*']
    opts.notdef_outline = True
    opts.hinting = False
    s = subset.Subsetter(opts)
    s.populate(unicodes=UNICODES)
    s.subset(f)
    for rec in f['name'].names:          # say what it is
        if rec.nameID in (1, 4, 16):
            rec.string = name
    ttf = os.path.join(out_dir, symbol + '.ttf')
    f.save(ttf)
    data = open(ttf, 'rb').read()
    with open(os.path.join(out_dir, header), 'w', newline='\n') as h:
        h.write('// %s: Archivo (SIL OFL 1.1, Copyright 2020 The Archivo Project Authors),\n'
                '// width 125, weight %d, subset to Latin by tools/fonts/make.py.  See\n'
                '// source/gfx/fonts/LICENSES.md.\n\n' % (name, weight))
        h.write('static const unsigned int %s_ttf_len = %d;\n' % (symbol, len(data)))
        h.write('static const unsigned char %s_ttf[] __attribute__((aligned(16))) = {\n' % symbol)
        for i in range(0, len(data), 16):
            h.write('  ' + ', '.join('0x%02x' % b for b in data[i:i + 16]) + ',\n')
        h.write('};\n')
    print(name, len(data), 'bytes', len(f.getGlyphOrder()), 'glyphs')

build(700, 'Archivo Expanded Bold', 'Archivo_ExpBold', 'archivo_expanded_bold.h')
build(800, 'Archivo Expanded ExtraBold', 'Archivo_ExpXBold', 'archivo_expanded_xbold.h')
