
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include "cp_vt.h"
#include "cp_fb.h"

#include <libspe2.h>
#include <pthread.h>

#include "png.h"
#include "fractal.h"
#include "parse-fractal.h"

#define DEFAULT_PARAMSFILE "fractal.data"
#define DEFAULT_OUTFILE "fractal.png"
#define DEFAULT_N_THREADS 1

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

int main(int argc, char **argv)
{
	struct spe_thread *threads;
	struct fractal_params *fractal;
	const char *outfile, *paramsfile;
	int opt, n_threads, i;

	/* set up default arguments */
	paramsfile = DEFAULT_PARAMSFILE;
	outfile = DEFAULT_OUTFILE;
	n_threads = DEFAULT_N_THREADS;

	/* parse arguments into datafile and outfile  */
	while ((opt = getopt(argc, argv, "p:o:n:")) != -1) {
		switch (opt) {
		case 'p':
			paramsfile = optarg;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'n':
			n_threads = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage: %s [-p paramsfile] "
						"[-o outfile] [-n n_threads]\n",
						argv[0]);
			return EXIT_FAILURE;
		}
	}

	/* parse the input datafile */
	fractal = parse_fractal(paramsfile);
	if (!fractal)
		return EXIT_FAILURE;

 	cp_vt vt;
 	cp_fb fb;
	cp_vt_open_graphics(&vt);
 	cp_fb_open(&fb,1);
 	fractal->imgbuf = (void*)fb.draw_addr[0];
	
	/* allocate an array for the SPE threads */
	threads = memalign(SPE_ALIGN, n_threads * sizeof(*threads));

	for (i = 0; i < n_threads; i++) {
		/* copy the fractal data into this thread's args */
		memcpy(&threads[i].args.fractal, fractal, sizeof(*fractal));

		/* set thread-specific arguments */
		threads[i].args.n_threads = n_threads;
		threads[i].args.thread_idx = i;

		threads[i].ctx = spe_context_create(0, NULL);
		spe_program_load(threads[i].ctx, &spe_fractal);
		pthread_create(&threads[i].pthread, NULL,
				spethread_fn, &threads[i]);
	}

	/* wait for the threads to finish */
	for (i = 0; i < n_threads; i++)
		pthread_join(threads[i].pthread, NULL);

	cp_vt_close(&vt);
	cp_fb_close(&fb);

	return EXIT_SUCCESS;
}
