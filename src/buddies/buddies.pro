
TEMPLATE = subdirs

SUBDIRS = \
  bd \
  strm2cif \
  strm2dxf \
  strm2gds \
  strm2gdstxt \
  strm2oas \
  strm2txt \
  strmclip \
  strmcmp \
  strmxor \

strm2cif.depends += bd
strm2dxf.depends += bd
strm2gds.depends += bd
strm2gdstxt.depends += bd
strm2oas.depends += bd
strm2txt.depends += bd
strmclip.depends += bd
strmcmp.depends += bd
strmxor.depends += bd
