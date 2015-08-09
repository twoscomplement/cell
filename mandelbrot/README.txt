A mandelbrot fractal renderer, optimised for Cell BE

Draws an image based on various input parameters in the file fractal.data.

Based on ideas and code from 
http://ozlabs.org/~jk/diary/tech/cell/hackfest08-solution-4.diary/

This version is further optimised (parallelisation and unrolling) to
significantly improve performance.

Also added is support for outputting to the framebuffer, using utilities from
http://cellperformance.beyond3d.com/articles/2007/03/handy-ps3-linux-framebuffer-utilities.html

This program served as a basis for a Tasmania University Computing Society
(TUCS) Tech Talk, presented in 2009. Video is available here:
http://youtu.be/FHcJ4jPcfNg
