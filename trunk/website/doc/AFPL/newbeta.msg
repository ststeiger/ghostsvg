Dear Ghostscript tester,

Thank you for joining the pre-release test program.  Please note the
following for your future reference, since most of it will not be repeated
in every fileset distribution announcement.

There are currently two distribution points for test filesets:
		ftp://tester:eboda@ftp.cs.wisc.edu/gs/
			(host: ftp.cs.wisc.edu = 128.105.2.15;
			 user name: tester;
			 password: eboda ("Adobe" spelled backwards))
		ftp://gs-beta:eboda@ftp.mathematik.uni-bielefeld.de/gs/
			(host: ftp.mathematik.uni-bielefeld.de = 129.70.14.50;
			 user name: gs-beta;
			 password: eboda)

You will be notified by e-mail when new filesets are available, but we
suggest you check this directory now, since it contains, among other things,
a set of test files (gs-tests.tar.gz) that are not part of the public
distribution.

Most pre-release filesets are complete filesets, but some are incremental.
You will need to fetch the most recent complete fileset, and all more recent
incremental ones.  Incremental filesets are .tar.gz archive files with the
word "new" or "nw#" in their names.  For example, suppose the following
files appear on the server:

-r--r--r--   1 ghost    root      2575055 Sep 21 15:02 ghostscript-5.04.tar.gz
-r--r--r--   1 ghost    root       319251 Sep 24 18:15 gs505new.tar.gz

You would need to fetch both of these files, and unpack the latter "on top
of" the former.

Please note that the filesets generally do not include the IJG JPEG library,
the PNG library, or the zlib library.  We occasionally distribute beta test
versions of these libraries from the Ghostscript tester directory, so you
should look for them there first.  The most recent released versions of
these libraries are available from the following places:
    IJG JPEG library:
	Standard distribution (tar + gzip format, Unix end-of-line):
		ftp.uu.net:/graphics/jpeg/jpegsrc.v*.tar.gz
	MS-DOS archive (PKZIP a.k.a. zip format, MS-DOS end-of-line):
		oak.oakland.edu:/SimTel/msdos/graphics/jpgsrc*.zip
    PNG library:
	ftp://swrinde.nde.swri.edu/pub/png/src/lpng*.zip
	    or libpng-*.tar.gz
	(zlib may be available here also)
    zlib:
	ftp://ftp.uu.net/pub/archiving/zip/zlib/zlib*.zip
	    or zlib-*.tar.gz

Possibly slightly out-of-date copies of these libraries are also available
from ftp.cs.wisc.edu, but you must log in as "anonymous" and get the
libraries from pub/ghost/.

Please use the standard bug report form (Bug-form.htm) for reporting
problems.  This helps reduce the amount of back-and-forth e-mail for
gathering the information I need to track the problem down.

I am mostly interested in problems you find in pre-release filesets.
However, if you find a problem in a public release, please try the same
input with the current test fileset before sending in a report.  If the
problem is still there, *or* if it has been less than 3 months since the
public release, please send in the bug report form and note which versions
(public and/or pre-release) have the problem.  If it has been more than 3
months since the public release *and* the problem is no longer present in
the pre-release code, don't bother to send in a report.

Please do your reasonable best to prevent pre-release files from being
copied around the universe.  This includes setting file protection to make
them not be public-readable.  I ask you to do this because the amount of
time and effort that it takes to do a real release (making and distributing
PC executables, making diffs, and especially handling bug reports) is much
larger than the effort required just to put the source code on the file
server.  Everyone has been very cooperative about this in the past, and it
has really helped me conserve my time for development.

Thanks again for your willingness to help improve Ghostscript.

				Sincerely,
