/**
 * Fuzzy comparison utility. Copyright 2001 artofcode LLC.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

typedef unsigned char uchar;
typedef int bool;
#define FALSE 0
#define TRUE 1

#define MIN(x,y) ((x)>(y)?(y):(x))
#define MAX(x,y) ((x)>(y)?(x):(y))

typedef struct _Image Image;

struct _Image {
  int (*close) (Image *self);
  int (*get_scan_line) (Image *self, uchar *buf);
  int (*seek) (Image *self, int y);
  int width;
  int height;
  int n_chan;
  int raster;
  int bpp; /* bits per pixel */
};

typedef struct _FuzzyParams FuzzyParams;

struct _FuzzyParams {
  int tolerance;    /* in pixel counts */
  int window_size;
};

typedef struct _FuzzyReport FuzzyReport;

struct _FuzzyReport {
  int n_diff;
  int n_outof_tolerance;
  int n_outof_window;
};

int
image_get_rgb_scan_line (Image *image, uchar *buf)
{
  uchar *image_buf;
  int width = image->width;
  int code;
  int x;

  if (image->n_chan == 3 && image->bpp == 8)
    return image->get_scan_line (image, buf);

  image_buf = malloc (image->raster);
  code = image->get_scan_line (image, image_buf);
  if (code < 0)
    {
      /* skip */
    }
  else if (image->n_chan == 1 && image->bpp == 8)
    {
      for (x = 0; x < width; x++)
	{
	  uchar g = image_buf[x];
	  buf[x * 3] = g;
	  buf[x * 3 + 1] = g;
	  buf[x * 3 + 2] = g;
	}
    }
  else if (image->n_chan == 1 && image->bpp == 1)
    {
      for (x = 0; x < width; x++)
	{
	  uchar g = -!(image_buf[x >> 3] && (128 >> (x & 7)));
	  buf[x * 3] = g;
	  buf[x * 3 + 1] = g;
	  buf[x * 3 + 2] = g;
	}
    }
  else
    code = -1;
  free (image_buf);
  return code;
}

int
image_close (Image *image)
{
  return image->close (image);
}

static int
no_seek(Image *self, int y)
{
  return 0;
}


typedef struct _ImagePnm ImagePnm;

struct _ImagePnm {
  Image super;
  FILE *f;
};

static int
image_pnm_close (Image *self)
{
  ImagePnm *pnm = (ImagePnm *)self;
  int code;

  code = fclose (pnm->f);
  free (self);
  return code;
}

static ImagePnm *
create_pnm_image_struct(Image *templ, const char *path)
{
  FILE *f = fopen(path,"w+b");
  ImagePnm *pnm = (ImagePnm *)malloc(sizeof(ImagePnm));

  if (pnm == NULL) {
    printf("Insufficient RAM.\n");
    return NULL;
  }
  if (f == NULL) {
    printf("Can't create the file %s\n", path);
    return NULL;
  }
  pnm->f = f;
  pnm->super = *templ;
  pnm->super.seek = no_seek;
  pnm->super.bpp = 8;    /* Now support this value only. */
  pnm->super.n_chan = 3; /* Now support this value only. */
  return pnm;
}

static ImagePnm *
create_pnm_image(Image *templ, const char *path)
{
  ImagePnm *pnm = create_pnm_image_struct(templ, path);

  if (pnm == NULL)
    return NULL;

  fprintf(pnm->f,"P6\n");
  fprintf(pnm->f,"# Generated by Ghostscript's fuzzy.c\n");
  fprintf(pnm->f,"%d %d\n", templ->width, pnm->super.height);
  fprintf(pnm->f,"255\n");
  return pnm;
}

typedef short WORD;
typedef long LONG;

typedef struct tagBITMAPFILEHEADER { 
  WORD    bfType; 
  LONG    bfSize; 
  WORD    bfReserved1; 
  WORD    bfReserved2; 
  LONG    bfOffBits; 
} BITMAPFILEHEADER; 

typedef struct tagBITMAPINFOHEADER{
  LONG   biSize; 
  LONG   biWidth; 
  LONG   biHeight; 
  WORD   biPlanes; 
  WORD   biBitCount;
  LONG   biCompression; 
  LONG   biSizeImage; 
  LONG   biXPelsPerMeter; 
  LONG   biYPelsPerMeter; 
  LONG   biClrUsed; 
  LONG   biClrImportant; 
} BITMAPINFOHEADER; 

static void 
Word(WORD *v, WORD x)
{
  *((uchar*)v + 0) = x & 255;
  *((uchar*)v + 1) = x >> 8;
}

static void 
Long(LONG *v, LONG x)
{
  *((uchar*)v + 0) = x & 255;
  *((uchar*)v + 1) = (x >> 8) & 255;
  *((uchar*)v + 2) = (x >> 16) & 255;
  *((uchar*)v + 3) = (x >> 24) & 255;
}

static int
seek_bmp_image(Image *self, int y)
{
  ImagePnm *pnm = (ImagePnm *)self;
  int nOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  long pos = nOffBits + self->raster * (self->height - y - 1);
  int r = fseek(pnm->f, pos, SEEK_SET);

  return r;
}

static ImagePnm *
create_bmp_image(Image *templ, const char *path)
{
  BITMAPFILEHEADER fh;
  BITMAPINFOHEADER ih;
  int raster = (templ->raster + 3) & ~3;
  int nOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  int nImageSize = raster * templ->height;
  int nFileSize = nOffBits + nImageSize;
  ImagePnm *pnm = create_pnm_image_struct(templ, path);

  if (pnm == NULL)
    return NULL;
  pnm->super.seek = seek_bmp_image;
  pnm->super.raster = raster;

  memset(&fh, 0, sizeof(fh));
  memset(&ih, 0, sizeof(ih));

  *((uchar*)&fh.bfType + 0) = 'B';
  *((uchar*)&fh.bfType + 1) = 'M';
  Long(&fh.bfSize, nFileSize);
  Long(&fh.bfOffBits, nOffBits);

  Long(&ih.biSize, sizeof(BITMAPINFOHEADER));
  Long(&ih.biWidth, templ->width);
  Long(&ih.biHeight, pnm->super.height);
  Word(&ih.biPlanes, 1);    /* Now support this value only. */
  Word(&ih.biBitCount, 24); /* Now support this value only. */
  ih.biCompression = 0;     /* Now support this value only. */
  ih.biSizeImage = 0;
  Long(&ih.biXPelsPerMeter, 3780);
  Long(&ih.biYPelsPerMeter, 3780);
  ih.biClrUsed = 0;
  ih.biClrImportant = 0;

  fwrite(&fh, sizeof(fh), 1, pnm->f);
  fwrite(&ih, sizeof(ih), 1, pnm->f);

  return pnm;
}


static int
image_pnm_get_scan_line (Image *self, uchar *buf)
{
  ImagePnm *pnm = (ImagePnm *)self;
  int n_bytes = self->raster;
  int code;

  code = fread (buf, 1, n_bytes, pnm->f);
  return (code < n_bytes) ? -1 : 0;
}

Image *
open_pnm_image (const char *fn)
{
  FILE *f = fopen (fn, "rb");
  int width, height;
  int n_chan, bpp;
  char linebuf[256];
  ImagePnm *image;

  if (f == NULL)
    return NULL;

  image = (ImagePnm *)malloc (sizeof(ImagePnm));
  image->f = f;
  if (fgets (linebuf, sizeof(linebuf), f) == NULL ||
      linebuf[0] != 'P' || linebuf[1] < '4' || linebuf[1] > '6')
    {
      fclose (f);
      return NULL;
    }
  switch (linebuf[1])
    {
    case '4':
      n_chan = 1;
      bpp = 1;
      break;
    case '5':
      n_chan = 1;
      bpp = 8;
      break;
    case '6':
      n_chan = 3;
      bpp = 8;
      break;
    default:
      fclose (f);
      return NULL;
    }
  do
    {
      if (fgets (linebuf, sizeof(linebuf), f) == NULL)
	{
	  fclose (f);
	  return NULL;
	}
    }
  while (linebuf[0] == '#');
  if (sscanf (linebuf, "%d %d", &width, &height) != 2)
    {
      fclose (f);
      return NULL;
    }
  if (bpp == 8)
    {
      do
	{
	  if (fgets (linebuf, sizeof(linebuf), f) == NULL)
	    {
	      fclose (f);
	      return NULL;
	    }
	}
      while (linebuf[0] == '#');
    }
  image->super.close = image_pnm_close;
  image->super.get_scan_line = image_pnm_get_scan_line;
  image->super.seek = no_seek;
  image->super.width = width;
  image->super.height = height;
  image->super.raster = n_chan * ((width * bpp + 7) >> 3);
  image->super.n_chan = n_chan;
  image->super.bpp = bpp;
  return &image->super;
}

Image *
open_image_file (const char *fn)
{
  /* This is the place to add a dispatcher for other image types. */
  return open_pnm_image (fn);
}

static uchar **
alloc_window (int row_bytes, int window_size)
{
  uchar **buf = (uchar **)malloc (window_size * sizeof(uchar *));
  int i;

  for (i = 0; i < window_size; i++)
    buf[i] = malloc (row_bytes);
  return buf;
}

static void
free_window (uchar **buf, int window_size)
{
  int i;

  for (i = 0; i < window_size; i++)
    free (buf[i]);
  free (buf);
}

static void
roll_window (uchar **buf, int window_size)
{
  int i;
  uchar *tmp1, *tmp2;

  tmp1 = buf[window_size - 1];
  for (i = 0; i < window_size; i++)
    {
      tmp2 = buf[i];
      buf[i] = tmp1;
      tmp1 = tmp2;
    }
}

static bool
check_window (uchar **buf1, uchar **buf2,
	      const FuzzyParams *fparams,
	      int x, int y, int width, int height)
{
  int tolerance = fparams->tolerance;
  int window_size = fparams->window_size;
  int i, j;
  const int half_win = window_size >> 1;
  const uchar *rowmid1 = buf1[half_win];
  const uchar *rowmid2 = buf2[half_win];
  uchar r1 = rowmid1[x * 3], g1 = rowmid1[x * 3 + 1], b1 = rowmid1[x * 3 + 2];
  uchar r2 = rowmid2[x * 3], g2 = rowmid2[x * 3 + 1], b2 = rowmid2[x * 3 + 2];
  bool match1 = FALSE, match2 = FALSE;

  for (j = -half_win; j <= half_win; j++)
    {
      const uchar *row1 = buf1[j + half_win];
      const uchar *row2 = buf2[j + half_win];
      for (i = -half_win; i <= half_win; i++)
	if ((i != 0 || j != 0) &&
	    x + i >= 0 && x + i < width &&
	    y + j >= 0 && y + j < height)
	  {
	    if (abs (r1 - row2[(x + i) * 3]) <= tolerance &&
		abs (g1 - row2[(x + i) * 3 + 1]) <= tolerance &&
		abs (b1 - row2[(x + i) * 3 + 2]) <= tolerance)
	      match1 = TRUE;
	    if (abs (r2 - row1[(x + i) * 3]) <= tolerance &&
		abs (g2 - row1[(x + i) * 3 + 1]) <= tolerance &&
		abs (b2 - row1[(x + i) * 3 + 2]) <= tolerance)
	      match2 = TRUE;
	  }
    }
  return !(match1 && match2);
}

static void
fuzzy_diff_images (Image *image1, Image *image2, const FuzzyParams *fparams,
		   FuzzyReport *freport, ImagePnm *image_out)
{
  int width = MAX(image1->width, image2->width);
  int height = MAX(image1->height, image2->height);
  int tolerance = fparams->tolerance;
  int window_size = fparams->window_size;
  int row_bytes = width * 3;
  unsigned int out_buffer_size = (image_out ? image_out->super.raster : 0);
  int half_win = window_size >> 1;
  uchar **buf1 = alloc_window (row_bytes, window_size);
  uchar **buf2 = alloc_window (row_bytes, window_size);
  uchar *out_buf = NULL;
  int y;

  if (image_out != NULL)
    {
      out_buf = malloc(out_buffer_size);
      if (out_buf == NULL) 
        printf("Can't allocate output buffer.\n");
    }

  /* Read rows ahead for half window : */
  for (y = 0; y < MIN(half_win, height); y++) 
    {
      image_get_rgb_scan_line (image1, buf1[half_win - y]);
      image_get_rgb_scan_line (image2, buf2[half_win - y]);
    }

  /* Do compare : */
  freport->n_diff = 0;
  freport->n_outof_tolerance = 0;
  freport->n_outof_window = 0;

  for (y = 0; y < height; y++)
    {
      int x;
      uchar *row1 = buf1[0];
      uchar *row2 = buf2[0];
      uchar *rowmid1 = buf1[half_win];
      uchar *rowmid2 = buf2[half_win];

      if (y < height - half_win)
        {
          image_get_rgb_scan_line (image1, row1);
          image_get_rgb_scan_line (image2, row2);
	}
      if (out_buf)
	memset(out_buf, 0, out_buffer_size);
      for (x = 0; x < width; x++)
	{
	  if (rowmid1[x * 3] != rowmid2[x * 3] ||
	      rowmid1[x * 3 + 1] != rowmid2[x * 3 + 1] ||
	      rowmid1[x * 3 + 2] != rowmid2[x * 3 + 2])
	    {
	      freport->n_diff++;
	      if (abs (rowmid1[x * 3] - rowmid2[x * 3]) > tolerance ||
		  abs (rowmid1[x * 3 + 1] - rowmid2[x * 3 + 1]) > tolerance ||
		  abs (rowmid1[x * 3 + 2] - rowmid2[x * 3 + 2]) > tolerance)
		{
		  freport->n_outof_tolerance++;
		  if (check_window (buf1, buf2, fparams, x, y, width, height)) {
		    freport->n_outof_window++;
		    if (out_buf) {
		      out_buf[x * 3 + 0] = abs(rowmid1[x * 3 + 0]- rowmid2[x * 3 + 0]);
		      out_buf[x * 3 + 1] = abs(rowmid1[x * 3 + 2]- rowmid2[x * 3 + 1]);
		      out_buf[x * 3 + 2] = abs(rowmid1[x * 3 + 3]- rowmid2[x * 3 + 2]);
		    }
		  }
		}
	    }
	}
      roll_window (buf1, window_size);
      roll_window (buf2, window_size);
      if (out_buf) {
        if (image_out->super.seek(&image_out->super, y))
	  {
	    printf("I/O Error seeking to the output image position.");
	    free(out_buf);
	    out_buf = NULL;
	  }
	else if (fwrite(out_buf, 1, out_buffer_size, image_out->f) != out_buffer_size)
	  {
	    printf("I/O Error writing the output image.\n");
	    free(out_buf);
	    out_buf = NULL;
	  }
      }
    }

  free_window (buf1, window_size);
  free_window (buf2, window_size);
  if (out_buf)
    free(out_buf);
  if (image_out)
    fclose(image_out->f);
}

static const char *
get_arg (int argc, char **argv, int *pi, const char *arg)
{
  if (arg[0] != 0)
    return arg;
  else
    {
      (*pi)++;
      if (*pi == argc)
	return NULL;
      else
	return argv[*pi];
    }
}

int
usage (void)
{
  fprintf (stderr, "Usage: fuzzy [-w window_size] [-t tolerance] a.ppm b.ppm [diff.ppm | diff.bmp]\n");
  return 1;
}

int
main (int argc, char **argv)
{
  Image *image1, *image2;
  ImagePnm *image_out = NULL;
  FuzzyParams fparams;
  FuzzyReport freport;
  const char *fn[3] = {0, 0, 0};
  int fn_idx = 0;
  int i;

  fparams.tolerance = 2;
  fparams.window_size = 3;

  for (i = 1; i < argc; i++)
    {
      const char *arg = argv[i];

      if (arg[0] == '-')
	{
	  switch (arg[1])
	    {
	    case 'w':
	      fparams.window_size = atoi (get_arg (argc, argv, &i, arg + 2));
	      if ((fparams.window_size & 1) == 0)
		{
		  fprintf (stderr, "window size must be odd\n");
		  return 1;
		}
	      break;
	    case 't':
	      fparams.tolerance = atoi (get_arg (argc, argv, &i, arg + 2));
	      break;
	    default:
	      return usage ();
	    }
	}
      else if (fn_idx < sizeof(fn)/sizeof(fn[0]))
	fn[fn_idx++] = argv[i];
      else
	return usage ();
    }

  if (fn_idx < 2)
    return usage ();

  image1 = open_image_file (fn[0]);
  if (image1 == NULL)
    {
      fprintf (stderr, "Error opening %s\n", fn[0]);
      return 1;
    }

  image2 = open_image_file (fn[1]);
  if (image2 == NULL)
    {
      image_close (image1);
      fprintf (stderr, "Error opening %s\n", fn[1]);
      return 1;
    }

  if (fn[2]) 
    image_out = 
       (!strcmp(fn[2]+ strlen(fn[2]) - 4, ".bmp") ? create_bmp_image
                                                  : create_pnm_image)
           (image1, fn[2]);

  fuzzy_diff_images (image1, image2, &fparams, &freport, image_out);

  if (image_out)
    image_pnm_close (&image_out->super);


  if (freport.n_outof_window > 0)
    {
      printf ("%s: %d different, %d out of tolerance, %d out of window\n",
	      fn[0], freport.n_diff, freport.n_outof_tolerance,
	      freport.n_outof_window);
      return 1;
    }
  else
    return 0;
}
