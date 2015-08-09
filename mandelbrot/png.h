#ifndef _PNG_H
#define _PNG_H

#include <stdint.h>

#include "fractal.h"

int write_png(const char *filename, int rows, int cols, struct pixel *image);

#endif /* _PNG_H */
