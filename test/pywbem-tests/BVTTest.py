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
Exercises randomly selected tests from the other tests to perform a quick BVT
'''
class BVTTest(TestSetUp):

    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password)
            
    def getCurrentState(self, vm_ref):
        vm_inst = self.conn.GetInstance(vm_ref)
        return vm_inst['status']

    def TestCIMAuthentication (self):
        self.TestBegin()
        result = 1
        print 'Enumerate with a bad user'
        try:
            localconn = pywbem.WBEMConnection('http://'+ self.IPAddress, ("badusername", self.Password))
            vsms = localconn.EnumerateInstanceNames("Xen_VirtualSystemManagementService")
            print 'SECURITY ERROR: Authentication worked with bad user - found %d VSMS' % len(vsms)
            result = 0
        except Exception, e:
            print 'SUCCESS: Got "%s" exception with bad user' % str(e)
        try:
            localconn = pywbem.WBEMConnection('http://'+ self.IPAddress, (self.UserName, "badpassword"))
            vsms = localconn.EnumerateInstanceNames("Xen_VirtualSystemManagementService")
            print 'SECURITY ERROR: Authentication worked with bad password - found %d VSMS' % len(vsms)
            result = 0
        except Exception, e:
            print 'SUCCESS: Got "%s" exception with bad password' % str(e)
        try:
            localconn = pywbem.WBEMConnection('http://'+ self.IPAddress, ("baduser", "badpassword"))
            vsms = localconn.EnumerateInstanceNames("Xen_VirtualSystemManagementService")
            print 'SECURITY ERROR: Authentication worked with bad user & password - found %d VSMS' % len(vsms)
            result = 0
        except Exception, e:
            print 'SUCCESS: Got "%s" exception with bad user & pass' % str(e)
        self.TestEnd(result)

    def startAndShutdownVM(self):
        self.TestBegin()
        print 'starting the HVM VM'
        in_params = {'RequestedState':'2'} # Start the VM
        ChangeVMState(self.conn, self.hvm_test_vm, in_params, True, '2')
        print 'sleeping...'
        #verify that that is on
        status = self.getCurrentState(self.hvm_test_vm)
        isSuccess = 1
        if (status == "OK"):
            print "State change successful"
        else:
            isSuccess = 0
        print 'clean shutting down the HVM VM'
        in_params = {'RequestedState':'32768'} # shutdown the VM
        ChangeVMState(self.conn, self.hvm_test_vm, in_params, True, '4')
        status = self.getCurrentState(self.hvm_test_vm)
        if (status == "Stopped"):
            print "State change successful"
        else:
            isSuccess = 0
        self.TestEnd(isSuccess)
    
    def create_new_VM(self):
        self.TestBegin()
        local_pv_vssd = CIMInstance("Xen_ComputerSystemSettingData")
        local_pv_vssd['Caption'] = "pv_create_new_VM"
        local_pv_vssd['ElementName'] = "pv_create_new_VM"
        local_pv_vssd['VirtualSystemType'] = "DMTF:xen:PV"
        local_pv_vssd['PV_Bootloader'] = "pygrub"
        local_pv_vssd['PV_Args'] = "Term=xterm"

        local_hvm_vssd = CIMInstance("Xen_ComputerSystemSettingData")
        local_hvm_vssd['Caption'] = "hvm_create_new_VM"
        local_hvm_vssd['ElementName'] = "hvm_create_new_VM"
        local_hvm_vssd['VirtualSystemType'] = "DMTF:xen:HVM"
        local_hvm_vssd['HVM_Boot_Params'] = ["order=dc"]
        local_hvm_vssd['HVM_Boot_Policy'] = "BIOS order"
        local_hvm_vssd['Platform'] = ["acpi=true","apic=true","pae=true","viridian=true"]

        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        hvm_params = {'SystemSettings': local_hvm_vssd, 'ResourceSettings': rasds}
        pv_params =  {'SystemSettings': local_pv_vssd, 'ResourceSettings': rasds}
        
        print 'create vm test'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        isSuccess = 0
        if ((new_pv_vm == None) and (new_hvm_vm == None)):
            print "VMs are not created"
            isSuccess = 0
        else:
            print "VMs are created"
            isSuccess = 1
        DeleteVM(self.conn, self.vsms[0], new_pv_vm)
        DeleteVM(self.conn, self.vsms[0], new_hvm_vm)
        self.TestEnd(isSuccess)

   
############################################################################
##  DestroySystem test cases
############################################################################    
        
    def Delete_VM(self):
        self.TestBegin()
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        #rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.nic_rasd]
        
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds }
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds}
        
        print 'create vm test'
        new_pv_vm = CreateVM(self.conn, self.vsms[0], pv_params)
        new_hvm_vm = CreateVM(self.conn, self.vsms[0], hvm_params)
        if ((new_pv_vm == None) and (new_hvm_vm == None)):
            print "VMs are not created"
            self.publish_result(0)
        
        print "VMs are created"
        disk_list = GetDisksForVM(self.conn, new_pv_vm)
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
        if (new_pv_vm != None):
                pv_val = DeleteVM(self.conn, self.vsms[0], new_pv_vm)
        if (new_hvm_vm != None):
                hvm_val = DeleteVM(self.conn, self.vsms[0], new_hvm_vm)
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
                    print "HVM_Disk is found after deletion"
                    hvm_notFound = 0
            for this_disk in disk_list:
                if (disk['DeviceID'] == this_disk['DeviceID']):
                    print "Disk is found after deletion"
                    pv_notFound = 0
        isSuccess = 0
        if ((pv_notFound == 1) and (hvm_notFound == 1) and (netNotFound == 1) and (pv_val == 1) and (hvm_val ==1)):
            isSuccess = 1

        else:
            isSuccess = 0
        self.TestEnd(isSuccess)
   
#################################################################
##  Add resource setting test
#################################################################

 # test Add Resources    

    def addProcessor(self):
        self.TestBegin()
        print "adding processor rasd"
        #pv_vm_instance_id = new_pv_vm['Name']
        #hvm_vm_instance_id = new_hvm_vm['Name']
        in_params = {'ResourceSetting': self.proc1_rasd, 'AffectedSystem': self.pv_test_vm}
        AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.proc1_rasd, 'AffectedSystem': self.hvm_test_vm}
        AddVMResource(self.conn, self.vsms[0], in_params)
        processor_list = GetProcessorsForVM(self.conn, self.pv_test_vm)
        
        processor_list1 = GetProcessorsForVM(self.conn, self.hvm_test_vm)
        result = 0
        if ((len(processor_list1) != 2) or (len(processor_list) != 2)):
            print 'PROCESSOR COUNT DIDNT MATCH'
            result = 0

        else:
            result = 1
        self.TestEnd(result)

        
    def addNIC(self):
        self.TestBegin()
        print 'Adding nic rasd'
        in_params = {'ResourceSetting': self.nic1_rasd, 'AffectedSystem': self.pv_test_vm }
        AddVMResource(self.conn, self.vsms[0], in_params)
        
        in_params = {'ResourceSetting': self.nic1_rasd, 'AffectedSystem': self.hvm_test_vm }
        AddVMResource(self.conn, self.vsms[0], in_params)
        network_list = GetNetworkPortsForVM(self.conn, self.pv_test_vm)
        network_list1 = GetNetworkPortsForVM(self.conn, self.hvm_test_vm)

        result = 0
        if ((len(network_list1) != 2) or (len(network_list) != 2)):
            print 'NETWORK COUNT does not match'
            result = 0
        else:
            result = 1
        self.TestEnd(result)

    def addMemory(self):
        self.TestBegin()
        print 'Adding memory rasd'
        in_params = {'ResourceSetting': self.mem1_rasd, 'AffectedSystem':self.pv_test_vm }
        n1 = AddVMResource(self.conn, self.vsms[0], in_params)
        in_params = {'ResourceSetting': self.mem1_rasd, 'AffectedSystem':self.hvm_test_vm }
        n2 = AddVMResource(self.conn, self.vsms[0], in_params)

        # Verify total memory is 1.5 G
        is_pv = 1
        memory_list = GetMemoryForVM(self.conn, self.pv_test_vm)
        for memory_ref in memory_list:
            memory_inst = self.conn.GetInstance(memory_ref)
            size_in_mb = memory_inst['NumberOfBlocks'] / (1024*1024)
            if size_in_mb != 1536:
                print 'MEMORY IS NOT 1.5G'
                is_pv = 0
        is_hvm = 1
        memory_list = GetMemoryForVM(self.conn, self.hvm_test_vm)
        for memory_ref in memory_list:
            memory_inst = self.conn.GetInstance(memory_ref)
            size_in_mb = memory_inst['NumberOfBlocks'] / (1024*1024)
            if size_in_mb != 1536:
                print 'MEMORY IS NOT 1.5G'
                is_hvm = 0
        result = 0
        if ((is_pv == 1) and (is_hvm == 1) and (n1 == 1) and (n2 == 1)):
            result = 1
        else:
            result = 0
        self.TestEnd(result)

    
############################################################################
##  Test on RemoveResourceSetting
############################################################################

    def removeValidResources(self):
        self.TestBegin()
        print 'deleting vm resources'
        # get all RASDs and select the ones we want to delete. Use the LIKE query to do it
        query_str = 'SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID LIKE "%s"' % ('%'+self.hvm_test_vm['Name']+'%')
        print 'executing ' + query_str
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        rasds_to_delete = []
        print "Total resources before delete "
        print len(rasds)
        # Delete requires refs to RASDs and not instances or strings
        for rasd in rasds:
            #if rasd['HostResource'] != None:
            #    rasd['HostResource'] = '' # hostresource is in WBEM URI format which is complicated to turn into a MOF string. Ignore here.
            if rasd['ElementName'] == 'testvdi1':
                # Copy it to anew instance of a RASD, otherwise pegasus returns an XML error
                # because the rasd object returned from the WQL query is of type NamedInstnace rather than Instance.
                newrasd = CIMInstance("Xen_DiskSettingData")
                newrasd['InstanceID']   = rasd['InstanceID']
                newrasd['ResourceType'] = rasd['ResourceType']
                rasds_to_delete.append(newrasd)
            if rasd['Address'] == "00:13:72:24:32:f4":
                newrasd = CIMInstance("Xen_NetworkPortSettingData")
                newrasd['InstanceID']   = rasd['InstanceID']
                newrasd['ResourceType'] = rasd['ResourceType']
                rasds_to_delete.append(newrasd)
        in_params = {'ResourceSettings': rasds_to_delete}
        DeleteVMResources(self.conn, self.vsms[0], in_params)
        # get the RASDs again to verify the remove
        rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
        print "Total resources after delete "
        print len(rasds)
        isSuccess = 1
        for rasd in rasds:
            if ((rasd['ElementName'] == 'testvdi1') or (rasd['Address'] == "00:13:72:24:32:f4")):
                print "Resource is not deleted"
                isSuccess = 0
                break
        self.TestEnd(isSuccess)
    
    
##############################################################################
##  Test on ModifySystemSetting
##############################################################################
    
    def modify_system_setting(self):
        self.TestBegin()
        print 'modify vm test'
        new_vssd = CIMInstance("Xen_ComputerSystemSettingData")
        new_vssd['InstanceID'] = "Xen:" + self.pv_test_vm['Name']
        new_vssd['PV_Args']         = "Term=xterm1"
        new_vssd['PV_Bootloader']   = "mybootloader";
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
                print 'Caption is %s' % vssd_inst['PV_Bootloader']
                isSuccess = 0
        self.TestEnd(isSuccess)


##########################################################################################
# Where did this test come from??
        
    def GetVSMSCapabilities (self, conn, vm_ref):
        ass_class = 'Xen_HostedComputerSystem'
        r_class = 'Xen_ComputerSystem'
        params         = {'ResultClass': r_class, 'AssocClass': ass_class }
        list = conn.AssociatorNames(vm_ref, **params)
        print len(list)
        association_class = 'Xen_ComputerSystemElementCapabilities' # association to traverse via Xen_ComputerSystem
        result_class      = 'Xen_VirtualSystemManagementCapabilities' # class we are looking for
        in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
        vm_cap_list = conn.AssociatorNames(vm_ref, **in_params)
        print len(vm_cap_list)
        return vm_cap_list

    def GetVSMSCaps(self):
        print "Enumerate VSMS capabilites"
        vsms_caps_list = self.GetVSMSCapabilities(self.conn, self.pv_test_vm)
        print len(vsms_caps_list)
        if vsms_caps_list == None:
            print "None"
        for vsms_caps in vsms_caps_list:
            caps_inst = self.conn.GetInstance(vsms_caps)
            if caps_inst['SynchronousMethodsSupported'] != None:
                print caps_inst['SynchronousMethodsSupported']
                print "Found"
            else:
                print "Not found"
            if verbose == "true":
                print caps_inst.items()
                
            else:
                if caps_inst['SynchronousMethodsSupported'] != None:
                    print '   SynchronousMethodsSupported' + ",".join(map(str, caps_inst['SynchronousMethodsSupported']))
                if caps_inst['AsynchronousMethodsSupported'] != None:
                    print '   AsynchronousMethodsSupported' + ",".join(map(str, caps_inst['AsynchronousMethodsSupported']))

    #######################################################################

    def get_resourcePool_from_host(self, result_class, association_class):
        vs_refs =  self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        returned_value = 0
        
        for vs in vs_refs:
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(vs, **in_params)
            
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Pool not found"
            for element in elements:
               print element.items()
        return returned_value


    def get_memoryPool_from_host(self):
        self.TestBegin()
        association_class = 'Xen_HostedMemoryPool'  # association to traverse 
        result_class      = 'Xen_MemoryPool' 
        returned_value = self.get_resourcePool_from_host(result_class,association_class)
        self.TestEnd(returned_value)
    
    def get_logicalDevice_from_host(self):
        self.TestBegin()
        vs_refs =  self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        processor = 0
        memory = 0
        print "Logical Device for VS"
        for vs in vs_refs:
            association_class = 'CIM_SystemDevice'  # association to traverse 
            result_class      = 'CIM_LogicalDevice'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(vs, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            for element in elements:
               #print element.items()
                if (element['CreationClassName'] == 'Xen_HostMemory'):
                    memory = 1
                if (element['CreationClassName'] == 'Xen_HostProcessor'):
                    processor = 1
                print element['CreationClassName'] +' : '+ element['DeviceID']
        if (memory == 0):
            print "Memory not found"
        if (processor == 0):
            print "Processor not found"
        result = 0
        if ( (memory == 0) or (processor == 0)):
            result = 0
        else:
            result = 1
        self.TestEnd(result)

    def get_VSSD_for_RASD(self):
        self.TestBegin()
        returned_value = 0
        rasd_refs = self.conn.EnumerateInstanceNames("CIM_ResourceAllocationSettingData")
        #print len(rasd_refs)
        print "VSSDs for RASDs"
        for rasd in rasd_refs:
            association_class = 'Xen_ComputerSystemSettingDataComponent'  # association to traverse 
            result_class      = 'Xen_ComputerSystemSettingData'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(rasd, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "No VSSD is found"
            for element in elements:
               print  element.items()
        self.TestEnd(returned_value)

    def find_possible_hosts_to_boot_on (self):
        self.TestBegin()
        successful = 0
        vm_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        for vm_ref in vm_refs:
            in_params = {'AffectedSystem':vm_ref}
            (rval, out_params) = self.conn.InvokeMethod('FindPossibleHostsToRunOn', self.vsms[0], **in_params)
            if rval == 0:
                if out_params != None and out_params['PossibleHosts']:
                    print 'VM %s can boot on %s' % (vm_ref['Name'], str(out_params['PossibleHosts']))
                else:
                    print 'VM cannot boot on any of the hosts'
                successful = 1
            else:
                print 'FindPossibleHostsToRunOn returned Error %d' % rval
        self.TestEnd(successful)

    def get_VSSD_for_ComputerSystem(self):
        self.TestBegin()
        vms_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        returned_value = 0
        for vm_ref in vms_refs:
            vssd_refs = GetVSSDsForVM(self.conn, vm_ref)
            if (len(vssd_refs) != 0):
                returned_value = len(vssd_refs)
            else:
                print "VSSD not found"
            print 'VSSD:'
            for vssd_ref in vssd_refs:
                vssd = self.conn.GetInstance(vssd_ref)
                print "   Virtual System Type:" + vssd["VirtualSystemType"]
                print "   Caption:" + vssd["Caption"]
        result = 0
        if (returned_value == 0):
            result = 0
        else:
            result = 1
        self.TestEnd(result)

    def get_enabledLogicalElementCapabilities_for_ComputerSystem(self):
        self.TestBegin()
        vms_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        returned_value = 0

        for vm_ref in vms_refs:
            print 'VM Capabilities:'
            vm_caps_list = GetVMCapabilities(self.conn, vm_ref)
            if (len(vm_caps_list) != 0):
                returned_value = len(vm_caps_list)

            else:
                print "Caps not found"

            for vm_caps in vm_caps_list:
                caps_inst = self.conn.GetInstance(vm_caps)
                
                if caps_inst['RequestedStatesSupported'] != None:
                    print '   RequestedStatesSupported:' + ",".join(map(str, caps_inst['RequestedStatesSupported']))
        self.TestEnd(returned_value)

    def get_RASD_from_VSSD(self):
        #vssd_refs = GetVSSDsForVM(self.conn,  self.hvm_test_vm)
        self.TestBegin()
        returned_value = 0
        vssd_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystemSettingData")
        print 'RASD:'
        for vssd_ref in vssd_refs:
            print '   RASDs From VSSD:'
            rasds = GetRASDsFromVSSD(self.conn, vssd_ref)
            if (len(rasds) != 0):
                returned_value = len(rasds)
            else:
                print "RASD not found"
            for rasd in rasds:
                print '      ResourceType:%d' % rasd['ResourceType']
        self.TestEnd(returned_value)

    def get_vms_from_host(self):
        # enumerate the VMs running in the host
        self.TestBegin()
        host_list = self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        print len(host_list)
        returned_value = 0
        for host in host_list:
            association_class = 'Xen_HostedComputerSystem' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_ComputerSystem' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(host, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "No VMs found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)

    def _CreateConsoleTargetVM(self, conn, vsms):
        vssd = CIMInstance('Xen_ComputerSystemSettingData')
        vssd['ElementName'] = "ConsoleTestVM"
        vssd['Description'] = "VM to test console access"
        return CreateVMBasedOnTemplateName(conn, vsms, "XenServer Transfer VM", vssd)

    def _CleanupConsoleTargetVM (self, conn ,vsms, console_vm):
        # cleanup the VM we created above
        DeleteVM(conn, vsms, console_vm)

    def get_session_to_login_to_vm_console (self):
        self.TestBegin()
        isSuccess = 0
        target_vm = self._CreateConsoleTargetVM(self.conn, self.vsms[0])
        if target_vm != None:
            try:
                print 'starting test console vm'
                in_params = {'RequestedState':'2'}
                isSuccess = ChangeVMState(self.conn, target_vm, in_params, True, '2')
                if isSuccess != 1:
                    print 'Couldnt start the console VM'
                else:
                    isSuccess = 0
                    print 'getting console for target vm %s' % str(target_vm['Name'])
                    consoles = GetConsoleForVM(self.conn, target_vm)
                    for console in consoles:
                        print 'logging into HVM console %s' % str(console.items())
                        in_params = {}
                        (rval, out_params) = self.conn.InvokeMethod('Login', console, **in_params)
                        if rval == 0 and 'SessionIdentifier' in out_params:
                            print 'Console can log into HVM session %s' % out_params['SessionIdentifier']
                            in_params = {"SessionIdentifier": out_params['SessionIdentifier']}
                            (rval, out_params) = self.conn.InvokeMethod('Logout', console, **in_params)
                            if rval == 0:
                                isSuccess = 1
                        print 
                    print 'Stopping target vm'
                    in_params = {'RequestedState': '4'} # shut it down
                    ChangeVMState(self.conn, target_vm, in_params, True, '4')
            finally:
                self._CleanupConsoleTargetVM(self.conn, self.vsms[0], target_vm)
                print 'Done with the session login'
        else:
            print 'ERROR: You need to have a the Demo Linux VM template available to create a VM out of'
        self.TestEnd(isSuccess)

########################################################
if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username, and password as arguments"
            print "Count is "+str(count)        
            sys.exit(0)
    
    Ip = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]

    cd = BVTTest(Ip, username, password)
    try:
        cd.TestCIMAuthentication()          # Make sure the CIM authentication works properly
        cd.get_memoryPool_from_host()       # Get the memory pool to allocate memory for VMs out of
        cd.get_logicalDevice_from_host()    # get all devices associated with a host
        cd.get_VSSD_for_ComputerSystem()    # get the VSSD associated with a VM
        cd.find_possible_hosts_to_boot_on() # find possible hosts that a VM can the boot on
        cd.get_enabledLogicalElementCapabilities_for_ComputerSystem() # get the virtualiation capabilities for a Host
        cd.get_vms_from_host()              # get VMs associated with a host
        cd.get_RASD_from_VSSD()             # get all resources allocated to a VM identified by the VSSD
        cd.create_new_VM()                  # Create a new VM
        cd.addProcessor()                   # Add Processors
        cd.addNIC()                         # Add a NIC 
        cd.addMemory()                      # add more memory
        cd.removeValidResources()           # remove some of the resources
        cd.modify_system_setting()          # modify the name of the VM
        cd.get_session_to_login_to_vm_console() # login to a VM and get a session id back
        cd.startAndShutdownVM()             # start and shutdown a VM
        cd.Delete_VM()                      # Delete the VM
    finally:
        cd.TestCleanup()
    
    sys.exit(0)
