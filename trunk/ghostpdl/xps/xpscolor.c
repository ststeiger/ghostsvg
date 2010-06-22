/* Copyright (C) 2006-2010 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* XPS interpreter - color functions */

#include "ghostxps.h"

#include "stream.h" /* for sizeof(stream) to work */

void
xps_set_color(xps_context_t *ctx, gs_color_space *cs, float *samples)
{
    gs_client_color cc;
    int i, n;

    if (ctx->opacity_only)
    {
        gs_setopacityalpha(ctx->pgs, 1.0);
        gs_setgray(ctx->pgs, samples[0]);
    }
    else
    {
        n = cs_num_components(cs);
        cc.pattern = 0;
        for (i = 0; i < n; i++)
            cc.paint.values[i] = samples[i + 1];

        gs_setopacityalpha(ctx->pgs, samples[0]);
        gs_setcolorspace(ctx->pgs, cs);
        gs_setcolor(ctx->pgs, &cc);
    }
}

static int unhex(int chr)
{
    const char *hextable = "0123456789ABCDEF";
    return strchr(hextable, (toupper(chr))) - hextable;
}

static int count_commas(char *s)
{
    int n = 0;
    while (*s)
    {
        if (*s == ',')
            n ++;
        s ++;
    }
    return n;
}

void
xps_parse_color(xps_context_t *ctx, char *base_uri, char *string,
        gs_color_space **csp, float *samples)
{
    char *p;
    int i, n;
    char buf[1024];
    char *profile;

    samples[0] = 1.0;
    samples[1] = 0.0;
    samples[2] = 0.0;
    samples[3] = 0.0;

    *csp = ctx->srgb;

    if (string[0] == '#')
    {
        if (strlen(string) == 9)
        {
            samples[0] = unhex(string[1]) * 16 + unhex(string[2]);
            samples[1] = unhex(string[3]) * 16 + unhex(string[4]);
            samples[2] = unhex(string[5]) * 16 + unhex(string[6]);
            samples[3] = unhex(string[7]) * 16 + unhex(string[8]);
        }
        else
        {
            samples[0] = 255.0;
            samples[1] = unhex(string[1]) * 16 + unhex(string[2]);
            samples[2] = unhex(string[3]) * 16 + unhex(string[4]);
            samples[3] = unhex(string[5]) * 16 + unhex(string[6]);
        }

        samples[0] /= 255.0;
        samples[1] /= 255.0;
        samples[2] /= 255.0;
        samples[3] /= 255.0;
    }

    else if (string[0] == 's' && string[1] == 'c' && string[2] == '#')
    {
        if (count_commas(string) == 2)
            sscanf(string, "sc#%g,%g,%g", samples + 1, samples + 2, samples + 3);
        if (count_commas(string) == 3)
            sscanf(string, "sc#%g,%g,%g,%g", samples, samples + 1, samples + 2, samples + 3);
    }

    else if (strstr(string, "ContextColor ") == string)
    {
        /* Crack the string for profile name and sample values */
        strcpy(buf, string);

        profile = strchr(buf, ' ');
        if (!profile)
        {
            gs_warn1("cannot find icc profile uri in '%s'", string);
            return;
        }

        *profile++ = 0;
        p = strchr(profile, ' ');
        if (!p)
        {
            gs_warn1("cannot find component values in '%s'", profile);
            return;
        }

        *p++ = 0;
        n = count_commas(p) + 1;
        i = 0;
        while (i < n)
        {
            samples[i++] = atof(p);
            p = strchr(p, ',');
            if (!p)
                break;
            p ++;
            if (*p == ' ')
                p ++;
        }
        while (i < n)
        {
            samples[i++] = 0.0;
        }

        /* Default fallbacks if the ICC stuff fails */
        if (n == 2) /* alpha + tint */
            *csp = ctx->gray;

        if (n >= 5) /* alpha + CMYK */
            *csp = ctx->cmyk;

        xps_set_icc(ctx, base_uri, profile, csp);
    }
}

void
xps_set_icc(xps_context_t *ctx, char *base_uri, char *profile, gs_color_space **csp)
{
    cmm_profile_t *iccprofile;
    xps_part_t *part;
    char partname[1024];

    /* Find ICC colorspace part */
    xps_absolute_path(partname, base_uri, profile, sizeof partname);

    /* See if we cached the profile */
    iccprofile = (cmm_profile_t*) xps_hash_lookup(ctx->colorspace_table, partname);
    if (!iccprofile)
    {
        part = xps_read_part(ctx, partname);

        /* Problem finding profile.  Don't fail, just use default */
        if (!part) {
            gs_warn1("cannot find icc profile part: %s", partname);
            return;
        }

        /* Create the profile */
        iccprofile = gsicc_profile_new(NULL, ctx->memory, NULL, 0);

        /* Set buffer */
        iccprofile->buffer = part->data;
        iccprofile->buffer_size = part->size;

        /* Parse */
        gsicc_init_profile_info(iccprofile);

        /* Problem with profile.  Don't fail, just use the default */
        if (iccprofile->profile_handle == NULL)
        {
            gsicc_profile_reference(iccprofile, -1);
            gs_warn1("there was a problem with the profile: %s", partname);
            return;
        }

        /* Done with the buffer */
        xps_free_part(ctx, part);
        iccprofile->buffer = NULL;
        iccprofile->buffer_size = 0;

        /* Add profile to xps color cache. */
        xps_hash_insert(ctx, ctx->colorspace_table, xps_strdup(ctx, partname), iccprofile);
    }

    /* Associate icc color space with the profile */
    ctx->icc->cmm_icc_profile_data = iccprofile;
    *csp = ctx->icc;
}

int
xps_parse_solid_color_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node)
{
    char *opacity_att;
    char *color_att;
    gs_color_space *colorspace;
    float samples[32];

    color_att = xps_att(node, "Color");
    opacity_att = xps_att(node, "Opacity");

    colorspace = ctx->srgb;
    samples[0] = 1.0;
    samples[1] = 0.0;
    samples[2] = 0.0;
    samples[3] = 0.0;

    if (color_att)
        xps_parse_color(ctx, base_uri, color_att, &colorspace, samples);

    if (opacity_att)
        samples[0] = atof(opacity_att);

    xps_set_color(ctx, colorspace, samples);

    xps_fill(ctx);

    return 0;
}
