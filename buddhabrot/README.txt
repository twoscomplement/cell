Buddhabrot/Nebulabrot renderer for PS3
--------------------------------------

Multi-SPU point calculation - ponts are calculated on multiple SPUs and sent to
PPU to be drawn to framebuffer.

Extends Mandelbrot program, adding support for remove viewing via VNC.

Example output can be seen in buddhabrot.png

Key to the implementation is the "Main draw loop" in fractal.c and the fractal
calculation code in spe-fractal.c.


cp_{fb,vt}.{h,c} are (c) Mike Acton.
