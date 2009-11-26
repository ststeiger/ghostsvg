#!/bin/tcsh

set machine=REPLACE_WITH_YOUR_MACHINE_NAME


ssh -i ~/.ssh/cluster_key marcos@casper.ghostscript.com touch /home/marcos/cluster/$machine.up

if (-e $machine.start) then
else

touch $machine.start
rm -f $machine.start

scp -i ~/.ssh/cluster_key  -q marcos@casper.ghostscript.com:/home/marcos/cluster/$machine.start . >&/dev/null

if (-e $machine.start) then
  ssh -i ~/.ssh/cluster_key marcos@casper.ghostscript.com rm /home/marcos/cluster/$machine.start

  scp -i ~/.ssh/cluster_key -q marcos@casper.ghostscript.com:/home/marcos/cluster/$machine.jobs.gz . >&/dev/null
  ssh -i ~/.ssh/cluster_key marcos@casper.ghostscript.com rm /home/marcos/cluster/$machine.jobs.gz

  scp -i ~/.ssh/cluster_key -q marcos@casper.ghostscript.com:/home/marcos/cluster/run.pl . >&/dev/null
  chmod +x run.pl

  touch $machine.jobs
  rm $machine.jobs
  gunzip $machine.jobs.gz

  perl run.pl $machine >&$machine.out


endif
endif
