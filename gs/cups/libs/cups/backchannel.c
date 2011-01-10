/*
 * "$Id: backchannel.c 8635 2009-05-14 21:18:35Z mike $"
 *
 *   Backchannel functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
 *   cupsBackChannelRead()  - Read data from the backchannel.
 *   cupsBackChannelWrite() - Write data to the backchannel.
 *   cups_setup()           - Setup select() 
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include <errno.h>
#ifdef WIN32
#  include <io.h>
#  include <fcntl.h>
#else
#  include <sys/time.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

static void	cups_setup(fd_set *set, struct timeval *tval,
		           double timeout);


/*
 * 'cupsBackChannelRead()' - Read data from the backchannel.
 *
 * Reads up to "bytes" bytes from the backchannel/backend. The "timeout"
 * parameter controls how many seconds to wait for the data - use 0.0 to
 * return immediately if there is no data, -1.0 to wait for data indefinitely.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ssize_t					/* O - Bytes read or -1 on error */
cupsBackChannelRead(char   *buffer,	/* I - Buffer to read into */
                    size_t bytes,	/* I - Bytes to read */
		    double timeout)	/* I - Timeout in seconds, typically 0.0 to poll */
{
  fd_set	input;			/* Input set */
  struct timeval tval;			/* Timeout value */
  int		status;			/* Select status */


 /*
  * Wait for input ready.
  */

  do
  {
    cups_setup(&input, &tval, timeout);

    if (timeout < 0.0)
      status = select(4, &input, NULL, NULL, NULL);
    else
      status = select(4, &input, NULL, NULL, &tval);
  }
  while (status < 0 && errno != EINTR && errno != EAGAIN);

  if (status < 0)
    return (-1);			/* Timeout! */

 /*
  * Read bytes from the pipe...
  */

#ifdef WIN32
  return ((ssize_t)_read(3, buffer, (unsigned)bytes));
#else
  return (read(3, buffer, bytes));
#endif /* WIN32 */
}


/*
 * 'cupsBackChannelWrite()' - Write data to the backchannel.
 *
 * Writes "bytes" bytes to the backchannel/filter. The "timeout" parameter
 * controls how many seconds to wait for the data to be written - use
 * 0.0 to return immediately if the data cannot be written, -1.0 to wait
 * indefinitely.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ssize_t					/* O - Bytes written or -1 on error */
cupsBackChannelWrite(
    const char *buffer,			/* I - Buffer to write */
    size_t     bytes,			/* I - Bytes to write */
    double     timeout)			/* I - Timeout in seconds, typically 1.0 */
{
  fd_set	output;			/* Output set */
  struct timeval tval;			/* Timeout value */
  int		status;			/* Select status */
  ssize_t	count;			/* Current bytes */
  size_t	total;			/* Total bytes */


 /*
  * Write all bytes...
  */

  total = 0;

  while (total < bytes)
  {
   /*
    * Wait for write-ready...
    */

    do
    {
      cups_setup(&output, &tval, timeout);

      if (timeout < 0.0)
	status = select(4, NULL, &output, NULL, NULL);
      else
	status = select(4, NULL, &output, NULL, &tval);
    }
    while (status < 0 && errno != EINTR && errno != EAGAIN);

    if (status <= 0)
      return (-1);			/* Timeout! */

   /*
    * Write bytes to the pipe...
    */

#ifdef WIN32
    count = (ssize_t)_write(3, buffer, (unsigned)(bytes - total));
#else
    count = write(3, buffer, bytes - total);
#endif /* WIN32 */

    if (count < 0)
    {
     /*
      * Write error - abort on fatal errors...
      */

      if (errno != EINTR && errno != EAGAIN)
        return (-1);
    }
    else
    {
     /*
      * Write succeeded, update buffer pointer and total count...
      */

      buffer += count;
      total  += count;
    }
  }

  return ((ssize_t)bytes);
}


/*
 * 'cups_setup()' - Setup select() 
 */

static void
cups_setup(fd_set         *set,		/* I - Set for select() */
           struct timeval *tval,	/* I - Timer value */
	   double         timeout)	/* I - Timeout in seconds */
{
  tval->tv_sec = (int)timeout;
  tval->tv_usec = (int)(1000000.0 * (timeout - tval->tv_sec));

  FD_ZERO(set);
  FD_SET(3, set);
}


/*
 * End of "$Id: backchannel.c 8635 2009-05-14 21:18:35Z mike $".
 */
