
CC	= powerpc-unknown-linux-gnu-gcc
CFLAGS	= -Wall -O3 -g -std=gnu99
LDFLAGS = -lpthread -lspe2

# we need libpng
CPPFLAGS += $(shell pkg-config --cflags libpng)
LDFLAGS += $(shell pkg-config --libs libpng)

all: fractal

fractal: fractal.o spe-fractal-embed.o parse-fractal.o png.o cp_vt.o cp_fb.o

spe-fractal-embed.o: spe-fractal
	ppu-embedspu -m32 spe_fractal $^ $@

spe-fractal: CC=spu-gcc
spe-fractal: LDFLAGS=-lm
spe-fractal: CFLAGS += -fwhole-program
spe-fractal: spe-fractal.c

clean:
	rm -f fractal
	rm -f spe-fractal
	rm -f *.o
