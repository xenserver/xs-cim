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
Exercises the DestroySystem method of the Xen_VirtualSystemManagementService class
This allows caller to delete a VM and (its undelying resources, except shared ISOs)
'''
class DestroySystemTest(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password)

    # Success test
    #     Create a VM with disk, CD drives, network virtual resources. 
    #     Delete it.
    #   Expected: 
    #     The VMs are deleted. All the virtual resources (disk and network) are deleted.
    #     The underlying physical ISO file used for the CD Drive is NOT deleted
    def Delete_VM(self):
        self.TestBegin()

        # In addition to 1 hard drive and a NIC, add a 3rd ISO CDrom and make sure the ISO file is not deleted at the end
        disk2_rasd = CIMInstance('Xen_DiskSettingData')
        disk2_rasd['ElementName'] = "testcdrom_via_iso"
        disk2_rasd['ResourceType'] = pywbem.Uint16(15)
        disk2_rasd['ResourceSubType'] = "CD"
        disk2_rasd['Device'] = "0"
        disk2_rasd['Bootable'] = False
        disk2_rasd['Access'] = pywbem.Uint8(1)

        # Specify Physical ISO file to use
        xen_tools_iso = self.conn.ExecQuery("WQL", "SELECT * FROM Xen_DiskImage WHERE ElementName = \"xs-tools.iso\"", "root/cimv2")
        num_isos = len(xen_tools_iso)
        disk2_rasd['HostResource'] = ["root/cimv2:Xen_DiskImage.DeviceID=\""+xen_tools_iso[0]['DeviceID']+"\""]
        rasds = [self.proc_rasd, self.mem_rasd, disk2_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_vssd = self.hvm_vssd.copy()
        hvm_vssd['ElementName'] = sys._getframe(0).f_code.co_name + '_HVM'
        pv_vssd = self.pv_vssd.copy()
        pv_vssd['ElementName'] = sys._getframe(0).f_code.co_name + '_PV'
        hvm_params = {'SystemSettings': hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        if ((new_pv_vm == None) and (new_hvm_vm == None)):
            print "VMs are not created"
            self.publish_result(0)
        print "VMs are created"

        # Keep track of all the resources to check for, once deleted
        disk_list = GetDisksForVM(self.conn, new_pv_vm)
        hvm_cd_list = GetDiskDrivesForVM(self.conn, new_hvm_vm)
        pv_cd_list = GetDiskDrivesForVM(self.conn, new_pv_vm)
        hvm_disk_list = GetDisksForVM(self.conn, new_hvm_vm)
        pv_net_list = GetNetworkPortsForVM(self.conn, new_pv_vm)
        rasdlist = []
        print "Network in rasd"
        for net in pv_net_list:
            pv_net_rasd_list = GetNetworkPortRASDs(self.conn, net)
            print len(pv_net_rasd_list)
            for pv_net_rasd in pv_net_rasd_list:
                print pv_net_rasd['InstanceID']
                rasdlist.append([str(pv_net_rasd['InstanceID'])])
        hvm_net_list = GetNetworkPortsForVM(self.conn, new_hvm_vm)
        for net in hvm_net_list:
            hvm_net_rasd_list = GetNetworkPortRASDs(self.conn, net)
            for hvm_net_rasd in hvm_net_rasd_list:
                print hvm_net_rasd['InstanceID']
                rasdlist.append([str(hvm_net_rasd['InstanceID'])])
        All_Disk_List = self.conn.EnumerateInstanceNames("Xen_Disk")
        for disk in All_Disk_List:
            for this_disk in hvm_disk_list:
                if (disk['DeviceID'] == this_disk['DeviceID']):
                    print "HVM_Disk is found before deletion"
            for this_disk in disk_list:
                if (disk['DeviceID'] == this_disk['DeviceID']):
                    print "PV_Disk is found before deletion"
        for disk_drive_ref in pv_cd_list:
            disk_drive = self.conn.GetInstance(disk_drive_ref)
            if disk_drive['ElementName'] != 'xs-tools.iso':
                print "PV_DiskDrive %s found. failure" % disk_drive['ElementName']
                self.publish_result(0)
        for disk_drive_ref in hvm_cd_list:
            disk_drive = self.conn.GetInstance(disk_drive_ref)
            if disk_drive['ElementName'] != 'xs-tools.iso':
                print "HVM DiskDrive %s found. failure" % disk_drive['ElementName']
                self.publish_result(0)

        # Now delete the VM
        pv_val = DeleteVM(self.conn, self.vsms[0], new_pv_vm)
        hvm_val = DeleteVM(self.conn, self.vsms[0], new_hvm_vm)

        # Verify that the virtual resuorces are all gone (except the physical ISO file)
        xen_tools_iso = self.conn.ExecQuery("WQL", "SELECT * FROM Xen_DiskImage WHERE ElementName = \"xs-tools.iso\"", "root/cimv2")
        num_isos_left = len(xen_tools_iso)
        All_net_List = self.conn.EnumerateInstanceNames("Xen_NetworkPortSettingData")
        netNotFound = 1
        for net in All_net_List:
            #print "Network"
            #print net.items()
            net_instance_id = str(net['InstanceID'])
            print net_instance_id 
            for rasd_net in rasdlist:
                if (net_instance_id == rasd_net):
                    print "Network rasd found after deletion"
                    netNotFound = 0
        All_Disk_List = self.conn.EnumerateInstanceNames("Xen_Disk")
        pv_notFound = 1
        hvm_notFound = 1
        for disk in All_Disk_List:
            for this_disk in hvm_disk_list:
                if (disk['DeviceID'] == this_disk['DeviceID']):
                    print "HVM_Disk is found after deletion %s" % disk['DeviceID']
                    hvm_notFound = 0
            for this_disk in disk_list:
                if (disk['DeviceID'] == this_disk['DeviceID']):
                    print "Disk is found after deletion %s " % disk['DeviceID']
                    pv_notFound = 0

        result = 0
        if ((pv_notFound == 1) and (hvm_notFound == 1) and (netNotFound == 1) and (pv_val == 1) and (hvm_val ==1) and (num_isos_left == num_isos)):
            result = 1
        else:
            print "%d %d %d %d %d %d %d" % (pv_notFound, hvm_notFound, netNotFound, pv_val, hvm_val, num_isos_left, num_isos)
            result = 0
        self.TestEnd(result)

    # Error test
    #     Try deleting a VM that does not exist (wrong CIM reference)
    #   Expected: The CIM call fails with an error.
    # 
    def delete_vm_not_exist(self):
        self.TestBegin()
        vm_name = "non-existing"
        val = DeleteVM(self.conn, self.vsms[0], vm_name)
        restul = 0
        if val == 0:
            print "Deleting a non-existing VM failed as expected"
            result = 1
        else:
            print "Deleting a non-existing VM reports success, not expected"
            result = 0
        self.TestEnd(result)

    # Error test
    #     Try deleting a VM that is turned ON. 
    #  Requirement: The platform supports HVM VMs 
    #  Expected: 
    #     The CIM call is supposed to shut down the VM and delete it.
    #     However, the HVM VM fails to shut down cleanly (since it has no xen-tools installed), 
    #     So, it will not be deleted by the CIM call.
    #     The PV VM will shut down cleanly and be deleted
    # 
    def delete_vm_on_state(self):
        self.TestBegin()
        result = 0

        print 'Creating and Starting a test PV and HVM VM'
        print 'TestRequirement: The underlying hardware platform supports HVM VMs (CPU is VT enabled)'

        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_vssd = self.hvm_vssd.copy()
        hvm_vssd['ElementName'] = sys._getframe(0).f_code.co_name + '_HVM'
        pv_vssd = self.pv_vssd.copy()
        pv_vssd['ElementName'] = sys._getframe(0).f_code.co_name + '_PV'
        hvm_params = {'SystemSettings': hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds}
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        if ((new_pv_vm == None) and (new_hvm_vm == None)):
            print "VMs are not created"
            self.publish_result(0)

        in_params = {'RequestedState':'2'} # Start the VMs
        ChangeVMState(self.conn, new_hvm_vm, in_params, True, 2)
        ChangeVMState(self.conn, new_pv_vm, in_params, True, 2)


        # Try deleting them, it should attempt to shutdown the VM and delete them
        val1 = DeleteVM(self.conn, self.vsms[0], new_hvm_vm)
        print "Deleting VM"
        if val1 == 0:
            print "DeleteVM failed for HVM VM with error since there are no tools installed and it couldnt shut it down cleanly, as expected"
            # Hardshutdown the VM, since no tools are installed (soft-shutdown will not work)
            in_params = {'RequestedState':'32768'} 
            ChangeVMState(self.conn, new_hvm_vm, in_params, True, 3)
            DeleteVM(self.conn, self.vsms[0], new_hvm_vm)
            result = 1
        else:
            print "Erroneously deleted an existing vm in ON state"
            result = 0
        val2 = DeleteVM(self.conn, self.vsms[0], new_pv_vm)
        if val2:
            print "DeleteVM succeeded for PV VM (clean shutdown and delete) as expected"
        else:
            print "DeleteVM for PV VM failed with error"
            if result:
                result = 0
        
        self.TestEnd(result)

    # Error test
    #     Pass in a NULL CIM reference to the DeleteVM method
    #  Expected: The CIM call fails with an error.
    # 
    def delete_null_vm(self):
        self.TestBegin()
        val = DeleteVM(self.conn, self.vsms[0], None)
        restul = 0
        if val == 0:
            print "cannot delete a non existing vm as expected"
            result = 1
        else:
            print "Deleting a non-existing VM succeeded, not expected"
            result = 0
        self.TestEnd(result)

    # Error test
    #     Pass in a bad CIM reference (types mixed up) to the DeleteVM method
    #  Expected: The CIM call fails with an error.
    # 
    def delete_vm_with_bad_reference(self):
        self.TestBegin()
        bad_vm_ref = self.hvm_test_vm.copy()
        bad_vm_ref['Name'] = 1123 # change the type of the key value to integer
        val = DeleteVM(self.conn, self.vsms[0], bad_vm_ref)
        restul = 0
        if val == 0:
            print "Deleting a non-existing VM failed as expected"
            result = 1
        else:
            print "Deleting a non-existing VM reports success, not expected"
            result = 0
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
    cd = DestroySystemTest(Ip, username, password)
    try:
        # Successful test
        cd.Delete_VM()  # delete a VM and make sure all the resources associated with it are deleted as well, (except underlying ISO files representing CD-Rom devices)

        # Error scenarios
        cd.delete_vm_with_bad_reference()  # delete a VM with a bad CIM reference
        cd.delete_vm_not_exist() # delete a VM that doesnt exist
        cd.delete_null_vm()      # delete a VM with a None (NULL) CIM reference 
        cd.delete_vm_on_state()  # delete a VM which is currently running
    finally:
        cd.TestCleanup()

    sys.exit(0)
    
