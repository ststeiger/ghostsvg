/* This file is part of Libspectre.
 * 
 * Copyright (C) 2007 Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Libspectre is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * Libspectre is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spectre-utils.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

static unsigned long
_spectre_get_pid (void)
{
#if defined(HAVE_SYS_TYPES_H) && defined(HAVE_UNISTD_H)
	return getpid ();
#elif defined(WIN32)
	//return GetCurrentProcessId ();
#endif
}

static int warn_initted = FALSE;
static int fatal_warnings = FALSE;
static int fatal_warnings_on_check_failed = FALSE;

static void
init_warnings (void)
{
	const char *s;
	
	if (warn_initted)
		return;

	warn_initted = TRUE;
	
	s = getenv ("SPECTRE_FATAL_WARNINGS");
	if (!s || !(*s))
		return;

	if (*s == '0') {
		fatal_warnings = FALSE;
		fatal_warnings_on_check_failed = FALSE;
	} else if (*s == '1') {
		fatal_warnings = TRUE;
		fatal_warnings_on_check_failed = TRUE;
	} else {
		fprintf (stderr,
			 "SPECTRE_FATAL_WARNINGS should be set to 0 or 1 if set, not '%s'",
			 s);
	}
}

/**
 * Prints a warning message to stderr. Can optionally be made to exit
 * fatally by setting SPECTRE_FATAL_WARNINGS, but this is rarely
 * used. This function should be considered pretty much equivalent to
 * fprintf(stderr). _spectre_warn_check_failed() on the other hand is
 * suitable for use when a programming mistake has been made.
 */
void
_spectre_warn (const char *format,
            ...)
{
	va_list args;

	if (!warn_initted)
		init_warnings ();
  
	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);

	if (fatal_warnings) {
		fflush (stderr);
		abort ();
	}
}

/**
 * Prints a "critical" warning to stderr when an assertion fails;
 * differs from _spectre_warn primarily in that it prefixes the pid and
 * defaults to fatal. This should be used only when a programming
 * error has been detected. (NOT for unavoidable errors that an app
 * might handle. Calling this means "there is a bug"
 */
void
_spectre_warn_check_failed (const char *format,
			    ...)
{
	va_list args;

	if (!warn_initted)
		init_warnings ();

	fprintf (stderr, "process %lu: ", _spectre_get_pid ());

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);

	if (fatal_warnings_on_check_failed) {
		fflush (stderr);
		abort ();
	}
}

#ifndef SPECTRE_DISABLE_ASSERT
void
_spectre_real_assert (int          condition,
		      const char  *condition_text,
		      const char  *file,
		      int          line,
		      const char  *func)
{
	if (_SPECTRE_UNLIKELY (!condition)) {
		_spectre_warn ("%lu: assertion failed \"%s\" file \"%s\" line %d function %s\n",
			       _spectre_get_pid (), condition_text, file, line, func);
		abort ();
	}
}
#endif /* SPECTRE_DISABLE_ASSERT */

static char *
spectre_strdup_vprintf (const char *format,
			va_list     args)
{
	char *string = NULL;
	int len;
#if defined(HAVE_VASPRINTF)
	len = vasprintf (&string, format, args);
	
	if (len < 0)
		string = NULL;
#else /* !HAVE_VASPRINTF */
	va_list args_copy;
	char c;

	SPECTRE_VA_COPY (args_copy, args);

	string = malloc ((vsnprintf (&c, 1, format, args) + 1) * sizeof (char));
	if (string) {
		len = vsprintf (string, format, args_copy);
		if (len < 0) {
			free (string);
			string = NULL;
		}
	}

	va_end (args_copy);
#endif	
	
	return string;
}

char *
_spectre_strdup_printf (const char *format, ...)
{
	char *buffer;
	va_list args;
	
	va_start (args, format);
	buffer = spectre_strdup_vprintf (format, args);
	va_end (args);
	
	return buffer;
}

char *
_spectre_strdup (const char *str)
{
	size_t len;
	char *copy;

	if (!str)
		return NULL;

	len = strlen (str) + 1;

	copy = malloc (len);
	if (!copy)
		return NULL;

	memcpy (copy, str, len);

	return copy; 
}

#define TOLOWER(c) (((c) >= 'A' && (c) <= 'Z') ? (c) - 'A' + 'a' : (c))
int
_spectre_strncasecmp (const char *s1,
		      const char *s2,
		      size_t      n)
{
	int c1, c2;

	while (n && *s1 && *s2)	{
		n -= 1;
		c1 = (int)(unsigned char) TOLOWER (*s1);
		c2 = (int)(unsigned char) TOLOWER (*s2);
		if (c1 != c2)
			return (c1 - c2);
		s1++;
		s2++;
	}

	return (n) ? (((int) (unsigned char) *s1) - ((int) (unsigned char) *s2)) : 0;
}

int
_spectre_strcasecmp (const char *s1,
		     const char *s2)
{
	int c1, c2;

	while (*s1 && *s2) {
		c1 = (int)(unsigned char) TOLOWER (*s1);
		c2 = (int)(unsigned char) TOLOWER (*s2);
		if (c1 != c2)
			return (c1 - c2);
		s1++;
		s2++;
	}

	return (((int)(unsigned char) *s1) - ((int)(unsigned char) *s2));
}
