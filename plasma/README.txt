A diamond-square plasma generator.


The particular goal in the design of this program was that it would run on a
single SPU and be able to generate full-screen 1920x1080 plasma at 60 fps,
which has been achieved.

The screen is divided into a number of 128x128 tiles which are rendered
individually.  Rendering of each tile depends on the tiles around it, and this
is resolved by storing the starting seed for all tiles, and recalculating
points as needed for the current tile.

While the program meets the original requirements (that of 60Hz, 1080p
performance), there is plenty of scope for further optimisation, as well as
making it look prettier than it does.

Implementation is in the file c2.cpp

I have written about parts of the implementation of this program, which may be
found at http://brnz.org/hbr/?tag=diamond-square

One of the other novelties of this code is that it is written entirely as an
SPU program, using mmap to allocate memory via the SPU syscall interface.
Details can be found here: http://brnz.org/hbr/?p=521

