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
  svn1 = os.system("svn up --ignore-externals -r" + rev)
  svn2 = os.system("svn up -r" + rev + " gs")
  if svn1 or svn1:
    log("SVN update failed!")
    return False
  return True

def build(clean=False):
  'compile the exectubles from the current source'
  if clean:
    cmd = "make clean && make"
  else:
    cmd = "make"
  if False:
    # build on the dispatch host
    make = os.system(cmd)
  else:
    # build on a compile node
    resources = 'nodes=1:build32'
    report = 'update.log'
    if os.path.exists(report): os.unlink(report)
    make = pbsjob(cmd, resources, stdout=report, stderr=report, mpi=False)
    while not os.path.exists(report):
      time.sleep(5)
  if make:
    log('build failed! exit code ' + str(make))
    return False
  # update successful
  return True

def getrev(cachedir="../queue.pdl"):
  'Read the queue directory and select a revision to test'
  if not os.path.exists(cachedir): os.mkdir(cachedir)
  revs = os.listdir(cachedir)
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
    cmd += ' ghostpdl-r' + rev
  cmd += ' (xefitra)" '
  #cmd += 'giles@ghostscript.com'
  cmd += 'gs-regression@ghostscript.com'
  os.system(cmd)

def ircfile(file, rev=None):
  'Notify CIA and thus irc'
  msg = ''.join(open(file).readlines())
  if msg:
    try:
      ciatest.Message(msg, rev=rev, module='ghostpdl').send()
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
      if free > nodes and not name in ('orange', 'green', 'total'): 
        nodes = free
        cluster = name
      clusters.append((name,procs,free))
  return (cluster, nodes)

def usage(name=sys.argv[0]):
  print "Usage: %s <revision>" % name
  print "launch a regression run on tticluster.com"
  print "testing ghostpdl svn rev <revision> against the default baseline"

def log(msg):
  '''print a timestamped log message. We use this for major tasks,
  and a normal print command for progress and error messages.'''
  print '[' + time.ctime() + '] ' + msg

def pbsjob(cmd, resources=None, workdir=None, 
	stdout=None, stderr=None, mpi=True):
  if not resources:
    cluster, nodes = choosecluster()
    while nodes < 2 or cluster == None:
	log('clusters busy, waiting for an opening')
	time.sleep(300)
	cluster, nodes = choosecluster()
    if nodes > 3 and cluster == 'red' or cluster == 'green':
      # request two cpus per node
      nodes /= 2
      ppn = ':ppn=2'
      # hack around an edge case
      nodes -= 1
    else:
      ppn = ''
    if nodes > 24:
      nodes = 24
    resources = 'nodes=%d:%s:run%s,walltime=1:00:00,cput=25000' % \
	(nodes, cluster, ppn)
    if ppn:
      ppnhelp = '(' + ppn[-1] + ' cpus per node)'
    else:
      ppnhelp = ''
    print 'requesting', nodes, 'nodes on', cluster, ppnhelp
  if stdout: filename = stdout + '.pbs'
  else: filename = 'regress.pbs'
  f = open(filename, 'w')
  f.write('#PBS -l ' + resources)
  if stdout:
    f.write(' -o ' + stdout)
    if stdout == stderr:
      f.write(' -j oe')
    elif stderr:
      f.write(' -e ' + stderr)
  if workdir:
    f.write(' -d ' + workdir)
  else:
    f.write(' -d ' + os.getcwd())
  f.write('\n\n')
  if mpi:
    f.write('mpiexec -comm mpich2-pmi ')
    f.write(' -nostdin -kill -nostdout')
    f.write(' ')
  f.write(cmd)
  f.write('\n')
  f.close()
  while True:
    log('qsub ' + filename)
    S = os.system('qsub ' + filename)
    log('qsub returned status: ' + repr(S))
    if S == 0: break
    time.sleep(100)

def runrev(workdir=None, rev=None, report=None, exe='main/obj/pcl6'):
  if not rev: rev = getrev()
  if not report: report = "regression-r" + rev + ".log"
  # remove the report if it exists since we use this to check completion
  if os.path.exists(report): os.unlink(report)
  start = time.time()
  cmd = 'bwpython ../regress.py'
  cmd += ' --batch --update'
  cmd += ' --exe ' + exe
  cmd += ' --device=ppmraw --device=pbmraw'
  if os.path.basename(exe) in ('pcl6', 'gxps'):
    cmd += ' --device=wtsimdi --device=bitrgb'
  pbsjob(cmd, resources=None, workdir=workdir, stdout=report)
  # wait for the run to finish
  while not os.path.exists(report):
    time.sleep(20)
  if os.path.getsize(report) < 1:
    f = open(report, "w")
    f.write("[report empty -- regression failed]\n")
    f.close()
  print "report is ready as '" + report + "'. total time %d seconds" % int(time.time() - start)
  ircfile(report, rev)
  mailfile(report, rev)

def mainloop():
  log("starting up")
  doing = True
  while True:
    rev = getrev()
    if rev:
      doing = True
      workdir = "."
      # create a working copy if necessary
      if not os.path.exists(workdir):
        print "couldn't find requested working copy '%s'\n" % workdir
        continue
      update(rev)
      log("building " + workdir)
      build(workdir)
      log("build complete")
      exes = {'ghostpcl':'main/obj/pcl6','ghostxps':'xps/obj/gxps',
	'ghostsvg':'svg/obj/gsvg'}
      for key in exes.keys():
        if os.path.exists(os.path.join(workdir, exes[key])):
          log("running regression on %s-r%s" % (key, rev))
          report = key + "-r" + rev + ".log"
          runrev(workdir, rev, report, exes[key])
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
