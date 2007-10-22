#/usr/bin/env python

# regression test dispatch script
# we can be used to launch a parallel regression
# or in daemon mode to run regressions on specific revisions

import os, sys, signal
import re, time
import ciatest

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
    if pid:
      os.kill(pid, signal.SIGHUP)
    else:
      sys.stderr.write("no pidfile or daemon not running.\n")
    os.unlink(self.pidfilename)

# PBS job server utilities

def choosecluster():
  '''Decide how many nodes of which cluster to run on.
     returns a (cluster_name, node_count) tuple.'''
  # figure out how many nodes are free
  r = re.compile('^\s+(?P<cluster>\w+).*\s+(?P<procs>\d+)\s+(?P<free>\d+)\s*$')
  clusters = []
  cluster = None
  nodes = 0
  upnodes = os.popen("upnodes")
  for line in upnodes.readlines():
    m = r.match(line)
    if m: 
      name = m.group("cluster")
      procs = int(m.group("procs"))
      free = int(m.group("free"))
      # remember the cluster with the most free nodes
      if free > nodes and name != 'orange' and name != 'total': 
        nodes = free
        cluster = name
      clusters.append((name,procs,free))
  return (cluster, nodes)

def pbsjob(cmd, resources=None, stdout=None, stderr=None, mpi=True):
  if not resources:
    cluster, nodes = choosecluster()
    if nodes > 1 and cluster == 'red' or cluster == 'green':
      # red reports two cpus per node
      nodes /= 2
      ppn = ':ppn=2'
      # hack: work around a corner case
      if nodes > 1: nodes = nodes - 1
    else:
      ppn = ''
    resources = 'nodes=%d:%s:run%s,walltime=20:00' % (nodes, cluster, ppn)
    print 'requesting', nodes, 'nodes on', cluster
  if stdout: jobname = stdout + '.pbs'
  else: jobname = 'regress.pbs'
  f = open(jobname, 'w')
  f.write('#PBS -l ' + resources)
  if stdout:
    f.write(' -o ' + stdout)
    if stdout == stderr:
      f.write(' -j oe')
    elif stderr:
      f.write(' -e ' + stderr)
  f.write(' -d ' + os.getcwd())
  f.write('\n\n')
  if mpi:
    f.write('mpiexec -comm mpich2-pmi ')
    f.write(' -nostdin -kill -nostdout')
    f.write(' ')
  f.write(cmd)
  f.write('\n')
  f.close()
  os.system('qsub ' + jobname)


# regression setup and reporting

def update(rev):
  'update the source to revision <rev>'
  svn = os.system("svn up -r" + rev)
  if svn:
    log("SVN update failed!")
    return False
  return True

def build(clean=False):
  'compile an executable from the current source'
  if clean:
    cmd = "make clean && nice ./autogen.sh && nice make"
  else:
    cmd = "nice make"
  if False:
    # build on the dispatch host
    make = os.system(cmd)
    make = make >> 8
  else:
    # build on a compile node
    resources = 'nodes=1:build32'
    report = 'update.log'
    if os.path.exists(report): os.unlink(report)
    make = pbsjob(cmd, resources, stdout=report, stderr=report, mpi=False)
    while not os.path.exists(report):
      time.sleep(5)
  if make:
    log("build failed! exit code " + str(make))
    return False
  # update successful
  return True

def getrev(cachedir="../queue.gs"):
  'Read the queue directory and select a revision to test'
  revs = os.listdir(cachedir)
  # we would ideally sort by mtime, but for now just alphabetical
  revs.sort()
  try:
    rev = revs[0]
    os.unlink(os.path.join(cachedir, rev))
  except IndexError:
    rev = None
  return rev

def mailfile(file, rev=None):
  'Mail out the report'
  cmd = 'cat ' + file + ' '
  cmd += '| mail -s "cluster regression'
  if rev:
    cmd += ' gs-r' + rev
  cmd += ' (xefitra)" '
  #cmd += 'giles@ghostscript.com'
  cmd += 'gs-regression@ghostscript.com'
  os.system(cmd)

def irclog(msg, rev=None):
  'Notify CIA and thus irc of a message'
  if msg:
    try:
      ciatest.Message(msg, rev).send()
    except:
      # ignore errors, the server sometimes barfs
      pass

def ircfile(file, rev=None):
  'Send a result file to CIA and thus irc'
  msg = ''.join(open(file).readlines())
  irclog(msg, rev)

def usage(name=sys.argv[0]):
  print "Usage: %s <revision>" % name
  print "launch a regression run on tticluster.com"
  print "testing gs svn rev <revision> against the default baseline"

def log(msg):
  print '[' + time.ctime() + '] ' + msg

def runrev(rev=None, report=None):
  if not rev: rev = getrev()
  if not report: report = "regression-r" + rev + ".log"
  log("running regression on gs-r" + rev)
  start = time.time()
  # remove the report if it exists since we use this to check completion
  if os.path.exists(report): os.unlink(report)
  if not update(rev): irclog("SVN update failed!", rev)
  elif not build(clean=True): irclog("Build failed!", rev)
  else:
    cmd = 'bwpython ../regress.py --batch --update'
    pbsjob(cmd, resources=None, stdout=report)
    # wait for the run to finish
    while not os.path.exists(report):
      time.sleep(20)
    print "report is ready as '" + report + "'. total time %d seconds" % int(time.time() - start)
    mailfile(report, rev)
    ircfile(report, rev)

def mainloop():
  log("starting up")
  doing = True
  while True:
    rev = getrev()
    if rev:
      doing = True
      report = "regression-r" + rev + ".log"
      runrev(rev, report)
    else:
      if doing:
        print "-- nothing to do --"
        sys.stdout.flush()
        doing = False
      time.sleep(100)

if __name__ == '__main__':
  if len(sys.argv) > 1 and sys.argv[1] == '-d':
    daemon = Daemon(mainloop)
    if len(sys.argv) > 2 and sys.argv[2] == 'stop':
      result = daemon.stop()
    else:
      result = daemon.start()
      if not result:
        print "couldn't start daemon!"
        sys.exit(1)
  elif len(sys.argv) > 2 and sys.argv[1] == '-r':
    # run a specific revision and quit
    rev = sys.argv[2]
    runrev(rev)
  else:
    # run with queues, but on the console for debugging
    mainloop()
