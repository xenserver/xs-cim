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
Exercises the RemoveResourceSettings method of the Xen_VirtualSystemManagementService class.
Allows caller to remove resources (mem, proc, disk, nic) associated with a VM
'''
class RemoveResourceSetting(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password)
        
#########################################################################################
    # Test on RemoveResourceSetting

    #passed
    def removeValidResources(self):
        self.TestBegin()
        isSuccess = 0
        print 'deleting vm resources'
        # get all RASDs and select the ones we want to delete. Use the LIKE query to do it
        vssds = GetVSSDsForVM(self.conn, self.hvm_test_vm)
        if vssds != None:
            rasds = GetRASDsFromVSSD(self.conn, vssds[0])
            rasds_to_delete = []
            print "Total resources before delete "
            print len(rasds)
            # Delete requires refs to RASDs and not instances or strings
            for rasd_orig in rasds:
                if rasd_orig['ElementName'] == self.__class__.__name__ + '_Disk1':
                    rasd = CIMInstance('Xen_DiskSettingData')
                    rasd['InstanceID'] = rasd_orig['InstanceID']
                    rasd['ResourceType'] = rasd_orig['ResourceType']
                    rasds_to_delete.append(rasd)
                if rasd_orig['Address'] != None and rasd_orig['Address'][0] == "00:13:72:24:32:f4":
                    rasd = CIMInstance('Xen_NetworkPortSettingData')
                    rasd['InstanceID'] = rasd_orig['InstanceID']
                    rasd['ResourceType'] = rasd_orig['ResourceType']
                    rasds_to_delete.append(rasd)
            in_params = {'ResourceSettings': rasds_to_delete}
            rval = DeleteVMResources(self.conn, self.vsms[0], in_params)
            if rval == 1:
                # get the RASDs again to verify the remove
                rasds = GetRASDsFromVSSD(self.conn, vssds[0])
                print "Total resources after delete "
                print len(rasds)
                foundRasds = False
                for rasd in rasds:
                    if ((rasd['ElementName'] == self.__class__.__name__ + '_Disk1') or (rasd['Address'] != None and rasd['Address'][0] == "00:13:72:24:32:f4")):
                        print "Resource is not deleted"
                        foundRasds = True
                        break
                if foundRasds == False:
                    isSuccess = 1
        self.TestEnd(isSuccess)
        
    #failed: bug 669
    def removeNonExistingResources(self):
        self.TestBegin()
        local_disk_rasd = CIMInstance('CIM_ResourceAllocationSettingData')
        local_disk_rasd['PoolID'] = self.DiskPoolID
        local_disk_rasd['Elementname'] = self.__class__.__name__ + '_Disk1'
        local_disk_rasd['ResourceType'] = pywbem.Uint16(19)
        local_disk_rasd['ResourceSubType'] = "Disk"
        local_disk_rasd['VirtualQuantity'] = pywbem.Uint64(2147483648)
        local_disk_rasd['AllocationUnits'] = "Bytes"

        rasds_to_delete = []
        rasds_to_delete.append(local_disk_rasd)
        in_params = {'ResourceSettings': rasds_to_delete}
        n = DeleteVMResources(self.conn, self.vsms[0], in_params)
        result = 0
        if (n == 1):
            print "Deleted non existing resource"
            result = 0
        else:
            result = 1
        self.TestEnd(result)
    
    def removeNullResources(self):
        self.TestBegin()
        print 'deleting NULL resources'
        rasds_to_delete = []
        in_params = {'ResourceSettings': rasds_to_delete}
        n = DeleteVMResources(self.conn, self.vsms[0], in_params)
        result = 0
        if (n == 1):
            print "Deleted NULL resorces!!!"
            result = 0
        else:
            result = 1
        self.TestEnd(result)

    def removeBadResources(self):
        self.TestBegin()
        print 'deleting Bad resources'
        bad_disk_rasd = CIMInstance('CIM_ResourceAllocationSettingData')
        bad_disk_rasd['PoolID'] = pywbem.Uint32(12345)
        bad_disk_rasd['Elementname'] = pywbem.Uint32(12345)
        bad_disk_rasd['ResourceType'] = "19"
        bad_disk_rasd['ResourceSubType'] = pywbem.Uint32(12345)
        bad_disk_rasd['VirtualQuantity'] = "2147483648"
        bad_disk_rasd['AllocationUnits'] = pywbem.Uint32(0)

        bad_network_rasd = CIMInstance('CIM_ResourceAllocationSettingData')
        bad_network_rasd['ResourceType'] = "33"
        bad_network_rasd['AllocationUnits'] = pywbem.Uint32(0)

        rasds_to_delete = [bad_disk_rasd, bad_network_rasd]
        in_params = {'ResourceSettings': rasds_to_delete}
        n = DeleteVMResources(self.conn, self.vsms[0], in_params)
        result = 0
        if (n == 1):
            print "Deleted Bad resorces!!!"
            result = 0
        else:
            result = 1
        self.TestEnd(result)

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
    cd = RemoveResourceSetting(Ip, username, password)
    try:
        print "Test RemoveResourceSetting method of the Xen_VirtualSystemManagementService class"
        # Success tests
        cd.removeValidResources()           # Remove a valid disk and a NIC associated with a VM.

        # Error scenarios
        cd.removeNullResources()            # Call RemoveResourceSettings by specifying a NULL resource
        cd.removeNonExistingResources()     # Call RemoveResourceSettings and specify a non-existent resource
        cd.removeBadResources()             # Call RemoveResourceSettings with a bad RASD
    finally:
        print 'Cleanup'
        cd.TestCleanup()

    sys.exit(0)
    
