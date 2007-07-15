#!/usr/bin/env python

# test script for submitting messages to CIA

server = 'http://cia.navi.cx'
project = 'ghostscript'
module = 'gs'

import xmlrpclib
import time

class Message:
  def __init__(self, log='', rev=''):
    self.log = log
    self.rev = rev
    self.server = server
    self.project = project
    self.module = module
    self.name = 'manual test'
    self.version = '0.1'
  def gen(self):
    xml = ' <generator>\n'
    xml += '  <name>' + str(self.name) + '</name>\n'
    xml += '  <version>' + str(self.version) + '</version>\n'
    xml += ' </generator>\n'
    return xml
  def source(self):
    xml = ' <source>\n'
    xml += '  <project>' + str(self.project) + '</project>\n'
    xml += '  <module>' + str(self.module) + '</module>\n'
    xml += ' </source>\n'
    return xml
  def message(self):
    xml = '<message>\n'
    xml += self.gen()
    xml += self.source()
    xml += ' <body>\n'
    xml += '  <commit>\n'
    if self.rev:
      xml += '   <revision>' + self.rev + '</revision>\n'
    xml += '   <author>regression</author>\n'
    xml += '   <log>' + str(self.log) + '</log>\n'
    xml += '  </commit>\n'
    xml += ' </body>\n'
    xml += ' <timestamp>' + str(int(time.time())) + '</timestamp>\n'
    xml += '</message>\n'
    return xml
  def send(self, server = None):
    if not server: server = self.server
    xmlrpclib.ServerProxy(server).hub.deliver(self.message())
  def __str__(self):
    return self.message()

def irc_report(filename, rev=''):
  file = open(filename)
  msg = ''.join(file.readlines())
  m = Message(msg, rev)
  m.send()

if __name__ == '__main__':
  filename = 'regression-r7832.log'
  print 'reporting results from \'%s\' to irc' % filename
  irc_report(filename, '7832')
