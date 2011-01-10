/*
 * "$Id: encode.c 9258 2010-08-13 01:34:04Z mike $"
 *
 *   Option encoding routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
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
 *   cupsEncodeOptions()   - Encode printer options into IPP attributes.
 *   cupsEncodeOptions2()  - Encode printer options into IPP attributes for
 *                           a group.
 *   _ippFindOption()      - Find the attribute information for an option.
 *   compare_ipp_options() - Compare two IPP options.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "ipp-private.h"
#include <stdlib.h>
#include <ctype.h>
#include "string.h"
#include "debug.h"


/*
 * Local list of option names and the value tags they should use...
 *
 * **** THIS LIST MUST BE SORTED ****
 */

static const _ipp_option_t ipp_options[] =
{
  { 1, "auth-info",		IPP_TAG_TEXT,		IPP_TAG_JOB },
  { 1, "auth-info-required",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "blackplot",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "blackplot-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "brightness",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "brightness-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "columns",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "columns-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "compression",		IPP_TAG_KEYWORD,	IPP_TAG_OPERATION },
  { 0, "copies",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "copies-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "device-uri",		IPP_TAG_URI,		IPP_TAG_PRINTER },
  { 0, "document-format",	IPP_TAG_MIMETYPE,	IPP_TAG_OPERATION },
  { 0, "document-format-default", IPP_TAG_MIMETYPE,	IPP_TAG_PRINTER },
  { 1, "exclude-schemes",	IPP_TAG_NAME,		IPP_TAG_OPERATION },
  { 1, "finishings",		IPP_TAG_ENUM,		IPP_TAG_JOB },
  { 1, "finishings-default",	IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { 0, "fit-to-page",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "fit-to-page-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "fitplot",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "fitplot-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "gamma",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "gamma-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "hue",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "hue-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "include-schemes",	IPP_TAG_NAME,		IPP_TAG_OPERATION },
  { 0, "job-impressions",	IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "job-k-limit",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "job-page-limit",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "job-priority",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "job-quota-period",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "job-sheets",		IPP_TAG_NAME,		IPP_TAG_JOB },
  { 1, "job-sheets-default",	IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 0, "job-uuid",		IPP_TAG_URI,		IPP_TAG_JOB },
  { 0, "landscape",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 1, "marker-change-time",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "marker-colors",		IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 1, "marker-high-levels",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "marker-levels",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "marker-low-levels",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "marker-message",	IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 1, "marker-names",		IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 1, "marker-types",		IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 1, "media",			IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { 0, "media-col",		IPP_TAG_BEGIN_COLLECTION, IPP_TAG_JOB },
  { 0, "media-col-default",	IPP_TAG_BEGIN_COLLECTION, IPP_TAG_PRINTER },
  { 0, "media-color",		IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { 1, "media-default",		IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "media-key",		IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { 0, "media-size",		IPP_TAG_BEGIN_COLLECTION, IPP_TAG_JOB },
  { 0, "media-type",		IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { 0, "mirror",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "mirror-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "natural-scaling",	IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "natural-scaling-default", IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "notify-charset",	IPP_TAG_CHARSET,	IPP_TAG_SUBSCRIPTION },
  { 1, "notify-events",		IPP_TAG_KEYWORD,	IPP_TAG_SUBSCRIPTION },
  { 1, "notify-events-default",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "notify-lease-duration",	IPP_TAG_INTEGER,	IPP_TAG_SUBSCRIPTION },
  { 0, "notify-lease-duration-default", IPP_TAG_INTEGER, IPP_TAG_PRINTER },
  { 0, "notify-natural-language", IPP_TAG_LANGUAGE,	IPP_TAG_SUBSCRIPTION },
  { 0, "notify-pull-method",	IPP_TAG_KEYWORD,	IPP_TAG_SUBSCRIPTION },
  { 0, "notify-recipient-uri",	IPP_TAG_URI,		IPP_TAG_SUBSCRIPTION },
  { 0, "notify-time-interval",	IPP_TAG_INTEGER,	IPP_TAG_SUBSCRIPTION },
  { 0, "notify-user-data",	IPP_TAG_STRING,		IPP_TAG_SUBSCRIPTION },
  { 0, "number-up",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "number-up-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "orientation-requested",	IPP_TAG_ENUM,		IPP_TAG_JOB },
  { 0, "orientation-requested-default", IPP_TAG_ENUM,	IPP_TAG_PRINTER },
  { 0, "page-bottom",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "page-bottom-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "page-left",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "page-left-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "page-ranges",		IPP_TAG_RANGE,		IPP_TAG_JOB },
  { 1, "page-ranges-default",	IPP_TAG_RANGE,		IPP_TAG_PRINTER },
  { 0, "page-right",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "page-right-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "page-top",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "page-top-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "penwidth",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "penwidth-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "port-monitor",		IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 0, "ppd-name",		IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 0, "ppi",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "ppi-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "prettyprint",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "prettyprint-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "print-quality",		IPP_TAG_ENUM,		IPP_TAG_JOB },
  { 0, "print-quality-default",	IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { 1, "printer-commands",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "printer-error-policy",	IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 0, "printer-info",		IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 0, "printer-is-accepting-jobs", IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "printer-is-shared",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "printer-location",	IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 0, "printer-make-and-model", IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 0, "printer-more-info",	IPP_TAG_URI,		IPP_TAG_PRINTER },
  { 0, "printer-op-policy",	IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 0, "printer-resolution",	IPP_TAG_RESOLUTION,	IPP_TAG_JOB },
  { 0, "printer-state",		IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { 0, "printer-state-change-time", IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "printer-state-reasons",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "printer-type",		IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { 0, "printer-uri",		IPP_TAG_URI,		IPP_TAG_OPERATION },
  { 1, "printer-uri-supported",	IPP_TAG_URI,		IPP_TAG_PRINTER },
  { 0, "queued-job-count",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "raw",			IPP_TAG_MIMETYPE,	IPP_TAG_OPERATION },
  { 1, "requested-attributes",	IPP_TAG_NAME,		IPP_TAG_OPERATION },
  { 1, "requesting-user-name-allowed", IPP_TAG_NAME,	IPP_TAG_PRINTER },
  { 1, "requesting-user-name-denied", IPP_TAG_NAME,	IPP_TAG_PRINTER },
  { 0, "resolution",		IPP_TAG_RESOLUTION,	IPP_TAG_JOB },
  { 0, "resolution-default",	IPP_TAG_RESOLUTION,	IPP_TAG_PRINTER },
  { 0, "saturation",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "saturation-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "scaling",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "scaling-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "sides",			IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { 0, "sides-default",		IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "wrap",			IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "wrap-default",		IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "x-dimension",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "y-dimension",		IPP_TAG_INTEGER,	IPP_TAG_JOB }
};


/*
 * Local functions...
 */

static int	compare_ipp_options(_ipp_option_t *a, _ipp_option_t *b);


/*
 * 'cupsEncodeOptions()' - Encode printer options into IPP attributes.
 *
 * This function adds operation, job, and then subscription attributes,
 * in that order. Use the cupsEncodeOptions2() function to add attributes
 * for a single group.
 */

void
cupsEncodeOptions(ipp_t         *ipp,		/* I - Request to add to */
        	  int           num_options,	/* I - Number of options */
		  cups_option_t *options)	/* I - Options */
{
  DEBUG_printf(("cupsEncodeOptions(%p, %d, %p)", ipp, num_options, options));

 /*
  * Add the options in the proper groups & order...
  */

  cupsEncodeOptions2(ipp, num_options, options, IPP_TAG_OPERATION);
  cupsEncodeOptions2(ipp, num_options, options, IPP_TAG_JOB);
  cupsEncodeOptions2(ipp, num_options, options, IPP_TAG_SUBSCRIPTION);
}


/*
 * 'cupsEncodeOptions2()' - Encode printer options into IPP attributes for a group.
 *
 * This function only adds attributes for a single group. Call this
 * function multiple times for each group, or use cupsEncodeOptions()
 * to add the standard groups.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

void
cupsEncodeOptions2(
    ipp_t         *ipp,			/* I - Request to add to */
    int           num_options,		/* I - Number of options */
    cups_option_t *options,		/* I - Options */
    ipp_tag_t     group_tag)		/* I - Group to encode */
{
  int		i, j;			/* Looping vars */
  int		count;			/* Number of values */
  char		*s,			/* Pointer into option value */
		*val,			/* Pointer to option value */
		*copy,			/* Copy of option value */
		*sep,			/* Option separator */
		quote;			/* Quote character */
  ipp_attribute_t *attr;		/* IPP attribute */
  ipp_tag_t	value_tag;		/* IPP value tag */
  cups_option_t	*option;		/* Current option */
  ipp_t		*collection;		/* Collection value */
  int		num_cols;		/* Number of collection values */
  cups_option_t	*cols;			/* Collection values */


  DEBUG_printf(("cupsEncodeOptions2(ipp=%p, num_options=%d, options=%p, "
                "group_tag=%x)", ipp, num_options, options, group_tag));

 /*
  * Range check input...
  */

  if (!ipp || num_options < 1 || !options)
    return;

 /*
  * Do special handling for the document-format/raw options...
  */

  if (group_tag == IPP_TAG_OPERATION)
  {
   /*
    * Handle the document format stuff first...
    */

    if ((val = (char *)cupsGetOption("document-format", num_options, options)) != NULL)
      ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	   NULL, val);
    else if (cupsGetOption("raw", num_options, options))
      ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	   NULL, "application/vnd.cups-raw");
    else
      ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	   NULL, "application/octet-stream");
  }

 /*
  * Then loop through the options...
  */

  for (i = num_options, option = options; i > 0; i --, option ++)
  {
    _ipp_option_t	*match;		/* Matching attribute */


   /*
    * Skip document format options that are handled above...
    */

    if (!strcasecmp(option->name, "raw") ||
        !strcasecmp(option->name, "document-format") ||
	!option->name[0])
      continue;

   /*
    * Figure out the proper value and group tags for this option...
    */

    if ((match = _ippFindOption(option->name)) != NULL)
    {
      if (match->group_tag != group_tag)
        continue;

      value_tag = match->value_tag;
    }
    else
    {
      int	namelen;		/* Length of name */


      namelen = (int)strlen(option->name);

      if (namelen < 9 || strcmp(option->name + namelen - 8, "-default"))
      {
	if (group_tag != IPP_TAG_JOB)
          continue;
      }
      else if (group_tag != IPP_TAG_PRINTER)
        continue;

      if (!strcasecmp(option->value, "true") ||
          !strcasecmp(option->value, "false"))
	value_tag = IPP_TAG_BOOLEAN;
      else
	value_tag = IPP_TAG_NAME;
    }

   /*
    * Count the number of values...
    */

    if (match && match->multivalue)
    {
      for (count = 1, sep = option->value, quote = 0; *sep; sep ++)
      {
	if (*sep == quote)
	  quote = 0;
	else if (!quote && (*sep == '\'' || *sep == '\"'))
	{
	 /*
	  * Skip quoted option value...
	  */

	  quote = *sep++;
	}
	else if (*sep == ',' && !quote)
	  count ++;
	else if (*sep == '\\' && sep[1])
	  sep ++;
      }
    }
    else
      count = 1;

    DEBUG_printf(("2cupsEncodeOptions2: option=\"%s\", count=%d",
                  option->name, count));

   /*
    * Allocate memory for the attribute values...
    */

    if ((attr = _ippAddAttr(ipp, count)) == NULL)
    {
     /*
      * Ran out of memory!
      */

      DEBUG_puts("1cupsEncodeOptions2: Ran out of memory for attributes!");
      return;
    }

   /*
    * Now figure out what type of value we have...
    */

    attr->group_tag = group_tag;
    attr->value_tag = value_tag;

   /*
    * Copy the name over...
    */

    attr->name = _cupsStrAlloc(option->name);

    if (count > 1)
    {
     /*
      * Make a copy of the value we can fiddle with...
      */

      if ((copy = strdup(option->value)) == NULL)
      {
       /*
	* Ran out of memory!
	*/

	DEBUG_puts("1cupsEncodeOptions2: Ran out of memory for value copy!");
	ippDeleteAttribute(ipp, attr);
	return;
      }

      val = copy;
    }
    else
    {
     /*
      * Since we have a single value, use the value directly...
      */

      val  = option->value;
      copy = NULL;
    }

   /*
    * Scan the value string for values...
    */

    for (j = 0, sep = val; j < count; val = sep, j ++)
    {
     /*
      * Find the end of this value and mark it if needed...
      */

      if (count > 1)
      {
	for (quote = 0; *sep; sep ++)
	{
	  if (*sep == quote)
	  {
	   /*
	    * Finish quoted value...
	    */

	    quote = 0;
	  }
	  else if (!quote && (*sep == '\'' || *sep == '\"'))
	  {
	   /*
	    * Handle quoted option value...
	    */

	    quote = *sep;
	  }
	  else if (*sep == ',' && count > 1)
	    break;
	  else if (*sep == '\\' && sep[1])
	  {
	   /*
	    * Skip quoted character...
	    */

	    sep ++;
	  }
	}

	if (*sep == ',')
	  *sep++ = '\0';
      }

     /*
      * Copy the option value(s) over as needed by the type...
      */

      switch (attr->value_tag)
      {
	case IPP_TAG_INTEGER :
	case IPP_TAG_ENUM :
	   /*
	    * Integer/enumeration value...
	    */

            attr->values[j].integer = strtol(val, &s, 10);

            DEBUG_printf(("2cupsEncodeOptions2: Added integer option value "
	                  "%d...", attr->values[j].integer));
            break;

	case IPP_TAG_BOOLEAN :
	    if (!strcasecmp(val, "true") ||
	        !strcasecmp(val, "on") ||
	        !strcasecmp(val, "yes"))
	    {
	     /*
	      * Boolean value - true...
	      */

	      attr->values[j].boolean = 1;

              DEBUG_puts("2cupsEncodeOptions2: Added boolean true value...");
	    }
	    else
	    {
	     /*
	      * Boolean value - false...
	      */

	      attr->values[j].boolean = 0;

              DEBUG_puts("2cupsEncodeOptions2: Added boolean false value...");
	    }
            break;

	case IPP_TAG_RANGE :
	   /*
	    * Range...
	    */

            if (*val == '-')
	    {
	      attr->values[j].range.lower = 1;
	      s = val;
	    }
	    else
	      attr->values[j].range.lower = strtol(val, &s, 10);

	    if (*s == '-')
	    {
	      if (s[1])
		attr->values[j].range.upper = strtol(s + 1, NULL, 10);
	      else
		attr->values[j].range.upper = 2147483647;
            }
	    else
	      attr->values[j].range.upper = attr->values[j].range.lower;

	    DEBUG_printf(("2cupsEncodeOptions2: Added range option value "
	                  "%d-%d...", attr->values[j].range.lower,
			  attr->values[j].range.upper));
            break;

	case IPP_TAG_RESOLUTION :
	   /*
	    * Resolution...
	    */

	    attr->values[j].resolution.xres = strtol(val, &s, 10);

	    if (*s == 'x')
	      attr->values[j].resolution.yres = strtol(s + 1, &s, 10);
	    else
	      attr->values[j].resolution.yres = attr->values[j].resolution.xres;

	    if (!strcasecmp(s, "dpc"))
              attr->values[j].resolution.units = IPP_RES_PER_CM;
            else
              attr->values[j].resolution.units = IPP_RES_PER_INCH;

	    DEBUG_printf(("2cupsEncodeOptions2: Added resolution option value "
	                  "%s...", val));
            break;

	case IPP_TAG_STRING :
           /*
	    * octet-string
	    */

            attr->values[j].unknown.length = (int)strlen(val);
	    attr->values[j].unknown.data   = strdup(val);

            DEBUG_printf(("2cupsEncodeOptions2: Added octet-string value "
	                  "\"%s\"...", (char *)attr->values[j].unknown.data));
            break;

        case IPP_TAG_BEGIN_COLLECTION :
	   /*
	    * Collection value
	    */

	    num_cols   = cupsParseOptions(val, 0, &cols);
	    if ((collection = ippNew()) == NULL)
	    {
	      cupsFreeOptions(num_cols, cols);

	      if (copy)
	        free(copy);

	      ippDeleteAttribute(ipp, attr);
	      return;
            }

	    attr->values[j].collection = collection;
	    cupsEncodeOptions2(collection, num_cols, cols, IPP_TAG_JOB);
            cupsFreeOptions(num_cols, cols);
	    break;

	default :
	    if ((attr->values[j].string.text = _cupsStrAlloc(val)) == NULL)
	    {
	     /*
	      * Ran out of memory!
	      */

	      DEBUG_puts("1cupsEncodeOptions2: Ran out of memory for string!");

	      if (copy)
	        free(copy);

	      ippDeleteAttribute(ipp, attr);
	      return;
	    }

	    DEBUG_printf(("2cupsEncodeOptions2: Added string value \"%s\"...",
	                  val));
            break;
      }
    }

    if (copy)
      free(copy);
  }
}


/*
 * '_ippFindOption()' - Find the attribute information for an option.
 */

_ipp_option_t *				/* O - Attribute information */
_ippFindOption(const char *name)	/* I - Option/attribute name */
{
  _ipp_option_t	key;			/* Search key */


 /*
  * Lookup the proper value and group tags for this option...
  */

  key.name = name;

  return ((_ipp_option_t *)bsearch(&key, ipp_options,
                                   sizeof(ipp_options) / sizeof(ipp_options[0]),
				   sizeof(ipp_options[0]),
				   (int (*)(const void *, const void *))
				       compare_ipp_options));
}


/*
 * 'compare_ipp_options()' - Compare two IPP options.
 */

static int				/* O - Result of comparison */
compare_ipp_options(_ipp_option_t *a,	/* I - First option */
                    _ipp_option_t *b)	/* I - Second option */
{
  return (strcmp(a->name, b->name));
}


/*
 * End of "$Id: encode.c 9258 2010-08-13 01:34:04Z mike $".
 */
