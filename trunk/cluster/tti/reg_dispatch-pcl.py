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

def update(rev):
  'update the executable to revision <rev>'
  workdir = "gs -r" + rev
  svn = os.system("cd " + workdir + " && svn up -r" + rev)
  make = os.system("cd " + workdir +" && make clean && nice make debug")

def getrev(cachedir="queue.pcl"):
  'Read the queue directory and select a revision to test'
  if not os.path.exists(cachedir): os.mkdir(cachedir)
  revs = os.listdir(cachedir)
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
    cmd += ' ghostpcl-r' + rev
  cmd += ' (xefitra)" '
  #cmd += 'giles@ghostscript.com'
  cmd += 'gs-regression@ghostscript.com'
  os.system(cmd)

def ircfile(file, rev=None):
  'Notify CIA and thus irc'
  msg = ''.join(open(file).readlines())
  if msg:
    try:
      ciatest.Message(msg, rev=rev, module='ghostpcl').send()
    except:
      pass

def choosecluster():
  '''Decide how many nodes of which cluster to run on.
     returns a (cluster_name, node_count) tuple.'''
  # figure out how many nodes are free
  r = re.compile('^\s+(?P<cluster>\w+).*\s+(?P<procs>\d+)\s+(?P<free>\d+)\s*$')
  clusters=[]
  nodes = 0
  cluster = None
  upnodes = os.popen("upnodes")
  for line in upnodes.readlines():
    m = r.match(line)
    if m: 
      name = m.group("cluster")
      procs = int(m.group("procs"))
      free = int(m.group("free"))
      # remember the cluster with the most free nodes
      if free > nodes and name != 'total': 
        nodes = free
        cluster = name
      clusters.append((name,procs,free))
  return (cluster, nodes)

def usage(name=sys.argv[0]):
  print "Usage: %s <revision>" % name
  print "launch a regression run on tticluster.com"
  print "testing gs svn rev <revision> against the default baseline"

def log(msg):
  '''print a timestamped log message. We use this for major tasks,
  and a normal print command for progress and error messages.'''
  print '[' + time.ctime() + '] ' + msg

def pbsjob(cmd, resources=None, stdout=None, stderr=None, mpi=True):
  if not resources:
    cluster, nodes = choosecluster()
    if nodes > 1 and cluster == 'red' or cluster == 'green':
      # red reports two cpus per node
      nodes /= 2
      ppn = ':ppn=2'
    else:
      ppn = ''
    resources = 'nodes=%d:%s:run%s,walltime=20:00' % (nodes, cluster, ppn)
    print 'requesting', nodes, 'nodes on', cluster
  f = open('regress.pbs', 'w')
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
  os.system('qsub regress.pbs')

def build(workdir=None, clean=False):
  'compile an executable from the current source'
  if clean:
    cmd = "make clean && nice ./autogen.sh && nice make"
  else:
    cmd = "nice make"
  if workdir:
    cmd = "cd " + workdir + " && " + cmd
  if False:
    # build on the dispatch host
    make = os.system(cmd)
    make = make >> 8
  else:
    # FIXME: alternate build on a compile node
    report = 'update.log'
    resources = 'nodes=1:compile'
    cmd += "\nexit"
    if os.path.exists(report): os.unlink(report)
    make = pbsjob(cmd, resources, stdout=report, mpi=False)
    while not os.path.exists(report): time.sleep(5)
  if make:
    log("build failed! exit code " + str(make))
    return False
  # update successful
  return True


def mainloop():
  log("starting up")
  doing = True
  while True:
    rev = getrev()
    if rev:
      doing = True
      workdir = "ghostpcl-r" + rev
      # create a working copy if necessary
      if not os.path.exists(workdir):
        print "couldn't find requested working copy '%s'\n" % workdir
        continue
      log("building " + workdir)
      build(workdir)
      log("build complete")
      if not os.path.exists(os.path.join(workdir, "reg_baseline.txt")):
        os.system("cp reg_baseline.txt " + workdir)
      log("running regression on ghostpcl-r" + rev)
      start = time.time()
      report = "regression-r" + rev + ".log"
      # remove the report if it exists since we use this to check completion
      if os.path.exists(report): os.unlink(report)
      f = open('regress.pbs', 'w')
      cluster, nodes = choosecluster()
      if nodes > 1 and (cluster == 'red' or cluster == 'green'):
        # red reports two cpus per node
        nodes /= 2
        ppn = ':ppn=2'
      else:
        ppn = ''
      f.write('#PBS -l nodes=%s:%s:run%s,walltime=20:00' % 
        (nodes, cluster, ppn))
      f.write(' -o ' + report)
      #f.write(' -j oe')
      #f.write(' -e /dev/null')
      f.write(' -e ' + report + '.err')
      f.write(' -d ' + os.path.join(os.getcwd(), workdir))
      f.write('\n\n')
      f.write('mpiexec -comm mpich2-pmi ')
      f.write(' -nostdin -kill -nostdout')
      f.write(' bwpython ../regress.py')
      f.write(' --batch --update')
      f.write(' --exe main/obj/pcl6')
      f.write('\n')
      f.close()
      print 'requesting', nodes, 'nodes on', cluster
      os.system('qsub regress.pbs')
      # wait for the run to finish
      while not os.path.exists(report):
        time.sleep(20)
      print "report is ready as '" + report + "'. total time %d seconds" % int(time.time() - start)
      ircfile(report, rev)
      #mailfile(report, rev)
      os.system("cp " + os.path.join(workdir, "reg_baseline.txt ") + " .")
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
  else:
    # don't run as a daemon by default
    mainloop()
