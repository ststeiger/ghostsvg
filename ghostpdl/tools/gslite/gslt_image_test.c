/* Copyright (C) 2006 artofcode LLC.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: gslt_image_test.c 2716 2007-01-05 05:27:37Z henrys $ */
/* example client for the gslt image loading library */

#include <stdlib.h>
#include <stdio.h>

typedef unsigned char byte;
#include "gslt.h"
#include "gslt_image.h"

gslt_image_t *
decode_image_file(gs_memory_t *mem, FILE *in)
{
    gslt_image_t *image;
    int len, bytes;
    byte *buf;

    /* get compressed data size */
    fseek(in, 0, SEEK_END);
    len = ftell(in);

    /* load the file into memory */
    fseek(in, 0, SEEK_SET);
    buf = malloc(len);
    if (buf == NULL) return NULL;

    bytes = fread(buf, 1, len, in);
    if (bytes != len) {
	free(buf);
	return NULL;
    }

    image = gslt_image_decode(mem, buf, len);
    free(buf);

    return image;
}

gslt_image_t *
decode_image_filename(gs_memory_t *mem, const char *filename)
{
    gslt_image_t *image;
    FILE *in;

    in = fopen(filename, "rb");
    if (in == NULL) return NULL;

    image = decode_image_file(mem, in);
    fclose(in);

    return image;
}

/* write out the image as a pnm file for verification */
int
write_image_file(gslt_image_t *image, FILE *out)
{
    byte *row;    
    int j, bytes;

    if (image == NULL || image->samples == NULL) {
	fprintf(stderr, "ignoring empty image object\n");
	return  -1;
    }

    if (image->components == 1 && image->bits == 1) {
	/* PBM file */
	int i;
	int rowbytes = (image->width+7)>>3;
	byte *local = malloc(rowbytes);

	fprintf(out, "P4\n%d %d\n", image->width, image->height); 
	row = image->samples;
	for (j = 0; j < image->height; j++) {
	    /* PBM images are inverted relative to our XPS/PS convention */
	    for (i = 0; i < rowbytes; i++)
		local[i] = row[i] ^ 0xFF;	
	    bytes = fwrite(local, 1, rowbytes, out);
	    row += image->stride;
	}
	free(local);
    } else if (image->components == 1 && image->bits == 8) {
	/* PGM file */
	fprintf(out, "P5\n%d %d\n255\n", image->width, image->height); 
	row = image->samples;
	for (j = 0; j < image->height; j++) {
	    bytes = fwrite(row, 1, image->width, out);
	    row += image->stride;
	}
    } else {
	/* PPM file */
	fprintf(out, "P6\n%d %d\n255\n", image->width, image->height); 
	row = image->samples;
	for (j = 0; j < image->height; j++) {
	    bytes = fwrite(row, image->components, image->width, out);
	    row += image->stride;
	}
    }

    return 0;
}

int
write_image_filename(gslt_image_t *image, const char *filename)
{
    FILE *out;
    int error;

    out = fopen(filename, "wb");
    if (out == NULL) {
	fprintf(stderr, "could not open '%s' for writing\n", filename);
	return -1;
    }

    error = write_image_file(image, out);
    fclose(out);

    return error;
}

int
main(int argc, const char *argv[])
{
    gs_memory_t *mem;
    gx_device *dev;
    gs_state *gs;
    const char *filename;
    gslt_image_t *image;
    char *s;
    int code = 0;

    /* get the device from the environment, or default */
    s = getenv("DEVICE");
    if (!s)
        s = "nullpage";

    /* initialize the graphicis library and set up a drawing state */
    mem = gslt_init_library();
    dev = gslt_init_device(mem, s);
    gs = gslt_init_state(mem, dev);

    gslt_get_device_param(mem, dev, "Name");
    gslt_set_device_param(mem, dev, "OutputFile", "gslt.out");

    filename = argv[argc-1];
    fprintf(stderr, "loading '%s'\n", filename);

    /* load and decode the image */
    image = decode_image_filename(mem, argv[argc-1]);
    if (image == NULL) {
	fprintf(stderr, "reading image failed.\n");
	code = -1;
    }
    /* save an uncompressed copy for verification */
    write_image_filename(image, "out.pnm");

    /* image could be drawn to the page here */

    /* release the image */
    gslt_image_free(mem, image);

    /* clean up the library */
    gslt_free_state(mem, gs);
    gslt_free_device(mem, dev);
    gslt_free_library(mem);
    return code;
}

