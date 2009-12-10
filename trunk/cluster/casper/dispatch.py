#!/usr/bin/env python

import os, sys, signal
import time

sleeptime = 30
queuedir = '/home/regression/tti/queue'
pclqueuedir = '/home/regression/tti/queue.pcl'
ssh_id = '/home/regression/.ssh/ttitunnel'
ssh_dest = 'atfxsw01@tticluster.com'

class Daemon:
  '''Class for forking off a daemon process.'''
  def __init__(self, main):
    self.main = main
    self.running = False
    self.pidfilename = "reg_dispatch.pid"
    self.logfilename = "reg_dispatch.log"
  def start(self, stdin=None, stdout=None, stderr=None):
    'Fork off a separate process, running our function.'

    # make sure we're not already running
    if os.path.exists(self.pidfilename):
      sys.stderr.write("daemon already running.\n")
      sys.stderr.write("stop it, or remove the stale pidfile ")
      sys.stderr.write(self.pidfilename + "\n")

    # fork from the parent process and return
    try:
      pid = os.fork()
      if pid > 0:
        # todo: grab the pid and return
        # after the daemon is running
        return True
    except OSError, e:
      sys.stderr.write("initial daemonization fork failed: (%d) %s\n" %
        (e.errno, e.strerror))
      return False

    # Decouple ourselves from the parent environment
    # normally we chdir("/") to avoid blocking the unmount of our
    # launch directory, but we can't do any work without it
    # and it makes things simpler to use the cwd
    #os.chdir("/")
    os.umask(0)
    os.setsid()

    # Do a second fork to avoid becoming a controlling terminal
    try:
      pid = os.fork()
      if pid > 0:
        # record the child's pid
        pidfile = open(self.pidfilename, "w")
        pidfile.write(str(pid) + "\n")
        pidfile.close()
        # exit the second parent
        sys.exit(0)
    except OSError, e:
      sys.stderr.write("second daemonization fork failed: (%d) %s\n" %
        (e.errno, e.strerror))
      sys.exit(1)

    # redirect standard file descriptors
    if not stdin: stdin="/dev/null"
    if not stdout: stdout = self.logfilename
    if not stderr: stderr = stdout
    si = file(stdin, 'r')
    so = file(stdout, 'a')
    se = file(stderr, 'a')
    os.dup2(si.fileno(), sys.stdin.fileno())
    os.dup2(so.fileno(), sys.stdout.fileno())
    os.dup2(se.fileno(), sys.stderr.fileno())

    # execute the reqested main function
    self.main()

  def stop(self):
    'Stop a running daemon.'
    pidfile = open(self.pidfilename, "r")
    pid = int(pidfile.readline())
    os.kill(pid, signal.SIGHUP)


def getrev(queue=queuedir):
  revs = os.listdir(queue)
  # ideally we'd sort by mtime, but alphabetical for now
  revs.sort()
  try:
    # call basename to strip hack attempts with relative paths
    rev = os.path.basename(revs[0])
  except IndexError:
    rev = None
  return rev

def mainloop():
  doing = True
  while True:
    # check for ghostscript runs
    rev = getrev(queuedir)
    pclrev = getrev(pclqueuedir)
    if rev:
      doing = True
      print 'submitting gs-r' + rev
      cmd = 'ssh -i ' + ssh_id + ' ' + ssh_dest + ' '
      cmd += 'touch ' + os.path.join('regression/queue.gs/', rev)
      os.system(cmd)
      print 'submitting ghostpdl-r' + rev
      cmd = 'ssh -i ' + ssh_id + ' ' + ssh_dest + ' '
      cmd += 'touch ' + os.path.join('regression/queue.pdl/', rev)
      os.system(cmd)
      os.unlink(os.path.join(queuedir,rev))
      continue
    elif pclrev:
      doing = True
      rev = pclrev
      print 'updating test suite to current HEAD'
      cmd = 'svn update tests_private'
      os.system(cmd)
      print 'pushing test suite update'
      cmd = 'rsync -avz --delete'
      cmd += ' --exclude .svn tests_private/*'
      cmd += ' ' + ssh_dest + ':tests_private/'
      os.system(cmd)
      os.unlink(os.path.join(pclqueuedir,rev))
    else:
      if doing:
        print '-- nothing to do --'
        doing = False
      time.sleep(sleeptime)


if __name__ == '__main__':
  daemon = Daemon(mainloop)
  result = daemon.start()
  if not result:
    print "couldn't start daemon!"
    sys.exit(1)

