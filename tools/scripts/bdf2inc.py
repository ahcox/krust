# A script to read a bitmap font in the old bdf format and output the glyphs in
# and array that can be included in a glsl shader.
import sys
import pprint
from pprint import pprint
import bdflib
from bdflib import reader
from bdflib.model import Glyph

MAX_CODEPOINT = 255;

#print(len(sys.argv))
#print(sys.argv)

bdf_filename  = sys.argv[1]
glsl_filename = sys.argv[2]
# How far above the visual bottom of the 8x16 pixel grid to place the base of
# the letters like capitals and all lower-case letters, excluding qypfj.
baseline = int(sys.argv[3])
above_line = 16 - baseline

def reverse_low_8_bits(intIn):
  reversed = 0;
  reversed |= (intIn >> 7) & 1
  reversed |= (intIn >> 5) & 2
  reversed |= (intIn >> 3) & 4
  reversed |= (intIn >> 1) & 8
  reversed |= (intIn << 1) & 16
  reversed |= (intIn << 3) & 32
  reversed |= (intIn << 5) & 64
  reversed |= (intIn << 7) & 128
  return reversed

#print("Converting BDF font file \"{}\" to source code in \"{}\".".format(bdf_filename, glsl_filename));
print(f"Converting BDF font file \"{bdf_filename}\" to source code in \"{glsl_filename}\".")

with open(bdf_filename, "rb") as handle:
    font = reader.read_bdf(handle)

print(font)
pprint(font.__dict__)

#for glyph in font.codepoints():
    #print(glyph)
    #print(glyph.__dict__)

# Capture the glyphs in order of increasing codepoint:
codepoints = [None] * (MAX_CODEPOINT + 1)

for glyph in font.glyphs:
    #print(glyph)
    if glyph.codepoint == -1:
        #print("Skipping glyph without a codepoint: ", glyph.name)
        continue
    if glyph.codepoint > 255:
        #print("Skipping glyph with a codepoint that is too high: ", glyph.name)
        continue
    print("\nFound a glyph of type: ", type(glyph))
    pprint(glyph.__dict__)
    codepoints[glyph.codepoint] = glyph

with open(glsl_filename, "w") as out:
    line = f"const uint8_t g_font[{MAX_CODEPOINT+1}][16]" + " = {"
    print(line, file=out)
    first = True
    for glyph in codepoints:
        if(first == False):
            out.write(",\n")
        first = False
        if(glyph == None):
            # ToDo: output a missing glyph box shape:
            out.write("    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)}")
            continue;
        print(f"\nGlyph code={glyph.codepoint}, name={glyph.name}")
        left_shift = 0
        right_shift = 0
        if(glyph.bbX > 0):
            right_shift = glyph.bbX
        if(glyph.bbX < 0):
            left_shift = -glyph.bbX
        print("shifts:", left_shift, right_shift)
        
        out.write("    {")
        # Adjust vertical bbox if it puts glyph pixels outside the 8x16 box:
        pixel_height = len(glyph.data)
        if(pixel_height + glyph.bbY > above_line):
          glyph.bbY = (above_line - pixel_height)
        
        # The glyph only has populated rows, so pad the output with zero rows:
        empties_above = above_line - pixel_height - glyph.bbY
        rows_written = 0;
        for i in range(empties_above):
            if rows_written < 15:
                out.write("uint8_t(0), ")
            else:
                out.write("uint8_t(0)")
            rows_written += 1
           
        for row in reversed(glyph.data):
            print("row raw: ", row)
            row = reverse_low_8_bits(row)
            print("row rev: ", row)
            row <<= left_shift
            row >>= right_shift
            print("row shf: ", row)
            #print(type(row));
            if rows_written < 15:
                out.write(f"uint8_t({row}), ")
            else:
                out.write(f"uint8_t({row})")
            rows_written += 1

        # The glyph only has populated rows, so pad the output with zero rows:
        empties_below = baseline + glyph.bbY
        for i in range(empties_below):
            if rows_written < 15:
                out.write("uint8_t(0), ")
            else:
                out.write("uint8_t(0)")
            rows_written += 1
        out.write("}")
        
    out.write("\n};\n")

