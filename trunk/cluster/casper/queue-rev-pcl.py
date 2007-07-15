#!/usr/bin/env python

pclrepos = '/var/lib/svn-private/ghostpcl'
gsrepos = '/var/lib/svn/ghostscript'
queuedir = '/home/regression/tti/queue.pcl'

import os

def getrev(repos):
  rev = os.popen('svnlook youngest ' + repos).readline().strip()
  return rev

def makerev():
  pclrev = getrev(pclrepos)
  gsrev = getrev(gsrepos)
  return pclrev + '+' + gsrev

os.system('touch ' + os.path.join(queuedir, makerev()))
