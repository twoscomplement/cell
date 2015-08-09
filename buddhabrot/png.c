
#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#include "png.h"
#include "fractal.h"

int write_png(const char *filename, int rows, int cols, struct pixel *image)
{
	FILE *fp;
	png_structp png;
	png_infop png_info;
	uint8_t **row_ptrs;
	int i;

	fp = fopen(filename, "wb");
	if (!fp) {
		perror("fopen");
		return -1;
	}

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		fprintf(stderr, "Couldn't create png_write_struct\n");
		return -1;
	}

	png_info = png_create_info_struct(png);
	if (!png_info) {
		fprintf(stderr, "Couldn't create png_info_struct\n");
		return -1;
	}

	if (setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, &png_info);
		fclose(fp);
		fprintf(stderr, "png writing failed\n");
		return -1;
	}

	png_init_io(png, fp);

	png_set_IHDR(png, png_info, cols, rows, 8, PNG_COLOR_TYPE_RGB_ALPHA,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);

	row_ptrs = malloc(rows * sizeof(*row_ptrs));

	image = (struct pixel*)((char*)image+1);

	for (i = 0; i < rows; i++)
		row_ptrs[i] = (uint8_t *)&image[i * cols];

	png_set_rows(png, png_info, row_ptrs);

	png_write_png(png, png_info, PNG_TRANSFORM_IDENTITY, NULL);

	fclose(fp);

	return 0;
}
