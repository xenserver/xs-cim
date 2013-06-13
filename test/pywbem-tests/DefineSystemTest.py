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
from xen_cim_operations import *
import os
import getpass
from TestSetUp import *

'''
Exercises the DefineSystem method of the Xen_VirtualSystemManagementService class.
This method allows caller to create a new VM with resource requirements (mem, proc, disk, nic) 
specified or unspecificied (default).
'''
class DefineSystemTest(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, True, True)
        self.pv_vssd = CIMInstance("Xen_ComputerSystemSettingData")
        self.pv_vssd['Caption']     = "pv_create_new_VM"
        self.pv_vssd["ElementName"] = "pv_create_new_VM"
        self.pv_vssd["VirtualSystemType"] = "DMTF:xen:PV"
        self.pv_vssd["PV_Bootloader"] = "pygrub"
        self.pv_vssd["PV_Args"] = "Term=xterm"

        self.local_hvm_vssd = CIMInstance("Xen_ComputerSystemSettingData")
        self.local_hvm_vssd["Caption"] = "hvm_create_new_VM"
        self.local_hvm_vssd["ElementName"] = "hvm_create_new_VM"
        self.local_hvm_vssd["VirtualSystemType"] = "DMTF:xen:HVM"
        self.local_hvm_vssd["HVM_Boot_Params"] = ["order=dc"]
        self.local_hvm_vssd["HVM_Boot_Policy"] = "BIOS order"
        self.local_hvm_vssd["Platform"] = ["acpi=true","apic=true","pae=true"]

        # A VSSD with incorrect property types
        self.invalid_pv_vssd = self.pv_vssd.copy()
        self.invalid_pv_vssd['VirtualSystemType'] = pywbem.Uint8(10) # bad type, integer instead of string
        self.invalid_pv_vssd['PV_Bootloader'] = pywbem.Uint8(20)     # bad boot loader, integer instead of string
        self.invalid_pv_vssd['AutomaticShutdownAction'] = '2'  # bad shutdown action type, string instead of integer
        self.invalid_pv_vssd['PV_Args'] = 'Blah'       # bad PV_Args

        self.no_pv_vssd = CIMInstance("Xen_ComputerSystemSettingData")
        self.no_pv_vssd['VirtualSystemIdentifier'] = '20904d23-8a89-1d63-134c-d2606f2fcc47'
        self.no_pv_vssd['Description'] = 'test-pv-vm'


##############################################################################        

    def _VMs_were_created (self, vm_list):
        result = 1
        for vm in vm_list:
            if (vm == None):
                print "VM was not created"
                result = 0
            else:
                vm_inst = self.conn.GetInstance(vm)
                if (vm_inst == None):
                    print "VM was not created."
                    result = 0
        return result

    def _Delete_VMs (self, vm_list):
        for vm in vm_list:
            if (vm != None):
                DeleteVM(self.conn, self.vsms[0], vm)

    def create_new_VM(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        time.sleep(3)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_hvm_vm, new_pv_vm])
        self.TestEnd(result)

    def provision_VM_from_template (self):
        self.TestBegin()
        result = 0
        pv_templates = self.conn.ExecQuery("WQL", "SELECT * FROM Xen_ComputerSystemTemplate WHERE ElementName = \"Demo Linux VM\"", "root/cimv2")
        if len(pv_templates) == 0:
            print 'Demo Linux VM template was not found. Make sure you have the XenServer Linux tools installed on the Xenserver'
        else:
            pv_template =  CIMInstanceName(classname=pv_templates[0].classname, keybindings={"InstanceID":pv_templates[0]["InstanceID"]})
            print pv_template.items()
            print '-------------------'
            hvm_templates = self.conn.ExecQuery("WQL", "SELECT * FROM Xen_ComputerSystemTemplate WHERE ElementName LIKE \"Windows Server 2008%\"", "root/cimv2")
            #Create a template reference from the instance above - other way of a creating VM from a template - recommended way
            hvm_template = CIMInstanceName(classname=hvm_templates[0].classname, keybindings={"InstanceID":hvm_templates[0]["InstanceID"]})
            print hvm_template.items()
            print '-------------------'
            pv_vssd = CIMInstance("Xen_ComputerSystemSettingData")
            pv_vssd["Description"] = "Test VM based on Demo Linux VM template"
            pv_vssd["ElementName"] = "debian-etch-test";

            hvm_vssd = CIMInstance("Xen_ComputerSystemSettingData")
            hvm_vssd["Description"] = "Test VM based on W2k8 template"
            hvm_vssd["ElementName"] = "w2k8-test"

            rasds = [self.proc_rasd, self.mem_rasd]
            hvm_params = {'SystemSettings': hvm_vssd, 'ResourceSettings': rasds, 'ReferenceConfiguration': hvm_template}
            pv_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds, 'ReferenceConfiguration': pv_template}
            print 'create vm from template test'
            new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
            new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
            time.sleep(3)
            result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
            self._Delete_VMs([new_hvm_vm, new_pv_vm])
        self.TestEnd(result)

    def copy_existing_VM (self):
        self.TestBegin()
        result = 0
        # Base a VM off of a template (disks should be provisioned to the SR specified)
        pv_templates = self.conn.ExecQuery("WQL", "SELECT * FROM Xen_ComputerSystemTemplate WHERE ElementName LIKE \"%XenServer Transfer VM%\"", "root/cimv2")
        if len(pv_templates) == 0:
            print 'Demo Linux VM template was not found. Make sure you have the XenServer Linux tools installed on the Xenserver'
        else:
            ref_template = CIMInstanceName(classname=pv_templates[0].classname, keybindings={"InstanceID":pv_templates[0]["InstanceID"]})
            print ref_template.items()
            # new VM definitions
            vssd1 = CIMInstance("Xen_ComputerSystemSettingData")
            vssd1['Description'] = "Test VM based on a Linux VM template"
            vssd1['ElementName'] = "TemplateCopyTest"
            vssd1['Other_Config'] = ["folder=/", "HideFromXenCenter=false"] 
            copytemplate_params =  {'SystemSettings': vssd1, 'StoragePool':self.sr_to_use, 'ReferenceConfiguration': ref_template}
            print 'Copy vm from template'
            new_vm1 = CopyVM(self.conn, self.vsms[0], "TemplateCopyTest", copytemplate_params)

            # Base a new VM off another VM (just make a copy of the new VM above)
            vssd2 = CIMInstance("Xen_ComputerSystemSettingData")
            vssd2["Description"] = "Test VM based on another VM"
            vssd2["ElementName"] = "VMCopyTest"
            vms = self.conn.ExecQuery("WQL", "SELECT * FROM Xen_ComputerSystemSettingData WHERE ElementName = \"TemplateCopyTest\"", "root/cimv2")
            ref_vm =  CIMInstanceName(classname=vms[0].classname, keybindings={"InstanceID":vms[0]["InstanceID"]})
            print ref_vm.items()
            copyvm_params =  {'SystemSettings': vssd2, 'StoragePool':self.sr_to_use, 'ReferenceConfiguration': ref_vm}
            print 'Copy vm from VM'
            new_vm2 = CopyVM(self.conn, self.vsms[0], "VMCopyTest", copyvm_params)

            # Make sure the VMs were created with the right metadata
            localresult0 = 0
            new_vssd_ref = CIMInstanceName('Xen_ComputerSystemSettingData')
            new_vssd_ref['InstanceID'] = 'Xen:'+new_vm1['Name']
            new_vssd = self.conn.GetInstance(new_vssd_ref)
            vm_inst1 = self.conn.GetInstance(new_vm1)
            vm_inst2 = self.conn.GetInstance(new_vm2)
            found_folder = False
            found_xshide = False
            for other_config in new_vssd['Other_Config']:
                if other_config == "folder=/":
                    found_folder = True
                elif other_config == "HideFromXenCenter=false":
                    found_xshide = True
            if vm_inst1['ElementName'] == vssd1['ElementName'] and vm_inst1['Description'] == vssd1['Description'] and found_folder == True and found_xshide == True and vm_inst2['ElementName'] == vssd2['ElementName'] and vm_inst2['Description'] == vssd2['Description']:
                localresult0 = 1
    
            # Try starting the VM, they should work, also verify that it started
            print 'Try starting them'
            in_params = {'RequestedState':'2'} # Start the VM
            localresult1 = ChangeVMState(self.conn, new_vm1, in_params, True, 2)
            localresult2 = ChangeVMState(self.conn, new_vm2, in_params, True, 2)
    
            # stop the VM for cleanup
            print 'Try stopping them'
            in_params = {'RequestedState':'4'} # Stop the VM
            localresult3 = ChangeVMState(self.conn, new_vm1, in_params, True, 4)
            localresult4 = ChangeVMState(self.conn, new_vm2, in_params, True, 4)

            # cleanup
            print 'Delete them'
            self._Delete_VMs([new_vm1, new_vm2])
            print 'Results: %d %d %d %d %d' % (localresult0, localresult1, localresult2, localresult3, localresult4)
            result = localresult0 and localresult1 and localresult2 and localresult3 and localresult4

        self.TestEnd(result )

    def convert_vm_to_template (self):
        self.TestBegin()
        result = 0
        print 'Copying %s to a template' % self.hvm_test_vm['Name']
        in_params = {'System':self.hvm_test_vm}
        new_template = ConvertVMToTemplate(self.conn, self.vsms[0], in_params)
        if new_template != None:
            print 'New Template: %s' % str(new_template.items())
            result = 1
        self.TestEnd(result)

    def create_VM_invalid_aUnits(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.invalid_aunits_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        time.sleep(3)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd2(result)
    
    def create_VM_invalid_PoolId(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.invalid_poolId_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        time.sleep(3)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd2(result)

    def create_VM_invalid_VQ(self):
        self.TestBegin()
        rasds = [self.invalid_vq_proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        time.sleep(2)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd2(result)

    def create_without_rasd(self):
        #rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        self.TestBegin()
        hvm_params = {'SystemSettings': self.hvm_vssd}
        pv_params =  {'SystemSettings': self.pv_vssd}
        print 'create vm without RASD test'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd(result)

    def create_with_invalid_nic(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.invalid_nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test with invalid nic'
        result = 0
        try:
            new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
            new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        except Exception, e:
            print e
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd2(result)

    def create_vm_without_resourceType(self):
        self.TestBegin()
        rasds = [self.nort_proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test with invalid nic'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd2(result)

    def create_without_vssd(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_params = {'ResourceSettings': rasds}
        pv_params =  {'ResourceSettings': rasds}
        print 'create vm test without vssd'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd2(result)

    def create_with_invalid_vssd(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        pv_params =  {'SystemSettings': self.invalid_pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test with invalid vssd'
        result = 0
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        result = self._VMs_were_created([new_pv_vm])
        self._Delete_VMs([new_pv_vm])
        self.TestEnd2(result)

    def create_with_emtpy_vssd(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        pv_params =  {'SystemSettings': self.no_pv_vssd, 'ResourceSettings': rasds}
        print 'create vm test with no vssd'
        new_pv_vm = None
        try:
            new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        except Exception, e:
            print e
            if e == "InvokeMethod(instancename): VirtualSystemType not specified":
                print " Excpetion got"
        result = self._VMs_were_created([new_pv_vm])
        self._Delete_VMs([new_pv_vm])
        self.TestEnd2(result)

    def create_without_rc(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds}
        result = 0
        print 'create vm test without reference configuaration'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd(result)

    def create_vm_with_invalid_resource_type(self):
        self.TestBegin()
        rasds = [self.invalid_proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds, 'ReferenceConfiguration': self.hvm_vssd}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds, 'ReferenceConfiguration': self.pv_vssd}
        print 'create vm test with invalid resource type for processor'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd2(result)

    def create_vm_with_invalid_rasd(self):
        self.TestBegin() 
        result = 1
        # This time specify strings where integer properties are expected, 
        # This used to cause crashes in the MOF string parsing logic at one point
        rasds = [self.proc_rasd, self.mem_rasd, self.bad_disk_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds, 'ReferenceConfiguration': self.hvm_vssd}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds, 'ReferenceConfiguration': self.pv_vssd}
        print 'create vm test with invalid resource type for processor'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        result = self._VMs_were_created([new_pv_vm, new_hvm_vm])
        self._Delete_VMs([new_pv_vm, new_hvm_vm])
        self.TestEnd2(result)

############################################################################

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username and password as arguments "
            print "Count is "+str(count)        
            sys.exit(0)
    Ip = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    cd = DefineSystemTest(Ip, username, password  )
    print "Test define System"
    try:
        # Exercise the DefineSystem method of the Xen_virtualSystemManagementService class in various scenarios
        # Success tests
        cd.create_new_VM()                      # Create VM based just on the settings passed in
        cd.provision_VM_from_template()         # Create VM based on a Xen template
        cd.create_without_rasd()                # create VM without any RASDs, just a VSSD
        cd.create_without_rc()                  # create without 'ReferenceConfiguration' (template) specified
        cd.copy_existing_VM()                   # Copy an existing VM and a template with disks (full disk copy)
        cd.convert_vm_to_template()             # Convert HVM VM to template

        # Failure tests                                        
        cd.create_VM_invalid_VQ()               # create VM with invalid VirtualQuantity in the RASD
        cd.create_VM_invalid_PoolId()           # create VM with invalid PoolID specified in the RASD
        cd.create_VM_invalid_aUnits()           # create VM with invalid Allocation Units specified in the RASD
        cd.create_with_invalid_nic()            # create VM with an invalid NIC
        cd.create_vm_without_resourceType()     # craete VM with no resource type specified in the RASD
        cd.create_without_vssd()                # create VM with no VSSD specified
        cd.create_with_invalid_vssd()           # create VM with an invalid VSSD specified
        cd.create_with_emtpy_vssd()             # create VM by specifying an empty VSSD
        cd.create_vm_with_invalid_resource_type() # create VM with invalid resource type specified in the RASD
        cd.create_vm_with_invalid_rasd()         # a bad RASd with property types mixed up (strings for integers and integers for strings)
    finally:
        cd.TestCleanup()
    sys.exit(0)
    
