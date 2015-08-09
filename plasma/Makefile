CXX		= spu-g++
CC		= spu-gcc
LD		= spu-g++
CFLAGS	= -O3 -std=gnu99 -ffast-math -I/usr/include -DNDEBUG
CXXFLAGS= -O3 --fast-math -Wall -Wextra -ffunction-sections -fdata-sections -DNDEBUG
LDFLAGS = -Wl,--gc-section
#-Wl,--print-gc-sections

all: plasma

plasma: c2.o cp_vt.o cp_fb.o ioctl.o
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f plasma
	rm -f *.o
