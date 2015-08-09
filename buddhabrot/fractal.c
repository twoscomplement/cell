/*
 * Buddhabrot/nebulabrot renderer
 *
 * Loosely based on Jeremy Kerr's Mandelbrot renderer from
 * http://ozlabs.org/~jk/diary/tech/cell/hackfest08-solution-4.diary/
 *
 * SPUs calculate points to be rendered and passes these to the PPU to do
 * the drawing in an in-order & coherent fashion.
 *
 */


#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include "cp_vt.h"
#include "cp_fb.h"

#include <rfb/rfb.h>

#include <libspe2.h>
#include <pthread.h>

#include "png.h"
#include "fractal.h"
#include "parse-fractal.h"

#define DEFAULT_PARAMSFILE "fractal.data"

extern spe_program_handle_t spe_fractal;

struct spe_thread {
	spe_context_ptr_t ctx;
	pthread_t pthread;
	struct spe_args args __attribute__((aligned(SPE_ALIGN)));
};

void *spethread_fn(void *data)
{
	struct spe_thread *spethread = data;
	uint32_t entry = SPE_DEFAULT_ENTRY;

	/* run the context, passing the address of our args structure to
	 * the 'argv' argument to main() */
	spe_context_run(spethread->ctx, &entry, 0,
			&spethread->args, NULL, NULL);

	return NULL;
}


// Iterate through the points in the array, performing the actual 'draw'
void draw_points(cpoint_ptr p, volatile uint* s) {
	// wait for s to change to ensure transfer of p has completed
	while(*s != 1);

	for(int i = 0; i < (16384 / 8); ++i) {
		// TODO Saturating arithmetic, or some form of HDR processing
		*p[i].addr += p[i].i;
	}
	
	// reset the sentinel
	*s = 0;
}

// Iterate through the first n points in the array
void draw_points_final(cpoint_ptr p, uint n) {
	for(int i = 0; i < n; ++i) {
		*p[i].addr += p[i].i;
	}
}


int main(int argc, char **argv)
{
	struct spe_thread* threads;
	spe_event_handler_ptr_t event_handler;
	struct fractal_params *fractal;
	const char *outfile, *paramsfile;
	int opt;
	int remote = 0;
	int n_threads = spe_cpu_info_get(SPE_COUNT_USABLE_SPES, -1);

	/* set up default arguments */
	paramsfile = DEFAULT_PARAMSFILE;
	outfile = 0; 

	/* parse arguments into datafile and outfile  */
	printf("Configuration:\n");
	while ((opt = getopt(argc, argv, "n:o:p:r")) != -1) {
		switch (opt) {
		case 'n':
			n_threads = atoi(optarg);
			break;
		case 'o':
			outfile = optarg;
			printf("\tImage will be written to %s\n", outfile);
			break;
		case 'p':
			paramsfile = optarg;
			printf("\tParams will be read from %s\n", paramsfile);
			break;
		case 'r':
			remote = 1;
			printf("\tRemote access via VNC enabled\n");
			break;
		default:
			fprintf(stderr, "Usage: %s [-p paramsfile] "
						"[-o outfile]\n"
						"[-n SPE count] [-r]\n", argv[0]);
			return EXIT_FAILURE;
		}
	}
	printf("\t%d SPEs\n\n", n_threads);


	/* parse the input datafile */
    /* We don't use much of this - iteration count and rows, cols */
	fractal = parse_fractal(paramsfile);
	if (!fractal)
		return EXIT_FAILURE;

	// Set up framebuffer
 	cp_vt vt;
 	cp_fb fb;
	cp_vt_open_graphics(&vt);
 	cp_fb_open(&fb,1);

 	fractal->imgbuf = (void*)fb.draw_addr[0];

	// Reset some params that were read for what we want
	fractal->x = 0;
	fractal->y = 0;
	fractal->delta = 4. / fractal->rows;

	threads = memalign(SPE_ALIGN, n_threads * sizeof(*threads));

	event_handler = spe_event_handler_create();
	if(!event_handler) {
		perror("spe_event_handler_create");
		return EXIT_FAILURE;
	}

	// Set up each thread
	for(int n = 0; n < n_threads; ++n) {
		threads[n].ctx = spe_context_create(
			SPE_EVENTS_ENABLE|SPE_CFG_SIGNOTIFY1_OR, NULL);
		threads[n].args.n_threads = n_threads;
		threads[n].args.thread_idx = n;
		
		spe_program_load(threads[n].ctx, &spe_fractal);
		
		memcpy(&threads[n].args.fractal, fractal, sizeof(*fractal));
		
		for(int q = 0; q < 8; ++q) {
			threads[n].args.fractal.pointbuf[q] = memalign(128, 16384);
			threads[n].args.fractal.sentinel[q] = memalign(16, 16);
		}
	
		// Register for intr mbox events - store fractal_params for thread for easy lookup later
		spe_event_unit_t event;
		event.events = SPE_EVENT_OUT_INTR_MBOX;
		event.spe = threads[n].ctx;
		event.data.ptr = &threads[n].args.fractal;
		if( -1 ==  spe_event_handler_register(event_handler, &event) ) {
			perror("spe_event_handler_register");
			return EXIT_FAILURE;
		}

		pthread_create(&threads[n].pthread, NULL, spethread_fn, &threads[n]);
	}

	// Start up VNC access, if requested
	rfbScreenInfoPtr rfbScreen = 0;
	if(remote) {
		rfbScreen = rfbGetScreen(&argc, argv, fb.w, fb.h, 8, 3, 4);
		rfbScreen->desktopName = "PS3 VNC";
		rfbScreen->frameBuffer = (void*)fractal->imgbuf;
		rfbScreen->alwaysShared = TRUE;
		rfbScreen->serverFormat.redShift = 16;
		rfbScreen->serverFormat.greenShift = 8;
		rfbScreen->serverFormat.blueShift = 0;
		rfbInitServer(rfbScreen);
	}

	int complete = 0;
	// Main draw loop - wait for interrupt from SPE, draw data.
	while(1) {
		spe_event_unit_t event;
		uint f;

		// Block for interrupt
		spe_event_wait(event_handler, &event, 1, -1);

		// Got interrupt, read mbox
		spe_out_intr_mbox_read(event.spe, &f, 1, SPE_MBOX_ANY_NONBLOCKING);

		// Retrieve appropriate fractal_params pointer that we stashed here earlier
		struct fractal_params* fractal = (struct fractal_params*)event.data.ptr;

		// thread finishing - check for high bit set
		if(f&(1<<31)) {
			// Mask the high bit back out
			f&=~(1<<31);
			
			// get the remaining number of items to be plotted
			uint remainder;
			spe_out_intr_mbox_read(event.spe, &remainder, 1, SPE_MBOX_ALL_BLOCKING);
			
			draw_points_final(fractal->pointbuf[f], remainder);

			++complete;
			if(complete==n_threads) {
				break;
			}
		}
		else {
			// Draw the data
			draw_points(fractal->pointbuf[f],  (uint*)fractal->sentinel[f]);

			// Signal the SPE that the buffer has been written
			spe_signal_write(event.spe, SPE_SIG_NOTIFY_REG_1, 1<<f);
		}

		// Mark screen as changed
		if(remote) {
			rfbMarkRectAsModified(rfbScreen, 0,0,fb.w, fb.h);
			rfbProcessEvents(rfbScreen,1);
		}
	}

	for(int n = 0; n < n_threads; ++n) {
		pthread_join(threads[n].pthread, NULL);
	}

    if(outfile) {
        int xx;
		// Set alpha properly for png write
        for(xx=0; xx<(fractal->rows*fractal->cols);++xx) {
            fractal->imgbuf[xx].a = 255;
        }
        write_png(outfile, fractal->rows, fractal->cols, fractal->imgbuf);
    }

	if(remote) {
		// Don't close straight away
		for(int c = 0; c < 100; ++c ) {
			printf(".");fflush(stdout);
			rfbProcessEvents(rfbScreen,1);
			sleep(1);
		}
		printf("\n");
	}

	cp_vt_close(&vt);
	cp_fb_close(&fb);

	return EXIT_SUCCESS;
}

