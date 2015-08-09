/**
 * Definitions shared between PPE and SPE
 */
#ifndef _COMMON_H
#define _COMMON_H

#define SPE_ALIGN 0x80

#include <stdint.h>

struct pixel {
	uint8_t a, r, g, b;
};

struct fractal_params {
	/* the number of rows and columns in the resulting image, in pixels */
	int cols, rows;

	/* the cartesian coordinates of the center of the image */
	float x, y;

	/* per-pixel increment of x and y */
	float delta;

	/* maximum number of iterations */
	int i_max;

	struct pixel *imgbuf;
};

struct spe_args {
	struct fractal_params fractal;
	int n_threads, thread_idx;
} __attribute__((aligned(SPE_ALIGN)));

#endif /* _COMMON_H */
