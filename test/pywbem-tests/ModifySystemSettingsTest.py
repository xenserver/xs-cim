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
Exercises the ModifySystemSettings method of the Xen_VirtualSystemManagementService class
Allows caller to modify the VM's properties
'''
class ModifySystemSettings(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password)
        self.pv_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        self.pv_vssd['InstanceID'] = "Xen:<VMID>"
        self.pv_vssd['ElementName'] = self.__class__.__name__ + "_PV"
        self.pv_vssd['VirtualSystemType'] = "DMTF:xen:PV"
        self.pv_vssd['PV_Bootloader'] = "pygrub"
        self.pv_vssd['PV_Bootloader_Args'] = ""
        self.pv_vssd['PV_Args'] = "Term=xterm"

#########################################################################################
    # Test on ModifySystemSetting
    
    def modify_system_setting(self):
        self.TestBegin()
        print 'modify vm test'
        new_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        new_vssd['InstanceID'] = "Xen:" + self.pv_test_vm['Name']
        new_vssd['PV_Args'] = "Term=xterm1"
        new_vssd['PV_Bootloader'] = 'mybootloader'

        #print self.pv_test_vm['Name']
        in_params = {'SystemSettings': new_vssd} 
        ModifyVM(self.conn, self.vsms[0], in_params)
        vssds = GetVSSDsForVM(self.conn, self.pv_test_vm)
        isSuccess = 1
        if vssds == None:
            print 'Couldnt find VSSD for PV VM after modify'
            isSuccess = 0
        for vssd in vssds:
            vssd_inst = self.conn.GetInstance(vssd)
            #print vssd_inst['VirtualSystemIdentifier'] 
            if vssd_inst['PV_Args'] != "Term=xterm1":
                print 'Kernel Options is %s' % vssd_inst['PV_Args']
                isSuccess = 0
            if vssd_inst['PV_Bootloader'] != "mybootloader":
                print 'Bootloader is %s' % vssd_inst['PV_Bootloader']
                isSuccess = 0
        self.TestEnd(isSuccess)

    def modifySystemSetting_ElementName(self):
        self.TestBegin()
        isSuccess = 1
        pv_vssd = self.pv_vssd
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.nic_rasd]
        in_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds}

        new_vm = CreateVM(self.conn, self.vsms[0], in_params)
        time.sleep(3)
        if (new_vm == None):
            print "Test VM is not created"

        # Get the latest VSSD (which includes the InstanceID)
        pv_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        pv_vssd['InstanceID'] = 'Xen:' +  new_vm['Name']
        pv_vssd['ElementName'] = 'Changed_ElementName'
        print pv_vssd

        in_params = {'SystemSettings': pv_vssd} 
        n = ModifyVM(self.conn, self.vsms[0], in_params)
        time.sleep(2)
        if (n == 1):
            print "Modification done"
        else:
            print "Not modified"
            isSuccess = 0
        vssds = GetVSSDsForVM(self.conn, new_vm)
        if vssds == None:
            print 'Couldnt find VSSD for PV VM after modify'
            isSuccess = 0
        for vssd in vssds:
            vssd_inst = self.conn.GetInstance(vssd)
            if (vssd_inst['VirtualSystemIdentifier'] == new_vm['Name']):
                print "Current ElementName : "+vssd_inst['ElementName']
                print "ElementName desired : Changed_ElementName"
                testElem = str(vssd_inst['ElementName'])
                index = testElem.find("Changed_ElementName")
                if (index == -1):
                    isSuccess = 0
        pv_val = DeleteVM(self.conn, self.vsms[0], new_vm)    
        self.TestEnd(isSuccess)

    def modifySystemSetting_Description(self):
        self.TestBegin()
        isSuccess = 1
        pv_vssd = self.pv_vssd
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.nic_rasd]
        in_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds}
        new_vm = CreateVM(self.conn, self.vsms[0], in_params)
        time.sleep(3)
        pv_vssd = pv_vssd.replace('modify-pv-description','Modified Description')
        pv_vssd = pv_vssd.replace('<VMID>', new_vm['Name'])
        print pv_vssd

        in_params = {'SystemSettings': pv_vssd} 
        n = ModifyVM(self.conn, self.vsms[0], in_params)
        time.sleep(2)
        if (n == 1):
            print "Modification done"
        else:
            print "Not modified"
            isSuccess = 0
        vssds = GetVSSDsForVM(self.conn, new_vm)
        
        if vssds == None:
            print 'Couldnt find VSSD for PV VM after modify'
            isSuccess = 0
        for vssd in vssds:
            vssd_inst = self.conn.GetInstance(vssd)
            
            if (vssd_inst['VirtualSystemIdentifier'] == new_vm['Name']):
                print "Current description : "+vssd_inst['Description']
                print "Description desired : Modified Description"
                if (vssd_inst['Description'] != "Modified Description"):
                    isSuccess = 0
                
        pv_val = DeleteVM(self.conn, self.vsms[0], new_vm)    
        self.TestEnd(isSuccess)

    def modifySystemSetting_VSType(self):
        self.TestBegin()
        isSuccess = 0
        pv_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        pv_vssd['Description'] = 'modify-pv-description'
        pv_vssd['ElementName'] = sys._getframe(0).f_code.co_name
        pv_vssd['VirtualSystemType'] = "DMTF:xen:PV"
        pv_vssd['PV_Bootloader'] = "pygrub"
        pv_vssd['PV_Bootloader_Args'] = ""
        pv_vssd['PV_Args'] = 'Term=xterm'

        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.nic_rasd]
        in_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds}
        new_vm = CreateVM(self.conn, self.vsms[0], in_params)
        time.sleep(3)

        pv_vssd['InstanceID']  = 'Xen:' + new_vm['Name']
        pv_vssd['VirtualSystemType'] = 'DMTF:xen:HVM'
        pv_vssd['PV_Bootloader'] = ""
        pv_vssd['PV_Bootloader_Args'] = ""
        pv_vssd['PV_Args'] = ''
        pv_vssd['HVM_Boot_Params'] = ["order=dc"];
        pv_vssd['HVM_Boot_Policy'] = "BIOS order";
        print pv_vssd
        in_params = {'SystemSettings': pv_vssd} 
        n = ModifyVM(self.conn, self.vsms[0], in_params)
        time.sleep(2)
        if (n == 1):
            print "Modification done"
            isSuccess = 1
        else:
            print "Not modified"
        vssds = GetVSSDsForVM(self.conn, new_vm)
        if vssds == None:
            print 'Couldnt find VSSD for PV VM after modify'
        for vssd in vssds:
            vssd_inst = self.conn.GetInstance(vssd)
            print vssd_inst.tomof()
            if (vssd_inst['VirtualSystemIdentifier'] == new_vm['Name']):
                print "Current VirtualSystemType : "+vssd_inst['VirtualSystemType']
                print "VS Type desired : hvm"
                vstype = str(vssd_inst['VirtualSystemType'])
                index = vstype.find("HVM")
                if (index != -1):
                    isSuccess = 1
        pv_val = DeleteVM(self.conn, self.vsms[0], new_vm)    
        self.TestEnd(isSuccess)

    def modifySystemSetting_OtherVSType(self):
        self.TestBegin()

        pv_vssd = self.pv_vssd
        isSuccess = 1
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.nic_rasd]
        in_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds}
        
        new_vm = CreateVM(self.conn, self.vsms[0], in_params)
        time.sleep(3)
        pv_vssd = pv_vssd.replace('xenSystem','hvmSystem')
        pv_vssd = pv_vssd.replace('<VMID>', new_vm['Name'])
        print pv_vssd

        in_params = {'SystemSettings': pv_vssd} 
        n = ModifyVM(self.conn, self.vsms[0], in_params)
        time.sleep(2)
        if (n == 1):
            print "Modification done"
        else:
            print "Not modified"
            isSuccess = 0
        vssds = GetVSSDsForVM(self.conn, new_vm)
        
        if vssds == None:
            print 'Couldnt find VSSD for PV VM after modify'
            isSuccess = 0
        for vssd in vssds:
            vssd_inst = self.conn.GetInstance(vssd)
            
            if (vssd_inst['VirtualSystemIdentifier'] == new_vm['Name']):
                print "Current OtherVirtualSystemType : "+vssd_inst['OtherVirtualSystemType']
                print "VS Type desired : hvmSystem"
                testStr = vssd_inst['OtherVirtualSystemType']
                index = testStr.find("hvmSystem")
                if (index == -1):
                    isSuccess = 0
                
        pv_val = DeleteVM(self.conn, self.vsms[0], new_vm)    
        self.TestEnd(isSuccess)
        
    def modifySystemSetting_InvalidVSType(self):
        self.TestBegin()
        pv_vssd = self.pv_vssd
        isSuccess = 1
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.nic_rasd]
        in_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds}
        new_vm = CreateVM(self.conn, self.vsms[0], in_params)
        vssds = GetVSSDsForVM(self.conn, new_vm)
        vssd_inst = self.conn.GetInstance(vssds[0])
        vstypebefore = vssd_inst['VirtualSystemType']
        print "Created VirtualSystemType : "+ vssd_inst['VirtualSystemType']
        pv_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        pv_vssd['InstanceID'] = "Xen:" + new_vm['Name']
        pv_vssd['VirtualSystemType'] = "InvalidVSType"
        print pv_vssd
        in_params = {'SystemSettings': pv_vssd} 
        n = ModifyVM(self.conn, self.vsms[0], in_params)
        time.sleep(2)
        vssds = GetVSSDsForVM(self.conn, new_vm)
        if vssds == None:
            print 'Couldnt find VSSD for PV VM after modify'
            isSuccess = 0
        for vssd in vssds:
            vssd_inst = self.conn.GetInstance(vssd)
            if (vssd_inst['VirtualSystemIdentifier'] == new_vm['Name']):
                print "Current VirtualSystemType : "+ vssd_inst['VirtualSystemType']
                vstypeafter = str(vssd_inst['VirtualSystemType'])
                index = vstypeafter.find("DMTF:Xen:PV")
                if (index == -1 or vstypeafter != vstypebefore): # Make sure the VSType didnt change
                    isSuccess = 0
        pv_val = DeleteVM(self.conn, self.vsms[0], new_vm)    
        self.TestEnd(isSuccess)
    
    def modify_non_existing_system_setting(self):
        self.TestBegin()
        print 'modify vm test'
        new_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        new_vssd['PV_Args'] = "Term=xterm1"
        new_vssd['PV_Bootloader'] = "mybootloader"
        vmid = "ed1bd47e-1ab8-d80a-aecf-06447871211b"
        new_vssd['VirtualSystemIdentifier'] = vmid
        vms_list = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        for vms in vms_list:
            if (vms['name'] == vmid):
                print "Existing VSSD: invalid test"
                return 0            
        print "VSDD to be changed:"
        print new_vssd
        in_params = {'SystemSettings': new_vssd} 
        ModifyVM(self.conn, self.vsms[0], in_params)
        #vssds = GetVSSDsForVM(self.conn, self.pv_test_vm)
        vssds = self.conn.EnumerateInstanceNames("Xen_ComputerSystemSettingData")
        isSuccess = 1
        print "VSSDs found"
        if vssds == None:
            print 'Couldnt find VSSD for PV VM after modify'
            isSuccess = 0
        for vssd in vssds:
            vssd_inst = self.conn.GetInstance(vssd)
            print "vmid " +str(vssd_inst['VirtualSystemIdentifier']) + "PV_Args " + str(vssd_inst['PV_Args']) + "PV_Bootloader " + str(vssd_inst['PV_Bootloader'])
            if (vssd_inst['VirtualSystemIdentifier'] == vmid ):
                isSuccess =0
                if vssd_inst['PV_Args'] != "Term=xterm1":
                    print 'Kernel Options is %s' % vssd_inst['PV_Args']
                    isSuccess = 0
                if vssd_inst['PV_Bootloader'] != "mybootloader":
                    print 'Bootloader is %s' % vssd_inst['PV_Bootloader']
                    isSuccess = 0
        self.TestEnd(isSuccess)

    def modifySystemSetting_UnModifyableCaption(self):
        self.TestBegin()
        pv_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        pv_vssd['Caption'] = "modify-pv-vm"
        pv_vssd['ElementName'] = sys._getframe(0).f_code.co_name
        pv_vssd['Description'] = "Test VM"
        pv_vssd['VirtualSystemType'] = "DMTF:xen:PV"
        pv_vssd['PV_Bootloader'] = "pygrub"
        pv_vssd['PV_BootloaderOptions'] = ""
        pv_vssd['OnPoweroff'] = pywbem.Uint8(0)
        pv_vssd['OnReboot'] =pywbem.Uint8(1)
        pv_vssd['OnCrash']= pywbem.Uint8(2)
        pv_vssd['KernelOptions'] = "Term=xterm"
        isSuccess = 0
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.nic_rasd]
        in_params =  {'SystemSettings': pv_vssd, 'ResourceSettings': rasds}
        new_vm = CreateVM(self.conn, self.vsms[0], in_params)
        time.sleep(3)
        pv_vssd['Caption'] = 'Changed_VM'
        pv_vssd['InstanceID'] = 'Xen:' + new_vm['Name']
        print pv_vssd
        in_params = {'SystemSettings': pv_vssd} 
        n = ModifyVM(self.conn, self.vsms[0], in_params)
        time.sleep(2)
        vssds = GetVSSDsForVM(self.conn, new_vm)
        if vssds == None:
            print 'Couldnt find VSSD for PV VM after modify'
            isSuccess = 0
        for vssd in vssds:
            vssd_inst = self.conn.GetInstance(vssd)
            if (vssd_inst['VirtualSystemIdentifier'] == self.get_vm_id(new_vm)):
                print "Current caption : "+vssd_inst['Caption']
                print "Caption desired : Changed_VM"
                testCap = str(vssd_inst['Caption'])
                print testCap
                index = testCap.find("Changed_VM")
                if (index == -1):
                    isSuccess = 1
        pv_val = DeleteVM(self.conn, self.vsms[0], new_vm)    
        self.TestEnd(isSuccess)

    def modify_NULL_system_setting(self):
        self.TestBegin()
        print 'modify vm test'
        result = 0
        in_params = {'SystemSettings': None} 
        isSuccess = ModifyVM(self.conn, self.vsms[0], in_params)
        if (isSuccess == 1):
            result = 0
        else:
            result = 1
        self.TestEnd(result)

########################################################

if __name__ == '__main__':
    #Ip = raw_input("Server IP Address: ")
    #username = raw_input("User Name: ")
    #password = getpass.getpass("Password: ")
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username and password as arguments "
            print "Count is "+str(count)        
            sys.exit(0)
    Ip = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    cd = ModifySystemSettings(Ip, username, password)
    ##################################################################
    try:
        print "Test on ModifySystemSetting"    
        # Success tests
        cd.modify_system_setting()                  # Modify the VM's properties and make sure they are changed
        cd.modifySystemSetting_ElementName()        # Change the Display name of a VM

        # Error scenarios
        cd.modifySystemSetting_VSType()             # Change the VM's virtualsystemtype
        cd.modifySystemSetting_InvalidVSType()      # change the VM's virtualsystemtype to something invalid
        cd.modifySystemSetting_UnModifyableCaption()# Try changing properties that are not modifyable
        cd.modify_non_existing_system_setting()     # Change a non-existent VM's settings
        cd.modify_NULL_system_setting()             # Test for NULL VSSD passed in
    finally:
        cd.TestCleanup()

    sys.exit(0)
    
