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

struct calculated_point {
	uint* addr;
	uint i;
};

typedef struct calculated_point* cpoint_ptr;
typedef vector unsigned int* vec_uint4_ptr;

struct fractal_params {
	/* the number of rows and columns in the resulting image, in pixels */
	int cols, rows;

	/* the cartesian coordinates of the center of the image */
	float x, y;

	vec_uint4_ptr sentinel[8];

	/* per-pixel increment of x and y */
	double delta;

	/* maximum number of iterations */
	int i_max;

	struct pixel* imgbuf;

	cpoint_ptr pointbuf[8];

	uint thread_idx;
};

struct spe_args {
	struct fractal_params fractal;
	int n_threads;
	int thread_idx;
} __attribute__((aligned(SPE_ALIGN)));

#endif /* _COMMON_H */
