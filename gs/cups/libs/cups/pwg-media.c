/*
 * "$Id: pwg-media.c 9310 2010-09-21 22:34:57Z mike $"
 *
 *   PWG media name API implementation for CUPS.
 *
 *   Copyright 2009-2010 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   _pwgGenerateSize()   - Generate a PWG size keyword.
 *   _pwgInitSize()       - Initialize a PWG size using IPP job template
 *                          attributes.
 *   _pwgMediaForLegacy() - Find a PWG media size by ISO/IPP legacy name.
 *   _pwgMediaForPPD()    - Find a PWG media size by Adobe PPD name.
 *   _pwgMediaForPWG()    - Find a PWG media size by 5101.1 self-describing
 *                          name.
 *   _pwgMediaForSize()   - Get the PWG media name for a given size.
 *   pwg_compare_legacy() - Compare two sizes using the legacy names.
 *   pwg_compare_ppd()    - Compare two sizes using the PPD names.
 *   pwg_compare_pwg()    - Compare two sizes using the PWG names.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <math.h>


/*
 * Local macros...
 */

#define _PWG_MEDIA_IN(p,l,a,x,y) {p, l, a, (int)(x * 2540), (int)(y * 2540)}
#define _PWG_MEDIA_MM(p,l,a,x,y) {p, l, a, (int)(x * 100), (int)(y * 100)}


/*
 * Local functions...
 */

static int	pwg_compare_legacy(_pwg_media_t *a, _pwg_media_t *b);
static int	pwg_compare_pwg(_pwg_media_t *a, _pwg_media_t *b);
static int	pwg_compare_ppd(_pwg_media_t *a, _pwg_media_t *b);


/*
 * Local globals...
 */

static _pwg_media_t const cups_pwg_media[] =
{					/* Media size lookup table */
  /* North American Standard Sheet Media Sizes */
  _PWG_MEDIA_IN("na_index-3x5_3x5in", NULL, "3x5", 3, 5),
  _PWG_MEDIA_IN("na_personal_3.625x6.5in", NULL, "EnvPersonal", 3.625, 6.5),
  _PWG_MEDIA_IN("na_monarch_3.875x7.5in", "monarch-envelope", "EnvMonarch", 3.875, 7.5),
  _PWG_MEDIA_IN("na_number-9_3.875x8.875in", "na-number-9-envelope", "Env9", 3.875, 8.875),
  _PWG_MEDIA_IN("na_index-4x6_4x6in", NULL, "4x6", 4, 6),
  _PWG_MEDIA_IN("na_number-10_4.125x9.5in", "na-number-10-envelope", "Env10", 4.125, 9.5),
  _PWG_MEDIA_IN("na_a2_4.375x5.75in", NULL, NULL, 4.375, 5.75),
  _PWG_MEDIA_IN("na_number-11_4.5x10.375in", NULL, "Env11", 4.5, 10.375),
  _PWG_MEDIA_IN("na_number-12_4.75x11in", NULL, "Env12", 4.75, 11),
  _PWG_MEDIA_IN("na_5x7_5x7in", NULL, "5x7", 5, 7),
  _PWG_MEDIA_IN("na_index-5x8_5x8in", NULL, "5x8", 5, 8),
  _PWG_MEDIA_IN("na_number-14_5x11.5in", NULL, "Env14", 5, 11.5),
  _PWG_MEDIA_IN("na_invoice_5.5x8.5in", "invoice", "Statement", 5.5, 8.5),
  _PWG_MEDIA_IN("na_index-4x6-ext_6x8in", NULL, NULL, 6, 8),
  _PWG_MEDIA_IN("na_6x9_6x9in", "na-6x9-envelope", "6x9", 6, 9),
  _PWG_MEDIA_IN("na_c5_6.5x9.5in", NULL, "6.5x9.5", 6.5, 9.5),
  _PWG_MEDIA_IN("na_7x9_7x9in", "na-7x9-envelope", "7x9", 7, 9),
  _PWG_MEDIA_IN("na_executive_7.25x10.5in", "executive", "Executive", 7.25, 10.5),
  _PWG_MEDIA_IN("na_govt-letter_8x10in", "na-8x10", "8x10", 8, 10),
  _PWG_MEDIA_IN("na_govt-legal_8x13in", NULL, "8x13", 8, 13),
  _PWG_MEDIA_IN("na_quarto_8.5x10.83in", "quarto", "Quarto", 8.5, 10.83),
  _PWG_MEDIA_IN("na_letter_8.5x11in", "na-letter", "Letter", 8.5, 11),
  _PWG_MEDIA_IN("na_fanfold-eur_8.5x12in", NULL, "FanFoldGerman", 8.5, 12),
  _PWG_MEDIA_IN("na_letter-plus_8.5x12.69in", NULL, "LetterPlus", 8.5, 12.69),
  _PWG_MEDIA_IN("na_foolscap_8.5x13in", NULL, "FanFoldGermanLegal", 8.5, 13),
  _PWG_MEDIA_IN("na_legal_8.5x14in", "na-legal", "Legal", 8.5, 14),
  _PWG_MEDIA_IN("na_super-a_8.94x14in", NULL, "SuperA", 8.94, 14),
  _PWG_MEDIA_IN("na_9x11_9x11in", "na-9x11-envelope", "9x11", 9, 11),
  _PWG_MEDIA_IN("na_arch-a_9x12in", "arch-a", "ARCHA", 9, 12),
  _PWG_MEDIA_IN("na_letter-extra_9.5x12in", NULL, "LetterExtra", 9.5, 12),
  _PWG_MEDIA_IN("na_legal-extra_9.5x15in", NULL, "LegalExtra", 9.5, 15),
  _PWG_MEDIA_IN("na_10x11_10x11in", NULL, "10x11", 10, 11),
  _PWG_MEDIA_IN("na_10x13_10x13in", "na-10x13-envelope", "10x13", 10, 13),
  _PWG_MEDIA_IN("na_10x14_10x14in", "na-10x14-envelope", "10x14", 10, 14),
  _PWG_MEDIA_IN("na_10x15_10x15in", "na-10x15-envelope", "10x15", 10, 15),
  _PWG_MEDIA_IN("na_11x12_11x12in", NULL, "11x12", 11, 12),
  _PWG_MEDIA_IN("na_edp_11x14in", NULL, "11x14", 11, 14),
  _PWG_MEDIA_IN("na_fanfold-us_11x14.875in", NULL, NULL, 11, 14.875),
  _PWG_MEDIA_IN("na_11x15_11x15in", NULL, "11x15", 11, 15),
  _PWG_MEDIA_IN("na_ledger_11x17in", "tabloid", "Tabloid", 11, 17),
  _PWG_MEDIA_IN("na_eur-edp_12x14in", NULL, NULL, 12, 14),
  _PWG_MEDIA_IN("na_arch-b_12x18in", "arch-b", "ARCHB", 12, 18),
  _PWG_MEDIA_IN("na_12x19_12x19in", NULL, "12x19", 12, 19),
  _PWG_MEDIA_IN("na_b-plus_12x19.17in", NULL, "SuperB", 12, 19.17),
  _PWG_MEDIA_IN("na_super-b_13x19in", "super-b", NULL, 13, 19),
  _PWG_MEDIA_IN("na_c_17x22in", "c", "AnsiC", 17, 22),
  _PWG_MEDIA_IN("na_arch-c_18x24in", "arch-c", "ARCHC", 18, 24),
  _PWG_MEDIA_IN("na_d_22x34in", "d", "AnsiD", 22, 34),
  _PWG_MEDIA_IN("na_arch-d_24x36in", "arch-d", "ARCHD", 24, 36),
  _PWG_MEDIA_IN("asme_f_28x40in", "f", NULL, 28, 40),
  _PWG_MEDIA_IN("na_wide-format_30x42in", NULL, NULL, 30, 42),
  _PWG_MEDIA_IN("na_e_34x44in", "e", "AnsiE", 34, 44),
  _PWG_MEDIA_IN("na_arch-e_36x48in", "arch-e", "ARCHE", 36, 48),
  _PWG_MEDIA_IN("na_f_44x68in", NULL, "AnsiF", 44, 68),

  /* Chinese Standard Sheet Media Inch Sizes */
  _PWG_MEDIA_IN("roc_16k_7.75x10.75in", NULL, NULL, 7.75, 10.75),
  _PWG_MEDIA_IN("roc_8k_10.75x15.5in", NULL, NULL, 10.75, 15.5),

  /* ISO Standard Sheet Media Sizes */
  _PWG_MEDIA_MM("iso_a10_26x37mm", "iso-a10", "A10", 26, 37),
  _PWG_MEDIA_MM("iso_a9_37x52mm", "iso-a9", "A9", 37, 52),
  _PWG_MEDIA_MM("iso_a8_52x74mm", "iso-a8", "A8", 52, 74),
  _PWG_MEDIA_MM("iso_a7_74x105mm", "iso-a7", "A7", 74, 105),
  _PWG_MEDIA_MM("iso_a6_105x148mm", "iso-a6", "A6", 105, 148),
  _PWG_MEDIA_MM("iso_a5_148x210mm", "iso-a5", "A5", 148, 210),
  _PWG_MEDIA_MM("iso_a5-extra_174x235mm", NULL, "A5Extra", 174, 235),
  _PWG_MEDIA_MM("iso_a4_210x297mm", "iso-a4", "A4", 210, 297),
  _PWG_MEDIA_MM("iso_a4-tab_225x297mm", NULL, NULL, 225, 297),
  _PWG_MEDIA_MM("iso_a4-extra_235.5x322.3mm", NULL, "A4Extra", 235.5, 322.3),
  _PWG_MEDIA_MM("iso_a3_297x420mm", "iso-a3", "A3", 297, 420),
  _PWG_MEDIA_MM("iso_a4x3_297x630mm", "iso-a4x3", NULL, 297, 630),
  _PWG_MEDIA_MM("iso_a4x4_297x841mm", "iso-a4x4", NULL, 297, 841),
  _PWG_MEDIA_MM("iso_a4x5_297x1051mm", "iso-a4x5", NULL, 297, 1051),
  _PWG_MEDIA_MM("iso_a4x6_297x1261mm", "iso-a4x6", NULL, 297, 1261),
  _PWG_MEDIA_MM("iso_a4x7_297x1471mm", "iso-a4x7", NULL, 297, 1471),
  _PWG_MEDIA_MM("iso_a4x8_297x1682mm", "iso-a4x8", NULL, 297, 1682),
  _PWG_MEDIA_MM("iso_a4x9_297x1892mm", "iso-a4x9", NULL, 297, 1892),
  _PWG_MEDIA_MM("iso_a3-extra_322x445mm", "iso-a3-extra", "A3Extra", 322, 445),
  _PWG_MEDIA_MM("iso_a2_420x594mm", "iso-a2", "A2", 420, 594),
  _PWG_MEDIA_MM("iso_a3x3_420x891mm", "iso-a3x3", NULL, 420, 891),
  _PWG_MEDIA_MM("iso_a3x4_420x1189mm", "iso-a3x4", NULL, 420, 1189),
  _PWG_MEDIA_MM("iso_a3x5_420x1486mm", "iso-a3x5", NULL, 420, 1486),
  _PWG_MEDIA_MM("iso_a3x6_420x1783mm", "iso-a3x6", NULL, 420, 1783),
  _PWG_MEDIA_MM("iso_a3x7_420x2080mm", "iso-a3x7", NULL, 420, 2080),
  _PWG_MEDIA_MM("iso_a1_594x841mm", "iso-a1", "A1", 594, 841),
  _PWG_MEDIA_MM("iso_a2x3_594x1261mm", "iso-a2x3", NULL, 594, 1261),
  _PWG_MEDIA_MM("iso_a2x4_594x1682mm", "iso-a2x4", NULL, 594, 1682),
  _PWG_MEDIA_MM("iso_a2x5_594x2102mm", "iso-a2x5", NULL, 594, 2102),
  _PWG_MEDIA_MM("iso_a0_841x1189mm", "iso-a0", "A0", 841, 1189),
  _PWG_MEDIA_MM("iso_a1x3_841x1783mm", "iso-a1x3", NULL, 841, 1783),
  _PWG_MEDIA_MM("iso_a1x4_841x2378mm", "iso-a1x4", NULL, 841, 2378),
  _PWG_MEDIA_MM("iso_2a0_1189x1682mm", NULL, NULL, 1189, 1682),
  _PWG_MEDIA_MM("iso_a0x3_1189x2523mm", NULL, NULL, 1189, 2523),
  _PWG_MEDIA_MM("iso_b10_31x44mm", "iso-b10", "ISOB10", 31, 44),
  _PWG_MEDIA_MM("iso_b9_44x62mm", "iso-b9", "ISOB9", 44, 62),
  _PWG_MEDIA_MM("iso_b8_62x88mm", "iso-b8", "ISOB8", 62, 88),
  _PWG_MEDIA_MM("iso_b7_88x125mm", "iso-b7", "ISOB7", 88, 125),
  _PWG_MEDIA_MM("iso_b6_125x176mm", "iso-b6", "ISOB6", 125, 176),
  _PWG_MEDIA_MM("iso_b6c4_125x324mm", NULL, NULL, 125, 324),
  _PWG_MEDIA_MM("iso_b5_176x250mm", "iso-b5", "ISOB5", 176, 250),
  _PWG_MEDIA_MM("iso_b5-extra_201x276mm", NULL, "ISOB5Extra", 201, 276),
  _PWG_MEDIA_MM("iso_b4_250x353mm", "iso-b4", "ISOB4", 250, 353),
  _PWG_MEDIA_MM("iso_b3_353x500mm", "iso-b3", "ISOB3", 353, 500),
  _PWG_MEDIA_MM("iso_b2_500x707mm", "iso-b2", "ISOB2", 500, 707),
  _PWG_MEDIA_MM("iso_b1_707x1000mm", "iso-b1", "ISOB1", 707, 1000),
  _PWG_MEDIA_MM("iso_b0_1000x1414mm", "iso-b0", "ISOB0", 1000, 1414),
  _PWG_MEDIA_MM("iso_c10_28x40mm", "iso-c10", NULL, 28, 40),
  _PWG_MEDIA_MM("iso_c9_40x57mm", "iso-c9", NULL, 40, 57),
  _PWG_MEDIA_MM("iso_c8_57x81mm", "iso-c8", NULL, 57, 81),
  _PWG_MEDIA_MM("iso_c7_81x114mm", "iso-c7", "EnvC7", 81, 114),
  _PWG_MEDIA_MM("iso_c7c6_81x162mm", NULL, NULL, 81, 162),
  _PWG_MEDIA_MM("iso_c6_114x162mm", "iso-c6", "EnvC6", 114, 162),
  _PWG_MEDIA_MM("iso_c6c5_114x229mm", NULL, "EnvC65", 114, 229),
  _PWG_MEDIA_MM("iso_c5_162x229mm", "iso-c5", "EnvC5", 162, 229),
  _PWG_MEDIA_MM("iso_c4_229x324mm", "iso-c4", "EnvC4", 229, 324),
  _PWG_MEDIA_MM("iso_c3_324x458mm", "iso-c3", "EnvC3", 324, 458),
  _PWG_MEDIA_MM("iso_c2_458x648mm", "iso-c2", "EnvC2", 458, 648),
  _PWG_MEDIA_MM("iso_c1_648x917mm", "iso-c1", "EnvC1", 648, 917),
  _PWG_MEDIA_MM("iso_c0_917x1297mm", "iso-c0", "EnvC0", 917, 1297),
  _PWG_MEDIA_MM("iso_dl_110x220mm", "iso-designated", "EnvDL", 110, 220),
  _PWG_MEDIA_MM("iso_ra2_430x610mm", "iso-ra2", NULL, 430, 610),
  _PWG_MEDIA_MM("iso_sra2_450x640mm", "iso-sra2", NULL, 450, 640),
  _PWG_MEDIA_MM("iso_ra1_610x860mm", "iso-ra1", NULL, 610, 860),
  _PWG_MEDIA_MM("iso_sra1_640x900mm", "iso-sra1", NULL, 640, 900),
  _PWG_MEDIA_MM("iso_ra0_860x1220mm", "iso-ra0", NULL, 860, 1220),
  _PWG_MEDIA_MM("iso_sra0_900x1280mm", "iso-sra0", NULL, 900, 1280),

  /* Japanese Standard Sheet Media Sizes */
  _PWG_MEDIA_MM("jis_b10_32x45mm", "jis-b10", "B10", 32, 45),
  _PWG_MEDIA_MM("jis_b9_45x64mm", "jis-b9", "B9", 45, 64),
  _PWG_MEDIA_MM("jis_b8_64x91mm", "jis-b8", "B8", 64, 91),
  _PWG_MEDIA_MM("jis_b7_91x128mm", "jis-b7", "B7", 91, 128),
  _PWG_MEDIA_MM("jis_b6_128x182mm", "jis-b6", "B6", 128, 182),
  _PWG_MEDIA_MM("jis_b5_182x257mm", "jis-b5", "B5", 182, 257),
  _PWG_MEDIA_MM("jis_b4_257x364mm", "jis-b4", "B4", 257, 364),
  _PWG_MEDIA_MM("jis_b3_364x515mm", "jis-b3", "B3", 364, 515),
  _PWG_MEDIA_MM("jis_b2_515x728mm", "jis-b2", "B2", 515, 728),
  _PWG_MEDIA_MM("jis_b1_728x1030mm", "jis-b1", "B1", 728, 1030),
  _PWG_MEDIA_MM("jis_b0_1030x1456mm", "jis-b0", "B0", 1030, 1456),
  _PWG_MEDIA_MM("jis_exec_216x330mm", NULL, NULL, 216, 330),
  _PWG_MEDIA_MM("jpn_chou4_90x205mm", NULL, "EnvChou4", 90, 205),
  _PWG_MEDIA_MM("jpn_hagaki_100x148mm", NULL, "Postcard", 100, 148),
  _PWG_MEDIA_MM("jpn_you4_105x235mm", NULL, "EnvYou4", 105, 235),
  _PWG_MEDIA_MM("jpn_chou2_111.1x146mm", NULL, NULL, 111.1, 146),
  _PWG_MEDIA_MM("jpn_chou3_120x235mm", NULL, "EnvChou3", 120, 235),
  _PWG_MEDIA_MM("jpn_oufuku_148x200mm", NULL, "DoublePostcardRotated", 148, 200),
  _PWG_MEDIA_MM("jpn_kahu_240x322.1mm", NULL, NULL, 240, 322.1),
  _PWG_MEDIA_MM("jpn_kaku2_240x332mm", NULL, "EnvKaku2", 240, 332),

  /* Chinese Standard Sheet Media Sizes */
  _PWG_MEDIA_MM("prc_32k_97x151mm", NULL, "PRC32K", 97, 151),
  _PWG_MEDIA_MM("prc_1_102x165mm", NULL, "EnvPRC1", 102, 165),
  _PWG_MEDIA_MM("prc_2_102x176mm", NULL, "EnvPRC2", 102, 176),
  _PWG_MEDIA_MM("prc_4_110x208mm", NULL, "EnvPRC4", 110, 208),
  _PWG_MEDIA_MM("prc_5_110x220mm", NULL, "EnvPRC5", 110, 220),
  _PWG_MEDIA_MM("prc_8_120x309mm", NULL, "EnvPRC8", 120, 309),
  _PWG_MEDIA_MM("prc_6_120x320mm", NULL, NULL, 120, 320),
  _PWG_MEDIA_MM("prc_3_125x176mm", NULL, "EnvPRC3", 125, 176),
  _PWG_MEDIA_MM("prc_16k_146x215mm", NULL, "PRC16K", 146, 215),
  _PWG_MEDIA_MM("prc_7_160x230mm", NULL, "EnvPRC7", 160, 230),
  _PWG_MEDIA_MM("om_juuro-ku-kai_198x275mm", NULL, NULL, 198, 275),
  _PWG_MEDIA_MM("om_pa-kai_267x389mm", NULL, NULL, 267, 389),
  _PWG_MEDIA_MM("om_dai-pa-kai_275x395mm", NULL, NULL, 275, 395),
  _PWG_MEDIA_MM("prc_10_324x458mm", NULL, "EnvPRC10", 324, 458),

  /* Other Metric Standard Sheet Media Sizes */
  _PWG_MEDIA_MM("om_small-photo_100x150mm", NULL, NULL, 100, 150),
  _PWG_MEDIA_MM("om_italian_110x230mm", NULL, "EnvItalian", 110, 230),
  _PWG_MEDIA_MM("om_postfix_114x229mm", NULL, NULL, 114, 229),
  _PWG_MEDIA_MM("om_large-photo_200x300", NULL, NULL, 200, 300),
  _PWG_MEDIA_MM("om_folio_210x330mm", "folio", "Folio", 210, 330),
  _PWG_MEDIA_MM("om_folio-sp_215x315mm", NULL, NULL, 215, 315),
  _PWG_MEDIA_MM("om_invite_220x220mm", NULL, "EnvInvite", 220, 220)
};


/*
 * '_pwgGenerateSize()' - Generate a PWG size keyword.
 */

void
_pwgGenerateSize(char       *keyword,	/* I - Keyword buffer */
                 size_t     keysize,	/* I - Size of keyword buffer */
		 const char *prefix,	/* I - Prefix for PWG size or NULL */
		 const char *name,	/* I - Size name or NULL */
		 int        width,	/* I - Width of page in 2540ths */
		 int        length)	/* I - Length of page in 2540ths */
{
  struct lconv	*loc;			/* Locale conversion data */
  double	uwidth,			/* Width in inches or millimeters */
		ulength;		/* Height in inches or millimeters */
  const char	*units;			/* Units to report */
  char		usize[12 + 1 + 12 + 3],	/* Unit size: NNNNNNNNNNNNxNNNNNNNNNNNNuu */
		*uptr;			/* Pointer into unit size */


  loc = localeconv();

  if ((width % 635) == 0 && (length % 635) == 0)
  {
   /*
    * Use inches since the size is a multiple of 1/4 inch.
    */

    uwidth  = width / 2540.0;
    ulength = length / 2540.0;
    units   = "in";

    if (!prefix)
      prefix = "oe";
  }
  else
  {
   /*
    * Use millimeters since the size is not a multiple of 1/4 inch.
    */

    uwidth  = width * 0.01;
    ulength = length * 0.01;
    units   = "mm";

    if (!prefix)
      prefix = "om";
  }

  uptr = usize;
  _cupsStrFormatd(uptr, uptr + 12, uwidth, loc);
  uptr += strlen(uptr);
  *uptr++ = 'x';
  _cupsStrFormatd(uptr, uptr + 12, ulength, loc);
  uptr += strlen(uptr);

 /*
  * Safe because usize can hold up to 12 + 1 + 12 + 4 bytes.
  */

  strcpy(uptr, units);

  if (!name)
    name = usize;

 /*
  * Format the name...
  */

  snprintf(keyword, keysize, "%s_%s_%s", prefix, name, usize);
}


/*
 * '_pwgInitSize()' - Initialize a PWG size using IPP job template attributes.
 */

int					/* O - 1 if size was initialize, 0 otherwise */
_pwgInitSize(_pwg_size_t *size,		/* I - Size to initialize */
             ipp_t       *job,		/* I - Job template attributes */
	     int         *margins_set)	/* O - 1 if margins were set, 0 otherwise */
{
  ipp_attribute_t *media,		/* media attribute */
		*media_bottom_margin,	/* media-bottom-margin member attribute */
		*media_col,		/* media-col attribute */
		*media_left_margin,	/* media-left-margin member attribute */
		*media_right_margin,	/* media-right-margin member attribute */
		*media_size,		/* media-size member attribute */
		*media_top_margin,	/* media-top-margin member attribute */
		*x_dimension,		/* x-dimension member attribute */
		*y_dimension;		/* y-dimension member attribute */
  _pwg_media_t	*pwg;			/* PWG media value */


 /*
  * Range check input...
  */

  if (!size || !job || !margins_set)
    return (0);

 /*
  * Look for media-col and then media...
  */

  memset(size, 0, sizeof(_pwg_size_t));
  *margins_set = 0;

  if ((media_col = ippFindAttribute(job, "media-col",
                                    IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
   /*
    * Got media-col, look for media-size member attribute...
    */

    if ((media_size = ippFindAttribute(media_col->values[0].collection,
				       "media-size",
				       IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
     /*
      * Got media-size, look for x-dimension and y-dimension member
      * attributes...
      */

      x_dimension = ippFindAttribute(media_size->values[0].collection,
				     "x-dimension", IPP_TAG_INTEGER);
      y_dimension = ippFindAttribute(media_size->values[0].collection,
                                     "y-dimension", IPP_TAG_INTEGER);

      if (x_dimension && y_dimension)
      {
        size->width  = x_dimension->values[0].integer;
	size->length = y_dimension->values[0].integer;
      }
      else if (!x_dimension)
      {
	_cupsSetError(IPP_INTERNAL_ERROR,
		      _("Missing x-dimension in media-size."), 1);
        return (0);
      }
      else if (!y_dimension)
      {
	_cupsSetError(IPP_INTERNAL_ERROR,
		      _("Missing y-dimension in media-size."), 1);
        return (0);
      }
    }
    else
    {
      _cupsSetError(IPP_INTERNAL_ERROR, _("Missing media-size in media-col."),
                    1);
      return (0);
    }

    /* media-*-margin */
    media_bottom_margin = ippFindAttribute(media_col->values[0].collection,
					   "media-bottom-margin",
					   IPP_TAG_INTEGER);
    media_left_margin   = ippFindAttribute(media_col->values[0].collection,
					   "media-left-margin",
					   IPP_TAG_INTEGER);
    media_right_margin  = ippFindAttribute(media_col->values[0].collection,
					   "media-right-margin",
					   IPP_TAG_INTEGER);
    media_top_margin    = ippFindAttribute(media_col->values[0].collection,
					   "media-top-margin",
					   IPP_TAG_INTEGER);
    if (media_bottom_margin && media_left_margin && media_right_margin &&
        media_top_margin)
    {
      *margins_set = 1;
      size->bottom = media_bottom_margin->values[0].integer;
      size->left   = media_left_margin->values[0].integer;
      size->right  = media_right_margin->values[0].integer;
      size->top    = media_top_margin->values[0].integer;
    }
  }
  else
  {
    if ((media = ippFindAttribute(job, "media", IPP_TAG_NAME)) == NULL)
      if ((media = ippFindAttribute(job, "media", IPP_TAG_KEYWORD)) == NULL)
        if ((media = ippFindAttribute(job, "PageSize", IPP_TAG_NAME)) == NULL)
	  media = ippFindAttribute(job, "PageRegion", IPP_TAG_NAME);

    if (media)
    {
      const char *name = media->values[0].string.text;
					/* Name string */

      if ((pwg = _pwgMediaForPWG(name)) == NULL)
      {
       /*
        * Not a PWG name, try a legacy name...
	*/

	if ((pwg = _pwgMediaForLegacy(name)) == NULL)
	{
	 /*
	  * Not a legacy name, try a PPD name...
	  */

	  const char	*suffix;	/* Suffix on media string */

	  pwg = _pwgMediaForPPD(name);
	  if (pwg && 
	      (suffix = name + strlen(name) - 10 /* .FullBleed */) > name &&
	      !strcasecmp(suffix, ".FullBleed"))
	  {
	   /*
	    * Indicate that margins are set with the default values of 0.
	    */

	    *margins_set = 1;
	  }
	}
      }

      if (pwg)
      {
        size->width  = pwg->width;
	size->length = pwg->length;
      }
      else
      {
        _cupsSetError(IPP_INTERNAL_ERROR, _("Unsupported media value."), 1);
	return (0);
      }
    }
    else
    {
      _cupsSetError(IPP_INTERNAL_ERROR, _("Missing media or media-col."), 1);
      return (0);
    }
  }

  return (1);
}


/*
 * '_pwgMediaForLegacy()' - Find a PWG media size by ISO/IPP legacy name.
 */

_pwg_media_t *				/* O - Matching size or NULL */
_pwgMediaForLegacy(
    const char *legacy)			/* I - Legacy size name */
{
  _pwg_media_t	key;			/* Search key */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


 /*
  * Range check input...
  */

  if (!legacy)
    return (NULL);

 /*
  * Build the lookup table for PWG names as needed...
  */

  if (!cg->leg_size_lut)
  {
    int			i;		/* Looping var */
    _pwg_media_t	*size;		/* Current size */

    cg->leg_size_lut = cupsArrayNew((cups_array_func_t)pwg_compare_legacy,
                                    NULL);

    for (i = (int)(sizeof(cups_pwg_media) / sizeof(cups_pwg_media[0])),
             size = (_pwg_media_t *)cups_pwg_media;
	 i > 0;
	 i --, size ++)
      if (size->legacy)
	cupsArrayAdd(cg->leg_size_lut, size);
  }

 /*
  * Lookup the name...
  */

  key.legacy = legacy;
  return ((_pwg_media_t *)cupsArrayFind(cg->leg_size_lut, &key));
}


/*
 * '_pwgMediaForPPD()' - Find a PWG media size by Adobe PPD name.
 */

_pwg_media_t *				/* O - Matching size or NULL */
_pwgMediaForPPD(const char *ppd)	/* I - PPD size name */
{
  _pwg_media_t	key,			/* Search key */
		*size;			/* Matching size */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


 /*
  * Range check input...
  */

  if (!ppd)
    return (NULL);

 /*
  * Build the lookup table for PWG names as needed...
  */

  if (!cg->ppd_size_lut)
  {
    int	i;				/* Looping var */

    cg->ppd_size_lut = cupsArrayNew((cups_array_func_t)pwg_compare_ppd, NULL);

    for (i = (int)(sizeof(cups_pwg_media) / sizeof(cups_pwg_media[0])),
             size = (_pwg_media_t *)cups_pwg_media;
	 i > 0;
	 i --, size ++)
      if (size->ppd)
        cupsArrayAdd(cg->ppd_size_lut, size);
  }

 /*
  * Lookup the name...
  */

  key.ppd = ppd;
  if ((size = (_pwg_media_t *)cupsArrayFind(cg->ppd_size_lut, &key)) == NULL)
  {
   /*
    * See if the name is of the form:
    *
    *   [Custom.]WIDTHxLENGTH[.FullBleed]    - Size in points/inches [borderless]
    *   [Custom.]WIDTHxLENGTHcm[.FullBleed]  - Size in centimeters [borderless]
    *   [Custom.]WIDTHxLENGTHft[.FullBleed]  - Size in feet [borderless]
    *   [Custom.]WIDTHxLENGTHin[.FullBleed]  - Size in inches [borderless]
    *   [Custom.]WIDTHxLENGTHm[.FullBleed]   - Size in meters [borderless]
    *   [Custom.]WIDTHxLENGTHmm[.FullBleed]  - Size in millimeters [borderless]
    *   [Custom.]WIDTHxLENGTHpt[.FullBleed]  - Size in points [borderless]
    */

    double		w, l,		/* Width and length of page */
			factor;		/* Unit scaling factor */
    char		*ptr;		/* Pointer into name */
    struct lconv	*loc;		/* Locale data */
    int			custom;		/* Custom page size? */

    if (!strncasecmp(ppd, "Custom.", 7))
    {
      custom = 1;
      factor = 2540.0 / 72.0;
      ptr    = (char *)ppd + 7;
    }
    else
    {
      custom = 0;
      factor = 2540.0;
      ptr    = (char *)ppd;
    }

    loc = localeconv();
    w   = _cupsStrScand(ptr, &ptr, loc);

    if (ptr && ptr > ppd && *ptr == 'x')
    {
      l = _cupsStrScand(ptr + 1, &ptr, loc);

      if (ptr &&
	  (!*ptr ||
	   !strcasecmp(ptr, "FullBleed") ||
	   !strcasecmp(ptr, ".FullBleed") ||
	   !strcasecmp(ptr, "cm") ||
	   !strcasecmp(ptr, "cm.FullBleed") ||
	   !strcasecmp(ptr, "ft") ||
	   !strcasecmp(ptr, "ft.FullBleed") ||
	   !strcasecmp(ptr, "in") ||
	   !strcasecmp(ptr, "in.FullBleed") ||
	   !strcasecmp(ptr, "m") ||
	   !strcasecmp(ptr, "m.FullBleed") ||
	   !strcasecmp(ptr, "mm") ||
	   !strcasecmp(ptr, "mm.FullBleed") ||
	   !strcasecmp(ptr, "pt") ||
	   !strcasecmp(ptr, "pt.FullBleed")))
      {
	size = &(cg->pwg_media);

	if (!strncasecmp(ptr, "cm", 2))
	  factor = 1000.0;
	else if (!strncasecmp(ptr, "ft", 2))
	  factor = 2540.0 * 12.0;
	else if (!strncasecmp(ptr, "in", 2))
	  factor = 2540.0;
	else if (!strncasecmp(ptr, "mm", 2))
	  factor = 100.0;
	else if (*ptr == 'm' || *ptr == 'M')
	  factor = 100000.0;
	else if (!strncasecmp(ptr, "pt", 2))
	  factor = 2540.0 / 72.0;

       /*
	* Not a standard size; convert it to a PWG custom name of the form:
	*
	*     [oe|om]_WIDTHxHEIGHTuu_WIDTHxHEIGHTuu
	*/

	size->width  = (int)(w * factor);
	size->length = (int)(l * factor);
	size->pwg    = cg->pwg_name;

	_pwgGenerateSize(cg->pwg_name, sizeof(cg->pwg_name),
	                 custom ? "custom" : NULL, custom ? ppd + 7 : NULL,
	                 size->width, size->length);
      }
    }
  }

  return (size);
}


/*
 * '_pwgMediaForPWG()' - Find a PWG media size by 5101.1 self-describing name.
 */

_pwg_media_t *				/* O - Matching size or NULL */
_pwgMediaForPWG(const char *pwg)	/* I - PWG size name */
{
  char		*ptr;			/* Pointer into name */
  _pwg_media_t	key,			/* Search key */
		*size;			/* Matching size */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


 /*
  * Range check input...
  */

  if (!pwg)
    return (NULL);

 /*
  * Build the lookup table for PWG names as needed...
  */

  if (!cg->pwg_size_lut)
  {
    int	i;				/* Looping var */

    cg->pwg_size_lut = cupsArrayNew((cups_array_func_t)pwg_compare_pwg, NULL);

    for (i = (int)(sizeof(cups_pwg_media) / sizeof(cups_pwg_media[0])),
             size = (_pwg_media_t *)cups_pwg_media;
	 i > 0;
	 i --, size ++)
      cupsArrayAdd(cg->pwg_size_lut, size);
  }

 /*
  * Lookup the name...
  */

  key.pwg = pwg;
  if ((size = (_pwg_media_t *)cupsArrayFind(cg->pwg_size_lut, &key)) == NULL &&
      (ptr = (char *)strchr(pwg, '_')) != NULL &&
      (ptr = (char *)strchr(ptr + 1, '_')) != NULL)
  {
   /*
    * Try decoding the self-describing name of the form:
    *
    * class_name_WWWxHHHin
    * class_name_WWWxHHHmm
    */

    double		w, l;		/* Width and length of page */
    struct lconv	*loc;		/* Locale data */

    ptr ++;
    loc = localeconv();
    w   = _cupsStrScand(ptr, &ptr, loc);

    if (ptr && *ptr == 'x')
    {
      l = _cupsStrScand(ptr + 1, &ptr, loc);

      if (ptr && (!strcmp(ptr, "in") || !strcmp(ptr, "mm")))
      {
	size = &(cg->pwg_media);

	if (!strcmp(ptr, "mm"))
	{
	  size->width  = (int)(w * 100);
	  size->length = (int)(l * 100);
	}
	else
	{
	  size->width  = (int)(w * 2540);
	  size->length = (int)(l * 2540);
	}

        strlcpy(cg->pwg_name, pwg, sizeof(cg->pwg_name));
	size->pwg = cg->pwg_name;
      }
    }
  }

  return (size);
}


/*
 * '_pwgMediaForSize()' - Get the PWG media name for a given size.
 */

_pwg_media_t *				/* O - PWG media name */
_pwgMediaForSize(int width,		/* I - Width in 2540ths */
		 int length)		/* I - Length in 2540ths */
{
  int		i;			/* Looping var */
  _pwg_media_t	*media;			/* Current media */
  int		dw, dl;			/* Difference in width and length */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


 /*
  * Range check input...
  */

  if (width <= 0 || length <= 0)
    return (NULL);

 /*
  * Look for a standard size...
  */

  for (i = (int)(sizeof(cups_pwg_media) / sizeof(cups_pwg_media[0])),
	   media = (_pwg_media_t *)cups_pwg_media;
       i > 0;
       i --, media ++)
  {
   /*
    * Adobe uses a size matching algorithm with an epsilon of 5 points, which
    * is just about 176/2540ths...
    */

    dw = media->width - width;
    dl = media->length - length;

    if (dw > -176 && dw < 176 && dl > -176 && dl < 176)
      return (media);
  }

 /*
  * Not a standard size; convert it to a PWG custom name of the form:
  *
  *     custom_WIDTHxHEIGHTuu_WIDTHxHEIGHTuu
  */

  _pwgGenerateSize(cg->pwg_name, sizeof(cg->pwg_name), "custom", NULL, width,
                   length);

  cg->pwg_media.pwg    = cg->pwg_name;
  cg->pwg_media.width  = width;
  cg->pwg_media.length = length;

  return (&(cg->pwg_media));
}


/*
 * 'pwg_compare_legacy()' - Compare two sizes using the legacy names.
 */

static int				/* O - Result of comparison */
pwg_compare_legacy(_pwg_media_t *a,	/* I - First size */
                   _pwg_media_t *b)	/* I - Second size */
{
  return (strcmp(a->legacy, b->legacy));
}


/*
 * 'pwg_compare_ppd()' - Compare two sizes using the PPD names.
 */

static int				/* O - Result of comparison */
pwg_compare_ppd(_pwg_media_t *a,	/* I - First size */
                _pwg_media_t *b)	/* I - Second size */
{
  return (strcmp(a->ppd, b->ppd));
}


/*
 * 'pwg_compare_pwg()' - Compare two sizes using the PWG names.
 */

static int				/* O - Result of comparison */
pwg_compare_pwg(_pwg_media_t *a,	/* I - First size */
                _pwg_media_t *b)	/* I - Second size */
{
  return (strcmp(a->pwg, b->pwg));
}


/*
 * End of "$Id: pwg-media.c 9310 2010-09-21 22:34:57Z mike $".
 */
