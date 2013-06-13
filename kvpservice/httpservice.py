#!/usr/bin/env python

# Copyright (c) Citrix Systems Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, 
# with or without modification, are permitted provided 
# that the following conditions are met:
#
# *   Redistributions of source code must retain the above 
#     copyright notice, this list of conditions and the 
#     following disclaimer.
# *   Redistributions in binary form must reproduce the above 
#     copyright notice, this list of conditions and the 
#     following disclaimer in the documentation and/or other 
#     materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
# SUCH DAMAGE.

import os, sys, time, socket, traceback, syslog

log_f = os.fdopen(os.dup(sys.stdout.fileno()), "aw")
pid = None
use_syslog = False

def reopenlog(log_file):
    global log_f
    if log_f:
        log_f.close()
    if log_file and log_file <> "stdout:":
        log_f = open(log_file, "aw")
    elif log_file and log_file == "stdout:":
        log_f = os.fdopen(os.dup(sys.stdout.fileno()), "aw")

def log(txt):
    global log_f, pid, use_syslog
    if use_syslog:
        syslog.syslog(txt)
        return
    if not pid:
        pid = os.getpid()
    t = time.strftime("%Y%m%dT%H:%M:%SZ", time.gmtime())
    print >>log_f, "%s [%d] %s" % (t, pid, txt)
    log_f.flush()

def success(result):
    return { "Status": "Success", "Value": result }

class MissingDependency(Exception):
    def __init__(self, missing):
        self.missing = missing
    def __str__(self):
        return "There is a missing dependency: %s not found" % self.missing

class Rpc_light_failure(Exception):
    def __init__(self, name, args):
        self.name = name
        self.args = args
    def failure(self):
        # rpc-light marshals a single result differently to a list of results
        args = list(self.args)
        marshalled_args = args
        if len(args) == 1:
            marshalled_args = args[0]
        return { 'Status': 'Failure',
                 'ErrorDescription': [ self.name, marshalled_args ] }

class InternalError(Rpc_light_failure):
    def __init__(self, error):
        Rpc_light_failure.__init__(self, "Internal_error", [ error ])

class UnmarshalException(InternalError):
    def __init__(self, thing, ty, desc):
        InternalError.__init__(self, "UnmarshalException thing=%s ty=%s desc=%s" % (thing, ty, desc))

class TypeError(InternalError):
    def __init__(self, expected, actual):
        InternalError.__init__(self, "TypeError expected=%s actual=%s" % (expected, actual))

class UnknownMethod(InternalError):
    def __init__(self, name):
        InternalError.__init__(self, "Unknown method %s" % name)


def is_long(x):
    try:
        long(x)
        return True
    except:
        return False

# Helper function to daemonise ##############################################
def daemonize():
    try:
        pid = os.fork()
    except OSError, e:
        raise Exception("%s [%d]" % (e.strerror, e.errno))
    
    if (pid == 0):
        os.setsid()

        try:
            pid = os.fork()
        except OSError, e:
            raise Exception("%s [%d]" % (e.strerror, e.errno))

        if (pid == 0):
            os.chdir('/')
            os.umask(0)
        else:
            os._exit(0)
    else:
        os._exit(0)
 
    # Ensuring that all file descriptors are closed - this stop SSH from
    # hanging amongst other things - ensuring it's a proper daemon.

    import resource
    maxfd = resource.getrlimit(resource.RLIMIT_NOFILE)[1]
    if (maxfd == resource.RLIM_INFINITY):
        maxfd = 1024

    for fd in range(0, maxfd):
        try:
            os.close(fd)
        except OSError:
            # FD wasn't open to begin with, ignore
            pass

    if (hasattr(os, "devnull")):
        REDIRECT_TO = os.devnull
    else:
        REDIRECT_TO = "/dev/null"

    os.open(REDIRECT_TO, os.O_RDWR)

    os.dup2(0,1)
    os.dup2(0,2)

    return

from SocketServer import UnixStreamServer
from SimpleXMLRPCServer import SimpleXMLRPCServer, SimpleXMLRPCRequestHandler, SimpleXMLRPCDispatcher
from xmlrpclib import ServerProxy, Fault, Transport
from socket import socket, SOL_SOCKET, SO_REUSEADDR, AF_UNIX, SOCK_STREAM

# Server XMLRPC from any HTTP POST path #####################################

class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = []

class UnixServer(UnixStreamServer, SimpleXMLRPCDispatcher):
    def __init__(self, addr, requestHandler=RequestHandler):
        self.logRequests = 0
        if os.path.exists(addr):
            os.unlink(addr)
        dir = os.path.dirname(addr)
        if not(os.path.exists(dir)):
            os.makedirs(dir)
        SimpleXMLRPCDispatcher.__init__(self)
        UnixStreamServer.__init__(self, addr, requestHandler)

class TCPServer(SimpleXMLRPCServer):
    def __init__(self, ip, port, requestHandler=RequestHandler):
        SimpleXMLRPCServer.__init__(self, (ip, port), requestHandler=requestHandler)
    def server_bind(self):
        self.socket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        SimpleXMLRPCServer.server_bind(self)

# This is a hack to patch slow socket.getfqdn calls that
# BaseHTTPServer (and its subclasses) make.
# See: http://bugs.python.org/issue6085
# See: http://www.answermysearches.com/xmlrpc-server-slow-in-python-how-to-fix/2140/
import BaseHTTPServer

def _bare_address_string(self):
    host, port = self.client_address[:2]
    return '%s' % host

BaseHTTPServer.BaseHTTPRequestHandler.address_string = \
        _bare_address_string

# This is a hack to allow_none by default, which only became settable in
# python 2.5's SimpleXMLRPCServer

import xmlrpclib

original_dumps = xmlrpclib.dumps
def dumps(params, methodname=None, methodresponse=None, encoding=None,
          allow_none=1):
    return original_dumps(params, methodname, methodresponse, encoding, allow_none)
xmlrpclib.dumps = dumps
