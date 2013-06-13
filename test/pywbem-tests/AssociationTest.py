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
Tests all the association classes.
Allows caller to get a related object instance given a source object instance (such as getting a xen vbd, given a vm)
The association classes as described in the various DMTF profiles are excercised.
SCVMM will not use any of these since they cannot be accessed using the WS-Management protocol
'''
class AssociationTest(TestSetUp):
    def __init__(self, IPAddress, UserName, Password):
        TestSetUp.__init__(self, IPAddress, UserName, Password, True, True)
        self.interop_ns = "root/cimv2"


#################################################################
    # Test on CIM_VSMS
    def get_ComputerSystem_from_vsms(self):
        self.TestBegin()
        vsms_refs = self.conn.EnumerateInstanceNames("Xen_VirtualSystemManagementService")
        #print len(vsms_refs)
        value = 0
        for vsms in vsms_refs:
            association_class = 'Xen_VirtualSystemManagementServiceAffectsComputerSystem' # association to traverse
            result_class      = 'Xen_ComputerSystem'                                      # class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            vm_list = self.conn.AssociatorNames(vsms, **in_params)
            print "Computer Systems for VSMS"
            if (len(vm_list) != 0):
                value = len(vm_list)
            else:
                print "No ComputerSystem is found"
            for vms in vm_list:
                print vms.items()
        self.TestEnd(value)
        
    def get_VSMS_Capabilities_from_VSMS(self):
        self.TestBegin()
        vsms_refs = self.conn.EnumerateInstanceNames("Xen_VirtualSystemManagementService")
        value = 0
        #print len(vsms_refs)
        for vsms in vsms_refs:
            association_class = 'CIM_ElementCapabilities' # association to traverse
            result_class      = 'Xen_VirtualSystemManagementCapabilities' # class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            vm_cap_list = self.conn.AssociatorNames(vsms, **in_params)
            if (len(vm_cap_list) != 0):
                value = len(vm_cap_list)
            else:
                print "No caps found"
            print "VSMS Capabilities:"
            for caps in vm_cap_list:
                print caps.items()
        self.TestEnd(value)
        
#################################################################
    # Test on CIM_RASD
    
    def get_ProcessorPool_from_processorRASD(self):
        self.TestBegin()        
        #Xen_ProcessorSettingData
        proc_refs = self.conn.EnumerateInstanceNames("Xen_ProcessorSettingData")
        isExists = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_ProcessorSettingAllocationFromPool'  # association to traverse 
            result_class      = 'Xen_ProcessorPool'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                print "Pool found"
                isExists = 1
                
            for element in elements:
                print element.items()
        '''if (returned_value == 0):
            self.publish_result(0)
        else:
            self.publish_result(1)'''
        self.TestEnd(isExists)

    def get_MemoryPool_from_memoryRASD(self):
        self.TestBegin()
        mem_refs = self.conn.EnumerateInstanceNames("Xen_MemorySettingData")
        
        isExists = 0
        
        for mem in mem_refs:
            
            association_class = 'Xen_MemorySettingAllocationFromPool'  # association to traverse 
            result_class      = 'Xen_MemoryPool'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(mem, **in_params)
            if (len(elements) != 0):
                print "memoryPool is found"
                isExists = 1
            for element in elements:
                print element.items()
        self.TestEnd(isExists)

    def get_NetworkPool_from_networkRASD(self):
        self.TestBegin()        
        mem_refs = self.conn.EnumerateInstanceNames("Xen_NetworkPortSettingData")
        
        isExists = 0
        #print len(mem_refs)
        for mem in mem_refs:
            
            association_class = 'CIM_ResourceAllocationFromPool'  # association to traverse 
            result_class      = 'Xen_NetworkConnectionPool'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(mem, **in_params)
            if (len(elements) != 0):
                print "networkPool is found"
                isExists = 1
                
            for element in elements:
                print element.items()
        self.TestEnd(isExists)

    def get_StoragePool_from_diskRASD(self):
        self.TestBegin()
        mem_refs = self.conn.EnumerateInstanceNames("Xen_DiskSettingData")
        isExists = 0
        #print len(mem_refs)
        for mem in mem_refs:
            association_class = 'CIM_ResourceAllocationFromPool'  # association to traverse 
            result_class      = 'Xen_StoragePool'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(mem, **in_params)
            if (len(elements) != 0):
                print "storagePool is found"
                isExists = 1
            for element in elements:
                print element.items()
        self.TestEnd(isExists)
    
    def get_VSSD_for_RASD(self):
        self.TestBegin()
        rc = 1
        rasds_to_check = ["Xen_MemorySettingData", 
                          "Xen_ProcessorSettingData", 
                          "Xen_DiskSettingData",
                          "Xen_NetworkPortSettingData"]
        rasd_refs = []
        for rasd_to_check in rasds_to_check:
            tmp_rasd_refs = self.conn.EnumerateInstanceNames(rasd_to_check)
            rasd_refs.extend(tmp_rasd_refs)
        #print len(rasd_refs)
        print "VSSDs for RASDs"
        for rasd in rasd_refs:
            association_class = 'CIM_VirtualSystemSettingDataComponent'  # association to traverse 
            result_class      = 'Xen_VirtualSystemSettingData'           # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(rasd, **in_params)
            if (len(elements) == 0):
                rc = 0
                print "No VSSD is found"
            else:
                for element in elements:
                    print  '%s (%s) to %s(%s)' % (rasd.classname, str(rasd.items()), 
                                                  element.classname, str(element.items()))
        self.TestEnd(rc)

    """Remaining:
        1. AllocationCaps
        2. ManagedSystemElement
    """        
                           
#################################################################
    
        
#################################################################
            
## Association Test for CIM_Registeredprofile

    def GetSystemFromProfile(self, profile_ref, class_association):
        #association_class = 'CIM_ElementConformsToProfile'  # association to traverse
        association_class =  class_association # association to traverse 
        result_class      = 'Xen_HostComputerSystem'                    # result class we are looking for
        in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
        elements = self.conn.AssociatorNames(profile_ref, **in_params)
        return elements
            
        
    def get_host_from_SV_profile(self):
        self.TestBegin()
        returned_value = 0
        association_class = 'Xen_ElementConformsToSystemVirtualizationProfile'
        profile_refs = self.conn.EnumerateInstanceNames("Xen_RegisteredSystemVirtualizationProfile", self.interop_ns)
        for profile_ref in profile_refs:
            profile = self.conn.GetInstance(profile_ref)
            print 'Elements conforming to profile:' + profile_ref['InstanceID']
            elements = self.GetSystemFromProfile(profile_ref, association_class )
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "No host is found"
            print "Host System" 
            for element in elements:
               print element.items()
        self.TestEnd(returned_value)
        #return profile_refs

    def get_host_from_vs_profile(self):
        self.TestBegin()
        returned_value = 0
        association_class = 'Xen_ElementConformsToVirtualSystemProfile'
        profile_refs = self.conn.EnumerateInstanceNames("Xen_RegisteredVirtualSystemProfile", self.interop_ns)
        
        for profile_ref in profile_refs:
            profile = self.conn.GetInstance(profile_ref)
            print 'Elements conforming to profile:' + profile_ref['InstanceID']
            
            result_class      = 'Xen_ComputerSystem'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(profile_ref, **in_params)
            #elements = self.GetSystemFromProfile(profile_ref, association_class)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "No VM is found"
            print "VMs...."    
            for element in elements:
               print element.items()
        self.TestEnd(returned_value)
        


############################################################################

    #Test on CIM_System association
    def get_vs_profile_from_host(self):
        self.TestBegin()
        host_list = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        #print len(host_list)
        returned_value = 0
        for host in host_list:
            association_class = 'Xen_ElementConformsToVirtualSystemProfile' # association to traverse
            result_class      = 'Xen_RegisteredVirtualSystemProfile' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(host, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "No vs profile is found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)

    def get_sv_profile_from_host(self):
        self.TestBegin()
        host_list = self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        #print len(host_list)
        returned_value = 0
        for host in host_list:
            association_class = 'Xen_ElementConformsToSystemVirtualizationProfile' # association to traverse
            result_class      = 'Xen_RegisteredSystemVirtualizationProfile' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(host, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "No sv profile found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)

    def get_ac_profile_from_host(self):
        pass
    def get_ra_profile_from_host(self):
        pass
    
    def get_vms_from_host(self):
        # enumerate the VMs running in the host
        self.TestBegin()
        host_list = self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        #print len(host_list)
        returned_value = 0
        for host in host_list:
            print 'Getting VM\'s for host %s' % (host['Name'])
            association_class = 'Xen_HostedComputerSystem' # association to traverse
            result_class      = 'Xen_ComputerSystem' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(host, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "    No VMs found"
            for element in elements:
                print '    ' + str(element.items())
        self.TestEnd(returned_value)

    def get_logicalDevice_from_host(self):
        self.TestBegin()
        vs_refs =  self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        # network is not associated with host, but with virtualswitch object
        # storage is not associated with host, but with storage pool object
        processor = 0
        memory = 0
        print "Logical Device for VS"
        for vs in vs_refs:
            #memory
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
        if ((memory == 0) or (processor == 0)):
            result = 0
        else:
            result = 1
        self.TestEnd(result)

    def get_switchport_from_switch(self):
        self.TestBegin()
        vs_refs =  self.conn.EnumerateInstanceNames("Xen_VirtualSwitch")
        print "Network Ports associated with Virtual Switch"
        result = 0
        for vs in vs_refs:
            ports = 0
            association_class = 'CIM_SystemDevice'  # association to traverse 
            result_class      = 'CIM_LogicalDevice'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(vs, **in_params)
            for element in elements:
                if (element['CreationClassName'] == 'Xen_VirtualSwitchPort'):
                    ports = ports + 1
            if (ports == 0):
                print "Virtual Switch Ports not found for %s" % vs['Name']
            else:
                print '%d Devices found for Virtual Switch %s, of which %d were ethernet ports' % (len(elements),  vs['Name'], ports)
                result = 1 # found at least one
        self.TestEnd(result)

    def get_network_connections (self):
        self.TestBegin()
        vsp_refs =  self.conn.EnumerateInstanceNames("Xen_VirtualSwitchPort")
        print 'found %d virtual switch ports' % len(vsp_refs)
        result = 1
        for vsp_ref in vsp_refs:
            # 
            # VirtualSwitch <----> SwitchPort <-----> LANEndPoint <---
            # ------------------ActiveConnection---------------------- 
            # --> LANEndpoint <-----> NetworkPort <-----> ComputerSystem
            # 
            # Get the siwtch end of the LAN Endpoint for the SwitchPort
            print 'getting the VirtualSwitchPort LANEndpoint'
            association_class = 'CIM_DeviceSAPImplementation'  # association to traverse 
            result_class      = 'CIM_LANEndpoint'              # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            lleps = self.conn.AssociatorNames(vsp_ref, **in_params)
            if len(lleps) != 1:
                print 'found %d LANEndpoints associated with the VirtualSwitchPort %s' % (len(lleps), vsp_ref['DeviceID'])
                result = 0
                break
            else:
                print 'Found VirtualSwitchlANEndpoint %s' % (lleps[0]['Name'])
            # Get the computersystem end of the LAN Endpoint 
            print 'getting the ComputerSystemLANEndpoint'
            association_class = 'CIM_ActiveConnection'  # association to traverse 
            result_class      = 'CIM_LANEndpoint'       # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            rleps = self.conn.AssociatorNames(lleps[0], **in_params)
            if len(rleps) != 1:
                print 'found %d ActiveConnections between the 2 LANEndpoints for %s' % (lleps[0]['Name'])
                result = 0
                break
            else:
                print 'Found ComputerSystemLANEndpoint %s' % (rleps[0]['Name'])
            # Get the NetworkPort associated with the computer system end of the LAN Endpoint 
            print 'getting the NetworkPort'
            association_class = 'CIM_DeviceSAPImplementation'  # association to traverse 
            result_class      = 'CIM_NetworkPort'       # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            nps = self.conn.AssociatorNames(rleps[0], **in_params)
            if len(nps) != 1:
                print 'found %d NetworkPort(s) connected to LANEndpoint for %s' % (len(nps), rleps[0]['Name'])
                result = 0
                break
            else:
                print 'Found NetworkPort % s' % (nps[0]['DeviceID'])
        self.TestEnd(result)

    def get_resourcePool_from_host(self, result_class, association_class):
        host_refs =  self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        returned_value = 0
        for host in host_refs:
            print 'Getting %s for host: %s' % (result_class, host['Name'])
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(host, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "    Pool not found"
            for element in elements:
               print '    ' + str(element.items())
        return returned_value

    def get_memoryPool_from_host(self):
        self.TestBegin()
        association_class = 'Xen_HostedMemoryPool'  # association to traverse 
        result_class      = 'Xen_MemoryPool' 
        returned_value = self.get_resourcePool_from_host(result_class,association_class)
        self.TestEnd(returned_value)

    def get_processorPool_from_host(self):
        self.TestBegin()
        association_class = 'Xen_HostedProcessorPool'  # association to traverse 
        result_class      = 'Xen_ProcessorPool' 
        returned_value = self.get_resourcePool_from_host(result_class,association_class)
        self.TestEnd(returned_value)

    def get_networkPool_from_switch(self):
        self.TestBegin()
        association_class = 'Xen_HostedNetworkConnectionPool'  # association to traverse 
        result_class      = 'Xen_NetworkConnectionPool' 
        sw_refs =  self.conn.EnumerateInstanceNames("Xen_VirtualSwitch")
        returned_value = 0
        for sw in sw_refs:
            print 'Getting %s for switch: %s' % (result_class, sw['Name'])
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(sw, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "    Pool not found"
            for element in elements:
               print '    ' + str(element.items())
        self.TestEnd(returned_value)

    def get_storagePool_from_host(self):
        self.TestBegin()
        association_class = 'Xen_HostedStoragePool'  # association to traverse 
        result_class      = 'Xen_StoragePool' 
        returned_value = self.get_resourcePool_from_host(result_class,association_class)
        self.TestEnd(returned_value)
        
    def get_vsms_from_host(self):
        self.TestBegin()
        vs_refs =  self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        returned_value = 0
        #print len(vs_refs)
        print "VSMS for VS"
        for vs in vs_refs:
            association_class = 'Xen_HostedVirtualSystemManagementService'  # association to traverse 
            result_class      = 'Xen_VirtualSystemManagementService'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(vs, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "VSMS is not found"
            for element in elements:
               print element.items()
        self.TestEnd(returned_value)
                       
    def get_vsms_caps_from_host(self):
        # enumerate the VSMS and then capabilities of each VSMS 
        self.TestBegin()
        vs_refs =  self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        returned_value = 0
        #print len(vs_refs)
        print "VSMS for VS"
        for vs in vs_refs:
            association_class = 'CIM_ElementCapabilities'  # association to traverse 
            #result_class      = 'Xen_VirtualSystemManagementCapabilities'                    # result class we are looking for
            result_class      = 'Xen_VirtualizationCapabilities'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(vs, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Caps not found"
            for element in elements:
               print element.items()
        self.TestEnd(returned_value)
        

#################################################################
    #Test on CIM_LogicalDevice
        
    def get_host_from_LogicalDevice(self):
        self.TestBegin()
        result = 0
        try:
            processor = self.get_host_from_Device('Xen_HostProcessor')
            if (processor == 0):
                print "Cannot enumerate host from Xen_HostProcessor"
            memory = self.get_host_from_Device('Xen_HostMemory')
            if (memory == 0):
                print "Cannot enumerate host from Xen_HostMemory"
            # Storatge and network are not directly relateed to host
            if ((processor == 0) or (memory == 0)): # or (storage == 0)):
                result = 0
            else:
                result = 1
        except Exception, e:
            print e
        self.TestEnd(result)

    def get_host_from_Device(self, device):
        dev_refs =  self.conn.EnumerateInstanceNames(device)
        for dev in dev_refs:
            association_class = 'CIM_SystemDevice'  # association to traverse 
            result_class      = 'Xen_HostComputerSystem'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(dev, **in_params)
            if (len(elements) == 0):
                return 0
                # if doesn't return : then failed
            else:
                print dev['CreationClassName'] + " can enumerate the Host "
                for element in elements:
                    print element.items()
        
    
    def get_logicalElement_from_Device(self, device, assoc_class):
        dev_refs =  self.conn.EnumerateInstanceNames(device)
        print 'Enumerated %d %s' % (len(dev_refs), device)
        for dev in dev_refs:
            association_class =  assoc_class # association to traverse 
            result_class      = 'CIM_LogicalElement'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(dev, **in_params)
            if (len(elements) == 0):
                print "Cannot enumerate the logical elements for device %s" % device
                return 0
                # if doesn't return : then failed
            else:
                print dev['CreationClassName'] + " - enumerated the following logical elements"
                for element in elements:
                    print element.items()

    def get_logicalElement_from_logicalDevice(self):
        self.TestBegin()
        result = 0
        try:
            processor = self.get_logicalElement_from_Device('Xen_HostProcessor', 'Xen_HostedProcessor')
            if (processor == 0):
                print "Xen_HostProcessor cannot enumerate the logical element"
            memory = self.get_logicalElement_from_Device('Xen_HostMemory', 'Xen_HostedMemory')
            if (memory == 0):
                print "Xen_HostMemory cannot enumerate the logical element"
            network = self.get_logicalElement_from_Device('Xen_VirtualSwitchPort', 'Xen_HostedNetworkPort')
            if (network == 0):
                print "Xen_HostNetworkPort cannot enumerate the logical element"
            storage = self.get_logicalElement_from_Device('Xen_DiskImage', 'Xen_HostedDisk')
            if (storage == 0):
                print "Xen_HostStorageExtent cannot enumerate the logical element"
            if ((processor == 0) or (memory == 0) or (storage == 0) or (network == 0)):
                result = 0
            else:
                result = 1
        except Exception, e:
            print e        
        self.TestEnd(result)
    
    '''def get_logicalElement_from_logicalDevice(self):

        processor = self.get_logicalElement_from_Device(self, 'Xen_Processor', 'Xen_HostedProcessor')
        memory = self.get_logicalElement_from_Device(self, 'Xen_Memory', 'Xen_HostedMemory')
        network = self.get_logicalElement_from_Device(self, 'Xen_NetworkPort', 'Xen_HostedNetworkPort')
        disk = self.get_logicalElement_from_Device(self, 'Xen_Disk', 'Xen_HostedDisk')'''
        
        

#################################################################
    # Test on CIM_ResourcePool
    
    def get_Host_from_storagePool(self):
        # enumerate the VMs running in the host
        self.TestBegin()
        pool_list = self.conn.EnumerateInstanceNames("Xen_StoragePool")
        #print len(pool_list)
        returned_value = 0
        for pool in pool_list:
            print 'Finding host for storage pool %s' % pool['InstanceID']
            association_class = 'Xen_HostedStoragePool' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_HostComputerSystem' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(pool, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "    Host not found"
            for element in elements:
                print "    " + str(element.items())
        self.TestEnd(returned_value)
        
    def get_Switch_from_networkPool(self):
        # enumerate the VMs running in the host
        self.TestBegin()
        pool_list = self.conn.EnumerateInstanceNames("Xen_NetworkConnectionPool")
        #print len(pool_list)
        isFailed = 1
        for pool in pool_list:
            print 'Finding host for network connection pool %s' % pool['InstanceID']
            association_class = 'Xen_HostedNetworkConnectionPool' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_VirtualSwitch' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(pool, **in_params)
            if (len(elements) != 0):
                print "    Switch not found"
                isFailed = 0
            for element in elements:
                print "    " + str(element.items())
        result = 0
        if (isFailed == 1):
            result = 0
        else:
            result = 1
        self.TestEnd(result)
        
    
    def get_Host_from_processorPool(self):
        # enumerate the VMs running in the host
        self.TestBegin()
        pool_list = self.conn.EnumerateInstanceNames("Xen_ProcessorPool")
        #print len(pool_list)
        returned_value = 0
        for pool in pool_list:
            print 'Finding host for processor pool %s' % pool['InstanceID']
            association_class = 'Xen_HostedProcessorPool' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_HostComputerSystem' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(pool, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "    Host not found"
            for element in elements:
                print "    " + str(element.items())
        self.TestEnd(returned_value)
        
    def get_Host_from_memoryPool(self):
        # enumerate the VMs running in the host
        self.TestBegin()
        pool_list = self.conn.EnumerateInstanceNames("Xen_MemoryPool")
        #print len(pool_list)
        returned_value = 0
        for pool in pool_list:
            print 'Finding host for memory pool %s' % pool['InstanceID']
            association_class = 'Xen_HostedMemoryPool' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_HostComputerSystem' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(pool, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "    Host not found"
            for element in elements:
                print "    " + str(element.items())
        self.TestEnd(returned_value)
    
    def get_memory_from_memoryPool(self):
        self.TestBegin()
        mem_refs = self.conn.EnumerateInstanceNames("Xen_MemoryPool")
        returned_value = 0
        #print len(mem_refs)
        for mem in mem_refs:
            association_class = 'Xen_MemoryAllocatedFromPool'  # association to traverse 
            result_class      = 'Xen_Memory'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(mem, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Memory not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)

    def get_processor_from_processorPool(self):
        self.TestBegin()
        proc_refs = self.conn.EnumerateInstanceNames("Xen_ProcessorPool")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_ProcessorAllocatedFromPool'  # association to traverse 
            result_class      = 'Xen_Processor'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "processor not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)
    
    def get_network_from_networkPool(self):
        self.TestBegin()
        proc_refs = self.conn.EnumerateInstanceNames("Xen_NetworkConnectionPool")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_NetworkPortAllocatedFromPool'  # association to traverse 
            result_class      = 'Xen_NetworkPort'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Network not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)
    
    def get_disk_from_storagePool(self):
        self.TestBegin()
        proc_refs = self.conn.EnumerateInstanceNames("Xen_StoragePool")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_DiskAllocatedFromPool'  # association to traverse 
            result_class      = 'Xen_Disk'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Disk not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)
    

    ###########Association: ResourceAllocationFromPool
    def get_memoryRASD_from_MemoryPool(self):
        self.TestBegin()
        mem_refs = self.conn.EnumerateInstanceNames("Xen_MemoryPool")
        returned_value = 0
        #print len(mem_refs)
        for mem in mem_refs:
            association_class = 'Xen_MemorySettingAllocationFromPool'  # association to traverse 
            result_class      = 'Xen_MemorySettingData'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(mem, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "RASD not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)

    def get_processorRASD_from_ProcessorPool(self):
        self.TestBegin()
        proc_refs = self.conn.EnumerateInstanceNames("Xen_ProcessorPool")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_ProcessorSettingAllocationFromPool'  # association to traverse 
            result_class      = 'Xen_ProcessorSettingData'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "RASD not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)

    
    def get_networkRASD_from_NetworkPool(self):
        self.TestBegin()
        proc_refs = self.conn.EnumerateInstanceNames("Xen_NetworkConnectionPool")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_NetworkPortSettingAllocationFromPool'  # association to traverse 
            result_class      = 'Xen_NetworkPortSettingData'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "RASD not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)

    def get_diskRASD_from_StoragePool(self):
        self.TestBegin()
        proc_refs = self.conn.EnumerateInstanceNames("Xen_StoragePool")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_DiskSettingAllocationFromPool'  # association to traverse 
            result_class      = 'Xen_DiskSettingData'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "RASD not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)

    def get_metrics_for_device (self, conn, device):
        isSuccess = 1
        num_metrics = 0
        refs = conn.EnumerateInstanceNames(device)
        for ref in refs:
            print '----- %s instance-----' % ref.classname
            print 'DeviceID=%s' % (ref['DeviceID'])
            association_class = 'CIM_MetricDefForME'  # association to traverse 
            result_class      = 'CIM_BaseMetricDefinition' # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = conn.AssociatorNames(ref, **in_params)
            print '    -----Metric Definition-----'
            if len(elements) == 0:
                isSuccess = 0
                print 'Metic definition not found for %s' % device
            for element in elements:
                print '    %s' % str(element.items())
                print '----- Metric Instances for this definition-----'
                association_class = 'CIM_MetricInstance'  # association to traverse 
                result_class      = 'CIM_BaseMetricValue' # result class we are looking for
                in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
                elements2 = conn.Associators(element, **in_params)
                for element2 in elements2:
                    print '    Metric %s(%s) Value=%s' % (element2.classname, element2['InstanceID'], element2['MetricValue'])
                if len(elements2) == 0:
                    print 'metric instances could not be found for device %s' % device
                    isSuccess = 0
            print '------Metric instances for ME ------'
            association_class = 'CIM_MetricForME'  # association to traverse 
            result_class      = 'CIM_BaseMetricValue' # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = conn.Associators(ref, **in_params)
            for element in elements:
                print '    Metric %s(%s): Value=%s' % (element.classname, element['InstanceID'], element['MetricValue'])
            print 'Found %d metrics' % len(elements)
            if len(elements) == 0:
                print 'metric instances could not be found for device %s' % device
                isSuccess = 0
        print '======================================='
        return isSuccess

    def TestMetrics (self):
        self.TestBegin()
        returned_value1 = self.get_metrics_for_device(self.conn, "Xen_HostProcessor")
        returned_value2 = self.get_metrics_for_device(self.conn, "Xen_Processor")
        returned_value3 = self.get_metrics_for_device(self.conn, "Xen_Disk")
        returned_value4 = self.get_metrics_for_device(self.conn, "Xen_NetworkPort")
        self.TestEnd(returned_value1 or returned_value2 or returned_value3 or returned_value4)

#################################################################
    # test on CIM_LogicalElement
    """
    test this for Xen_Disk, Xen_Memory, Xen_Processor, Xen_NetworkPort
    
    """
    def get_processorPool_from_processor(self):
        self.TestBegin()
        proc_refs = self.conn.EnumerateInstanceNames("Xen_Processor")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_ProcessorAllocatedFromPool'  # association to traverse 
            result_class      = 'Xen_ProcessorPool'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Pool not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)
    
    def get_memoryPool_from_memory(self):
        self.TestBegin()
        #Xen_Memory
        mem_refs = self.conn.EnumerateInstanceNames("Xen_Memory")
        returned_value = 0
        #print len(mem_refs)
        for mem in mem_refs:
            association_class = 'Xen_MemoryAllocatedFromPool'  # association to traverse 
            result_class      = 'Xen_MemoryPool'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(mem, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Pool not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)
        
   
    def get_device_from_LogicalElement(self, logicalElement, assoc_class):
        dev_refs =  self.conn.EnumerateInstanceNames(logicalElement)
        for dev in dev_refs:
            association_class =  assoc_class # association to traverse 
            result_class      = 'CIM_LogicalDevice'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(dev, **in_params)
            if (len(elements) == 0):
                return 0
                # if doesn't return : then failed
            else:
                print dev['CreationClassName'] + " can enumerate the logical device"
                for element in elements:
                    print element.items()
                    
    def get_Device_from_Element(self):
        self.TestBegin()
        result = 0
        try:
            processor = self.get_device_from_LogicalElement('Xen_Processor', 'Xen_HostedProcessor')
            if (processor == 0):
                print "Xen_Processor cannot enumerate the logical element"
            memory = self.get_device_from_LogicalElement('Xen_Memory', 'Xen_HostedMemory')
            if (memory == 0):
                print "Xen_Memory cannot enumerate the logical element"
            network = self.get_device_from_LogicalElement('Xen_NetworkPort', 'Xen_HostedNetworkPort')
            if (network == 0):
                print "Xen_NetworkPort cannot enumerate the logical element"
            storage = self.get_device_from_LogicalElement('Xen_Disk', 'Xen_HostedDisk')
            if (storage == 0):
                print "Xen_Disk cannot enumerate the logical element"
                
            if ((processor == 0) or (memory == 0) or (storage == 0) or (network == 0)):
                result = 0
            else:
                result = 1
        except Exception, e:
            print e
        self.TestEnd(result)
    
    def get_resourcePool_from_Network(self):
        self.TestBegin()
        proc_refs = self.conn.EnumerateInstanceNames("Xen_NetworkPort")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_NetworkPortAllocatedFromPool'  # association to traverse 
            result_class      = 'Xen_NetworkConnectionPool'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Pool not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)
    
    def get_resourcePool_from_Storage(self):
        self.TestBegin()
        
        proc_refs = self.conn.EnumerateInstanceNames("Xen_Disk")
        returned_value = 0
        #print len(proc_refs)
        for proc in proc_refs:
            association_class = 'Xen_DiskAllocatedFromPool'  # association to traverse 
            result_class      = 'Xen_StoragePool'                    # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            elements = self.conn.AssociatorNames(proc, **in_params)
            if (len(elements) != 0):
                returned_value = len(elements)
            else:
                print "Pool not found"
            for element in elements:
                print element.items()
        self.TestEnd(returned_value)


#################################################################
    #Test on CIM_ComputerSystem(VM)

    #returns 0, may be a bug
    # Bug 571
    def get_vsms_from_ComputerSystem(self):
        self.TestBegin()
        vms_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        returned_value = 0
        #print len(vms_refs)
        for vm_ref in vms_refs:
            association_class = 'CIM_ServiceAffectsElement' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_VirtualSystemManagementService' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            #print vm_ref['Caption']
            list = self.conn.AssociatorNames(vm_ref, **in_params)
            if (len(list) != 0):
                returned_value = len(list)
            else:
                print "VSMS not found"
            for vs in list:
                print vs
        self.TestEnd(returned_value)
        

    # passed    
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

    #passed    
    def get_host_for_ComputerSystem(self):
        self.TestBegin()
        vms_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        retunred_value = 0
        for vm_ref in vms_refs:
            print 'Getting host for %s' % vm_ref['Name'] 
            association_class = 'Xen_HostedComputerSystem' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_HostComputerSystem' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            #print vm_ref['Caption']
            list = self.conn.AssociatorNames(vm_ref, **in_params)
            if (len(list) != 0):
                returned_value = len(list)
            else:
                print "    Host not found"
            for host in list:
                print "    " + str(host.items())
        self.TestEnd(returned_value)
        
    #passed
    def get_VSSD_for_ComputerSystem(self):
        self.TestBegin()
        vms_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        retunred_value = 0
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
        self.TestEnd(returned_value)
#################################################################   
    # Test on CIM_VSSD

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
                print '      %s (%s) to %s (%s) of type' % (rasd.classname, rasd['InstanceID'], vssd_ref.classname, vssd_ref['InstanceID'])
                print '          ResourceType:%d' % rasd['ResourceType']
        self.TestEnd(returned_value)        
    #passed
    def get_ComputerSystem_from_VSSD(self):
        self.TestBegin()
        returned_value = 0
        vssd_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystemSettingData")
        #print len(vssd_refs)
        for vssd_ref in vssd_refs:
            association_class = 'Xen_ComputerSystemSettingsDefineState' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_ComputerSystem' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            list = self.conn.AssociatorNames(vssd_ref, **in_params)
            if (len(list) != 0):
                returned_value = len(list)
            else:
                print "VMs not found"
            for element in list:
                elem_inst = self.conn.GetInstance(element)
                print "Caption" + elem_inst['Caption']
                print "EnabledState :"
                print elem_inst['EnabledState']
        self.TestEnd(returned_value)

#################################################################   

    def CreateTestVM(self):
        rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
        
        hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': rasds, 'ReferenceConfiguration': self.hvm_vssd}
        pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': rasds, 'ReferenceConfiguration': self.pv_vssd}
        
        print 'create vm test'
        self.pv_test_vm = CreateVM(self.conn, self.vsms, pv_params)
        self.hvm_test_vm = CreateVM(self.conn, self.vsms, hvm_params)
        if self.pv_test_vm == None or self.hvm_test_vm == None:
            sys.exit(1)
        
    def GetVM(self):
        pv_uuid = "20904d23-8a89-1d63-134c-d2606f2fcc47";
        hvm_uuid = "20904d23-8a89-1d63-134c-d2606f2fcc46";
        try:
            pvvm = GetVMFromUUID(self.conn, pv_uuid)
        except Exception, e:
            print e
        try:    
            hvm_vm = GetVMFromUUID(self.conn, hvm_uuid)
        except Exception, e:
            print e
    

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
                
            else:
                
                if verbose == "true":
                    print caps_inst.items()
                
                else:
                    if caps_inst['SynchronousMethodsSupported'] != None:
                        print '   SynchronousMethodsSupported' + ",".join(map(str, caps_inst['SynchronousMethodsSupported']))
                    if caps_inst['AsynchronousMethodsSupported'] != None:
                        print '   AsynchronousMethodsSupported' + ",".join(map(str, caps_inst['AsynchronousMethodsSupported']))

        print '*****************************************************************'

############################################################################################

    # Test on VSMS_caps

        
    def get_Host_from_VSMS_Caps(self):
        self.TestBegin()
        vms_refs = self.conn.EnumerateInstanceNames("Xen_VirtualizationCapabilities")
        returned_value = 0
        for vm_ref in vms_refs:
            association_class = 'CIM_ElementCapabilities' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_HostComputerSystem' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            #print vm_ref['Caption']
            list = self.conn.AssociatorNames(vm_ref, **in_params)
            if (len(list) != 0):
                returned_value = len(list)
            else:
                print "Host not found"
            for host in list:
                print host.items()
        self.TestEnd(returned_value)

##########################################################################################        
   #Test CIM_AllocationCapabilities
    def resource_pool_from_resource_allocation_cap(self):
        self.TestBegin()
        ac_refs = self.conn.EnumerateInstanceNames("CIM_AllocationCapabilities")
        returned_value = 0
        for ac_ref in ac_refs:
            association_class = 'CIM_ElementCapabilities' # association to traverse via Xen_ComputerSystem
            result_class      = 'CIM_ResourcePool' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            list = self.conn.AssociatorNames(ac_ref, **in_params)
            if (len(list) != 1):
                print "%s: %d resource pools found instead of just 1" % (ac_ref.classname, len(list))
            else:
                returned_value = 1
                print '%s: Found exactly 1 pool' % (ac_ref.classname)
            for pool in list:
                print '%s: %s' % (pool.classname, str(pool.items()))
        self.TestEnd(returned_value)

    def get_RASD_from_AllocationCapabilities(self):
        self.TestBegin()
        ac_refs = self.conn.EnumerateInstanceNames("CIM_AllocationCapabilities")
        returned_value = 0
        #print len(vms_refs)
        for ac_ref in ac_refs:
            print 'Finding RASD associated with AllocationCapabilities %s (%s)' % (ac_ref['InstanceID'], ac_ref.classname)
            association_class = 'CIM_SettingsDefineCapabilities' # association to traverse via Xen_ComputerSystem
            result_class      = 'CIM_ResourceAllocationSettingData' # class we are looking for
            #result_class      = 'CIM_ResourcePool' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            list = self.conn.AssociatorNames(ac_ref, **in_params)
            if (len(list) != 0):
                returned_value = len(list)
            else:
                print "RASD not found"
            for rasd in list:
                print rasd.items()
        self.TestEnd(returned_value)

    def resource_alloc_cap_from_resource_pool(self):
        self.TestBegin()
        pool_refs = self.conn.EnumerateInstanceNames("CIM_ResourcePool")
        returned_value = 0
        for pool_ref in pool_refs:
            print 'getting associated allocation capabilities for %s' % pool_ref.classname
            association_class = 'CIM_ElementCapabilities' # association to traverse via Xen_ComputerSystem
            result_class      = 'CIM_AllocationCapabilities' # class we are looking for
            in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
            #print vm_ref['Caption']
            list = self.conn.AssociatorNames(pool_ref, **in_params)
            if (len(list) != 1):
                print "%s: %d AllocationCapabilities found, instead of 1" % (pool_ref.classname, len(list))
            else:
                returned_value = 1
                print "%s: exactly 1 AllocationCapabilities found" % (pool_ref.classname)
            for ac in list:
                print ac.items()
        self.TestEnd(returned_value)

    def DeleteInitialVM(self):
        if (self.hvm_test_vm != None):
            DeleteVM(self.conn, self.vsms[0], self.hvm_test_vm)
        if (self.pv_test_vm != None):
            DeleteVM(self.conn, self.vsms[0], self.pv_test_vm)

#######################################################################################
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
    username= sys.argv[2]
    password = sys.argv[3]
    at = AssociationTest(Ip, username, password)
    try:
        ##############################
        # Test CIM_RegisteredProfile
        print "Test CIM_RegisteredProfile"
        at.get_host_from_SV_profile()       # get central instance of the System Virtualization Profile
        at.get_host_from_vs_profile()       # get central instance of the Virtual System Profile
        at.get_vs_profile_from_host()       # get the Virtual System profile registration from a host
        at.get_sv_profile_from_host()       # get the System Virtualization Profile registration class from a host
        
        print "Test CIM_VSMS"
        at.get_vsms_from_ComputerSystem()   # Get the VirtualSystemManagementService associated with a VM
        at.get_ComputerSystem_from_vsms()   # Get all VMs associated with a VirtualSystemManagementService
        at.get_VSMS_Capabilities_from_VSMS()# Get the Capabilities associated with a VirtualSystemManagementService class

        print 'Test VSMS Capabilities'
        at.get_vsms_caps_from_host()        # Get VSMS capabilities associated with the 
        at.get_Host_from_VSMS_Caps()        # Get Capabilities of the VirtualSystemManagementService class
        at.get_enabledLogicalElementCapabilities_for_ComputerSystem() # get the Virtualization Capavilities associated with a host

        print "Test CIM_System"
        at.get_vms_from_host()              # Get all VMs hosted on a xenserver host
        at.get_logicalDevice_from_host()    # get all devices associated with a host
        at.get_memoryPool_from_host()       # Get the memory pool associated with a host
        at.get_processorPool_from_host()    # get the processor pool associated with a host
        at.get_networkPool_from_switch()    # get the network pool associated with a virtual switch
        at.get_storagePool_from_host()      # get the storage pool associated with a host
        at.get_vsms_from_host()             # get the virtual system management service associated with a host
        at.get_host_for_ComputerSystem()    # Get the host that a VM is running on

        print "Test CIM_LogicalDevice"
        at.get_logicalElement_from_logicalDevice() # Get the computerSystem a device belongs to
        at.get_host_from_LogicalDevice()           # Get the host associated a host logical device belongs to

        print 'Test Xen_VirtualSwitch'
        at.get_switchport_from_switch()     # get switchports associated with a VirtualSwitch
        at.get_network_connections()        # get network connections to VMs associated with a VirtualSwitch
        
        print "Test CIM_VSSD and CIM_RASD"
        at.get_ComputerSystem_from_VSSD()   # Get the VM associated with a VirtualSystemSettingData (VSSD)
        at.get_RASD_from_VSSD()             # Get all the ResourceAllocationSettingDatas (RASDs) for resource added to a VM identified by the VSSD
        at.get_VSSD_for_ComputerSystem()    # Get the VSSD associated with a VM
        at.get_VSSD_for_RASD()              # get the VSSD associated with devices identified by the RASD
        at.get_MemoryPool_from_memoryRASD() # get the memory pool associated with memory resource identified by the RASd 
        at.get_ProcessorPool_from_processorRASD() # get the processor pool associated with processor resource identified by the RASd
        at.get_NetworkPool_from_networkRASD() # get the network pool associated with network resource identified by the RASd
        at.get_StoragePool_from_diskRASD()  # get the storage pool associated with disk resource identified by the RASd
        
        #Test CIM_ResourcePool
        print "Test CIM_ResourcePool"
        at.get_memory_from_memoryPool()             # Get the memory allocated from a memorypool
        at.get_processor_from_processorPool()       # Get all processors allocated from a processor pool
        at.get_memoryRASD_from_MemoryPool()         # Get all memory resources identified by the RASD allocated from the memory pool
        at.get_processorRASD_from_ProcessorPool()   # Get all processor resources identified by the RASD allocated from the processor pool
        at.get_Host_from_memoryPool()               # Find the host that a memory pool is associated with
        at.get_Host_from_processorPool()            # Find the hosts that a processor pool is associated with
        at.get_Switch_from_networkPool()              # Find the host that a network pool is associated with
        at.get_Host_from_storagePool()              # Find the host that a storage pool is associated with
        at.get_networkRASD_from_NetworkPool()       # Find all resources allocated from a network pool
        at.get_diskRASD_from_StoragePool()          # find all resources allocated from a storage pool
        at.get_disk_from_storagePool()              # find all disk devices allocated to VMs from a storage pool
        at.get_network_from_networkPool()           # find all network devices allocated to VMs from a network pool
    
        #Test on CIM Logical Element
        print "Test on CIM Logical Element"
        at.get_Device_from_Element()                # Get all devices assocated with a VM
        at.get_memoryPool_from_memory()             # Get memory pool associated that a memory device was allocated out of
        at.get_processorPool_from_processor()       # Get processor pool associated that a proc device was allocated out of
        at.get_resourcePool_from_Network()          # Get network connection pool that a NIC device was allocated out of
        at.get_resourcePool_from_Storage()          # Get stroage pool that a disk device was allocated out of
    
        #Test on AllocationCapabilities
        print "Test on AllocationCapabilities"
        at.resource_pool_from_resource_allocation_cap# Get resource pool associated with its capabilities
        at.get_RASD_from_AllocationCapabilities()   # get the RASD (default/min/max) associated with allocation capabilities
        at.resource_alloc_cap_from_resource_pool()               # find the allocation capabilities associated with a resource pool

        print 'Test on device metrics'
        #at.TestMetrics()                            # Test all metrics available with all devices associated with a VM
    finally:
        at.TestCleanup()
    sys.exit(0)
    
