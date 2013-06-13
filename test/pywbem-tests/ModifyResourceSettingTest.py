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
from xen_cim_operations import *
from TestSetUp import *

'''
Exercises the ModifyResourceSettings method of the Xen_VirtualSystemManagementService class
Allows caller to modify resources used by a vm (change a disks's properties, a network card's properties etc)
'''
class ModifyResourceSetting(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password)
        
    def modifyValidResources_disk(self):
        self.TestBegin()
        query_str = 'SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID LIKE "%s"' % ('%'+self.get_vm_id(self.hvm_test_vm)+'%')
        print 'executing ' + query_str
        disks_rasd = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        rasds_to_modify = []
        print "Total resources before modify "
        print len(disks_rasd)
        for rasd_orig in disks_rasd:
            if rasd_orig['ElementName'] == self.__class__.__name__ + '_Disk1':
                # BUGBUG Pegasus doesnt like passing back the results from the call above. It complains about bad XML.
                rasd = CIMInstance('Xen_DiskSettingData')
                rasd['ResourceType'] = rasd_orig['ResourceType']
                rasd['InstanceID']   = rasd_orig['InstanceID']
                rasd['HostResource'] = rasd_orig['HostResource']
                rasd['ElementName']  = 'modifiedvdi'
                rasds_to_modify.append(rasd)
        in_params = {'ResourceSettings': rasds_to_modify}
        try:
            ModifyVMResources(self.conn, self.vsms[0], in_params)
        except Exception, e:
            isSuccess = 0
        # get the RASDs again to verify the remove
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        print "Total resources after modify "
        print len(rasds)
        isSuccess = 0
        for rasd in rasds:
            if rasd != None and rasd['ElementName'] != None:
                print 'New RASD elementname:' + rasd['ElementName']
                if rasd['ElementName'] == 'modifiedvdi':
                    print "Modified"
                    isSuccess = 1
        self.TestEnd(isSuccess)

    def attachAndDetachISOToEmptyDVD (self):
        self.TestBegin()
        hvm_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        hvm_vssd['Description'] = 'test-hvm-vm'
        hvm_vssd['ElementName'] = sys._getframe(0).f_code.co_name
        hvm_vssd['VirtualSystemType'] = 'DMTF:xen:HVM'
        hvm_vssd['HVM_Boot_Params'] = ['order=dc']
        hvm_vssd['HVM_Boot_Policy'] = 'BIOS order'
        hvm_vssd['Platform'] = ['acpi=true','apic=true','pae=true']

        empty_cd_rasd = CIMInstance('Xen_DiskSettingData')
        empty_cd_rasd['ElementName'] = "testcdrom"
        empty_cd_rasd['ResourceType'] = pywbem.Uint16(15)
        empty_cd_rasd['ResourceSubType'] = 'CD'

        # Create the test VM
        print 'creating VM'
        rasds = [empty_cd_rasd]
        hvm_params = {'SystemSettings': hvm_vssd, 'ResourceSettings': rasds}
        hvm_test_vm = CreateVM(self.conn, self.vsms[0], hvm_params)

        query_str = 'SELECT * FROM Xen_DiskSettingData WHERE InstanceID LIKE "%' + hvm_test_vm['Name'] + '%"'
        cd_rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")

        # BUGBUG: Specify a new RASD here, because pegasus doesnt like us sending back what it sent us in the above query resposen
        cd_rasd = CIMInstance('Xen_DiskSettingData')
        cd_rasd['ResourceType'] = cd_rasds[0]['ResourceType']
        cd_rasd['InstanceID'] = cd_rasds[0]['InstanceID']
        # Specify the ISO to attach to the DVD
        query_str = 'SELECT * FROM Xen_DiskImage WHERE ElementName = \"xs-tools.iso\"'
        tools_isos = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        cd_rasd['HostResource'] = ['root/cimv2:Xen_DiskImage.DeviceID=\"'+ tools_isos[0]['DeviceID']+'\"']
        print cd_rasd.items()
        print 'Inserting xs-tools.iso into the empty DVD'
        rasds = [cd_rasd]
        modify_params = {'ResourceSettings': rasds }
        attach_result = ModifyVMResources(self.conn, self.vsms[0], modify_params)

        # Verify the updated HostResource
        query_str = 'SELECT * FROM Xen_DiskSettingData WHERE InstanceID LIKE "%' + hvm_test_vm['Name'] + '%"'
        if attach_result == 1:
            cd_rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
            print 'Updated HostResource: %s' % cd_rasds[0]['HostResource']

        # Detach the ISO and leave an empty DVD behind
        print 'Detaching xs-tools.iso from the DVD'
        cd_rasd['HostResource'] = ['Xen_DiskImage.DeviceID=\"\"']
        rasds = [cd_rasd]
        modify_params = {'ResourceSettings': rasds }
        detach_result = ModifyVMResources(self.conn, self.vsms[0], modify_params)

        # Verify the updated HostResource
        if detach_result == 1:
            cd_rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
            print 'Updated HostResource: %s' % cd_rasds[0]['HostResource']
            if cd_rasds[0]['HostResource'] == None:
                print 'detach success'

        result = attach_result and detach_result

        # Cleanup
        DeleteVM(self.conn, self.vsms[0], hvm_test_vm)
        self.TestEnd(result)


    def modifyValidResources_network_address(self):
        self.TestBegin()
        # This is not a valid test anymore.. modifying a mac address by removing an existing vif and creating a new one
        # under the covers is not valid because it changes the key property of the CIM obejct (uuid).
        query_str = 'SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID LIKE "%s"' % ('%'+self.get_vm_id(self.hvm_test_vm)+'%')
        print 'executing ' + query_str
        net_rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        rasds_to_modify = []
        print "Total resources before modify "
        print len(net_rasds)
        for rasd_orig in net_rasds:
            if rasd_orig['Address'] == "00:13:72:24:32:f4":
                # BUGBUG pegasus doesnt like us passing the same instanec back. Complains about bad XML
                rasd = CIMInstance('Xen_NetworkPortSettingData')
                rasd['ResourceType'] = rasd_orig['ResourceType']
                rasd['InstanceID'] = rasd_orig['InstanceID']
                rasd['Address'] = '00:13:72:24:32:45'
                rasds_to_modify.append(rasd)
        in_params = {'ResourceSettings': rasds_to_modify}
        ModifyVMResources(self.conn, self.vsms[0], in_params)
        # get the RASDs again to verify the remove
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        print "Total resources after modify "
        print len(rasds)
        isSuccess = 0
        for rasd in rasds:
            if rasd['Address'] == '00:13:72:24:32:45':
                print "Modified nic is found"
                isSuccess = 1
        self.TestEnd(isSuccess)

    def modifyResources_Invalid_Pool(self):
        self.TestBegin()
        print self.hvm_test_vm.items()
        query_str = 'SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID LIKE "%s"' % ('%'+self.get_vm_id(self.hvm_test_vm)+'%')
        print 'executing ' + query_str
        disks_rasd = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        rasds_to_modify = []
        print "Total resources before modify "
        print len(disks_rasd)
        result = 0
        for rasd_orig in disks_rasd:
            if rasd_orig['ElementName'] == self.__class__.__name__ + '_Disk1':
                # BUGBUG Pegasus doesnt like passing back the results from the call above. It complains about bad XML.
                rasd = CIMInstance('Xen_DiskSettingData')
                rasd['ResourceType'] = rasd_orig['ResourceType']
                rasd['InstanceID'] = rasd_orig['InstanceID']
                rasd['HostResource'] = rasd_orig['HostResource']
                rasd['PoolId'] = "ed1bd47e-1ab8-d80a-aecf-06447871211c" # random PoolID
                rasds_to_modify.append(rasd)
        in_params = {'ResourceSettings': rasds_to_modify}
        try:
            val = ModifyVMResources(self.conn, self.vsms[0], in_params)
        except Exception, e:
            print 'Caught exception %s while modifying VM resource' % str(e)
        result = 1
        # get the RASDs again to verify the remove
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        print "Total resources after modify "
        print len(rasds)
        for rasd in rasds:
            if (rasd['ElementName'] == self.__class__.__name__ + '_Disk1'):
                print rasd.items()
                if (rasd['PoolId'] == "ed1bd47e-1ab8-d80a-aecf-06447871211c"):
                    print "Resource is modified"
                    result = 0
                    break
        self.TestEnd(result)

    def modifyResources_invalid_address(self):
        self.TestBegin()
        query_str = 'SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID LIKE "%s"' % ('%'+self.get_vm_id(self.hvm_test_vm)+'%')
        print 'executing ' + query_str
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        rasds_to_modify = []
        print "Total resources before modify "
        result = 0
        print len(rasds)
        for rasd_orig in rasds:
            if rasd_orig['Address'] == "00:13:72:24:32:f4":
                # BUGBUG pegasus doesnt like us passing the same instanec back. Complains about bad XML
                rasd = CIMInstance('Xen_NetworkPortSettingData')
                rasd['ResourceType'] = rasd_orig['ResourceType']
                rasd['InstanceID'] = rasd_orig['InstanceID']
                rasd['Address'] = '00:13:72:24:32:rt'
                rasds_to_modify.append(rasd)
        in_params = {'ResourceSettings': rasds_to_modify}
        try:
            result = ModifyVMResources(self.conn, self.vsms[0], in_params)
        except Exception, e:
            result = 1;
        print result
        # get the RASDs again to verify the remove
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        print "Total resources after modify "
        print len(rasds)
        for rasd in rasds:
            if (rasd['ResourceType'] == 33):
                print rasd.items()
                if (rasd['Address'] == "00:13:72:24:32:rr"):
                    print "Resource is not modified"
                    result = 0
                    break
        self.TestEnd(result)

    def modifyValidResources_processor_VQ(self):
        self.TestBegin()
        query_str = 'SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID LIKE "%s"' % ('%'+self.get_vm_id(self.hvm_test_vm)+'%')
        print 'executing ' + query_str
        proc_rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        rasds_to_modify = []
        print "Total resources before modify "
        print len(proc_rasds)
        for rasd_orig in proc_rasds:
            if rasd_orig['ResourceType'] == 3:
                # BUGBUG pegasus doesnt like us passing the same instanec back. Complains about bad XML
                rasd = CIMInstance('Xen_ProcessorSettingData')
                rasd['ResourceType'] = rasd_orig['ResourceType']
                rasd['InstanceID'] = rasd_orig['InstanceID']
                rasd['VirtualQuantity'] = pywbem.cim_types.Uint64(5)
                rasds_to_modify.append(rasd)
        in_params = {'ResourceSettings': rasds_to_modify}
        ModifyVMResources(self.conn, self.vsms[0], in_params)
        # get the RASDs again to verify the remove
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        print "Total resources after modify "
        print len(rasds)
        isSuccess = 0
        for rasd in rasds:                        
            if (rasd['ResourceType'] == 3):
                print rasd.items()
                if (rasd['VirtualQuantity'] != 5):
                    print "Resource is not modified"
                    isSuccess = 0
                    break
                else:
                    print "Modified proc is found"
                    isSuccess = 1
        self.TestEnd(isSuccess)

    def modifyValidResources_memory_VQ_AU(self):
        self.TestBegin()
        query_str = 'SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID LIKE "%s"' % ('%'+self.get_vm_id(self.hvm_test_vm)+'%')
        print 'executing ' + query_str
        mem_rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        rasds_to_modify = []
        print "Total resources before modify "
        print len(mem_rasds)
        for rasd_orig in mem_rasds:
            if rasd_orig['ResourceType'] == 4:
                # BUGBUG pegasus doesnt like us passing the same instance back. Complains about bad XML
                rasd = CIMInstance('Xen_MemorySettingData')
                rasd['ResourceType'] = rasd_orig['ResourceType']
                rasd['InstanceID'] = rasd_orig['InstanceID']
                rasd['VirtualQuantity'] = pywbem.cim_types.Uint64(16384)
                rasd['AllocationUnits'] = "bytes*2^20"
                rasds_to_modify.append(rasd)
        in_params = {'ResourceSettings': rasds_to_modify}
        n = ModifyVMResources(self.conn, self.vsms[0], in_params)
        # get the RASDs again to verify the remove
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        print "Total resources after modify "
        print len(rasds)
        isSuccess = 0
        for rasd in rasds:                        
            if (rasd['ResourceType'] == 4):
                print rasd.items()
                if (rasd['VirtualQuantity'] != 16384):
                    print "Resource is not modified"
                    isSuccess = 0
                    break
                else:
                    print "Memory was modified correctly"
                    isSuccess = 1
        self.TestEnd(isSuccess)
            
    def modifyResources_invalid_resource_type(self):
        self.TestBegin()
        query_str = 'SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID LIKE "%s"' % ('%'+self.get_vm_id(self.hvm_test_vm)+'%')
        print 'executing ' + query_str
        mem_rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        rasds_to_modify = []
        result = 1
        print "Total resources before modify "
        print len(mem_rasds)       
        for rasd_orig in mem_rasds:
            if rasd_orig['ResourceType'] == 4:
                rasd = CIMInstance('Xen_MemorySettingData')
                rasd['ResourceType'] = rasd_orig['ResourceType']
                rasd['InstanceID'] = rasd_orig['InstanceID']
                rasd['ResourceType'] = pywbem.cim_types.Uint16(100)
                rasds_to_modify.append(rasd)
        try:
            in_params = {'ResourceSettings': rasds_to_modify}
            result=ModifyVMResources(self.conn, self.vsms[0], in_params)
        except Exception, e:
            result=0 # expect this to throw an exception
        # get the RASDs again to verify the remove
        print 'ModifyVMResources returned %d' % result
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        print "Total resources after modify "
        print len(rasds)
        for rasd in rasds:                        
            if (rasd['ResourceType'] == 500):
                print "Can modify with invalid resource type"
                result = 1
        self.TestEnd2(result)
        
        
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
    cd = ModifyResourceSetting(Ip, username, password )
    ##########################################################
    print "Test on ModifyResourceSetting"
    try:
        # Excercise the ModifyResourceSettings method of the Xen_VirtualsystemManagementService class
        # Successful tests
        cd.modifyValidResources_disk()            # Modify the settings of a disk associated with a VM
        cd.modifyValidResources_processor_VQ()    # Modify the processor settings of a VM
        cd.modifyValidResources_memory_VQ_AU()    # Modify the memory settings of a VM
        cd.attachAndDetachISOToEmptyDVD()         # Create a VM with an empty DVD, attach an ISO to the DVD and detach the ISO.

        # Error scenarios
        cd.modifyResources_invalid_resource_type() # Modify the resource settings using a RASD with invalid resource type specified
        cd.modifyResources_Invalid_Pool()          # Modify the resource settings using a RASD with invalid Pool ID 
        cd.modifyResources_invalid_address()       # Modify the NIC settings to an invalid MAC address
    finally:
        pass
        cd.TestCleanup()
    sys.exit(0)
    
