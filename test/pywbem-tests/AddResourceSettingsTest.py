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
import os
import getpass
from xen_cim_operations import *
from TestSetUp import *

'''
Exercises the AddResourceSettings method of the Xen_VirtualSystemManagementService class. 
This method allows caller to add new resources (mem, proc, disk, nic) to a VM
'''
class AddResourceSettingsTest(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password)

    def addResourceInvalidAUnit(self):
        self.TestBegin()
        print 'Adding disk rasd with bogus Allocation Units - Expecting failure'
        isSuccess = 1
        disk_list1 = GetDisksForVM(self.conn, self.pv_test_vm)
        disk_list2 = GetDisksForVM(self.conn, self.hvm_test_vm)
        in_params = {'ResourceSetting': self.invalid_aunits_rasd, 'AffectedSystem': self.pv_test_vm}
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.invalid_aunits_rasd, 'AffectedSystem': self.hvm_test_vm}
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        disk_list3 = GetDisksForVM(self.conn, self.pv_test_vm)
        disk_list4 = GetDisksForVM(self.conn, self.hvm_test_vm)
        if (n1 != 0) or (n2 != 0):
            isSuccess = 0
        if (len(disk_list1) + 1 == len(disk_list3)):
            isSuccess = 0
        if (len(disk_list2) + 1 == len(disk_list4)):
            isSuccess = 0
        self.TestEnd(isSuccess)
        
    def addResourceInvalidPoolId(self):
        self.TestBegin()
        print 'Adding disk rasd with invalid Pool ID (bogus SR) - Expecting failure'
        isSuccess = 1
        disk_list1 = GetDisksForVM(self.conn, self.pv_test_vm)
        disk_list2 = GetDisksForVM(self.conn, self.hvm_test_vm)
        in_params = {'ResourceSetting': self.invalid_poolId_rasd, 'AffectedSystem': self.pv_test_vm}
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.invalid_poolId_rasd, 'AffectedSystem': self.hvm_test_vm}
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        disk_list3 = GetDisksForVM(self.conn, self.pv_test_vm)
        disk_list4 = GetDisksForVM(self.conn, self.hvm_test_vm)
        if (n1 != 0) or (n2 != 0):
            isSuccess = 0
        if (len(disk_list1) + 1 == len(disk_list3)):
            isSuccess = 0
        if (len(disk_list2) + 1 == len(disk_list4)):
            isSuccess = 0
        self.TestEnd(isSuccess)
        
    def addValidDisk(self):
        self.TestBegin()
        print 'Adding a valid disk rasd. Expecting no failures'
        isSuccess = 1
        disk_list1 = GetDisksForVM(self.conn, self.pv_test_vm)
        disk_list2 = GetDisksForVM(self.conn, self.hvm_test_vm)
        in_params = {'ResourceSetting': self.disk2_rasd, 'AffectedSystem': self.pv_test_vm}
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.disk2_rasd, 'AffectedSystem': self.hvm_test_vm}
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        disk_list3 = GetDisksForVM(self.conn, self.pv_test_vm)
        disk_list4 = GetDisksForVM(self.conn, self.hvm_test_vm)
        if (n1 != 1) or (n2 != 1):
            isSuccess = 0
        if (len(disk_list1) + 1 != len(disk_list3)):
            isSuccess = 0
        if (len(disk_list2) + 1 != len(disk_list4)):
            isSuccess = 0
        self.TestEnd(isSuccess)
    
    def addValidProcessor(self):
        self.TestBegin()
        processor_list_before1 = GetProcessorsForVM(self.conn, self.pv_test_vm)
        processor_list_before2 = GetProcessorsForVM(self.conn, self.hvm_test_vm)
        print 'Adding a valid processor rasd. Expecting no failures'
        in_params = {'ResourceSetting': self.proc1_rasd, 'AffectedSystem': self.pv_test_vm}
        AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.proc1_rasd, 'AffectedSystem': self.hvm_test_vm}
        AddVMResource(self.conn, self.vsms[0], in_params)
        processor_list1 = GetProcessorsForVM(self.conn, self.pv_test_vm)
        processor_list2 = GetProcessorsForVM(self.conn, self.hvm_test_vm)
        result = 0
        if ((len(processor_list1) != len(processor_list_before1) + self.proc1_rasd['VirtualQuantity']) or 
            (len(processor_list2) != len(processor_list_before2) + self.proc1_rasd['VirtualQuantity'])):
            print 'Found %d procs instead of %d in PV VM' % (len(processor_list1), len(processor_list_before1) + self.proc1_rasd['VirtualQuantity'])
            print 'Found %d procs instead of %d in HVM VM' % (len(processor_list2), len(processor_list_before2) + self.proc1_rasd['VirtualQuantity'])
            result = 0
        else:
            result = 1
        self.TestEnd(result)

    def addInvalidNIC(self):
        self.TestBegin()
        result = 1
        print 'Adding nic rasd with bad MAC address specified. Expecting failure'
        in_params = {'ResourceSetting': self.invalid_nic_rasd, 'AffectedSystem': self.pv_test_vm }
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.invalid_nic_rasd, 'AffectedSystem': self.hvm_test_vm }
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        if ((n1 == 1) or (n2 == 1)):
            print 'AddVMResource call succeeded instead of failing'
            result = 0
        network_list = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        network_list1 = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        if ((len(network_list1) != 1) or (len(network_list) != 1)):
            print 'Found %d NICs instead of 1' % len(network_list)
            result = 0
        self.TestEnd(result)

    def addValidNIC(self):
        self.TestBegin()
        result = 0
        print 'Adding a valid nic rasd. Expecting NO failures.'
        before1 = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        before2 = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        in_params = {'ResourceSetting': self.nic1_rasd, 'AffectedSystem': self.pv_test_vm }
        AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.nic1_rasd, 'AffectedSystem': self.hvm_test_vm }
        AddVMResource(self.conn, self.vsms[0], in_params)
        after1 = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        after2 = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        if ((len(before1)+1 != len(after1)) or (len(before2)+1 != len(after2))):
            print 'Found %d instead of %d for PV vm' % (len(after1), len(before1))
            print 'Found %d instead of %d for HVM vm' % (len(after2), len(before2))
            result = 0
        else:
            result = 1
        self.TestEnd(result)

    def addValidMemory(self):
        self.TestBegin()
        print 'Adding a valid memory rasd, expecting NO failures'
        memory_refs = GetMemoryForVM(self.conn, self.pv_test_vm)
        memory_before1 = self.conn.GetInstance(memory_refs[0])['NumberOfBlocks']
        memory_refs = GetMemoryForVM(self.conn, self.hvm_test_vm)
        memory_before2 = self.conn.GetInstance(memory_refs[0])['NumberOfBlocks']
        in_params = {'ResourceSetting': self.mem1_rasd, 'AffectedSystem':self.pv_test_vm }
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.mem1_rasd, 'AffectedSystem':self.hvm_test_vm }
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        # Verify total memory is 1.5 G
        result = 1
        memory_refs = GetMemoryForVM(self.conn, self.pv_test_vm)
        memory_after1 = self.conn.GetInstance(memory_refs[0])['NumberOfBlocks']
        memory_refs = GetMemoryForVM(self.conn, self.hvm_test_vm)
        memory_after2 = self.conn.GetInstance(memory_refs[0])['NumberOfBlocks']
        if memory_after1 != memory_before1 + self.mem1_rasd['VirtualQuantity']*1024*1024:
            print 'Memory is %d instead of %d for PV VM' % (memory_after1, memory_before1 + self.mem1_rasd['VirtualQuantity']*1024*1024)
            result = 0
        if memory_after2 != memory_before2 + self.mem1_rasd['VirtualQuantity']*1024*1024:
            print 'Memory is %d instead of %d for HVM VM' % (memory_after2, memory_before2 + self.mem1_rasd['VirtualQuantity']*1024*1024)
            result = 0
        if ((result == 1) and (n1 == 1) and (n2 == 1)):
            result = 1
        else:
            result = 0
        self.TestEnd(result)

    def addProcessorInvalidVQ(self):
        self.TestBegin()
        print 'Adding processor rasd with invalid virtual quantity. Expecting failure'
        in_params = {'ResourceSetting': self.invalid_vq_proc_rasd, 'AffectedSystem': self.pv_test_vm}
        AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.invalid_vq_proc_rasd, 'AffectedSystem': self.hvm_test_vm}
        AddVMResource(self.conn, self.vsms[0], in_params)
        processor_list = GetProcessorsForVM(self.conn, self.pv_test_vm)
        print len(processor_list)
        processor_list1 = GetProcessorsForVM(self.conn, self.hvm_test_vm)
        print len(processor_list1)
        if ((len(processor_list1) > 1) or (len(processor_list) > 1)):
            print 'Processor count %d or %d didnt match expected value 1' % (len(processor_list1), len(processor_list))
            result = 0
        else:
            result = 1
        self.TestEnd(result)
            
    def addEmtpyResource(self):
        self.TestBegin()
        print 'Adding an Empty Rasd, Expecting Failure'
        isSuccess = 0
        in_params = {'ResourceSetting': None, 'AffectedSystem':self.pv_test_vm }
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': None, 'AffectedSystem':self.hvm_test_vm }
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        if (n1 == 0) and (n2 == 0):
            isSuccess = 1
        else:
            print 'Was able to add an emtpy resource'
        self.TestEnd(isSuccess)

    def addNullResource(self):
        self.TestBegin()
        print 'Calling AddResource with no resource specified, Expecting failure'
        isSuccess = 0
        network_list1 = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        network_list2 = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        in_params = {'AffectedSystem':self.pv_test_vm }
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'AffectedSystem':self.hvm_test_vm }
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        network_list3 = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        if len(network_list1) == len(network_list3):
            isSuccess = 1
            print "Got Expected Error"
        network_list4 = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        if len(network_list2) == len(network_list4):
            isSuccess = 1
            print "Got Expected Error"
        self.TestEnd(isSuccess)
        
    def addResourceWithNoResourceType(self):
        self.TestBegin()
        print 'Adding NIC rasd with no Resource Type specified. Expecting failure'
        isSuccess = 0
        network_list1 = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        network_list2 = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        in_params = {'ResourceSetting': self.nort_nic_rasd, 'AffectedSystem':self.pv_test_vm }
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.nort_nic_rasd, 'AffectedSystem':self.hvm_test_vm }
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        network_list3 = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        if (len(network_list1) == len(network_list3)):
            isSuccess = 1
            print "Got Expected Error: Cannot add without resource type"
        network_list4 = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        if (len(network_list2) == len(network_list3)):
            isSuccess = 1
            print "Got Expected Error: Cannot add without resource type"
        self.TestEnd(isSuccess)

    def addNICRASDWithSameMAC(self):
        self.TestBegin()
        print 'Adding NIC rasd, which is already part of the test VM. Expecting NO Failures. This is allowed in Xen, i.e. you can add 2 NICs with the same MAC'
        isSuccess = 1
        network_list = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        pv_nics = len(network_list)
        network_list = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        hvm_nics = len(network_list)
        in_params = {'ResourceSetting': self.nic_rasd, 'AffectedSystem':self.pv_test_vm }
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.nic_rasd, 'AffectedSystem':self.hvm_test_vm }
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)
        network_list = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        if len(network_list) != pv_nics + 1:
            isSuccess = 0
            print 'NETWORK COUNT IS %d instead of 2' % len(network_list)
        network_list = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)
        if len(network_list) != hvm_nics + 1:
            isSuccess = 0
            print 'NETWORK COUNT IS %d instead of 2' % len(network_list)
        self.TestEnd(isSuccess)

    def addResourcesWithNullVSSD(self):
        self.TestBegin()
        result = 0
        print 'Adding nic rasd with no VSSD specified'
        in_params = {'ResourceSettings': [self.nic1_rasd]}
        n1 = AddResourcesToVM(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSettings': [self.nic1_rasd]}
        n2 = AddResourcesToVM(self.conn, self.vsms[0], in_params)
        network_list = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        if(n1 == 0 and n2 == 0):
            result = 1
        self.TestEnd(result)


    def addMultipleRASDs(self):
        self.TestBegin()
        print 'Adding NIC/Disk/Processor/Memory rasd at the same time'
        isSuccess = 1
        network_list_before = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        disk_list_before = GetDisksForVM(self.conn, self.pv_test_vm)
        proc_list_before = GetProcessorsForVM (self.conn, self.pv_test_vm)
        mem_ref_list_before  = GetMemoryForVM (self.conn, self.pv_test_vm)
        mem_list_before = []
        for mem in mem_ref_list_before:
            mem_list_before.append(self.conn.GetInstance(mem))
        #Now add them all in one shot
        rasds = [self.proc1_rasd, self.mem1_rasd, self.disk1_rasd, self.nic1_rasd]
         
        # AddResourceSettings takes a VSSD ref instead of a VM ref
        keys = {'InstanceID': 'Xen:' + self.pv_test_vm['Name']}
        affected_conf = CIMInstanceName('Xen_VirtualSystemSettingData', keys)
        in_params = {'ResourceSettings': rasds, 'AffectedConfiguration':affected_conf }
        n1 = AddResourcesToVM(self.conn, self.vsms[0], in_params)

        network_list_after = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        disk_list_after = GetDisksForVM(self.conn, self.pv_test_vm)
        proc_list_after = GetProcessorsForVM (self.conn, self.pv_test_vm)
        mem_ref_list_after  = GetMemoryForVM (self.conn, self.pv_test_vm)
        mem_list_after = []
        for mem in mem_ref_list_after:
            mem_list_after.append(self.conn.GetInstance(mem))

        if len(network_list_after) != len(network_list_before) + 1:
            isSuccess = 0
            print 'VIF count is %d instead of %d' % (len(network_list_after), len(network_list_before) + 1)
        if len(disk_list_after) != len(disk_list_before) + 1:
            isSuccess = 0
            print 'VBD count is %d instead of %d' % (len(disk_list_after), len(disk_list_before) + 1)
        if len(proc_list_after) != len(proc_list_before) + self.proc1_rasd['VirtualQuantity']:
            isSuccess = 0
            print 'Proc count is %d instead of %d' % (len(proc_list_after), len(proc_list_before) + self.proc1_rasd['VirtualQuantity'])
        if mem_list_after[0]['NumberOfBlocks'] != mem_list_before[0]['NumberOfBlocks'] + self.mem1_rasd['VirtualQuantity']*1024*1024:
            isSuccess = 0
            print 'Memory is %d instead of %d' % (mem_list_after[0]['NumberOfBlocks'], mem_list_before[0]['NumberOfBlocks'] + self.mem1_rasd['VirtualQuantity']*1024*1024)

        self.TestEnd(isSuccess)

########################################################

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username and password as arguments "
            print "Count is "+str(count)        
            sys.exit(0)
    Ip = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    cd = AddResourceSettingsTest(Ip, username, password)
    try:
        print "Test AddResourceSetting"
        # Negative tests
        cd.addEmtpyResource()           # Add an empty resource
        cd.addNullResource()            # Add with no resource specified
        cd.addResourceWithNoResourceType()  # Add a resource with not resource type specified
        cd.addResourcesWithNullVSSD()   # Add a resource but dont specify which VM to add to
        cd.addProcessorInvalidVQ()      # Add a RASD with invalid Virtual Quantity
        cd.addResourceInvalidAUnit()    # Add a RASD with invalid Allocation Units
        cd.addResourceInvalidPoolId()   # Add a RASD with invalid PoolID
        cd.addInvalidNIC()              # Add a NIC RASD with invalid MAC address
        cd.addMultipleRASDs()           # Add NIC/Disk/Mem/Proc rasds all at once

        # Success tests
        cd.addNICRASDWithSameMAC()      # Add a NIC resource with the same MAC twice. XenServer allows this.
        cd.addValidDisk()               # Add a valid Disk
        cd.addValidProcessor()          # Add a valid Processor
        cd.addValidNIC()                # Add a valid NIC
        cd.addValidMemory()             # Add a valid Memory

    finally:
        cd.TestCleanup()

    sys.exit(0)
    
