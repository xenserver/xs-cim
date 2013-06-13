#!/usr/bin/env python

'''Copyright (C) 2008 Citrix Systems Inc.

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
import getpass
import os
from xen_cim_operations import *
from TestSetUp import *

'''
Excercises methods from the Xen_VirtualSystemMigrationService class.
Tests to verify migratability of VMs from one host to another. 
PRECONDITION: Requires that a VM named 'MigrateTestVM' be present.
'''
class KvpMigrationTest(TestSetUp):
    def __init__(self, Ip, userName, password):
        vmForMigration = '57bcb4bc-7550-0c3c-069c-5e2099277f21'

        TestSetUp.__init__(self, Ip, userName, password, True, False)
        self.vsmig = None
        vms_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        for vm_ref in vms_refs:
            print '...>'
            print vm_ref
            if(vm_ref['Name']==vmForMigration) :
                self.migrate_vm = vm_ref
                break;

    def find_migrationservice (self):
        self.TestBegin()
        rc = 1
        hosts = self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        if len(hosts) == 0:
            print "Exception: Couldnt find a host"
            rc = 0
        else:
            association_class = 'CIM_HostedService' # association to traverse via Xen_ComputerSystem
            result_class      = 'CIM_VirtualSystemMigrationService'  # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            self.vsmig             = self.conn.AssociatorNames(hosts[0], **in_params)
            if len(self.vsmig) != 1:
                print 'Error: Couldnt not find a Xen_VirtualSystemMigrationService using HostedVirtualSystemMigrationService'
                rc = 0
            else:
                print 'Found Migration service: ' + str(self.vsmig[0].items())
        self.TestEnd(rc)

    def get_host_for_vm (self, vm_ref):
        association_class = 'Xen_HostedComputerSystem' # Memory RASD
        result_class      = 'Xen_HostComputerSystem'
        in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
        hosts = self.conn.AssociatorNames(vm_ref, **in_params)
        if len(hosts) == 0:
            return None
        return hosts[0]

    def get_host_to_migrate_to (self):
        hosts = self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        if len(hosts) < 2:
            print 'Error: Not enough xen hosts found - (%d)' % len(hosts)
            return None
        current_host = self.get_host_for_vm(self.migrate_vm)
        next_host = hosts[0]
        for host in hosts:
            print 'checking that %s is not %s' % (host['Name'], current_host['Name']) 
            if current_host == None or host['Name'] != current_host['Name']:
                next_host = host
                break
        if current_host == None:
            print 'migrating %s to %s' % (self.migrate_vm['Name'], 
                                          next_host['Name'])
        else:
            print 'migrating %s from %s to %s' % (self.migrate_vm['Name'], 
                                                  current_host['Name'], next_host['Name'])
        return next_host

    def check_if_migrate_vm_to_host_possible (self):
        self.TestBegin()
        rc = 1
        try:
            next_host = self.get_host_to_migrate_to()
            if next_host != None:
                in_params = {'ComputerSystem': self.migrate_vm,
                            'DestinationSystem': next_host}
                (rc, out_params) = self.conn.InvokeMethod('CheckVirtualSystemIsMigratableToSystem', self.vsmig[0], **in_params)
                print 'check_if_migrate_vm_to_host_possible returned %d and ismigratable %d' % (rc, out_params['IsMigratable'])
        except Exception, e:
            print 'Exception %s' % str(e)
        self.TestEnd2(rc)

    def check_if_migrate_vm_to_host_ip_possible (self):
        self.TestBegin()
        rc = 1
        try:
            next_host = self.get_host_to_migrate_to()
            if next_host != None:
                next_host_inst = self.conn.GetInstance(next_host)
                ip_addr = next_host_inst['OtherIdentifyingInfo'][0]
                in_params = {'ComputerSystem': self.migrate_vm,
                            'DestinationHost': ip_addr}
                (rc, out_params) = self.conn.InvokeMethod('CheckVirtualSystemIsMigratableToHost', self.vsmig[0], **in_params)
                print 'check_if_migrate_vm_to_host_ip_possible returned %d and ismigratable %d' % (rc, out_params['IsMigratable'])
        except Exception, e:
            print 'Exception %s' % str(e)
        self.TestEnd2(rc)

    def migrate_vm_to_host (self):
        self.TestBegin()
        rc = 1 
        try:
            next_host = self.get_host_to_migrate_to()
            if next_host != None:
                in_params = {'ComputerSystem': self.migrate_vm,
                            'DestinationSystem': next_host}
                (rc, out_params) = self.conn.InvokeMethod('MigrateVirtualSystemToSystem', self.vsmig[0], **in_params)
                if rc == 4096:
                    job = WaitForJobCompletion(self.conn, out_params['Job'])
                    if job['JobState'] == 7:
                        rc = 0
                    else:
                        rc = job['ErrorCode']
                print 'migrate_vm_to_host returned %d' % rc
        except Exception, e:
            print 'Exception %s' % str(e)
        self.TestEnd2(rc)

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username, and password as arguments"
            print "Count is "+str(count)        
            sys.exit(0)

    mt = KvpMigrationTest(sys.argv[1], sys.argv[2], sys.argv[3])
    try:       
        # Success tests
        mt.find_migrationservice()                      # Find the MigrationService that helps with migration
        mt.check_if_migrate_vm_to_host_possible()       # check if Migrate of a VM to a host is possible (given host's CIM reference)
        mt.check_if_migrate_vm_to_host_ip_possible()    # check if Migrate of a VM to a host is possible (given host's IP address)
        mt.migrate_vm_to_host()                         # migrate a VM to a host (given the host's CIM referenc)

    
    except:
        sys.exit(1)
sys.exit(0)
