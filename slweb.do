redo-ifchange slweb.o slweb.c defs.h
${SLWEB_CC:-gcc} -g -Wall -std=c99 -o $3 slweb.o -lunistring

