/*
 * "$Id: string.h 9261 2010-08-13 21:54:11Z mike $"
 *
 *   String definitions for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_STRING_H_
#  define _CUPS_STRING_H_

/*
 * Include necessary headers...
 */

#  include <stdio.h>
#  include <stdlib.h>
#  include <stdarg.h>
#  include <ctype.h>
#  include <locale.h>
#  include <errno.h>

#  include <config.h>

#  ifdef HAVE_STRING_H
#    include <string.h>
#  endif /* HAVE_STRING_H */

#  ifdef HAVE_STRINGS_H
#    include <strings.h>
#  endif /* HAVE_STRINGS_H */

#  ifdef HAVE_BSTRING_H
#    include <bstring.h>
#  endif /* HAVE_BSTRING_H */


/*
 * Stuff for WIN32 and OS/2...
 */

#  if defined(WIN32) || defined(__EMX__)
#    define strcasecmp	_stricmp
#    define strncasecmp	_strnicmp
#  endif /* WIN32 || __EMX__ */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * String pool structures...
 */

#  define _CUPS_STR_GUARD	0x12344321

typedef struct _cups_sp_item_s		/**** String Pool Item ****/
{
#  ifdef DEBUG_GUARDS
  unsigned int	guard;			/* Guard word */
#  endif /* DEBUG_GUARDS */
  unsigned int	ref_count;		/* Reference count */
  char		str[1];			/* String */
} _cups_sp_item_t;


/*
 * Replacements for the ctype macros that are not affected by locale, since we
 * really only care about testing for ASCII characters when parsing files, etc.
 * These are used only within libcups since the rest of CUPS doesn't call
 * setlocale() for LC_CTYPE and doesn't have to worry about third-party
 * libraries doing so (and if they do that is a bug: NetSNMP, I'm looking at
 * you!)
 *
 * The _CUPS_INLINE definition controls whether we get an inline function body,
 * and external function body, or an external definition.
 */

#  if defined(__GNUC__) || __STDC_VERSION__ >= 199901L
#    define _CUPS_INLINE static inline
#  elif defined(_MSC_VER)
#    define _CUPS_INLINE static __inline
#  elif defined(_CUPS_STRING_C_)
#    define _CUPS_INLINE
#  endif /* __GNUC__ || __STDC_VERSION__ */

#  ifdef _CUPS_INLINE
_CUPS_INLINE int			/* O - 1 on match, 0 otherwise */
_cups_isalnum(int ch)			/* I - Character to test */
{
  return ((ch >= '0' && ch <= '9') ||
          (ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z'));
}

_CUPS_INLINE int			/* O - 1 on match, 0 otherwise */
_cups_isalpha(int ch)			/* I - Character to test */
{
  return ((ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z'));
}

_CUPS_INLINE int			/* O - 1 on match, 0 otherwise */
_cups_isspace(int ch)			/* I - Character to test */
{
  return (ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' ||
          ch == '\v');
}

_CUPS_INLINE int			/* O - 1 on match, 0 otherwise */
_cups_isupper(int ch)			/* I - Character to test */
{
  return (ch >= 'A' && ch <= 'Z');
}
#  else
extern int _cups_isalnum(int ch);
extern int _cups_isalpha(int ch);
extern int _cups_isspace(int ch);
extern int _cups_isupper(int ch);
#  endif /* _CUPS_INLINE */


/*
 * Prototypes...
 */

extern void	_cups_strcpy(char *dst, const char *src);

#  ifndef HAVE_STRDUP
extern char	*_cups_strdup(const char *);
#    define strdup _cups_strdup
#  endif /* !HAVE_STRDUP */

#  ifndef HAVE_STRCASECMP
extern int	_cups_strcasecmp(const char *, const char *);
#    define strcasecmp _cups_strcasecmp
#  endif /* !HAVE_STRCASECMP */

#  ifndef HAVE_STRNCASECMP
extern int	_cups_strncasecmp(const char *, const char *, size_t n);
#    define strncasecmp _cups_strncasecmp
#  endif /* !HAVE_STRNCASECMP */

#  ifndef HAVE_STRLCAT
extern size_t _cups_strlcat(char *, const char *, size_t);
#    define strlcat _cups_strlcat
#  endif /* !HAVE_STRLCAT */

#  ifndef HAVE_STRLCPY
extern size_t _cups_strlcpy(char *, const char *, size_t);
#    define strlcpy _cups_strlcpy
#  endif /* !HAVE_STRLCPY */

#  ifndef HAVE_SNPRINTF
extern int	_cups_snprintf(char *, size_t, const char *, ...)
#    ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 3, 4)))
#    endif /* __GNUC__ */
;
#    define snprintf _cups_snprintf
#  endif /* !HAVE_SNPRINTF */

#  ifndef HAVE_VSNPRINTF
extern int	_cups_vsnprintf(char *, size_t, const char *, va_list);
#    define vsnprintf _cups_vsnprintf
#  endif /* !HAVE_VSNPRINTF */

/*
 * String pool functions...
 */

extern char	*_cupsStrAlloc(const char *s);
extern void	_cupsStrFlush(void);
extern void	_cupsStrFree(const char *s);
extern char	*_cupsStrRetain(const char *s);
extern size_t	_cupsStrStatistics(size_t *alloc_bytes, size_t *total_bytes);


/*
 * Floating point number functions...
 */

extern char	*_cupsStrFormatd(char *buf, char *bufend, double number,
		                 struct lconv *loc);
extern double	_cupsStrScand(const char *buf, char **bufptr,
		              struct lconv *loc);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_STRING_H_ */

/*
 * End of "$Id: string.h 9261 2010-08-13 21:54:11Z mike $".
 */
