
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <asm/ps3fb.h>

#include "parse-fractal.h"

#define streq(a, b) (!strcasecmp(a, b))

struct fractal_params *parse_fractal(const char *filename)
{
	FILE *fp;
	struct fractal_params *fractal;
	char name[7];
	float value;

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Can't open file %s: %s\n",
				filename, strerror(errno));
		return NULL;
	}

	fractal = malloc(sizeof(*fractal));
	if (!fractal) {
		perror("malloc");
		goto err_close;
	}

	memset(fractal, 0, sizeof(*fractal));

	while (!feof(fp)) {

		int rc = fscanf(fp, "%6s = %f", name, &value);

		if (rc != 2)
			continue;

		if (streq(name, "cols")) {
			fractal->cols = (int)(floor(value));

		} else if (streq(name, "rows")) {
			fractal->rows = (int)(floor(value));

		} else if (streq(name, "x")) {
			fractal->x = value;

		} else if (streq(name, "y")) {
			fractal->y = value;

		} else if (streq(name, "delta")) {
			fractal->delta = value;

		} else if (streq(name, "i_max")) {
			fractal->i_max = value;

		} else {
			fprintf(stderr, "Unknown configutation directive %s\n",
					name);
			goto err_free;
		}
	}

	if( fractal->rows == 0 || fractal->cols == 0 ) {
		// code lifted from fb_info.c found at cellperformance.com
		const int fb_file = open( "/dev/fb0", O_RDWR );
		const int open_fb_error = (fb_file >> ((sizeof(int)*8)-1));
		if(open_fb_error < 0) {
			fprintf(stderr, "Could not open /dev/fb0.  Check permissions.\n");
			perror("open");
			goto err_free;
		}

		struct ps3fb_ioctl_res res;
		int ps3_screeninfo_error = ioctl(fb_file, PS3FB_IOCTL_SCREENINFO, (unsigned long)&res);
		if (ps3_screeninfo_error == -1) {
			fprintf(stderr, "Error: PS3FB_IOCTL_SCREENINFO Failed and image dimensions not specified\n");
			perror("ioctl");
			goto err_free;
		}
		if (fractal->cols == 0) {
			fractal->cols = res.xres;
			printf("xres detected as %d.\n", res.xres);
		}
		if (fractal->rows == 0) {
			fractal->rows = res.yres;
			printf("yres detected as %d.\n", res.yres);
		}

		int close_fb_error = close(fb_file);
		if(close_fb_error == -1) {
			fprintf(stderr, "Warning: Could not close file handle used for /dev/fb0\n");
		}
	}


	if (!fractal->cols) {
		fprintf(stderr, "No columns specified\n");
	}

	if (!fractal->rows) {
		fprintf(stderr, "No rows specified\n");
	}

	if (!fractal->x) {
		fprintf(stderr, "No x value specified in %s\n", filename);
		goto err_free;
	}

	if (!fractal->y) {
		fprintf(stderr, "No y value specified in %s\n", filename);
		goto err_free;
	}

	if (!fractal->delta) {
		fprintf(stderr, "No delta value specified in %s\n", filename);
		goto err_free;
	}

	if (!fractal->i_max) {
		fprintf(stderr, "No i_max value specified in %s\n", filename);
		goto err_free;
	}

	fclose(fp);
	return fractal;

err_free:
	free(fractal);
err_close:
	fclose(fp);
	return NULL;


}

