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

import sys
import httpservice
import XenAPI
import os
import XenAPI
import subprocess
import urllib2
import base64
import logging
import logging.handlers
import traceback
import time
import re

xapi_plugin = 'xscim'
unix_socket = "/var/xapi/plugin/%s" % xapi_plugin
http_handler = "/services/plugin/%s" % xapi_plugin

store_loc = "/opt/xs-cim-cmpi/store"
log_loc = "/var/log/kvpd.log"
log = None
kvp_tag = "kvp_enabled"
kvpmigration_tag = "kvpmigration_active"
xs_inventory = "/etc/xensource-inventory"
pid_file = "/var/run/kvpd.pids"

def write_pid():
    pid = os.getpid()
    of = open(pid_file,'a')
    of.write("%s\n" % pid)
    of.close()

def should_timeout(start, timeout):
    return time.time() - start > float(timeout)

def configure_logging(name):
    global log
    log = logging.getLogger(name)
    log.setLevel(logging.DEBUG)
    fileh = logging.FileHandler(log_loc)
    fileh.setLevel(logging.DEBUG)
    formatter = logging.Formatter('%%(asctime)s: %s %%(levelname)-8s %%(filename)s:%%(lineno)-10d %%(message)s' % name)
    fileh.setFormatter(formatter)
    log.addHandler(fileh)
    log.debug("Added fileh")
    sth = logging.StreamHandler(sys.__stdout__)
    sth.setLevel(logging.DEBUG)
    log.addHandler(sth)
    log.debug("Added sth")

def should_timeout(start, timeout):
    """Method for evaluating whether a time limit has been met"""
    return time.time() - start > float(timeout)

def get_xs_inventory():
    fh = open(xs_inventory,'r')
    lines = fh.readlines()
    fh.close()
    rec = {}
    for line in lines:
        arr = line.split('=')

        if len(arr) != 2:
            raise Exception("Error: could not parse '%s' line in %s" % line, xs_inventory)

        rec[arr[0]] = arr[1].strip().strip("'")

    log.debug("Parsed XS inventory record: %s" % rec)
    return rec


def KVPClientRetryDec(f):
    attempts = 2

    def fn(*args, **kwargs):
        exception = None
        for i in range(attempts):
            try:
                return f(*args, **kwargs)
            except urllib2.URLError, e:
                exception = e
                # Determine what sort of exception occured
                # and so determine how we should handle it.
                log.debug("Exception '%s' caught. Attempting retry %d of %d" % 
                          (str(e), i, attempts))

                # HTTP 401: Unathorized
                if e.code == 401:
                    # The credentials in Xenstore appear
                    # to have become out of sync with what
                    # the windows service is using. We should
                    # request that the client retries.

                    if not args:
                        raise Exception("Error: function has zero arguments. Unable to retry")

                    # Ask the service to restart, this should
                    # generate new credentials in xenstore.
                    kvpclient = args[0]
                    kvpclient.SETUP()
            except Exception, e:
                # For the moment, we do not want to interfere with normal
                # excpetions as the consequence of re-executing the function
                # may not be intended. (e.g. if a delete fails, but has actually
                # removed the KVP, the second delete will fail, but this time with a
                # 'not present' call. Rather, we should just raise the exception
                # back to the caller.
                raise e

        # We have had our max number of attempts. We should just raise
        # the exception to the caller now.
        raise exception
    return fn

                            

class FirewallRules(object):

    paths = []
    IFCONFIG = '/sbin/ifconfig'
    AWK = '/bin/awk'

    wildcard = "*"

    def __init__(self, network='xenapi', vm_filt=lambda vm_rec:'kvp_enabled' \
                     in vm_rec['other_config'].keys() \
                     and not vm_rec['is_a_snapshot'] \
                     and vm_rec['power_state'] == "Running"):
        self.session = XenAPI.xapi_local()
        self.session.login_with_password("","")
        self.network = network
        self.vm_filt = vm_filt
        self.network_ref = self.get_internal_management_network()
        self.bridge = self.session.xenapi.network.get_bridge(self.network_ref)
        log.debug("Bridge = %s" % self.bridge)
        self.dom0_mac = self.get_dom0_mac()

    def make_local_call(self, call):
        """Function wrapper for making a simple call to shell"""
        log.debug(' '.join(call))
        process = subprocess.Popen(call, stdout=subprocess.PIPE)
        stdout, stderr = process.communicate()
        if process.returncode == 0:
            log.debug(stdout)
            return str(stdout).strip()
        else:
            log.debug("ERR: %s, %s" % (stdout, stderr))

    def get_dom0_mac(self):
        log.debug("get_dom0_mac for bridge %s" % self.bridge)
        call = [self.IFCONFIG, self.bridge]
        res = self.make_local_call(call)
        try:
            mac = res.split('\n')[0].split()[4]
            return mac
        except Exception, e:
            #log.error("Error: %s" % str(e))
            #log.error("Could not parse '%s' for it's MAC" % res)
            log.error("Could not find a MAC")
            return None

    def refresh(self, events=None):
        """Refresh the rules, which means re-generate, adn re-enforce"""
        if events:
            log.debug("Events caused refresh: %s" % events)
        log.debug("Refresh rules")
        log.debug("Current Paths: %s" % self.paths)
        self.generate_isolation_rules()
        log.debug("New Paths: %s" % self.paths)
        self.enforce_rules()
        log.debug("Refresh complete")

    def enforce_rules(self):
        """Write the collected rules out to file"""
        raise Exception("Error: unimplemented")
        
    def add_path(self, mac_src, mac_dst):
        log.debug("Path add: %s --> %s" % (mac_src, mac_dst))
        paths = list(self.paths)
        paths.append((mac_src, mac_dst))
        self.paths = paths

    def get_vms(self):
        res = []
        vms = self.session.xenapi.VM.get_all_records()
        for vm_ref, vm_rec in vms.iteritems():
            if self.vm_filt(vm_rec):
                res.append(vm_ref)

        log.debug("VMs returned after filter: %s" % res)
        return res

    def get_internal_management_network(self):
        networks = self.session.xenapi.network.get_all()
        for network in networks:
            rec = self.session.xenapi.network.get_record(network)
            # Due to a bug in Sanibel/Boston, there can be more than one host internal management
            # network when pooling several XS hosts. This means we need to check for the bridge name,
            # and use 'xenapi' consistently across the two hosts.
            if 'is_host_internal_management_network' in rec['other_config'].keys() and rec['bridge'] == 'xenapi':
                log.debug("Found management network: %s" % rec)
                return network

        raise Exception("Error: cannot get internal management network!")

    def is_running(self, vm_ref):
        """Check whether a given VM is running or not"""
        power_state = self.session.xenapi.VM.get_power_state(vm_ref)
        return power_state.lower() == "running"

    def generate_isolation_rules(self):
        """Generate rules to only allow communication between VMs and
        dom0, and hence drop traffic that is being directed at other VMs
        plugged into the same network"""
        
        # Remove existing paths
        self.paths = []

        management_net = self.get_internal_management_network()

        # Filter VMs
        vms = self.get_vms()
        
        for vm in vms:
            vifs = self.session.xenapi.VM.get_VIFs(vm)
            
            # Iterate through VIFs on this box and check
            # for any interfaces attached to the management
            # network.
            for vif in vifs:
                vif_rec = self.session.xenapi.VIF.get_record(vif)
                log.debug("VIF:%s MAC=%s (VM=%s)" % (vif, vif_rec['MAC'], vm))
                if vif_rec['network'] == management_net:
                    # Allow communication between this VM and Dom0
                    dom0_mac = self.get_dom0_mac()
                    self.add_path(vif_rec['MAC'], dom0_mac)
                    self.add_path(dom0_mac, vif_rec['MAC'])


class OVSRules(FirewallRules):
    """Subclass of the generic 'FirewallRules' class. Used to load
    the rules into the OVS"""

    OVS_OFCTL = "/usr/bin/ovs-ofctl"
    OVS_VSCTL = "/usr/bin/ovs-vsctl"

    class VIFs(object):
        """Inner class for storing VIF related info"""
        
        def __init__(self):
            self.dict = {}
        
        def add(self, vif_ref, vif_rec):
            mac = vif_rec['MAC']
            self.dict[mac] = (vif_ref, vif_rec)

        def get_config(self, mac, config):
            if mac not in self.dict.keys():
                raise Exception("Error: have not found that MAC address (%s) in (%s)" % 
                                (mac, self.dict.keys()))

            log.debug(self.dict[mac])
            log.debug("VIF Record: %s" % str(self.dict[mac]))
            ref, rec = self.dict[mac]
            return rec[config]

        def get_device(self, mac):
            return self.get_config(mac, 'device')

        def get_vm(self, mac):
            log.debug("Get VM for MAC: %s" % mac)
            return self.get_config(mac, 'VM')

    def get_ovs_ports(self, switch):
        call = [self.OVS_VSCTL, 'list-ports', switch]
        res = self.make_local_call(call)
        if not res:
            return None
        return res.split('\n')

    def get_ovs_port_number(self, switch, vif_name):
	"""Lookup OVS port number for specified VIF"""
	call = [self.OVS_OFCTL, 'dump-ports', switch, vif_name]
	res = self.make_local_call(call)
	output = re.search('port\ (\d)+:', res)
	if not output:
	    raise Exception("Error: cannot find port number for '%s' (%s)" % (vif_name, res))

	# Strip of the surrounding labels
	port_id = output.group(0).strip(':').replace('port ','')
	return port_id


    def parse_vifs(self):
        """Enumerate all the VIFs belonging to KVP enabled VMS on the system 
        and provide a mapping between MAC and VIF_ref"""
        # Create the VIFs datastore
        vifs = self.VIFs()

        vms = self.get_vms()
        for vm in vms:
            vif_refs = self.session.xenapi.VM.get_VIFs(vm)
            for vif_ref in vif_refs:
                vif_rec = self.session.xenapi.VIF.get_record(vif_ref)
                vifs.add(vif_ref, vif_rec)

        return vifs

    def get_domid(self, vm_ref, timeout=60):
        """Wrapper call to return the domain ID for a given domain,
        retrying within the provided timeout limits"""
        domid = "-1"
        start = time.time()

        while domid == "-1" and not should_timeout(start, timeout):
            log.debug("Attempting to get domain id for %s" % vm_ref)
            domid = self.session.xenapi.VM.get_domid(vm_ref)
            time.sleep(5)
        
        if domid == "-1":
            raise Exception("Error: VM has not yet been switched on! (%s)" \
                            % [self.session.xenapi.VM.get_uuid(vm_ref), vm_ref])

        log.debug("Domain ID '%s' returned for VM '%s'" % (domid, vm_ref))
        return domid
    
    def enforce_rules(self):
        """Write rules out to config file and load them into the OVS"""

        log.debug("Enforing rules for OVS...")

        if not self.paths:
            return

        rules = []
        
        parsed_vifs = self.parse_vifs()

        dom0_mac = self.get_dom0_mac()

        if not dom0_mac:
            log.debug("No %s visible in Dom0 yet" % self.bridge)
            return

        log.debug("Paths to enforce: %s" % self.paths)

        for src_mac, dst_mac in self.paths:
            
            # check whether the dst_mac is dom0, in which case, the output should be 'local'.
            # otherwise we should construct the VIF id to forward traffic too.

            log.debug("Allowing flow: %s --> %s" % (src_mac, dst_mac))

            if dst_mac == dom0_mac:
                action = "local"
            else:
                device = parsed_vifs.get_device(dst_mac)
                vm_ref = parsed_vifs.get_vm(dst_mac)
                domid = self.get_domid(vm_ref)

                # Compose the name of the bridge in Dom0
                vif_id = "vif%s.%s" % (domid, device)

		port_id = self.get_ovs_port_number(self.bridge, vif_id)

                action = "output:%s" % port_id
            
            ovs_rule = 'dl_src=%s dl_dst=%s action=%s' % (src_mac, dst_mac, action)
            rules.append(ovs_rule)

        # clear all of the current rules in the ovs
        call = [self.OVS_OFCTL, "del-flows", self.bridge]        
            
        for rule in rules:
            call = [self.OVS_OFCTL, "add-flow", self.bridge, rule]
            log.debug("Add flow: %s" % call)
            log.debug(self.make_local_call(call))

class XAPIEventListener(object):
    """Generic class for performing a provided action when 
    a given event occurs"""

    def __init__(self, event_classes, actions):
        self.session = XenAPI.xapi_local()
        self.session.login_with_password("","")

        self.classes = event_classes
        self.actions = actions

        # Register the session object to listen for
        # updates to tasks.
        self.session.xenapi.event.register(self.classes)

    def run_forever(self):
        """Listen for XAPI events forever"""
        while True:
            triggers = []
            try:
                # Run the service to retrieve a list of
                # objects that we should run particular
                # actions on.
                triggers = self.perform_service()
            except Exception, e:
                log.error(traceback.format_exc())
                log.error("Error: %s" % str(e))

            if triggers:

                for action in self.actions:
                    log.debug("Executing action...")
                    action(triggers)

    def perform_service(self):
        triggers = []
        for event in self.session.xenapi.event.next():
            if self.filter(event):
                triggers.append(event)
        return triggers


class VMStartListener(XAPIEventListener):
    
    def __init__(self, actions):
        XAPIEventListener.__init__(self, ['vm_metrics'], actions)
        host_uuid = get_xs_inventory()['INSTALLATION_UUID']
        self.host = self.session.xenapi.host.get_by_uuid(host_uuid)

    def get_vm_from_metrics(self, metrics_ref):
        vms = self.session.xenapi.VM.get_all_records()

        for vm_ref, vm_rec in vms.iteritems():
            if vm_rec['metrics'] == metrics_ref:
                return vm_ref
        return None

    def parse_start_time(self, time_str):
        """Parse a time sting which looks like this:
        <DateTime '20120713T18:06:55Z' at 1a71170>"""
        print time_str
        match = re.search(r'\d{8}T\d{2}:\d{2}:\d{2}Z', str(time_str))
        if not match:
            raise Exception("Error: cannot find a match for timestring (%s)" % time_str)

        t = time.strptime(match.group(), "%Y%m%dT%H:%M:%SZ")
        return t

    def vm_on_this_host(self, vm_ref):
        """Helper function for making sure we care about the VM in question"""
        vms_host = self.session.xenapi.VM.get_resident_on(vm_ref)
        return (self.host == vms_host)

    def perform_service(self):
        vm_metrics = self.session.xenapi.VM_metrics.get_all_records()
        print "vm_metrics: %s" % vm_metrics
        vms = []
        for event in self.session.xenapi.event.next():
            log.debug("Handling event: %s" % event)
            metrics_ref = event['ref']
            start_time = self.parse_start_time(event['snapshot']['start_time'])
            original_start_time = self.parse_start_time(vm_metrics[metrics_ref]['start_time'])

            log.debug("%s > %s" % (str(start_time), str(original_start_time)))
            if start_time > original_start_time:
                vm_ref = self.get_vm_from_metrics(metrics_ref)
                log.debug("VM %s has started" % vm_ref)
                if vm_ref not in vms and self.vm_on_this_host(vm_ref):
                    log.debug("VM '%s' added to queue" % vm_ref)
                    vms.append(vm_ref)
        
        return vms


def run_setup_listener():
    """Default thread function 'run'."""
    worker_pid = os.fork()
    if not worker_pid:
	write_pid()
        try:
            rules = OVSRules()
            actions = [re_setup_vms, rules.refresh]
            listener = VMStartListener(actions)
            listener.run_forever()
        except Exception, e:
            log.error(traceback.format_exc())
            log.error("Exception: %s" % str(e))

def re_setup_vms(vm_refs):
    """Method use to re-setup a particular VM, writing info required
    to xenstore. This method should be called by a event listener,
    and handles multiple events occuring simultaneously."""
    log.debug("Entering re_setup_vms for vms '%s'" % vm_refs)

    for ref in vm_refs:
        try:
            kvpclient = KVPHTTPSClient(vm_ref=ref)
            log.debug("Setup VM %s" % ref)
            try:
                kvpclient._setup()
                log.debug("Setup OK")
            except Exception, e:
                log.error(traceback.format_exc())
                log.error("Exception raised: %s" % str(e))
        except XenAPI.Failure, reason:
            reason_dict = reason._details_map()
            if reason_dict['0'] == 'HANDLE_INVALID':
                # Some of the 'applies to' objects are not VMs
                # that's OK. Just continue.
                log.debug("Non VM ref found: %s" % ref)
                continue
        except Exception, e:
            log.debug("Exception: %s" % str(e))


class KVPHTTPSClient(object):
    """Class object for making requests against the Windows KVP service"""
    
    KVP_ROOT = "/local/domain/%s/vm-data/kvp"
    XENSTORE_READ = "/usr/bin/xenstore-read"
    XENSTORE_WRITE = "/usr/bin/xenstore-write"
    XENSTORE_CHMOD = "/usr/bin/xenstore-chmod"
    XENSTORE_RM = "/usr/bin/xenstore-rm"
    XENSTORE_LS = "/usr/bin/xenstore-ls"

    def __init__(self, vm_uuid=None, vm_ref=None):
        """Perform initial operations such as reading credentials from XenStore"""
        # setup local XAPI session
        self.session = XenAPI.xapi_local()
        self.session.login_with_password("","")

        if not vm_uuid and not vm_ref:
            raise Exception("Error: either a vm_uuid or reference must be passed")
        
        if vm_ref:
            self.vm = vm_ref
        else:
            # get vm reference for uuid
            self.vm = self.session.xenapi.VM.get_by_uuid(vm_uuid)

        # get domain id for xenstore ops
        self.domid = self.session.xenapi.VM.get_domid(self.vm)

    def _get_credentials(self):
        self.username = self._xenstore_read('auth/username')
        self.password = self._xenstore_read('auth/password')
        self.cert = self._xenstore_read('auth/cert')
        self.url = "%s/kvp" % self._xenstore_read('daemon-ip').strip('/')

    def make_local_call(self, call):
        """Function wrapper for making a simple call to shell"""
        log.debug(' '.join(call))
        process = subprocess.Popen(call, stdout=subprocess.PIPE)
        stdout, stderr = process.communicate()
        if process.returncode == 0:
            log.debug(stdout)
            return str(stdout).strip()
        else:
            log.debug("ERR: %s, %s" % (stdout, stderr))

    def _xenstore_ls(self):
        call = [self.XENSTORE_LS,'-p', self.KVP_ROOT % self.domid]
        return self.make_local_call(call)

    def _enable_xenstore_rw(self):
        log.debug("Enable Xenstore RW")
        call = [self.XENSTORE_RM, self.KVP_ROOT % self.domid]
        self.make_local_call(call)
        call = [self.XENSTORE_WRITE, self.KVP_ROOT % self.domid, ""]
        self.make_local_call(call)
        call = [self.XENSTORE_CHMOD, self.KVP_ROOT % self.domid, "n%s" % self.domid]
        return self.make_local_call(call)

    def _xenstore_read(self, loc):
        root = self.KVP_ROOT % self.domid
        call = [self.XENSTORE_READ, "%s/%s" % (root, loc)]
        return self.make_local_call(call)

    def _xenstore_write(self, loc, value):
        root = self.KVP_ROOT % self.domid
        call = [self.XENSTORE_WRITE, "%s/%s" % (root, loc), value]
        return self.make_local_call(call)

    def get_auth_handler(self):
        p_mgr = urllib2.HTTPPasswordMgrWithDefaultRealm()
        p_mgr.add_password(None, self.url, self.username, self.password)
        
        handler = urllib2.HTTPBasicAuthHandler(p_mgr)
        return handler
    
    def set_auth_header(self, request):
        b64string = base64.b64encode('%s:%s' % (self.username, self.password))
        request.add_header("Authorization", "Basic %s" % b64string)

    def restart_windows_service(self):
        self._xenstore_write('cmd/re-attach-daemon', '1')

    def is_daemon_running(self):
        """Verify whether the windows daemon is running or not"""
        return self._xenstore_read('daemon-status') == '0' and self._xenstore_read('cmd/re-attach-daemon') == '0'

    def verify_completion(self, fn, timeout):
        start = time.time()
        while not should_timeout(start, timeout):
            if fn():
                return True
            else:
                log.debug("Trying again...")
                time.sleep(1)
        return False

    def wait_for_service(self,timeout=30):
        if not self.verify_completion(self.is_daemon_running, timeout):
            self._xenstore_ls()
            log.debug("Error: waited %d seconds for service, and it's still not ready." % timeout)
            raise Exception("Error: timed out waiting for service response.")
        log.debug("Service running and ready")
        self._get_credentials()   

    @KVPClientRetryDec
    def PUT(self, key, value):
        self.wait_for_service()

        url = "%s/%s" % (self.url, key)
        log.debug("PUT to url %s" % url)

        opener = urllib2.build_opener(urllib2.HTTPHandler)
        request = urllib2.Request(url, value)
        request.add_header('Content-Type', 'text/plain')
        self.set_auth_header(request)
        request.get_method = lambda: 'PUT'
        url = opener.open(request)
        opener.close()
        return url

    @KVPClientRetryDec
    def GET(self, key=None):
        self.wait_for_service()

        if key:
            url = "%s/%s" % (self.url, key)
        else:
            url = "%s/" % self.url

        log.debug("GET from url %s" % url)
        
        opener = urllib2.build_opener(urllib2.HTTPHandler)
        request = urllib2.Request(url)
        self.set_auth_header(request)
        log.debug("Make a request")
        response = urllib2.urlopen(request)
        log.debug("Response retrieved")
        data = response.read()
        log.debug("Data returned: %s" % data)
        response.close()
        log.debug("Connection Closed")
        return data

    @KVPClientRetryDec
    def DEL(self, key):
        self.wait_for_service()

        url = "%s/%s" % (self.url, key)
        
        log.debug("DEL resource at url %s" % url)
        
        opener = urllib2.build_opener(urllib2.HTTPHandler)
        request = urllib2.Request(url)
        self.set_auth_header(request)
        request.get_method = lambda: 'DELETE'
        url = opener.open(request)
        opener.close()
        return url

    def _cleanup_vifs(self):
	log.debug("Entering _cleanup_vifs")
        vifs = self.session.xenapi.VM.get_VIFs(self.vm)
        to_destroy = []
        for vif in vifs:
            if kvp_tag in self.session.xenapi.VIF.get_other_config(vif):
                to_destroy.append(vif)

        for vif in to_destroy:
	    log.debug("Destroying VIF: '%s'" % vif)
            self.session.xenapi.VIF.unplug(vif)
            self.session.xenapi.VIF.destroy(vif)

    def _get_host_internal_network(self):
        networks = self.session.xenapi.network.get_all()

        for network in networks:
            rec = self.session.xenapi.network.get_record(network)
            if 'is_host_internal_management_network' in rec['other_config'].keys() \
                    and rec['bridge'] == 'xenapi':
                return network

        raise Exception("Error: could not find host internal network")

    def _create_vif(self, m_network):
        device = len(self.session.xenapi.VM.get_VIFs(self.vm)) + 1
        return self.session.xenapi.VIF.create({'device': str(device),
                                               'network': m_network,
                                               'VM': self.vm,
                                               'MAC': '',
                                               'MTU': '1504',
                                               'other_config': {kvp_tag: 'true'},
                                               'qos_algorithm_type': '',
                                               'qos_algorithm_params': {}})

    def _clearup(self):
        log.debug("Clearing up state...")
        # setup the specified VM on a private network
        self._cleanup_vifs()

        oc = self.session.xenapi.VM.get_other_config(self.vm)
        if kvp_tag in oc:
            del(oc[kvp_tag])
            self.session.xenapi.VM.set_other_config(self.vm, oc)
        
    def get_management_vif(self):
        """Return a reference to the management VIF if it exists,
        else return the None value"""
        res = []
        vifs = self.session.xenapi.VM.get_VIFs(self.vm)
        for vif in vifs:
            oc = self.session.xenapi.VIF.get_other_config(vif)
            if kvp_tag in oc:
                res.append(vif)
                
        if not res:
            return None

        if len(res) > 1:
            raise Exception("Error: more than one management VIF found: %s" % res)
        
        return res[0]

    def setup_persistent_xenstore_data(self, mac):
        """Make sure Xenstore has been setup with the information required to restart
        the daemon on a VM reboot"""
        
        device_key = "network-device"
        restart_key = "cmd/re-attach-daemon"

	xenstore_data = self.session.xenapi.VM.get_xenstore_data(self.vm)
        
        if device_key in xenstore_data and restart_key in xenstore_data \
                and xenstore_data[device_key] == mac:
            log.debug("Xenstore_data has already been set. Skipping...") 
            # We do not have to do anything
            return
        else:
            xenstore_data[device_key] = mac
	    xenstore_data[restart_key] = '1'
	
            log.debug("Update persistent")
	    self.session.xenapi.VM.set_xenstore_data(self.vm, xenstore_data)
            
            # Due to the way in which XAPI handles this setting of xenstore data
            # we now need to make sure we refresh what's currently in xenstore
            # and set the correct permissions.
    
            log.debug("Enabling xenstore as being read-write")
            self._enable_xenstore_rw()

            self._xenstore_write(device_key, mac)
            return

            
    def _setup_network_interface(self):	
        # get the management network
	m_network = self._get_host_internal_network()

        # create a VIF
        vif = self._create_vif(m_network)

        # plug VIF
        self.session.xenapi.VIF.plug(vif)

        management_vif = self.get_management_vif()
        if not management_vif:
            raise Exception("Error: could not complete setup, could not find management VIF")

        mac = self.session.xenapi.VIF.get_MAC(management_vif)
	# Make sure that the contents of xenstore set on VM boot is updated
	self.setup_persistent_xenstore_data(mac)
	    
        # Update the current network device in dom0
        self._xenstore_write('network-device', mac)
	return management_vif

    def _setup(self):
        timeout = '60'

	# create the VIF used for communicating with the guest
	self._setup_network_interface()

        # restart windows service
        self.restart_windows_service()
        
        # wait for the 'OK' response via xenstore
        if not self.verify_completion(self.is_daemon_running, timeout):
            log.debug("Failed to verify the daemon has restarted")

        # Refresh the data we have for the service
        self._get_credentials()
        # update VM key to say 'ready for KVP'
        try:
            self.session.xenapi.VM.add_to_other_config(self.vm, kvp_tag, 'true')
        except XenAPI.Failure, reason:
            reason_dict = reason._details_map()
            if reason_dict['0'] == 'MAP_DUPLICATE_KEY':
                log.debug("other_config key has already been set. Leaving it as is.")
            else:
                raise Exception("Error: %s" % str(reason))
    
    def SETUP(self):
        self._clearup()
        self._setup()

    def _prepare_migration(self):
        oc = self.session.xenapi.VM.get_other_config(self.vm)
        if kvp_tag in oc:
            self._clearup()
            self.session.xenapi.VM.add_to_other_config(self.vm, kvpmigration_tag, 'true') 
        else:
            raise Exception("Error: unexpected call of _prepare_migration as kvp does not seem to be installed on this partition") 

    def PREPARE_MIGRATION(self):
        self._prepare_migration()   
 
    def _finish_migration(self):
        oc = self.session.xenapi.VM.get_other_config(self.vm)
        if kvpmigration_tag in oc:
            del(oc[kvpmigration_tag])
            self.session.xenapi.VM.set_other_config(self.vm, oc)
            self._clearup()
            self._setup()        
        else:
            raise Exception("Error: unexpected call of _finish_migration because kvp migration seems not to have happened")

    def FINISH_MIGRATION(self):
        self._finish_migration()

def get_local_xapi_session():
    """Login to Xapi locally. This will only work if this script is being run 
    on Dom0. For this, no credentials are required."""
    session = XenAPI.xapi_local()
    session.login_with_password("", "")
    return session

class RequestHandler(httpservice.RequestHandler):

    def _path_to_dict(self):
        """Convert a provided URL path into a dictionary of key/values."""

	path = self.path.replace(http_handler,'')        
        path = path.strip('/')

	# Ignore any query parameters if they have been
	# passed in by the caller
	if '?' in path:
	    path = path.split('?')[0]

	log.debug("Path: %s" % path)
        args = path.split('/')

	

        if len(args) % 2 != 0:
            log.debug("Exception: args: %s" % args)
            raise Exception("Error: non_even list of arguments")
        
        rec = {}
        i = 0
        while i < len(args) - 1:
            rec[args[i]] = args[i+1]
            i = i + 2
        return rec

    def _shutdown_connection(self):
	# shutdown the connection
	self.wfile.flush()
	self.connection.shutdown(1)    
	log.debug("Connection closed")

    def _return_error(self):
        """Return a default error code/message"""
        log.debug("Error Occurred")
        self.send_response(404)
        self.end_headers()
        self._shutdown_connection()

    def _return_ok(self):
        """Return a HTTP 200 OK response"""
        self.send_response(200)
        self.end_headers()
	log.debug("Returned HTTP 200 OK")
	self._shutdown_connection()

    def _send_data(self, data, content_type="text/html"):
        self.send_response(200)
        self.send_header("Content-type", content_type)
        self.send_header("Content-length", str(len(data)))
        log.debug("Content-length = %s" % str(len(data)))
        self.end_headers()
        self.wfile.write(data)
	self._shutdown_connection()

    def do_GET(self):
        log.debug("do_GET: %s %s" % (repr(self), self.path))
        response = None
        
        # Would expect either:
        # * /vm/<VM-UUID>/key/<KEY>
        # * /vm/<VM-UUID>

        # Return error if URL arguments
        # are incorrect.

        try:
            rec = self._path_to_dict()
        except Exception, e:
            log.error(traceback.format_exc())
            log.error("Exception: %s" % str(e))
            return self._return_error()

        if 'vm' not in rec.keys():
            return self._return_error()

        kvpclient = KVPHTTPSClient(rec['vm'])

        try:
            if 'key' in rec.keys():
                data = kvpclient.GET(rec['key'])
            else:
                data = kvpclient.GET()
        except Exception, e:
            log.error(traceback.format_exc())
            log.error("Exception: %s" % str(e))
            return self._return_error()

        # Send the retrieved data back to the client
        if data:
            self._send_data(data)
        else:
            return self._return_error()

    def do_POST(self):
        log.debug("do_POST: %s (%s)" % (repr(self), self.path))

        try:
            rec = self._path_to_dict()
            log.debug("Parsed record: %s" % rec)
        except Exception, e:
            log.error(traceback.format_exc())
            log.error("Exception: %s" % str(e))
            return self._return_error()
        
        if 'vm' not in rec.keys():
            log.debug("No vm reference found in url: %s" % rec)
            return self._return_error()

        # Create the client object
        try:
            kvpclient = KVPHTTPSClient(rec['vm'])
        except Exception, e:
            log.debug("Exception: %s" % str(e))
            return self._return_error()
            

        log.debug("keys = %s" % rec.keys())

        if 'vm' and 'cmd' in rec.keys():
            log.debug("cmd found: %s" % rec['cmd'])
            if rec['cmd'] == "setup":
                log.debug("Executing setup request")
                try:
                    kvpclient.SETUP()
                    log.debug("Setup complete")
                    return self._return_ok()
                except Exception, e:
                    log.error(traceback.format_exc())
                    log.debug("Exception: %s" % str(e))
                    self.return_error()
            elif rec['cmd'] == "delete" and 'key' in rec.keys():
                log.debug("Executing delete request")
                try:
                    kvpclient.DEL(rec['key'])
                    log.debug("Key deleted")
                    self._return_ok()
                except Exception, e:
                    log.debug("Exception: %s" % str(e))

                    # Before returning a failure, we should check
                    # whether the key has been deleted. If it has,
                    # then we can return OK. (See CA-89892)
                    
                    try:
                        value = kvpclient.GET(rec['key'])
                        log.debug("Key '%s' should have been deleted. Value '%s' returned." % (rec['key'],value))
                    except urllib2.URLError, e:
                        if e.code == 404:
                            log.debug("Key '%s' was successfully delete despite prior exception." % rec['key'])
                            self._return_ok()

                        log.error(traceback.format_exc())
                        log.debug("Exception '%s' raised as well as previous exception." % str(e))
                    except Exception, e:
                        log.error(traceback.format_exc())
                        log.debug("Unexpected exception occured: '%s'" % str(e))
                        
                    # If we've got this far, then we have not established that the key has been
                    # deleted, and so we should return a failure code to the caller.
                    self._return_error()
            elif rec['cmd'] == "preparemigration":
                log.debug("Executing prepare migration request")
                try:
                    kvpclient.PREPARE_MIGRATION()
                    log.debug("Migration prepared")
                    self._return_ok()
                except Exception, e:
                    log.error(traceback.format_exc())
                    log.debug("Exception: %s" % str(e))
                    self._return_error()
            elif rec['cmd'] == "finishmigration":
                log.debug("Executing finish migration request")
                try:
                    kvpclient.FINISH_MIGRATION()
                    log.debug("Migration finished")
                    self._return_ok()
                except Exception, e:
                    log.error(traceback.format_exc())
                    log.debug("Exception: %s" % str(e))
                    self._return_error()

            else:
                self._return_error()

        elif 'vm' and 'key' in rec.keys():
            
            if 'content-length' not in self.headers.keys():
                print "Error: no content-length in headers"
                return self._return_error()

            length = int(self.headers['content-length'])
            value = self.rfile.read(length)

            try:
                kvpclient.PUT(rec['key'], value)
            except Exception, e:
                log.error(traceback.format_exc())
                log.error("Exception: %s" % e)
                
                # Allow a single retry to ensure that we are not
                # hitting a problem like MS-71 (SSL protocol violation).
                
                try:
                    kvpclient.PUT(rec['key'], value)
                except Exception, e:
                    log.error(traceback.format_exc())
                    log.error("Another exception: %s" % e)
                    self._return_error()

            return self._return_ok()
        else:
            return self._return_error()
                                       
if __name__ == "__main__":
    from optparse import OptionParser
    import ConfigParser

    settings = {
        "log": "stdout:",
        "port": None,
        "ip": None,
        "daemon": False,
	"service": False,
        "config": None,
        "pidfile": None,
        "www": None,
        }
    string_t = lambda x:x
    int_t = lambda x:int(x)
    bool_t = lambda x:x == True
    types = {
        "log": string_t,
        "port": int_t,
        "ip": string_t,
        "daemon": bool_t,
	"service": bool_t,
        "config": string_t,
        "pidfile": string_t,
        "www": string_t,
    }


    parser = OptionParser()
    #parser.add_option("-l", "--log", dest="logfile", help="log to LOG", metavar="LOG")
    parser.add_option("-p", "--port", dest="port", help="listen on PORT", metavar="PORT")
    parser.add_option("-i", "--ip-addr", dest="ip", help="listen on IP", metavar="IP")
    parser.add_option("-s", "--service", action="store_true", dest="service", help="listen on specified XAPI service", metavar="SERVICE")
    parser.add_option("-d", "--daemon", action="store_true", dest="daemon", help="run as background daemon", metavar="DAEMON")
    (options, args) = parser.parse_args()
    options = options.__dict__

    for setting in settings:
        if setting in options and options[setting]:
            settings[setting] = types[setting](options[setting])
            

    tcp = settings["ip"] and settings["port"]


    if not tcp and not settings["service"]:
        print >>sys.stderr, "Need an --ip-addr and --port or a --socket. Use -h for help"
        sys.exit(1)

    
    if settings["daemon"]:
        print "Daemonizing Service..."
        httpservice.daemonize()

    # setup logging
    configure_logging('kvpd')

    if tcp:
        server = httpservice.TCPServer(settings["ip"], settings["port"], requestHandler=RequestHandler)
    else:
        server = httpservice.UnixServer(unix_socket, requestHandler=RequestHandler)

    # Start the XAPI listener which takes care of
    # ensuring that the appropraite firewall rules
    # have been setup as well as creating the required
    # xenstore keys.
        
    #run_setup_listener()

    server.register_introspection_functions() #for debugging
    
    try:
        print "Serving requests forever"
	write_pid()
        server.serve_forever()
    except Exception, e:
        log.error(traceback.format_exc())
        log.debug("Exception")
        log.error("Exception: %s" % str(e))
    
