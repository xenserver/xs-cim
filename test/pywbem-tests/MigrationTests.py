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
class MigrationTest(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, True, False)
        # Find the VMMigration service
        self.vsmig = None
        self.LocalTestSetup()

    def LocalTestSetup (self):
        # NOTE: Find the VM to migrate - this test requires a MigrateTestVM to be already present with the xen tools already installed.
        vssd = CIMInstance('Xen_ComputerSystemSettingData')
        vssd['ElementName'] = "MigrateTestVMBase"
        vssd['Description'] = "VM to test migration"
        vssd['Other_Config'] = ["HideFromXenCenter=false"] 
        target_vm = CreateVMBasedOnTemplateName(self.conn, self.vsms[0], "XenServer Transfer VM", vssd)

        # Since the transfer vm template is in local storage, lets try to duplicate the VM on shared storage
        keys = {'InstanceID': 'Xen:%s' % target_vm['Name']}
        new_vm_vssd = CIMInstanceName('Xen_ComputerSytemSettingData', keys)
        vssd['ElementName'] = "MigrateTestVM"
        copytemplate_params =  {'SystemSettings':vssd, 'StoragePool':self.sr_to_use, 'ReferenceConfiguration':new_vm_vssd}
        self.migrate_vm = CopyVM(self.conn, self.vsms[0], "MigrateTestVM", copytemplate_params)

        # Start the VM
        in_params = { 'RequestedState' : '2' }
        ChangeVMState (self.conn, self.migrate_vm, in_params, True, '2')

        # Done with the original vm, delete it
        DeleteVM(self.conn, self.vsms[0], target_vm)

    def LocalTestCleanup (self):
        if self.migrate_vm != None:
            in_params = { 'RequestedState' : '4' }
            ChangeVMState (self.conn, self.migrate_vm, in_params, True, '4')
            DeleteVM(self.conn, self.vsms[0], self.migrate_vm)
        TestSetUp.TestCleanup(self)

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

    def find_migrationservice2 (self):
        # According to use case 9.1.1 in the Virtual System Migration profile
        self.TestBegin()
        rc = 0
        association_class = "CIM_ServiceAffectsElement"
        result_class = "CIM_VirtualSystemMigrationService"
        in_params = {'ResultClass': result_class, 'AssocClass': association_class}
        self.vsmig = self.conn.AssociatorNames(self.migrate_vm, **in_params)
        if len(self.vsmig) != 1:
            print 'Error: Couldnt not find a Xen_VirtualSystemMigrationService using CIM_ServiceAffectsElement'
        else:
            print 'Found Migration service: ' + str(self.vsmig[0].items())
            rc = 1
        self.TestEnd(rc)

    def discover_migration_capabilites (self):
        self.TestBegin()
        rc = 0
        association_class = "CIM_ElementCapabilities"
        result_class = "CIM_VirtualSystemMigrationCapabilities"
        in_params = {'ResultClass': result_class, 'AssocClass': association_class}
        caps = self.conn.AssociatorNames(self.vsmig[0], **in_params)
        for cap_ref in caps:
            cap = self.conn.GetInstance(cap_ref)
            print 'Found Migration Capabilities: ' + str(cap.items())
            if 3 in cap['DestinationHostFormatsSupported']:
                rc = 1
        # Make sure there's just 1 capabilities instanece and that the hostformat supported includes IP address
        if len(caps) != 1:
            rc = 0
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

    def migrate_vm_to_host_ip (self):
        self.TestBegin()
        rc = 1 
        try:
            next_host = self.get_host_to_migrate_to()
            if next_host != None:
                next_host_inst = self.conn.GetInstance(next_host)
                if next_host_inst != None:
                    ip_addr = next_host_inst['OtherIdentifyingInfo'][0]
                    in_params = {'ComputerSystem': self.migrate_vm,
                                'DestinationHost': ip_addr}
                    (rc, out_params) = self.conn.InvokeMethod('MigrateVirtualSystemToHost', self.vsmig[0], **in_params)
                    if rc == 4096:
                        job = WaitForJobCompletion(self.conn, out_params['Job'])
                        if job['JobState'] == 7:
                            rc = 0
                        else:
                            rc = job['ErrorCode']
                else:
                    print "Error getting the instance"
                print 'migrate_vm_to_host_ip returned %d' % rc
        except Exception, e:
            print 'Exception %s' % str(e)
        self.TestEnd2(rc)

    def migrate_vm_to_host_name (self):
        self.TestBegin()
        rc = 1 
        try:
            next_host = self.get_host_to_migrate_to()
            if next_host != None:
                next_host_inst = self.conn.GetInstance(next_host)
                if next_host_inst != None:
                    print 'Migrate to host name %s' % next_host_inst['ElementName']
                    hostname = next_host_inst['ElementName']
                    in_params = {'ComputerSystem': self.migrate_vm,
                                'DestinationHost': hostname}
                    (rc, out_params) = self.conn.InvokeMethod('MigrateVirtualSystemToHost', self.vsmig[0], **in_params)
                    if rc == 4096:
                        job = WaitForJobCompletion(self.conn, out_params['Job'])
                        if job['JobState'] == 7:
                            rc = 0
                        else:
                            rc = job['ErrorCode']
                else:
                    print "Error getting the instance"
                print 'migrate_vm_to_host_ip returned %d' % rc
        except Exception, e:
            print 'Exception %s' % str(e)
        self.TestEnd2(rc)

    def find_migration_service_error_test (self):
        self.TestBegin()
        result = 1
        hosts = self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        if len(hosts) == 0:
            print "Exception: Couldnt find hosts"
            result = 0
        else:
            print 'Error Test: None refernce being passed as SourceClass'
            try:
                association_class = 'CIM_HostedService' 
                result_class      = 'CIM_VirtualSystemMigrationService' 
                in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
                vsmig             = self.conn.AssociatorNames(None, **in_params) # use a NULL reference
                if len(vsmig) != 0:
                    print 'Error: Found a VSMS without a host' # found a VSMS inspite of an errorneous call
                    result = 0
            except Exception, e:
                print 'Exception %s' % str(e)
            print 'Error Test: Bad AssociationClass'
            try:
                association_class = 'CIM_HostedComputerSystem'  # WRONG association class 
                result_class      = 'CIM_VirtualSystemMigrationService'
                in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
                vsmig             = self.conn.AssociatorNames(hosts[0], **in_params)
                if len(vsmig) != 0:
                    print 'Error: Found a VSMS with the wrong association'
                    result = 0
            except Exception, e:
                print 'Exception %s' % str(e)
        self.TestEnd(result)

    def check_if_migrate_vm_possible_error_test (self):
        self.TestBegin()
        result = 1
        print 'Error Test: Bad IP address'
        try:
            in_params = {'ComputerSystem': self.migrate_vm,
                        'DestinationHost': '192.168.1.1'} # <<<- error
            (rc, out_params) = self.conn.InvokeMethod('CheckVirtualSystemIsMigratableToHost', self.vsmig[0], **in_params)
            if rc == 0 and out_params['IsMigratable'] == 1:
                result = 0
            print 'MigrateToHost returned %d and ismigratable %d' % (rc, out_params['IsMigratable'])
        except Exception, e:
            print 'exception thrown %s' % str(e)
        host = self.get_host_to_migrate_to()
        print 'Error Test: host reference passed instead of IP address'
        try:
            in_params = {'ComputerSystem': self.migrate_vm,
                        'DestinationHost': host} # <<--- error
            (rc, out_params) = self.conn.InvokeMethod('CheckVirtualSystemIsMigratableToHost', self.vsmig[0], **in_params)
            if rc == 0 and out_params['IsMigratable'] == 1:
                result = 0
            print 'MigrateToHost returned %d and ismigratable %d' % (rc, out_params['IsMigratable'])
        except Exception, e:
            print 'exception thrown %s' % str(e)
        print 'Error Test: NULL host reference'
        try:
            in_params = {'ComputerSystem': self.migrate_vm,
                        'DestinationSystem': None} # <<< ERROR
            (rc, out_params) = self.conn.InvokeMethod('CheckVirtualSystemIsMigratableToSystem', self.vsmig[0], **in_params)
            if rc == 0 and out_params['IsMigratable'] == 1:
                result = 0
            print 'MigrateToHost returned %d and ismigratable %d' % (rc, out_params['IsMigratable'])
        except Exception, e:
            print 'exception thrown %s' % str(e)
        print 'Error Test: Bad VM reference'
        try:
            migrate_vm = self.migrate_vm.copy()
            migrate_vm['Name'] = 'bad reference' 
            in_params = {'ComputerSystem': migrate_vm, # <<<< ERROR
                        'DestinationSystem': host}
            (rc, out_params) = self.conn.InvokeMethod('CheckVirtualSystemIsMigratableToSystem', self.vsmig[0], **in_params)
            if rc == 0 and out_params['IsMigratable'] == 1:
                result = 0
            print 'MigrateToHost returned %d and ismigratable %d' % (rc, out_params['IsMigratable'])
        except Exception, e:
            print 'exception thrown %s' % str(e)
        print 'Error Test: Bad Host reference'
        try:
            host = self.get_host_to_migrate_to()
            host['Name'] = 'aaaa-123-455=235=-346356345' # bad uuid (key) for host
            in_params = {'ComputerSystem': self.migrate_vm,
                        'DestinationSystem': host} # <<<< ERROR
            (rc, out_params) = self.conn.InvokeMethod('CheckVirtualSystemIsMigratableToSystem', self.vsmig[0], **in_params)
            if rc == 0 and out_params['IsMigratable'] == 1:
                result = 0
            print 'MigrateToHost returned %d and ismigratable %d' % (rc, out_params['IsMigratable'])
        except Exception, e:
            print 'exception thrown %s' % str(e)
        self.TestEnd(result)

    def migrate_vm_error_test (self):
        self.TestBegin()
        result = 1
        bad_migrate_vm = CIMInstanceName('Xen_ComputerSystem')
        bad_migrate_vm['Name'] = 'bad-reference'
        bad_migrate_vm['CreationClassName'] = 'Xen_ComputerSystem'
        bad_host_reference = CIMInstanceName('Xen_HostComputerSystem')
        bad_host_reference['Name'] = 'bad reference'
        bad_host_reference['CreationClassName'] = 'Xen_HostComputerSystem'
        print 'Error Test: Bad IP address'
        try:
            in_params = {'ComputerSystem': self.migrate_vm,
                        'DestinationHost': '192.168.1.1'} # <<<< ERROR
            (rc, out_params) = self.conn.InvokeMethod('MigrateVirtualSystemToHost', self.vsmig[0], **in_params)
            print 'MigrateToHost returned %d' % rc
            if rc == 0:
                result = 0
        except Exception, e:
            print 'Exception occured %s' % str(e)

        print 'Error Test: Host reference where IP address is required'
        try:
            in_params = {'ComputerSystem': self.migrate_vm,
                        'DestinationHost': next_host} # <<<< ERROR
            (rc, out_params) = self.conn.InvokeMethod('MigrateVirtualSystemToHost', self.vsmig[0], **in_params)
            print 'MigrateToHost returned %d' % rc
            if rc == 0:
                result = 0
        except Exception, e:
            print 'Exception occured %s' % str(e)

        print 'Error Test: Bad VM reference'
        try:
            in_params = {'ComputerSystem': bad_migrate_vm, # <<<< ERROR
                        'DestinationSystem': next_host} 
            (rc, out_params) = self.conn.InvokeMethod('MigrateVirtualSystemToSystem', self.vsmig[0], **in_params)
            print 'MigrateToHost returned %d' % rc
            if rc == 0:
                result = 0
        except Exception, e:
            print 'Exception occured %s' % str(e)

        print 'Error Test: None host reference'
        try:
            in_params = {'ComputerSystem': self.migrate_vm,
                        'DestinationSystem': None}
            (rc, out_params) = self.conn.InvokeMethod('MigrateVirtualSystemToSystem', self.vsmig[0], **in_params)
            print 'MigrateToHost returned %d' % rc
            if rc == 0:
                result = 0
        except Exception, e:
            print 'Exception occured %s' % str(e)
        self.TestEnd(result)

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username, and password as arguments"
            print "Count is "+str(count)        
            sys.exit(0)

    mt = MigrationTest(sys.argv[1], sys.argv[2], sys.argv[3])
    try:       
        # Success tests
        mt.find_migrationservice()                      # Find the MigrationService that helps with migration
        mt.find_migrationservice2()                     # Find the MigrationService according to profile use case 9.1.1 
        mt.discover_migration_capabilites()             # Discover the migration capabilities (what kind of migrations does the service support)
        mt.check_if_migrate_vm_to_host_possible()       # check if Migrate of a VM to a host is possible (given host's CIM reference)
        mt.check_if_migrate_vm_to_host_ip_possible()    # check if Migrate of a VM to a host is possible (given host's IP address)
        mt.migrate_vm_to_host()                         # migrate a VM to a host (given the host's CIM referenc)
        mt.migrate_vm_to_host_ip()                      # migrate a VM to a host (give the host's IP address)
        mt.migrate_vm_to_host_name()                    # migrate a VM to a host (give the hostname)

        # Error tests
        mt.find_migration_service_error_test()          # errors in finding the migration service
        mt.check_if_migrate_vm_possible_error_test()    # check for all kinds of errors in the check API
        mt.migrate_vm_error_test()                      # check for all kinds of errors in the migrate api
    finally:
        mt.LocalTestCleanup()
    
    sys.exit(0)

