#    Copyright (C) 2001 Artifex Software Inc.
# 
# This software is provided AS-IS with no warranty, either express or
# implied.
# 
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
# 
# For more information about licensing, please refer to
# http://www.ghostscript.com/licensing/. For information on
# commercial licensing, go to http://www.artifex.com/licensing/ or
# contact Artifex Software, Inc., 101 Lucas Valley Road #110,
# San Rafael, CA  94903, U.S.A., +1(415)492-9861.

# $Id$

# gstestgs.py
#
# base classes for regression testing

import os
import string
import gsconf
from gstestutils import GSTestCase

class Ghostscript:
	def __init__(self):
		self.command = '/usr/bin/gs'
		self.dpi = 72
		self.band = 0
		self.device = ''
		self.infile = 'input'
		if os.name == 'nt':
			self.nullfile = 'nul'
		else:
			self.nullfile = '/dev/null'
		self.outfile = self.nullfile
		self.gsoptions = ''

		# log file options
		# NOTE: we always append to the log.  if it is desired to start a new
		# log, it is the responsibility of the caller to clear/erase the old
		# one.
		self.log_stdout = self.nullfile
		self.log_stderr = self.nullfile

	def process(self):
		bandsize = 30000000
		if (self.band): bandsize = 10000
		
		cmd = self.command
		cmd = cmd + ' ' + self.gsoptions
		cmd = cmd + ' -dQUIET -dNOPAUSE -dBATCH -K600000 '
		if self.dpi:
			cmd = cmd + '-r%d ' % (self.dpi,)
		cmd = cmd + '-dMaxBitmap=%d ' % (bandsize,)
		cmd = cmd + '-sDEVICE=%s ' % (self.device,)
		cmd = cmd + '-sOutputFile=%s ' % (self.outfile,)

		cmd = cmd + ' -c false 0 startjob pop '

		if string.lower(self.infile[-4:]) == ".pdf":
			cmd = cmd + ' -dFirstPage=1 -dLastPage=1 '
		else:
			cmd = cmd + '- < '

		cmd = cmd + ' %s >> %s 2>> %s' % (self.infile, self.log_stdout, self.log_stderr)


		# before we execute the command which might append to the log
		# we output a short header to show the commandline that generates
		# the log entry.
		if len(self.log_stdout) > 0 and self.log_stdout != self.nullfile:
			try:
				log = open(self.log_stdout, "a")
				log.write("===\n%s\n---\n" % (cmd,))
				log.close()
			except:
				pass
		if len(self.log_stderr) > 0 and self.log_stderr != self.nullfile:
			try:
				log = open(self.log_stderr, "a")
				log.write("===\n%s\n---\n" % (cmd,))
				log.close()
			except:
				pass
			

		ret = os.system(cmd)

		if ret == 0:
			return 1
		else:
			return 0

		
class GhostscriptTestCase(GSTestCase):
	def __init__(self, gs='gs', dpi=72, band=0, file='test.ps', device='pdfwrite', gsoptions='', log_stdout='', log_stderr='', track_daily=0):
		self.gs = gs
		self.file = file
		self.dpi = dpi
		self.band = band
		self.device = device
		self.gsoptions = gsoptions
		self.log_stdout = log_stdout
		self.log_stderr = log_stderr
		self.track_daily = track_daily
		GSTestCase.__init__(self)


class GSCrashTestCase(GhostscriptTestCase):
	def runTest(self):
		gs = Ghostscript()
		gs.command = self.gs
		gs.dpi = self.dpi
		gs.band = self.band
		gs.device = self.device
		gs.infile = self.file
		gs.gsoptions = self.gsoptions

		self.assert_(gs.process(), 'ghostscript failed to render file: ' + self.file)

class GSCompareTestCase(GhostscriptTestCase):
	def shortDescription(self):
		file = "%s.%s.%d.%d" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi, self.band)

		if self.band:
			return "Checking %s (%s/%ddpi/banded) against baseline" % (os.path.basename(self.file), self.device, self.dpi)
		else:
			return "Checking %s (%s/%ddpi/noband) against baseline" % (os.path.basename(self.file), self.device, self.dpi)

	def runTest(self):
		import string, os, gssum
		
		file = "%s.%s.%d.%d" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi, self.band)

		gs = Ghostscript()
		gs.command = self.gs
		gs.device = self.device
		gs.dpi = self.dpi
		gs.band = self.band
		gs.infile = self.file
		gs.outfile = file
		gs.gsoptions = self.gsoptions
		if self.log_stdout:
			gs.log_stdout = self.log_stdout
		if self.log_stderr:
			gs.log_stderr = self.log_stderr

		if gs.process():
			sum = gssum.make_sum(file)
		else:
			sum = ''
		os.unlink(file)

		# add test result to daily database
		if self.track_daily:
			gssum.add_file(file, dbname=gsconf.dailydb, sum=sum)

		if not sum:
			self.fail("output file could not be created"\
				  "for file: " + self.file)
		else:
			self.assertEqual(sum, gssum.get_sum(file),
					 'md5sum did not match baseline (' +
					 file + ') for file: ' + self.file)
