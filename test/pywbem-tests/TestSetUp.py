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
from xen_cim_operations import *

class TestSetUp:
    def __init__(self, Ip, userName, password, need_shared_storage=False, create_vms=True, logfile=None):
        dir = os.getcwd()
        if logfile == None:
            self.logfilename= sys._getframe(2).f_code.co_filename + ".log"
        else:
            self.logfilename = logfile
        print "Location of result log : " + self.logfilename
        self.oldstdout = sys.stdout
        self.oldstderr = sys.stderr
        self.logfile = open(self.logfilename,'w')
        sys.stdout = self.logfile
        sys.stderr = self.logfile

        print 'TestSetup...'

        verbose = "false"
        for arg in sys.argv:
            if arg == "verbose":
                verbose = "true"

        # some useful counters
        self.TestsPassed = 0
        self.TestsFailed = 0
        self.TestFailedDescriptions = ""

        #self.inputMsg = ">"
        self.IPAddress = Ip
        self.UserName = userName
        self.Password = password
        
        
        self.hvmname = 'test-hvm-vm'
        self.pvname = 'test-pv-vm'
        self.pv_test_vm= None
        self.hvm_test_vm = None

        # create a CIM connection to the server
        self.conn = pywbem.WBEMConnection('http://'+self.IPAddress, (self.UserName, self.Password))

        # Enumerate the Xen Pool
        my_pool = None
        pools = self.conn.EnumerateInstances("Xen_HostPool")
        if len(pools) != 0:
            my_pool = pools[0]

        # Enumerate all networks and pick an appropriate network to use
        network_to_use = None
        networks = self.conn.EnumerateInstances("Xen_NetworkConnectionPool")
        for network in networks:
            network_to_use = network # pick the first one ? this will be what the VM will be connected on
            break
        print "Using Network: %s (%s)" % (network_to_use['PoolID'], network_to_use['Name'])

        # Pick an appropriate SR for use in tests
        self.sr_to_use = None
        sr_to_use_local = None
        if my_pool != None:
            # Try the default storage of the host pool, if available
            if my_pool['DefaultStoragePoolID'] != None and my_pool['DefaultStoragePoolID'] != '':
                print 'default storage pool id %s' % my_pool['DefaultStoragePoolID']
                #query_str = "SELECT * FROM Xen_StoragePool WHERE InstanceID = \""+my_pool['DefaultStoragePoolID']+"\""
                query_str = "SELECT * FROM Xen_StoragePool WHERE InstanceID LIKE \"%"+my_pool['DefaultStoragePoolID']+"%\""
                print 'Executing query: %s' % query_str
                defsrs = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
                sr_to_use_local = defsrs[0]
                #sr_ref = CIMInstanceName(classname="Xen_StoragePool", keybindings={"InstanceID":'Xen:Shared\\'+my_pool["DefaultStoragePoolID"]})
                #sr_to_use_local  = self.conn.GetInstance(sr_ref)
        if sr_to_use_local == None: # No default SR is available
            srs = self.conn.EnumerateInstances("Xen_StoragePool")
            for sr in srs:
                if sr['ResourceSubType'] == 'nfs' or sr['ResourceSubType'] == 'lvmoiscsi':
                    sr_to_use_local = sr
                    break
                elif need_shared_storage == False and sr['Name'] == 'Local storage':
                    sr_to_use_local = sr
                    break
        print "Using SR: %s (%s)" % (sr_to_use_local["PoolID"] , sr_to_use_local['Name'])

        # create CIM reference out of CIM instance
        self.sr_to_use =  CIMInstanceName(classname=sr_to_use_local.classname, keybindings={"InstanceID":sr_to_use_local["InstanceID"]})
        self.DiskPoolID = sr_to_use_local['PoolID']                

        # Get instance of Virtual System Management Service
        self.vsms = self.conn.EnumerateInstanceNames("Xen_VirtualSystemManagementService")

        ################ Virtual System Setting Data (VSSD) ################
        # VSSD for a PV VM type
        self.pv_vssd = CIMInstance("Xen_ComputerSystemSettingData")
        self.pv_vssd['Description']         = 'Test PV VM'
        self.pv_vssd['ElementName']         = self.__class__.__name__ + '_PV'
        self.pv_vssd['VirtualSystemType']   = 'DMTF:xen:PV' 
        self.pv_vssd['PV_Bootloader']       = 'pygrub'
        self.pv_vssd['AutomaticShutdownAction'] = pywbem.Uint8(2)
        self.pv_vssd['AutomaticStartupAction'] = pywbem.Uint8(3)
        self.pv_vssd['AutomaticRecoveryAction'] = pywbem.Uint8(3)
        self.pv_vssd['PV_Args']             = 'Term=xterm'
        self.pv_vssd['Other_Config']        = ['HideFromXenCenter=false']

       
        # Virtual System setting data for an HVM type 
        self.hvm_vssd = CIMInstance("Xen_ComputerSystemSettingData")
        self.hvm_vssd['Description'] = 'Test HVM VM'
        self.hvm_vssd['ElementName'] = self.__class__.__name__ + '_HVM'
        self.hvm_vssd['VirtualSystemType'] = 'DMTF:xen:HVM'
        self.hvm_vssd['HVM_Boot_Params'] = ['order=dc']
        self.hvm_vssd['HVM_Boot_Policy'] = 'BIOS order'
        self.hvm_vssd['Platform'] = ['acpi=true','apic=true','pae=true']

        #######################################################################
        # define all the Virtual System Resource Allocation Setting Data (RASD)
        #######################################################################
         
        #######################################################################
        # RASD to specify processor allocation for the VM
        # Processor RASD
        self.proc_rasd = CIMInstance('CIM_ResourceAllocationSettingData')
        self.proc_rasd['ResourceType'] = pywbem.Uint16(3)
        self.proc_rasd['VirtualQuantity'] = pywbem.Uint64(1)
        self.proc_rasd['AllocationUnits'] = 'count'

        # Another processor RASD with different limit/weight values
        self.proc1_rasd = self.proc_rasd.copy()
        self.proc1_rasd['VirtualQuantity'] = pywbem.Uint64(1)
        self.proc1_rasd['Limit'] = pywbem.Uint32(95)    # max host CPU it could take up in %
        self.proc1_rasd['Weight'] = pywbem.Uint32(512)  # relative weight between VCPUs (1-65536)

        # Processor RASD with wrong resource type
        self.invalid_proc_rasd = self.proc_rasd.copy()
        self.invalid_proc_rasd['ResourceType'] = pywbem.Uint32(10000)

        # processor RASD with no resource type specified, the classname has to be base class
        self.nort_proc_rasd = CIMInstance('CIM_ResourceAllocationSettingData')
        self.nort_proc_rasd['VirtualQuantity'] = pywbem.Uint64(1)
        self.nort_proc_rasd['AllocationUnits'] = 'count'
        
        # processor RASD with invalid quantity specified
        self.invalid_vq_proc_rasd = self.proc_rasd.copy()
        self.invalid_vq_proc_rasd['VirtualQuantity'] = pywbem.Uint64(10000)

        # plain old bad processor rasd - mixed types for properties
        self.bad_proc_rasd = self.proc_rasd.copy()
        self.bad_proc_rasd['ResourceType'] = '3' # string instead of integer
        self.bad_proc_rasd['VirtualQuantity'] = '1'
        self.bad_proc_rasd['AllocationUnits'] = pywbem.Uint8(1) # integer instead of string
        self.bad_proc_rasd['Limit'] = '100'      # string instead of integer

        #######################################################################
        # memory RASD to specify memory allocation settings for a VM
        self.mem_rasd = CIMInstance('Xen_MemorySettingData')
        self.mem_rasd['ResourceType'] = pywbem.Uint16(4)
        self.mem_rasd['VirtualQuantity'] = pywbem.Uint64(512)
        self.mem_rasd['AllocationUnits'] = 'byte*2^20'

        # 2nd RASD to specify more memory
        self.mem1_rasd = self.mem_rasd.copy()
        self.mem1_rasd['VirtualQuantity'] = pywbem.Uint64(1024)

        # mix in wrong types (integers for strings, and strings for integers)
        self.bad_mem_rasd = self.mem_rasd.copy()
        self.bad_mem_rasd['AllocationUnits'] = pywbem.Uint32(20)
        self.bad_mem_rasd['VirtualQuantity'] = "1024"
        self.bad_mem_rasd['ResourceType'] = "4"

        #######################################################################
        # Resource Allocation Settings to specify Virtual Disk allocation
        
        # Start off with a CD drive 
        self.disk0_rasd = CIMInstance('Xen_DiskSettingData')
        self.disk0_rasd['ElementName'] = self.__class__.__name__ + '_CDRom'
        self.disk0_rasd['ResourceType'] = pywbem.Uint16(15)    # DVD Drive
        self.disk0_rasd['ResourceSubType'] = 'CD'
        # self.disk1_rasd['PoolID'] = '<SR ID>' # (No need to specify a SR or VDI, should create an empty DVD
        # self.disk1_rasd['HostResource'] = '<VDI ID>' # (No need to specify a SR or VDI, should create an empty DVD
        self.disk0_rasd['Bootable'] = True
        self.disk0_rasd['Access'] = pywbem.Uint8(1)

        # A virtual disk of 2 GB size
        self.disk1_rasd = self.disk0_rasd.copy()
        self.disk1_rasd['PoolID'] = sr_to_use_local['PoolID']
        self.disk1_rasd['ElementName'] = self.__class__.__name__ + '_Disk1'
        self.disk1_rasd['ResourceType'] = pywbem.Uint16(19)   # Storage extent
        self.disk1_rasd['ResourceSubType'] = 'Disk'
        self.disk1_rasd['VirtualQuantity'] = pywbem.Uint64(2147483648) # 2GB
        self.disk1_rasd['AllocationUnits'] = 'byte'

        # Another virtual disk of 2 GB size
        self.disk2_rasd = self.disk0_rasd.copy()
        self.disk2_rasd['PoolID'] = sr_to_use_local['PoolID']
        self.disk2_rasd['ResourceType'] = pywbem.Uint16(19)   # Storage extent
        self.disk2_rasd['ResourceSubType'] = 'Disk'
        self.disk2_rasd['ElementName'] = self.__class__.__name__ + '_Disk2'
        self.disk2_rasd['VirtualQuantity'] = pywbem.Uint64(2147483648) # 2GB
        self.disk2_rasd['AllocationUnits'] = 'byte'

        # mix in wrong types (integers for strings, and strings for integers)
        self.bad_disk_rasd = self.disk0_rasd.copy()
        self.bad_disk_rasd['PoolID'] = sr_to_use_local['PoolID']
        self.bad_disk_rasd['ElementName'] = pywbem.Uint8(1)
        self.bad_disk_rasd['ResourceType'] = '15'
        self.bad_disk_rasd['HostResource'] = pywbem.Uint16(12345)
        self.bad_disk_rasd['Bootable'] = pywbem.Uint8(1)

        # create a disk out of a non-existent SR
        self.invalid_poolId_rasd = self.disk0_rasd.copy()
        self.invalid_poolId_rasd['PoolId'] = 'ed1bd47e-1ab8-d80a-aecf-06447871211c'

        # Specify bad allocation units
        self.invalid_aunits_rasd = self.disk0_rasd.copy()
        self.invalid_aunits_rasd['PoolID'] = sr_to_use_local['PoolID']
        self.invalid_poolId_rasd['AllocationUnits'] = 'KiloMeters'

        #######################################################################
        # Specify the network connection resource allocation setting data 
        # The system will create a virtual NIC for each resource
        # 
        self.nic_rasd = CIMInstance('Xen_NetworkPortSettingData')
        self.nic_rasd['ResourceType'] = pywbem.Uint16(33)          # ethernet connection type
        self.nic_rasd['Address'] = '00:13:72:24:32:f4' # manually generated MAC
        self.nic_rasd['PoolID']  = network_to_use['PoolID']

        # NIC RASD With no mac address - generate one
        # NIC RASD with no ElementName either - pick next available address
        self.nic1_rasd = self.nic_rasd.copy()
        del self.nic1_rasd['Address']

        # RASD with no resource type
        self.nort_nic_rasd = self.nic_rasd.copy()
        del self.nort_nic_rasd['ResourceType']

        # RASD with bad MAC address specified
        self.invalid_nic_rasd = self.nic_rasd.copy()
        self.invalid_nic_rasd['Address'] = '00:13:72:24:32:rr'

        # mixed types for properties (strings for integers and vice versa)
        self.bad_nic_rasd = self.nic_rasd.copy()
        self.bad_nic_rasd['ResourceType'] = '33'            # supposed to be a int
        self.bad_nic_rasd['ElementName'] = pywbem.Uint8(0)  # supposed to be a string
        self.bad_nic_rasd['Address'] = pywbem.Uint64(001372243245) # supposed to be a string

        #######################################################################
        # Create the test VMs if requested during the test
        #
        # Create the PV VM from the Demo Linux VM template (previously the Debian Etch Template)
        try:
            pv_template_list = self.conn.ExecQuery("WQL", "SELECT * FROM Xen_ComputerSystemTemplate WHERE ElementName LIKE \"%XenServer Transfer VM%\"", "root/cimv2")
            self.pv_template = CIMInstanceName(classname=pv_template_list[0].classname, keybindings={"InstanceID":pv_template_list[0]["InstanceID"]})

            self.rasds = [self.proc_rasd, self.mem_rasd, self.disk0_rasd, self.disk1_rasd, self.nic_rasd]
            self.hvm_params = {'SystemSettings': self.hvm_vssd, 'ResourceSettings': self.rasds}
            self.pv_params =  {'SystemSettings': self.pv_vssd, 'ResourceSettings': self.rasds, 'ReferenceConfiguration':self.pv_template }
            self.pv_test_vm = None
            self.hvm_test_vm = None

            if create_vms:
                #print 'using template %s to create PV vm: ' % str(debian_template)
                print 'Creating PV vm'
                self.pv_test_vm = CreateVM(self.conn, self.vsms[0], self.pv_params)
                print 'Creating HVM vm'
                self.hvm_test_vm = CreateVM(self.conn, self.vsms[0], self.hvm_params)
        except Exception, e:
            print "Exception: %s. Has the TVM template been installed?" % e

    ###########################################################################
    # Helper routines
    # #########################################################################
    def get_vm_id (self, vm_ref):
        return vm_ref['Name']

    def read_config(self):
        try:
            fin = open(r"config.txt","r")
            linelist = fin.readlines()
            fin.close()
            for line in linelist:
                ar = line.split(':')
                if (ar[0] == "Server IP"):
                    self.ip = ar[1].strip()
                print ar[0]
                print ar[1]
                #parse line
        except Exception, e:
            print e
    
    def TestCleanup(self):
        print 'Test cleanup.....'
        if (self.hvm_test_vm != None):
            DeleteVM(self.conn, self.vsms[0], self.hvm_test_vm)
            print "Delete HVM VM "
            print self.hvm_test_vm

        if (self.pv_test_vm != None):
            DeleteVM(self.conn, self.vsms[0], self.pv_test_vm)
            print "Delete PV VM "
            print self.pv_test_vm
        self.logfile.close()
        sys.stderr = self.oldstderr
        sys.stdout = self.oldstdout
        print 'Tests Passed: %d' % self.TestsPassed
        print 'Tests Failed: %d ( %s )' % (self.TestsFailed, self.TestFailedDescriptions)


    def TestBegin (self):
        print '################################################'
        print 'Start: ' + sys._getframe(1).f_code.co_name

    # result = 1 is success, 0 is failure
    def TestEnd (self, result):
        self.publish_result(result)
        print 'End: ' + sys._getframe(1).f_code.co_name

    # result = 0 is success, 1 is failure
    def TestEnd2 (self, result):
        if result == 0:
            result = 1
        else:
            result = 0
        self.publish_result(result)
        print 'End: ' + sys._getframe(1).f_code.co_name

    def pause_for_user (self, msg):       
        input = raw_input(msg)
            
    def publish_result(self, result):
        if (result == 0):
            print "Test Failed"
            self.TestsFailed = self.TestsFailed + 1
            if self.TestFailedDescriptions == "":
                self.TestFailedDescriptions = sys._getframe(2).f_code.co_name
            else:
                self.TestFailedDescriptions = "%s, %s" % (self.TestFailedDescriptions, sys._getframe(2).f_code.co_name)
        else:
            print "Test Passed"
            self.TestsPassed = self.TestsPassed + 1

    # The following function is required to write the output of the test
    # test in a form that Silk understands
    def create_output_xml(self, errorCount, testName):
        f = open(r'output.xml','w')
        f.write('<ResultElement TestItem=\"'+testName+'\">')
        f.write('\n')
        f.write('<ErrorCount>'+str(errorCount)+'</ErrorCount>')
        f.write('\n')
        f.write('<WarningCount>0</WarningCount>')
        f.write('\n')
        f.write('<Incident>')
        f.write('\n')
        f.write('<Message></Message>')
        f.write('\n')
        if (errorCount == 0):
            f.write('<Severity>Passed</Severity>')
        else:
            f.write('<Severity>Error</Severity>')
        f.write('\n')
        f.write('<Detail>')
        f.write('\n')
        f.write('<TestName></TestName>')
        f.write('\n')
        f.write('<Info></Info>')
        f.write('\n')
        f.write('</Detail>')
        f.write('\n')
        f.write('</Incident>')
        f.write('\n')
        f.write('</ResultElement>')
        f.close()

if __name__ == '__main__':
    print "Test SetUp Done"
