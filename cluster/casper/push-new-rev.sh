#!/bin/sh

# run me to request a cluster regression for an svn rev.
rev=$1

SSH_ID=/home/regression/.ssh/ttitunnel
SSH_HOST=atfxsw01@tticluster.com

ssh -i $SSH_ID $SSH_HOST touch regression/rev.queue/$1
# todo: check for failure, this doesn't work
if ! test $?; then
  echo "regression request submitted to the cluster"
fi
