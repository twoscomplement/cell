/**
 * SPE program to do a memset, by DMAing a SPE buffer into an address
 * provided by the PPE.
 *
 * The argv argument to main will contain the address of our parameters,
 * contained within a spe_args structure. We need to DMA this into the
 * SPE local store first.
 *
 * We then set up a local buffer, and set it to the byte we want to memset
 *
 * This bufffer is then DMA-ed to the PPE address specified in the spe_args
 * structure, until we've copied the correct amount of data to the PPE.
 *
 * Since the MFC generally works on 16-byte addresses, we 16-byte align
 * all buffers that will be used in DMA transfers. Rather than hardcoding
 * 16 all over the place, we use SPE_ALIGN instead.
 */

#include <stdio.h>
#include <stdint.h>
#include <spu_mfcio.h>
#include <string.h>
#include <math.h>

#include "common.h"

#define CHUNK_SIZE 16384

#define unlikely(x) (__builtin_expect(!!(x), 0))

#include <stdio.h>

#include "fractal.h"

static float interpolate(float x, float x_min, float x_max,
		float y_min, float y_max)
{
	x = (x - x_min) / (x_max - x_min);
	return x * (y_max - y_min) + y_min;

}

/*
 * given a the i and i_max values from a point in our (x,y) coorinates,
 * compute the colour of the pixel at that point.
 *
 * This function does a simplified Hue,Saturation,Value transformation to
 * RGB. We take i/i_max as the Hue, and keep the saturation and value
 * components fixed.
 */
void colour_map(struct pixel *pix, vector unsigned int i,
		unsigned int i_max)
{
	const float saturation = 0.8;
	const float value = 0.8;
	float v_min, hue, desc, asc, step;
	int x;

	for (x = 0; x < 4; x++) {


		hue = spu_extract(spu_convtf(i, 0), x) / (i_max + 1);
		v_min = value * (1 - saturation);

		/* create two linear curves, between value and v_min, of the
		 * proportion of a colour to include in the rgb output. One
		 * is ascending over the 60 degrees, the other descending
		 */
		step = (float)((int)floor(hue) % 60) / 60.0;
		asc  = (step * value) + ((1.0 - step) * v_min);
		desc = (step * v_min) + ((1.0 - step) * value);

		if (hue < 0.25) {
			pix->r = value * 255;
			pix->g = interpolate(hue, 0.0, 0.25, v_min, value)
				* 255;
			pix->b = v_min * 255;

		} else if (hue < 0.5) {
			pix->r = interpolate(hue, 0.25, 0.5, value, v_min)
				* 255;
			pix->g = value * 255;
			pix->b = v_min * 255;

		} else if (hue < 0.75) {
			pix->r = v_min * 255;
			pix->g = value * 255;
			pix->b = interpolate(hue, 0.5, 0.75, v_min, value)
				* 255;

		} else {
			pix->r = v_min * 255;
			pix->g = interpolate(hue, 0.75, 1.0, value, v_min)
				* 255;
			pix->b = value * 255;
		}
		pix->a = 255;

		pix++;
	}

}
/**
 * Render a fractal, given the parameters specified in @params
 */
void render_fractal(struct fractal_params *params,
		int start_row, int n_rows)
{
	int r, x, y;
	unsigned int i;
	/* complex numbers: c and z */
	vector float cr, ci, zr, zi;
	vector float x_min, y_min, tmp;
	vector float increments;
	vector unsigned int escaped;
	vector unsigned int escaped_i;
	/* we can't pass commas to a macro, so use a const var instead */
	const vector float limit = {4.0f, 4.0f, 4.0f, 4.0f};

	increments = spu_splats(params->delta) *
		(vector float){0.0f, 1.0f, 2.0f, 3.0f};

	x_min = spu_splats(params->x - (params->delta * params->cols / 2)) +
		increments;
	y_min = spu_splats(params->y - (params->delta * params->rows / 2));

	for (r = 0; r < params->rows && r < n_rows; r++) {
		y = r + start_row;
		ci = y_min + spu_splats((float)(y * params->delta));

		for (x = 0; x < params->cols; x += 4) {
			escaped_i = (vector unsigned int){0, 0, 0, 0};
			cr = x_min + spu_splats(params->delta * x);

			zr = (vector float){0, 0, 0, 0};
			zi = (vector float){0, 0, 0, 0};

			for (i = 0; i < params->i_max; i+=16)  {

#define ITERATE()		/* z = z^2 + c */				\
				tmp = zr*zr - zi*zi + cr;			\
				zi =  (vector float){2.0f, 2.0f, 2.0f, 2.0f}	\
					* zr * zi + ci;				\
				zr = tmp;					\
										\
				/* escaped = abs(z) > 2.0 */			\
				escaped = spu_cmpgt(zr * zr + zi * zi, limit);	\
										\
				/* escaped_i = escaped ? escaped_i : i */	\
				escaped_i = spu_sel(spu_splats(i), escaped_i,	\
						escaped)			\
									
				ITERATE(); ITERATE(); ITERATE(); ITERATE();
				ITERATE(); ITERATE(); ITERATE(); ITERATE();
				ITERATE(); ITERATE(); ITERATE(); ITERATE();
				ITERATE(); ITERATE(); ITERATE(); ITERATE();

				/* if all four words in escaped are non-zero,
				 * we're done */
				if (!spu_extract(spu_orx(~escaped), 0))
					break;
			}

			colour_map(&params->imgbuf[r * params->cols + x],
					escaped_i, params->i_max);

		}
	}
}

/*
 * Our local buffer to DMA out to the PPE. This needs to be aligned to
 * a SPE_ALIGN-byte boundary
 */
struct pixel buf[CHUNK_SIZE / sizeof(struct pixel)]
	__attribute__((aligned(SPE_ALIGN)));

/*
 * The argv argument will be populated with the address that the PPE provided,
 * from the 4th argument to spe_context_run()
 */
int main(uint64_t speid, uint64_t argv, uint64_t envp)
{
	struct spe_args args __attribute__((aligned(SPE_ALIGN)));
	int row, bytes_per_row, rows_per_dma, rows_per_spe;
	uint64_t ppe_buf;

	/* DMA the spe_args struct into the SPE. The mfc_get function
	 * takes the following arguments, in order:
	 *
	 * - The local buffer pointer to DMA into
	 * - The remote address to DMA from
	 * - A tag (0 to 15) to assign to this DMA transaction. The tag is
	 *   later used to wait for this particular DMA to complete.
	 * - The transfer class ID (don't worry about this one)
	 * - The replacement class ID (don't worry about this one either)
	 */
	mfc_get(&args, argv, sizeof(args), 0, 0, 0);

	/* Wait for the DMA to complete - we write the tag mask with
	 * (1 << tag), where tag is 0 in this case */
	mfc_write_tag_mask(1 << 0);
	mfc_read_tag_status_all();

	/* initialise our local buffer */
	ppe_buf = (uint64_t)(unsigned long)args.fractal.imgbuf;
	args.fractal.imgbuf = buf;

	rows_per_spe = args.fractal.rows / args.n_threads;
	bytes_per_row = sizeof(*buf) * args.fractal.cols;
	rows_per_dma = sizeof(buf) / bytes_per_row;

	for (row = rows_per_dma * args.thread_idx;
			row < args.fractal.rows;
			row += rows_per_dma * args.n_threads) {

		render_fractal(&args.fractal, row,
				rows_per_dma);

		mfc_put(buf, ppe_buf + row * bytes_per_row,
				bytes_per_row * rows_per_dma,
				0, 0, 0);

		/* Wait for the DMA to complete */
		mfc_write_tag_mask(1 << 0);
		mfc_read_tag_status_all();
	}


	return 0;
}
