# Cell
Various code fragments written to run on the Cell Broadband Engine

> "the largest repository of non-library Cell code on the internet?"

## buddhabrot
A buddhabrot/nebulabrot renderer. Some articles about this are here: http://brnz.org/hbr/?tag=buddhabrot

## mandelbrot
A mandelbrot renderer, based on Jeremy Kerr's code (originally obtained from http://ozlabs.org/~jk/diary/tech/cell/hackfest08-solution-4.diary/ ) and used as a basis for a Tasmania University Computing Society (TUCS) Tech Talk in 2009 - video is available at http://youtu.be/FHcJ4jPcfNg.

## plasma
A diamond-square plasma generator, rendering 1920x1080 pixels at 60Hz, using a single SPU. Written as a self-contained SPU application, allocating memory via mmap syscall from the SPU. http://brnz.org/hbr/?tag=diamond-square has several articles about this code.

Projects rely on Linux virtual terminal & framebuffer code by Mike Acton, obtained from http://cellperformance.beyond3d.com/articles/2007/03/handy-ps3-linux-framebuffer-utilities.html The plasma project has modifications to run this code directly from a SPU.
