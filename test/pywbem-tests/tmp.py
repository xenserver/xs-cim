#!/usr/bin/env python

import sys
import pywbem
import time
from xml.dom import minidom
import os
from xen_cim_operations import *
from TestSetUp import *
from StorageTests import StorageTests
from BVTTest import BVTTest
from ModifySystemSettingsTest import ModifySystemSettings
from ModifyResourceSettingTest import ModifyResourceSetting
from AddResourceSettingsTest import AddResourceSettingsTest
from DefineSystemTest import DefineSystemTest
from AssociationTest import AssociationTest
from MigrationTests import MigrationTest
from NetworkTests import NetworkTests
from RequestStateChangeTest import RequestStateChange
from PoolTests import PoolTest
import bits

class Test(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, False, False) # uses the log file
        self.vsms = self.conn.EnumerateInstanceNames('Xen_VirtualSystemManagementService')

        vssd_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")

        for ref in vssd_refs:
            if ref['Name'] == '86a0c952-ab91-d699-69de-8e3d5435281c':
                self.test_vm = ref
                print "found vm %s" % self.test_vm

    def test1 (self):
        vsms = self.conn.EnumerateInstanceNames("Xen_VirtualSwitchManagementService")
        vssd = CIMInstance('Xen_VirtualSystemSettingData')
        vssd['ElementName'] = 'Shashi &amp; Network'
        vssd['Description'] = "Test &amp; network created by the network test"
        in_params = {'SystemSettings': vssd}
        (rval, out_params) = self.conn.InvokeMethod('DefineSystem', vsms[0], **in_params)

    def test2 (self):
        vsms = self.conn.EnumerateInstanceNames("Xen_VirtualSystemManagementService")
        vssd = CIMInstance('Xen_VirtualSystemSettingData')
        vssd['ElementName'] = 'Shashi &amp; Kiran'
        vssd['VirtualSystemType']   = 'DMTF:xen:PV' 
        vssd['PV_Bootloader']       = 'pygrub'
        vssd['AutomaticShutdownAction'] = pywbem.Uint8(2)
        vssd['AutomaticStartupAction'] = pywbem.Uint8(3)
        vssd['AutomaticRecoveryAction'] = pywbem.Uint8(3)
        vssd['PV_Args']             = 'Term=xterm'
        vssd['Other_Config']        = ['HideFromXenCenter=false']
        vssd['Description'] = "Test &amp; vm created by the network test"
        in_params = {'SystemSettings': vssd}
        (rval, out_params) = self.conn.InvokeMethod('DefineSystem', vsms[0], **in_params)

    def test3 (self):
        print "uploadig to http://5cbbf0b78d3f9873:d4122dea8ea5f60e@192.168.2.62:80/vdi_uuid_5aad0561-944d-4404-af6d-b515562805e4"
        #bits.upload('/tmp/SnapshotTreeImportSample/e66e3c22-bdfa-4575-b35f-c9df8c7382ad.vhd', 
        bits.upload('/tmp/SnapshotTreeImportSample/e66e3c22-bdfa-4575-b35f-c9df8c7382ad.vhd', 
                    'http', '192.168.2.62', '80', '5cbbf0b78d3f9873', 'd4122dea8ea5f60e', 
                    '/vdi_uuid_5aad0561-944d-4404-af6d-b515562805e4')
        print 'upload successful'

    def test4 (self):
	print "Executing test 4 - Enumerating Xen_KVP objects"
	vsms = self.conn.EnumerateInstanceNames("Xen_KVP")
	print "Xen_KVPs: %s" % vsms

        for vsm in vsms:
            print "Get Instance"
            instance = self.conn.GetInstance(vsm)
            print "%s = %s" % (instance['Key'], instance['Value'])
            print instance

    def test5 (self):
        print "Executing test 5 - Enumerate Instances"
        instances = self.conn.EnumerateInstances("Xen_KVP")
        for instance in instances:
            print instance.items()

    def test_setup_KVP (self):
        print "Setup KVP Communication"
        SetupKVPCommunication(self.conn, self.test_vm)
                

    def test_create_KVP (self):
        #in_params = {'RequestedState':'2'}
        #ChangeVMState(self.conn, self.test_vm, in_params, True, '2')
        key = "testkey2"
        value = "testvalue2"

        kvp_rasd = CIMInstance('Xen_KVP')
        kvp_rasd['ResourceType'] = pywbem.Uint16(40000)
        kvp_rasd['Key'] = key
        kvp_rasd['Value'] = value

        print "KVP RASD = %s" % kvp_rasd
        print "PV: %s" % self.test_vm

        in_params = {'ResourceSetting': kvp_rasd, 'AffectedSystem': self.test_vm}
        AddVMResource(self.conn, self.vsms[0], in_params)

        rasds_to_delete = []
        # Enumerate the created RASD
        instances = self.conn.EnumerateInstanceNames("Xen_KVP")

        for instance in instances:
            print "Instance: %s" % instance
            tmp1 = CIMInstance("Xen_KVP")
            tmp1['DeviceID'] = instance['DeviceID']
            tmp1['ResourceType'] = pywbem.Uint16(40000)
            tmp1['InstanceID'] = instance['DeviceID']
            #tmp1['Key'] = "testkey2"
            #tmp1['Value'] = "testvalue2"
            #tmp1['Value'] = instance['Value']
            rasds_to_delete.append(tmp1)

        vssds = GetVSSDsForVM(self.conn, self.test_vm)
        rasds = GetRASDsFromVSSD(self.conn, vssds[0])

        print "Rasds: %s" % rasds
        print "Keys 1: %s" % rasds[0].keys()

        print "Keys 2: %s" % rasds_to_delete[0].keys()
        print "About to delete %s" % rasds_to_delete
        in_params = {'ResourceSettings': rasds_to_delete}
        rc = DeleteVMResources(self.conn, self.vsms[0], in_params)
        print rc
        
        print "done"

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username, and password as arguments"
            print "Count is "+str(count)        
            sys.exit(0)
    tst  = Test(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = BVTTest(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = ModifySystemSettings(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = ModifyResourceSetting(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = MigrationTest(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = DefineSystemTest(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = AssociationTest(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = NetworkTests(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = StorageTests(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = AddResourceSettingsTest(sys.argv[1], sys.argv[2], sys.argv[3])
    #tst = RequestStateChange(sys.argv[1], sys.argv[2], sys.argv[3])
    try:
        #tst.test4()
        #tst.test5()
        #tst.test_setup_KVP()
        tst.test_create_KVP()
        #tst.create_nfsiso_sr()
        #tst.get_session_to_login_to_vm_console()
        #tst.get_vms_from_host()
        #tst.get_host_for_ComputerSystem()
        #tst.create_iscsi_sr()
        #tst.create_vm_without_resourceType()
        #tst.find_migrationservice()
        #tst.migrate_vm_to_host_name()
        #tst.find_migrationservice()
        #tst.addValidProcessor()
        #tst.addValidMemory()
        #tst.get_host_from_SV_profile()
        #tst.ConvertInternalNetworkToExternalVLANNetwork()
        #tst.SetManagementInterfaceTest()
        #tst.CreateInternalNetwork()
        #tst.get_networkPool_from_switch()
    except Exception, e:
        print 'Exception: %s' % str(e)
    finally:
        tst.TestCleanup()
        print 'done'
    sys.exit(0)
