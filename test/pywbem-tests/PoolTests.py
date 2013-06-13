#!/usr/bin/env python

'''Copyright (C) 2009 Citrix Systems Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
=========================================================================
'''

import sys
import pywbem
import time
from xen_cim_operations import *
import os
from TestSetUp import *

'''
Exercises methods in the Xen_HostPool class. 
Allows caller to configure a XenServer host pool (default SR, add/remove hosts)
'''
class PoolTest(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, need_shared_storage=False, create_vms=False)
        self.pool_user = userName
        self.pool_pass = password
        pools = self.conn.EnumerateInstanceNames("Xen_HostPool")
        self.pool_ref = pools[0]
        self.pool_inst = self.conn.GetInstance(self.pool_ref)
        print self.pool_ref.items()
        self.srs = self.conn.EnumerateInstanceNames("Xen_StoragePool")
        self.bad_sr_ref = self.srs[0]
        self.bad_sr_ref['InstanceID'] = 'bad_reference'
        self.hosts = self.conn.EnumerateInstanceNames('Xen_HostComputerSystem')
        self.bad_host_ref = self.hosts[0]
        self.bad_host_ref['Name'] = 'bad-refefence'
        self.management_ip = "192.168.5.30"
        self.management_ip_mask = "255.255.255.0"
        self.management_purpose = "Storage"
        self.management_interface = "eth1"

    def __list_hosts (self):
        hosts = self.conn.EnumerateInstanceNames('Xen_HostComputerSystem')
        return hosts

    def __host_exists_in_pool (self, host_ref):
        hosts = self.__list_hosts()
        result = False
        for host in hosts:
            if host['Name'] == host_ref['Name']:
                result = True
                break
        return result

    def get_pool_inst (self):
        # print 'getting instance of [%s]' % str(self.pool_ref)
        pool_inst = self.conn.GetInstance(self.pool_ref)
        return pool_inst

    def list_hosts (self):
        self.TestBegin()
        result = 0
        hosts = self.__list_hosts()
        if len(hosts) > 0:
            result = 1
        for host in hosts:
            print '%s' % host['Name']
            if len(hosts) > 0:
                print 'Found Hosts in pool'
            else:
                print 'No hosts in pool'
        self.TestEnd(result)
        return hosts

    def select_host_to_remove(self):
        hosts = self.__list_hosts()
        for host in hosts:
             if self.pool_inst['Master'] != host['Name']:
                 return host # master cannot be removed
        return None

    def create_pool (self):
        self.TestBegin()
        poolname = self.pool_inst['ElementName']
        pooldescription = self.pool_inst['Description']
        # toggle between existing poolname and existing poolname + 'Changed'
        if poolname.rfind('Changed') != -1:
            poolname = poolname.rstrip('Changed')
            pooldescription = pooldescription.rstrip('Changed')
        else:
            poolname = poolname + 'Changed'
            pooldescription = pooldescription + 'Changed'
        print 'Creating a new pool %s (%s)' % (poolname, pooldescription)
        result = 0
        in_params = {'Name':poolname, 'Description':pooldescription}
        try:
            (rval, out_params) = self.conn.InvokeMethod('Create', self.pool_ref, **in_params)
            if rval == 0:
                pool_inst = self.conn.GetInstance(self.pool_ref)
                if cmp(pool_inst['ElementName'], poolname) == 0 and cmp(pool_inst['Description'], pooldescription) == 0:
                    result = 1
                else:
                    print 'Error: pool name and description werent set properly %s (%s)' % (pool_inst['ElementName'], pool_inst['Description'])
            else:
                print 'Error returned from the Create method: %d' % rval
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        self.TestEnd(result)

    ###########################################################################
    # Test requirement: 
    # 1. The password for the host being added is the same as the one passed into
    # the test script
    # 2. The pool is not AD joined otherwise the host being added will receive an 
    # error complaining about EXTERNAL_AUTH_MISMATCH
    ########################################################################### 
    def add_host (self, host_ip):
        self.TestBegin()
        print 'Adding %s to pool %s' % (host_ip, self.pool_ref['InstanceID'])
        print 'TestRequirement: username/password passed into the script should work for the host being added'
        print 'TestRequirement: The pool is not domain joined'
        print 'TestRequirement: The pool is not licensed'
        result = 0
        hosts_before = self.__list_hosts()
        in_params = {'HostName':host_ip, 'Username':self.pool_user, 'Password':self.pool_pass}
        try:
            (rval, out_params) = self.conn.InvokeMethod('AddHost', self.pool_ref, **in_params)
            if rval == 0:
                result = 1
                hosts_after = self.__list_hosts()
                if len(hosts_after) == len(hosts_before) + 1:
                    print 'Found Host in pool'
                else:
                    print 'Host was not added correctly'
            else:
                print 'Error returned from the AddHost method: %d' % rval
                result = 0
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
            if 'POOL_JOINING_EXTERNAL_AUTH_MISMATCH' in str(e):
                print 'Your pool is domain joined and the host being added is not. AddHost is not expected to work.'
            elif 'LICENCE_RESTRICTION' in str(e):
                print 'Your pool hosts are licensed and the host being added is not. AddHost is not expected to work.'
        self.TestEnd(result)

    def remove_host (self, host_ref):
        self.TestBegin()
        in_params = {'Host':host_ref}
        host_exists = self.__host_exists_in_pool(host_ref)
        host_ip = ''
        result = 0
        print 'TestRequirement: The pool is not licensed.'
        try:
            if host_exists == False:
                print 'Host %s doesnt exist in the pool. Cannot run test' % (host_ref['Name'])
            else:
                host_inst = self.conn.GetInstance(host_ref)
                host_ip = host_inst['OtherIdentifyingInfo'][0]
                print 'Attempting to remove host %s (%s) from pool %s' % (host_inst['Name'], host_ip, self.pool_ref['InstanceID'])
                (rval, out_params) = self.conn.InvokeMethod('RemoveHost', self.pool_ref, **in_params)
                if rval == 0:
                    host_exists = self.__host_exists_in_pool(host_ref)
                    if host_exists == False:
                        result = 1 
                        try:
                            print 'Try removing the Host %s after its already been removed from the pool' % host_ip
                            (rval, out_params) = self.conn.InvokeMethod('RemoveHost', self.pool_ref, **in_params)
                            if rval == 0:
                                print 'RemoveHost worked for a host that is not in the pool. Error!!'
                                result = 0
                        except Exception, e:
                            print 'Exception (%s) received trying remove the server from the pool, as excepted' % str(e)
                    else:
                        print 'Method returned error code 0 but host still exists in pool' 
                else:
                    print 'Method returned error code %d' % (rval)
                    result = 0
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        self.TestEnd(result)
        return host_ip

    def reboot_host (self, host_ref):
        self.TestBegin()
        result = 0
        in_params = {'RequestedState': pywbem.Uint16(10)} # reboot
        try:
            (rval, out_params) = self.conn.InvokeMethod('RequestStateChange', host_ref, **in_params)
            if rval == 0 or rval == 4096:
                if rval == 4096:
                    job_ref = out_params['Job']
                    job = WaitForJobCompletion(self.conn, job_ref)
                    if job['JobState'] == 7:
                        print "Host has been issued a 'reboot'"
                        result = 1
                    else:
                        print "ERROR: Reboot job didnt succeed - State %d instead of 7 (successful)" % job['JobState']
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        self.TestEnd(result)

    def take_host_offline (self, host_ref):
        self.TestBegin()
        result = 0
        in_params = {'RequestedState': pywbem.Uint16(3)} # Offline
        try:
            (rval, out_params) = self.conn.InvokeMethod('RequestStateChange', host_ref, **in_params)
            if rval == 4096 or rval == 0:
                if rval == 4096:
                    job_ref = out_params['Job']
                    job = WaitForJobCompletion(self.conn, job_ref)
                    if job['JobState'] == 7:
                        print "Host has been issued a 'enter maintanence mode'"
                        result = 1
                    else:
                        print "ERROR: Host state change job didnt complete - State %d instead of 7 (successful)" % job['JobState']
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        self.TestEnd(result)

    def add_management_network (self, host):
        self.TestBegin()
        result = 0
        try:
            nic_to_change = None
            nics = self.conn.EnumerateInstances("Xen_HostNetworkPortSettingData")
            for nic in nics:
                if nic['Connection'][0] == self.management_interface and nic['ElementName'] == host['Name']:
                    nic['IPConfigurationMode'] = pywbem.Uint8(2) # Static
                    nic['IPAddress'] = self.management_ip
                    nic['IPSubnetMask'] = self.management_ip_mask
                    nic['ManagementPurpose'] = self.management_purpose
                    nic_to_change = nic.tomof()
                    break
            in_params = {"ResourceSettings": nic_to_change}
            (rval, out_params) = self.conn.InvokeMethod('ModifyResourceSettings', self.vsms, **in_params)
            if rval == 0:
                result = 1
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        self.TestEnd(result)


    ###########################################################################
    # Test requirement: 
    # More than 1 shared storage SR is available
    ########################################################################### 
    def change_default_sr (self):
        self.TestBegin()
        isSuccess = 0
        print 'TestRequirement: Availability of more than 1 shared storage SR.'
        pool_inst = self.get_pool_inst()
        def_sr_before = pool_inst["DefaultStoragePoolID"]
        srs = self.conn.EnumerateInstanceNames("Xen_StoragePool")
        sr_to_change_to = None
        for sr in srs:
            # find a shared SR to set the default sr to. Also use the SR that's not currently in use
            if (sr['InstanceID'].find("Shared") != -1) and (sr['InstanceID'].find(def_sr_before) == -1):
                sr_inst = self.conn.GetInstance(sr)
                if sr_inst['ResourceSubType'] == 'nfs' or sr_inst['ResourceSubType'] == 'lvmoiscsi':
                    sr_to_change_to = sr
                    break;
        if sr_to_change_to != None:
            in_params = {'StoragePool': sr_to_change_to}
            (rval, out_params) = self.conn.InvokeMethod('SetDefaultStoragePool', self.pool_ref, **in_params)
            if rval == 0:
                pool_inst = self.conn.GetInstance(self.pool_ref)
                sr_to_change_after = pool_inst["DefaultStoragePoolID"]
                if sr_to_change_to['InstanceID'].find(sr_to_change_after) != -1 :
                    isSuccess = 1
                    print "Default SR is now %s" % sr_to_change_after
                    # try changing it to the same storage pool
                    in_params = {'StoragePool': sr_to_change_to}
                    (rval, out_params) = self.conn.InvokeMethod('SetDefaultStoragePool', self.pool_ref, **in_params)
                    if rval != 0:
                        isSuccess = 0
                else:
                    print "Default SR change failed"
            else:
                print "Default SR change failed with return value %d" % rval
        else:
            print 'Couldnt find a second SR to change to. Is there a second shared SR available ?'
            isSuccess = 1
        self.TestEnd(isSuccess)

    def change_default_sr2 (self):
        self.TestBegin()
        isSuccess = 0
        print 'TestRequirement: Availability of more than 1 shared storage SR.'
        pool_inst = self.get_pool_inst()
        def_sr_before = pool_inst["DefaultStoragePoolID"]
        srs = self.conn.EnumerateInstanceNames("Xen_StoragePool")
        sr_to_change_to = None
        for sr in srs:
            # find a shared SR to set the default sr to. Also use the SR that's not currently in use
            if (sr['InstanceID'].find("Shared") != -1) and (sr['InstanceID'].find(def_sr_before) == -1):
                sr_inst = self.conn.GetInstance(sr)
                if sr_inst['ResourceSubType'] == 'nfs' or sr_inst['ResourceSubType'] == 'lvmoiscsi':
                    sr_to_change_to = sr
                    break;
        if sr_to_change_to != None:
            in_params = {'StoragePool': sr_to_change_to}
            (rval, out_params) = self.conn.InvokeMethod('SetDefaultStoragePool', self.pool_ref, **in_params)
            if rval == 0:
                pool_inst = self.conn.GetInstance(self.pool_ref)
                sr_to_change_after = pool_inst["DefaultStoragePoolID"]
                if sr_to_change_to['InstanceID'].find(sr_to_change_after) != -1 :
                    isSuccess = 1
                    print "Default SR is now %s" % sr_to_change_after
                    # try changing it to the same storage pool
                    in_params = {'StoragePool': sr_to_change_to}
                    (rval, out_params) = self.conn.InvokeMethod('SetDefaultStoragePool', self.pool_ref, **in_params)
                    if rval != 0:
                        isSuccess = 0
                else:
                    print "Default SR change failed"
            else:
                print "Default SR change failed with return value %d" % rval
        else:
            print 'Couldnt find a second SR to change to. Is there a second shared SR available ?'
            isSuccess = 1
        self.TestEnd(isSuccess)

    def add_host_error_tests (self, host_ip):
        self.TestBegin()
        result = 1
        print 'Trying to add host %s to pool %s with bad user, password and hostname' % (host_ip, self.pool_ref['InstanceID'])
        try:
            in_params = {'HostName':"192.168.1.1", 'Username':self.pool_user, 'Password':self.pool_pass}
            (rval0, out_params) = self.conn.InvokeMethod('AddHost', self.pool_ref, **in_params)
            result = 0
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        try:
            in_params = {'HostName':host_ip, 'Username':"borat", 'Password':self.pool_pass}
            (rval1, out_params) = self.conn.InvokeMethod('AddHost', self.pool_ref, **in_params)
            result = 0
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        try:
            in_params = {'HostName':host_ip, 'Username':self.pool_user, 'Password':"AliGInDaHouse"}
            (rval2, out_params) = self.conn.InvokeMethod('AddHost', self.pool_ref, **in_params)
            result = 0
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        self.TestEnd(result)

    def remove_host_error_tests (self, host_ip):
        self.TestBegin()
        result = 1
        try:
            print 'Error test: Attempting to remove a bad host reference'
            in_params = { 'Host': self.bad_host_ref }
            (rval, out_params) = self.conn.InvokeMethod('RemoveHost', self.pool_ref, **in_params)
            if rval == 0:
                print 'RemoveHost worked without any errors. ERROR!!'
                result = 0
        except Exception, e:
            print 'Exception caught : %s\n' % str(e)
        try:
            print 'Error test: Attempting to remove a None reference'
            in_params = { 'Host': None }
            (rval, out_params) = self.conn.InvokeMethod('RemoveHost', self.pool_ref, **in_params)
            if rval == 0:
                print 'RemoveHost worked without any errors. ERROR!!'
                result = 0
        except Exception, e:
            print 'Exception caught : %s\n' % str(e)
        try:
            print 'Error test: Attempting to call Remove with no input parameters'
            in_params = {}
            (rval, out_params) = self.conn.InvokeMethod('RemoveHost', self.pool_ref, **in_params)
            if rval == 0:
                print 'RemoveHost worked without any errors. ERROR!!'
                result = 0
        except Exception, e:
            print 'Exception caught : %s\n' % str(e)
        self.TestEnd(result)

    def change_sr_error_tests (self):
        self.TestBegin()
        result = 1
        try:
            print 'Error test: Set the default storage pool to a bad reference'
            in_params = {'StoragePool': self.bad_sr_ref}
            (rval, out_params) = self.conn.InvokeMethod('SetDefaultStoragePool', self.pool_ref, **in_params)
            if rval == 0:
                print 'Setting a default storage to a bad reference worked when it should have failed'
                result = 0
        except Exception, e:
            print "Exception thrown: %s" % str(e)
        try:
            print 'Error test: Set the default storage pool to a NULL reference'
            in_params = {'StoragePool': None}
            (rval, out_params) = self.conn.InvokeMethod('SetDefaultStoragePool', self.pool_ref, **in_params)
            if rval == 0:
                print 'Setting a default storage to a bad NULL reference worked when it should have failed'
                result = 0
        except Exception, e:
            print "Exception thrown: %s" % str(e)
        try:
            print 'Error test: Set the default storage pool and pass it no parameters'
            in_params = {}
            (rval, out_params) = self.conn.InvokeMethod('SetDefaultStoragePool', self.pool_ref, **in_params)
            if rval == 0:
                print 'Setting a default storage by passing it no parameters worked when it should have failed'
                result = 0
        except Exception, e:
            print "Exception thrown: %s" % str(e)

        self.TestEnd(result)

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username and password as arguments "
            print "Count is "+str(count)        
            sys.exit(0)
    Ip = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    pooltest = PoolTest(Ip, username, password)
    print "HostPool Tests"
    try:
        # Success tests
        pooltest.change_default_sr()          # Change the default Storage Repository associated with a pool
        pooltest.change_default_sr2()         # change the default Storage Repository back to the original SR

        #
        # The tests require that licensing be turned off
        # 
        hosts = pooltest.list_hosts()         # get the list of all hosts associated with this Pool
        host_ips = []
        i = 0
        host_ip = '192.168.1.1'               # pick up a real IP address below
        while i < len(hosts):
            host_to_remove = pooltest.select_host_to_remove()   # select the next host to remove
            if host_to_remove != None:
                pooltest.take_host_offline(host_to_remove)      # enter maintainence mode
                pooltest.reboot_host(host_to_remove)            # reboot 
                time.sleep(300)               # need to wait long enough for the host to come back up
                host_ip = pooltest.remove_host(host_to_remove)  # move a host out of the pool
                host_ips.append([host_ip, host_to_remove])      # insert the hosts into a list to be pulled out later
                time.sleep(300)               # Need to wait long enough until the xapi db gets rebuilt
            i = i + 1
        pooltest.create_pool()                # create a new pool which includes the host as the master
        i = 0
        while i < len(host_ips):
            host_handle = host_ips.pop()      # pull out the hosts to be added one by one
            pooltest.add_host(host_handle[0]) # add all hosts back to the pool

        # Error Tests
        pooltest.add_host_error_tests(host_ip)
        pooltest.remove_host_error_tests(host_ip)
        pooltest.change_sr_error_tests()
    finally:
        pooltest.TestCleanup()
    sys.exit(0)
