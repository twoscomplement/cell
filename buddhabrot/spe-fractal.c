/**
 * Originally based on code from mandelbrot renderer by Jeremy Kerr.
 * http://ozlabs.org/~jk/diary/tech/linux/
 */

#include <stdio.h>
#include <stdint.h>
#include <spu_mfcio.h>
#include <string.h>
#include <math.h>

#include "common.h"

#define unlikely(x) (__builtin_expect(!!(x), 0))

#include <stdio.h>

#include "fractal.h"

// For counting the number of calls to colour_map
int cmap_calls;
// For counting the number of DMA put ops of pixel data
int dma_puts;

// Local buffers for pixel data.  8 is more than necessary.
static struct calculated_point points[16384] __attribute__((aligned(128)));

// Index into points array
static uint fill = 0;

/*
 * Colour the given framebuffer address pix. 
 * i and params may be used to select the colour.
 */
static void write_colour(struct pixel *pix, float i, 
						 struct fractal_params *params)
{
	// Mask for keeping track of ppe finishing with buffers
	static int valid = 0xff;
	static vector unsigned int sentinel = {1,0,0,0};

	++cmap_calls;

	uint colour;

	// Various colouring alternatives are possible here

	// ignore the first few steps - reduces backgroud noise
	if(i<20) return;

/*	if(i==0) return;
	if(params->i_max < 10000) {
		colour = 0x00010000;
	} else if(params->i_max < 20000) {
		if(i<10000) return;
		colour = 0x00000200;
	} else {
		if(i<20000) return;
		colour = 0x00000004;
	}*/

	colour = 0x00010100;

	// If starting to fill a new buffer, check that any earlier use
	// is finished with - ppe signals completion
	if(fill%2048 == 0) {
		while(!(valid&(1<<(fill/2048)))) {
			valid |= spu_read_signal1();
		}
	}

	// set values
	points[fill].addr = (uint*)pix;
	points[fill].i = colour;
	++fill;

	// if we just filled a buffer, send it to ppe
	if(fill%2048==0) {
		// select the specific buffer that is full
		int f = (fill / 2048) - 1;
		mfc_put(&points[f*2048], (uint)params->pointbuf[f], 16384, 0, 0, 0);
		// fence a sentinel - the ppe will spin on this completing...
		// What's a better way to achieve sync?
		mfc_putf(&sentinel, (uint)params->sentinel[f], 16, 0, 0, 0);
		// interrupt the ppe
		spu_write_out_intr_mbox(f);
		// unmask the relevant bit
		valid&=~(1<<f);

		if(fill==16384) {
			fill = 0;
		}
		++dma_puts;
	} 
}


/**
 * Render a fractal, given the parameters specified in @params
 * Not optimised. Vectorise+unroll will be a big win.
 * And mix in write_colour() for more benefits.
 */
static void render_fractal(struct fractal_params *params,
		int start_row, int row_skip, double compute_delta)
{
	int i, j, r, x, y;
	double px, py;
	/* complex numbers: c and z */
	double cr, ci, zr, zi;
	double x_min, y_min, tmp;

	x_min = params->x - (params->delta * params->cols / 2);
	y_min = params->y - (params->delta * params->rows / 2);

	for (r = start_row; r < params->rows; r+=row_skip) {
		y = r;// + start_row;
		ci = y_min + y * params->delta + compute_delta;

		for (x = 0; x < params->cols; x++) {
			cr = x_min + x * params->delta;// + compute_delta;

			zr = 0;
			zi = 0;

			for (i = 0; i < params->i_max; i++)  {
				/* z = z^2 + c */
				tmp = zr*zr - zi*zi + cr;
				zi =  2.0 * zr * zi + ci;
				zr = tmp;

				/* if abs(z) > 2.0 */
				if (unlikely(zr*zr + zi*zi > 4.0))
					break;
			}

			if(i < params->i_max) {
				zi = 0;
				zr = 0;
				for(j = 0; j < i; ++j) {
					tmp = zr*zr - zi*zi + cr;
					zi =  2.0 * zr * zi + ci;
					zr = tmp;

					/* if abs(z) > 2.0 */
					if (zr*zr + zi*zi > 4.0)
						break;

					px = (zi - x_min)/params->delta;
					py = (zr - y_min)/params->delta;
					write_colour(&params->imgbuf[(int)py*params->cols + (int)px],
								j, params);
				}
			}
		}
	}
}

/*
 * The argv argument will be populated with the address that the PPE provided,
 * from the 4th argument to spe_context_run()
 */
int main(uint64_t speid, uint64_t argv, uint64_t envp)
{
	struct spe_args args __attribute__((aligned(SPE_ALIGN)));

	mfc_get(&args, argv, sizeof(args), 0, 0, 0);

	mfc_write_tag_mask(1 << 0);
	mfc_read_tag_status_all();

	cmap_calls = 0;
	dma_puts = 0;
	spu_write_decrementer(-1);

	// Run multiple renders with offsets.  Should be factored into render_fractal()
	render_fractal(&args.fractal, args.thread_idx, args.n_threads, 0.);
	render_fractal(&args.fractal, args.thread_idx, args.n_threads, 
					args.fractal.delta * 7 / 8);
	render_fractal(&args.fractal, args.thread_idx, args.n_threads, 
					args.fractal.delta * 3 / 4);
	render_fractal(&args.fractal, args.thread_idx, args.n_threads, 
					args.fractal.delta * 5 / 8);
	render_fractal(&args.fractal, args.thread_idx, args.n_threads, 
					args.fractal.delta / 2);
	render_fractal(&args.fractal, args.thread_idx, args.n_threads, 
					args.fractal.delta * 3 / 8);
	render_fractal(&args.fractal, args.thread_idx, args.n_threads, 
					args.fractal.delta / 4);
	render_fractal(&args.fractal, args.thread_idx, args.n_threads, 
					args.fractal.delta / 8);

	// Send remaining points
	if(fill%2048) {
		// select the last buffer used
		int f = fill / 2048;
		mfc_put(&points[f*2048], (uint)args.fractal.pointbuf[f], 16384, 0, 0, 0);
		// Block for completion
		mfc_write_tag_mask(1<<0);
		mfc_read_tag_status_all();
		// Send a message with top bit set to indicate final item
		spu_write_out_intr_mbox((1<<31)|f);
		// Send another message indicating count
		spu_write_out_intr_mbox(fill%2048);
		++dma_puts;
	} 

	// Report some stats
	uint ticks = -1 - spu_read_decrementer();
	printf("cmap calls %d ticks %u calls/tick %f\n", 
			cmap_calls, ticks, (double)cmap_calls/ticks );
	printf("dma puts %d\n", dma_puts);

	return 0;
}
