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
from xen_cim_operations import *
from TestSetUp import *

'''
Excecises methods in the Xen_VirtualSwitchManagementService class.
Allows caller to create/delete internal/external/bonded networks.
'''
class NetworkTests(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, False, False)
        vsms = self.conn.EnumerateInstanceNames("Xen_VirtualSwitchManagementService")
        self.vsms = vsms[0]
        self.vssd = CIMInstance('Xen_VirtualSystemSettingData')
        self.vssd['ElementName'] = self.__class__.__name__ + 'Network'
        self.vssd['Description'] = "Test network created by the network test"

    def EnumerateSwitches (self):
        self.TestBegin()
        result = 1
        sws = self.conn.EnumerateInstanceNames("Xen_VirtualSwitch")
        if len(sws) == 0:
            result = 0
        for sw in sws:
            sw_inst = self.conn.GetInstance(sw)
            print 'Found Virtual Switch : %s (%s)' % (sw_inst['Name'], sw_inst['ElementName'])
        self.TestEnd(result)

    def EnumerateHostNetworkPorts (self):
        self.TestBegin()
        result = 1
        nwps = self.conn.EnumerateInstanceNames("Xen_HostNetworkPort")
        if len(nwps) == 0:
            result = 0
        for nwp in nwps:
            nwp_inst = self.conn.GetInstance(nwp)
            print 'Found Host Network Port : %s (%s)' % (nwp_inst['DeviceID'], nwp_inst['Name'])
        self.TestEnd(result)


    def __CreateNetwork (self, rasds):
        new_switch = None
        try:
            in_params = {'SystemSettings': self.vssd}
            if rasds != None:
                in_params['ResourceSettings'] = rasds
            (rval, out_params) = self.conn.InvokeMethod('DefineSystem', self.vsms, **in_params)
            print 'CreateNetwork returned %d' % rval
            if (rval == 0) and out_params != None:
                new_switch = out_params['ResultingSystem']
                print 'Created new switch %s' % new_switch.items()
        except pywbem.CIMError, arg:
            print 'Caught exception when calling %s' % (__name__)
            if arg[0] != pywbem.CIM_ERR_NOT_SUPPORTED:
                print 'InvokeMethod(instancename): %s' % arg[1]
        return new_switch

    def __ModifyNetwork (self, vssd):
        result = 0
        try:
            in_params = {'SystemSettings': vssd}
            print vssd.items()
            (rval, out_params) = self.conn.InvokeMethod('ModifySystemSettings', self.vsms, **in_params)
            if rval == 0:
                result = 1
        except pywbem.CIMError, arg:
            print 'Caught exception when calling %s' % (__name__)
            if arg[0] != pywbem.CIM_ERR_NOT_SUPPORTED:
                print 'InvokeMethod(instancename): %s' % arg[1]
        return result

    def __DeleteNetwork (self, switch):
        result = 0
        if switch != None:
            try:
                in_params = {'AffectedSystem': switch}
                (rval, out_params) = self.conn.InvokeMethod('DestroySystem', self.vsms, **in_params)
                if rval == 0:
                    switch_inst = None
                    try:
                        switch_inst = self.conn.GetInstance(switch)
                    except:
                        print 'switch couldnt be found - success'
                    if switch_inst == None:
                        result = 1
                    else:
                        print 'Switch %s doesnt appear to have been deleted' % switch_inst['Name']
            except pywbem.CIMError, arg:
                print 'Caught exception when calling %s' % (__name__)
                if arg[0] != pywbem.CIM_ERR_NOT_SUPPORTED:
                    print 'InvokeMethod(instancename): %s' % arg[1]
        else:
            print 'Test is not executed since switch doesnt exist'
        return result

    def __FindNicsAssociatedWithSwitch (self, switch):
        print switch.items()
        nics = []
        nwps = self.conn.EnumerateInstances("Xen_HostNetworkPortSettingData")
        for nwp in nwps:
            if nwp['VirtualSwitch'] == switch['Name']:
                nics.insert(0, nwp)
        print 'Found %d nics associated with switch %s' % (len(nics), switch['Name'])
        return nics
    #
    # Attach a host NIC to a Virtual Switch 
    # This changes a 'private' network to an 'externally connected' via the host NIC
    # Xen automatically connects the same ethernet interface (eth0, eth1) on all
    # hosts and hence makes the Virtual Switch available on all hosts
    # 
    def __AttachNicToNetwork (self, switch, nics):
        result = 0
        if switch != None:
            try:
                rasd = CIMInstance('Xen_HostNetworkPortSettingData')
                rasd['ResourceType'] = pywbem.Uint16(33)
                rasd['VlanTag'] = pywbem.Uint64(23)
                rasd['Connection'] = nics
                print 'adding resources %s' % rasd.items()
                vssd = CIMInstanceName('Xen_VirtualSwitchSettingData')
                vssd['InstanceID'] = 'Xen:'+switch['Name']
                nic_rasds = [rasd]
                in_params = {'AffectedConfiguration': vssd, 'ResourceSettings': nic_rasds}
                (rval, out_params) = self.conn.InvokeMethod('AddResourceSettings', self.vsms, **in_params)
                if rval == 0:
                    result = 1
                    try:
                        nics = self.__FindNicsAssociatedWithSwitch(switch)
                        if len(nics) == 0:
                            result = 0
                    except Exception, e:
                        print 'exception %s was received' % str(e)
                        result = 0
                else:
                    print 'Method returned error %d' % rval
            except pywbem.CIMError, arg:
                result = 0
                print 'Caught exception when calling %s' % (__name__)
                if arg[0] != pywbem.CIM_ERR_NOT_SUPPORTED:
                    print 'InvokeMethod(instancename): %s' % arg[1]
        else:
            print 'Test is not executed since switch doesnt exist'
        return result

    #
    # Remove a host NIC connection from a Virtual Switch 
    # This changes an 'externally connected' network to a 'private' network.
    # 
    def __RemoveNicsFromNetwork(self, switch, nic_rasds):
        result = 0
        rasds = []
        for nic_rasd in nic_rasds:
            # BUGBUG Using the nic_rasd directly results in pegasus complaining about XML validataion errors 
            # because it finds LOCALINSTANCEPATH in the XML instead of serialized XML
            rasd = CIMInstance('Xen_HostNetworkPortSettingData')
            rasd['InstanceID'] = nic_rasd['InstanceID']
            rasd['ResourceType'] = nic_rasd['ResourceType']
            rasds.append(rasd)
        in_params = {'ResourceSettings': rasds}
        (rval, out_params) = self.conn.InvokeMethod('RemoveResourceSettings', self.vsms, **in_params)
        if rval == 0:
            nics = self.__FindNicsAssociatedWithSwitch(switch)
            if len(nics) == 0:
                result = 1
            else:
                result = 0
        return result

    #
    # Set a host NIC as a 'management' NIC, which sets the DNS configuration for the NIC etc.
    # 
    def __SetManagementInterface (self, nic_rasd_orig, mode, ip, mask, purpose):
        result = 0
        try:
            # BUGBUG Using the nic_rasd_orig directly results in pegasus complaining about XML validataion errors 
            # because it finds LOCALINSTANCEPATH in the XML instead of serialized XML
            nic_rasd = CIMInstance('Xen_HostNetworkPortSettingData')
            nic_rasd['IPConfigurationMode'] = pywbem.Uint8(mode) # 0=None,1=DHCP,2=static,3=undefined
            nic_rasd['IPAddress'] = ip
            nic_rasd['IPSubnetMask'] = mask
            nic_rasd['ManagementPurpose'] = purpose
            nic_rasd['InstanceID'] = nic_rasd_orig['InstanceID'] 
            rasds = [nic_rasd]
            in_params = {'ResourceSettings': rasds}
            (rval, out_params) = self.conn.InvokeMethod('ModifyResourceSettings', self.vsms, **in_params)
            if rval == 0:
                query_str = "SELECT * FROM Xen_HostNetworkPortSettingData WHERE InstanceID = \"" + nic_rasd['InstanceID'] + "\""
                new_rasds = self.conn.ExecQuery("WQL", query_str, "root/cimv2")
                print 'Checking interface %d(%d), %s(%s), %s(%s), %s(%s)' % (new_rasds[0]['IPConfigurationMode'], mode, new_rasds[0]['IPAddress'], ip, new_rasds[0]['IPSubnetMask'], mask, new_rasds[0]['ManagementPurpose'], purpose)
                if mode != 0:
                    if new_rasds[0]['IPConfigurationMode'] == mode and new_rasds[0]['IPAddress'] == ip and new_rasds[0]['IPSubnetMask'] == mask and new_rasds[0]['ManagementPurpose'] == purpose:
                        print 'interface has been updated'
                        result = 1
                else:
                    if new_rasds[0]['IPConfigurationMode'] == 0 and new_rasds[0]['IPAddress'] == ip and new_rasds[0]['IPSubnetMask'] == mask and new_rasds[0]['ManagementPurpose'] == None :
                        print 'interface has been updated'
                        result = 1
        except pywbem.CIMError, arg:
            result = 0
            print 'Caught exception when calling %s' % (__name__)
            if arg[0] != pywbem.CIM_ERR_NOT_SUPPORTED:
                print 'InvokeMethod(instancename): %s' % arg[1]
        return result

    # Test to set the management interface on a NIC and unset it
    def SetManagementInterfaceTest (self):
        self.TestBegin()
        result_set = []
        nic_rasds = []
        print 'TestRequirement: Presence of eth2. This test attempts to set a fake management IP address on all NICs named eth2'
        ip_address = "192.168.100.100"
        nics = self.conn.EnumerateInstances("Xen_HostNetworkPortSettingData")
        testresult = 0
        for nic in nics:
            testresult = 1
            if nic['Connection'][0] == 'eth2':
                print 'Setting management interface on %s' % nic['InstanceID']
                result_set.append(self.__SetManagementInterface(nic, 2, '192.168.100.100', '255.255.255.0', 'test')) # Static IP
                print 'Removing management interface on %s' % nic['InstanceID']
                result_set.append(self.__SetManagementInterface(nic, 0, '', '', '')) # unset the management interface, No IP address specified
        for result in result_set:
            print 'result is %d' % result
            if result == 0:
                testresult = 0
        self.TestEnd(testresult)

    # Code is duplicated here because Silk needs test cases to be unique
    def DeleteInternalNetwork (self, switch):
        self.TestBegin()
        rc = self.__DeleteNetwork(switch)
        self.TestEnd(rc)
    def DeleteInternalNetwork2 (self, switch):
        self.TestBegin()
        rc = self.__DeleteNetwork(switch)
        self.TestEnd(rc)

    def DeleteExternalNetwork (self, switch):
        self.TestBegin()
        rc = self.__DeleteNetwork(switch)
        self.TestEnd(rc)
    def DeleteExternalNetwork2 (self, switch):
        self.TestBegin()
        rc = self.__DeleteNetwork(switch)
        self.TestEnd(rc)

    def DeleteBondedNetwork (self, switch):
        self.TestBegin()
        rc = self.__DeleteNetwork(switch)
        self.TestEnd(rc)
    def DeleteBondedNetwork2 (self, switch):
        self.TestBegin()
        rc = self.__DeleteNetwork(switch)
        self.TestEnd(rc)
    def DeleteBondedNetwork3 (self, switch):
        self.TestBegin()
        rc = self.__DeleteNetwork(switch)
        self.TestEnd(rc)

    def CreateInternalNetwork (self):
        self.TestBegin()
        result = 0
        internal_network = self.__CreateNetwork(None)
        if internal_network != None:
            result = 1
        self.TestEnd(result)
        return internal_network

    def CreateExternalNetwork (self):
        self.TestBegin()
        result = 0
        rasd = CIMInstance('Xen_HostNetworkPortSettingData')
        rasd['ResourceType'] = pywbem.Uint16(33)
        rasd['Connection'] = ["eth0"]
        rasd['VlanTag'] = pywbem.Uint64(3)
        rasds = [rasd]
        external_network = self.__CreateNetwork(rasds)
        if external_network != None:
            result = 1
        # create another VLAN for the same interface with a different VLAN id
        rasd = CIMInstance('Xen_HostNetworkPortSettingData')
        rasd['ResourceType'] = pywbem.Uint16(33)
        rasd['Connection'] = ["eth0"]
        rasd['VlanTag'] = pywbem.Uint64(4)
        rasds = [rasd]
        external_network2 = self.__CreateNetwork(rasds)
        if external_network2 == None:
            result = 0
        else:
            self.__DeleteNetwork(external_network2)
        self.TestEnd(result)
        return external_network

    def CreateBondedNetwork (self):
        self.TestBegin()
        # eth1 and eth2 PIFs are bonded and a network is created out of them
        rasd1 = CIMInstance('Xen_HostNetworkPortSettingData')
        rasd1['ResourceType'] = pywbem.Uint16(33)
        rasd1['Connection'] = ["eth1"]
        rasd1['VlanTag'] = pywbem.Uint64(2)
        rasd2 = CIMInstance('Xen_NetworkPortSettingData')
        rasd2['ResourceType'] = pywbem.Uint16(33)
        rasd2['Connection'] = ["eth2"]
        rasd2['VlanTag'] = pywbem.Uint64(2)
        rasds = [rasd1, rasd2]
        result = 0
        bonded_network = self.__CreateNetwork(rasds)
        if bonded_network != None:
            result = 1
        self.TestEnd(result)
        return bonded_network

    def CreateBondedNetwork2 (self):
        self.TestBegin()
        # A second way of specify the two interfaces to bond (in the same RASD)
        # eth1 and eth2 PIFs are bonded and a network is created out of them
        rasd1 = CIMInstance('Xen_HostNetworkPortSettingData')
        rasd1['ResourceType'] = pywbem.Uint16(33)
        rasd1['Connection'] = ["eth1", "eth2"]
        rasd1['VlanTag'] = pywbem.Uint64(2)
        rasds = [rasd1]
        result = 0
        bonded_network = self.__CreateNetwork(rasds)
        if bonded_network != None:
            result = 1
        self.TestEnd(result)
        return bonded_network

    def ModifyExistingNetwork (self, switch):
        self.TestBegin()
        # Update the name and description of the switch and make sure its updated
        vssd = CIMInstance('Xen_VirtualSwitchSettingData')
        vssd['InstanceID'] = 'Xen:'+switch['Name']
        vssd['ElementName'] = 'ModifiedName'
        vssd['Description'] = 'ModifiedDescription'
        result = self.__ModifyNetwork(vssd)
        if result == 1:
            vssd_ref = CIMInstanceName('Xen_VirtualSwitchSettingData')
            vssd_ref['InstanceID'] = vssd['InstanceID']
            new_vssd = self.conn.GetInstance(vssd_ref)
            if new_vssd['ElementName'] == vssd['ElementName'] and new_vssd['Description'] == vssd['Description']:
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def ConvertInternalNetworkToExternalVLANNetwork (self):
        self.TestBegin()
        result = 0
        # create an internal network first
        interfaces = ['eth0']
        switch = self.__CreateNetwork(None)
        result = self.__AttachNicToNetwork(switch, interfaces)
        self.TestEnd(result)
        return switch

    def ConvertInternalNetworkToExternalBondedNetwork (self):
        self.TestBegin()
        result = 0
        # create an internal network first
        interfaces = ['eth1', 'eth2']
        switch = self.__CreateNetwork(None)
        result = self.__AttachNicToNetwork(switch, interfaces)
        self.TestEnd(result)
        return switch

    def ConvertExternalVLANNetworkToInternalNetwork (self):
        self.TestBegin()
        result = 0
        # create an internal network first
        rasd = CIMInstance('Xen_HostNetworkPortSettingData')
        rasd['ResourceType'] = pywbem.Uint16(33)
        rasd['Connection'] = ["eth0"]
        rasd['VlanTag'] = pywbem.Uint64(256)
        switch = self.__CreateNetwork(rasd)
        print switch.items()
        rasds = self.__FindNicsAssociatedWithSwitch(switch)
        result = self.__RemoveNicsFromNetwork(switch, rasds)
        self.TestEnd(result)
        return switch

    def CreateInvalidBondedNetwork (self):
        self.TestBegin()
        # 2 invalid pifs (with the wrong interfaces specified) are bonded
        rasd1 = CIMInstance('Xen_NetworkPortSettingData')
        rasd1['ResourceType'] = pywbem.Uint16(33)
        rasd1['Connection'] = ["eth11234", "eth2345"]
        rasd1['VlanTag'] = pywbem.Uint64(2)
        rasds = [rasd1]
        result = 0
        bonded_network = self.__CreateNetwork(rasds)
        if bonded_network == None:
            result = 1
        self.TestEnd(result)

    def CreateInvalidBondedNetwork2 (self):
        self.TestBegin()
        # one invalid pif is bonded with an valid pif
        rasd1 = CIMInstance('Xen_HostNetworkPortSettingData')
        rasd1['ResourceType'] = pywbem.Uint16(33)
        rasd1['Connection'] = ["eth11234", "eth1"]
        rasd1['VlanTag'] = pywbem.Uint64(2)
        rasds = [rasd1]
        result = 0
        bonded_network = self.__CreateNetwork(rasds)
        if bonded_network == None:
            result = 1
        self.TestEnd(result)

    def CreateNetworkErrorTests (self):
        self.TestBegin()
        result = 1
        # rasd with wrong resourcetype specified (needs to be 33)
        print 'Error Test: bad RASD with wrong resourcetype'
        try:
            rasd_with_wrong_resource_type = CIMInstance('CIM_ResourceAllocationSettingData')
            rasd_with_wrong_resource_type['ResourceType'] = pywbem.Uint16(19)
            rasd_with_wrong_resource_type['Connection'] = ["eth0"]
            rasd_with_wrong_resource_type['VlanTag'] = pywbem.Uint64(2)
            rasds = [rasd_with_wrong_resource_type]
            network = self.__CreateNetwork(rasds)
            if network != None:
                print 'Virtual Switch was created when it shouldnt have'
                result = 0
        except Exception, e:
            print 'Exception: %s' % str(e)
        # RASD where Connection property is of invalid type
        print 'Error Test: bad RASD with bad Connection property'
        try:
            rasd_with_wrong_connection = CIMInstance('CIM_ResourceAllocationSettingData')
            rasd_with_wrong_connection['ResourceType'] = pywbem.Uint16(33)
            rasd_with_wrong_connection['Connection'] = pywbem.Uint32(1234)
            rasd_with_wrong_connection['VlanTag'] = pywbem.Uint64(2)
            rasds = [rasd_with_wrong_connection]
            network = self.__CreateNetwork(rasds)
            if network != None:
                print 'Virtual Switch was created when it shouldnt have'
                result = 0
        except Exception, e:
            print 'Exception: %s' % str(e)

        self.TestEnd(result)

    def DeleteNetworkErrorTests (self):
        self.TestBegin()
        result = 1
        # specify a bad (non-existent) switch reference
        print 'Error Test: Bad switch reference'
        try:
            switch = {'Name': 'bad-reference', 'CreationClassName': 'Xen_VirtualSwitch'}
            in_params = {'AffectedSystem': switch}
            (rval, out_params) = self.conn.InvokeMethod('DestroySystem', self.vsms, **in_params)
            if rval == 0:
                print 'Delete switch worked when it shouldnt have'
                result = 0
        except Exception, e:
            print 'Exception: %s' % str(e)
        self.TestEnd(result)

    def AddInterfaceToSwitchErrorTests (self):
        self.TestBegin()
        result = 1
        # pass in a bad switch reference to the Add Call
        print 'Error Test: Bad switch reference'
        switch = self.__CreateNetwork(None)
        try:
            badswitch = {'Name': 'bad-reference', 'CreationClassName': 'Xen_VirtualSwitch'}
            connections = 'eth0'
            rval = self.__AttachNicToNetwork(badswitch, connections)
            if rval == 1:
                print 'AddResourceSettings worked when it shouldnt have'
                result = 0
        except Exception, e:
            print 'Exception: %s' % str(e)
        # specify in a bad connection reference to the Add Call
        print 'Error Test: Bad (non-existent) Ethernet connection in RASD'
        try:
            connections = 'eth12334'
            rval = self.__AttachNicToNetwork(switch, connections)
            if rval == 1:
                print 'AddResourceSettings worked when it shouldnt have'
                result = 0
        except Exception, e:
            print 'Exception: %s' % str(e)
        # specify in a good and bad connection reference to the Add Call
        print 'Error Test: Bad (non-existent) Ethernet connection in RASD'
        try:
            connections = 'eth1,eth12334'
            rval = self.__AttachNicToNetwork(switch, connections)
            if rval == 1:
                print 'AddResourceSettings worked when it shouldnt have'
                result = 0
        except Exception, e:
            print 'Exception: %s' % str(e)
        print 'Error Test: emtpy Ethernet connection in RASD'
        try:
            connections = ''
            rval = self.__AttachNicToNetwork(switch, connections)
            if rval == 1:
                print 'AddResourceSettings worked when it shouldnt have'
                result = 0
        except Exception, e:
            print 'Exception: %s' % str(e)
        self.__DeleteNetwork(switch)
        self.TestEnd(result)

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username, and password as arguments"
            print "Count is "+str(count)        
            sys.exit(0)

    st = NetworkTests(sys.argv[1], sys.argv[2], sys.argv[3])
    try:
        # Success tests
        st.EnumerateSwitches()                  # List all available Xen networks
        st.EnumerateHostNetworkPorts()          # list all PIFs available on the host
        st.SetManagementInterfaceTest()         # test to set and unset a management interface 

        switch1 = st.CreateInternalNetwork()    # create an internal network (no PIFs attached)
        switch2 = st.CreateExternalNetwork()    # create an external network (1 PIF attached using a VLAN tag)
        st.ModifyExistingNetwork(switch2)       # modify the name and description of the switch
        switch3 = st.CreateBondedNetwork()      # create a bonded network (more than 1 PIF attached to the same switch)
        st.DeleteInternalNetwork(switch1)       # Delete the bonded network
        st.DeleteExternalNetwork(switch2)       # delete the external network
        st.DeleteBondedNetwork(switch3)         # delete the internal network
        switch4 = st.CreateBondedNetwork2()     # create a bonded network with 2 PIFS by specifiying them int he same RASD (workaround for WS-Man array marshalling issue)
        st.DeleteBondedNetwork2(switch4)         # delete the newly bonded network
        switch5 = st.ConvertInternalNetworkToExternalVLANNetwork()   # create an internal network and then add a PIF to it
        st.DeleteExternalNetwork2(switch5)       # delete the switch
        switch6 = st.ConvertInternalNetworkToExternalBondedNetwork() # create an internal network and then add it to a bonded NIC
        st.DeleteBondedNetwork3(switch6)         # delete the switch
        switch7 = st.ConvertExternalVLANNetworkToInternalNetwork() # remove a pif from the switch and convert it from an external network to an internal network
        st.DeleteInternalNetwork2(switch7)
                                                 
        # Error tests
        st.CreateInvalidBondedNetwork()         # Create a bonded network where one of the PIFs is non-existent
        st.CreateInvalidBondedNetwork2()        # create a bonded network where both the PIFs are the same.
        st.CreateNetworkErrorTests()            # Pass invalid parameters to the DefineSystem method and expect errors back
        st.DeleteNetworkErrorTests()            # Pass invalid parameters to the DestroySystem method and expect errors back
        st.AddInterfaceToSwitchErrorTests()     # Pass invalid parameters to the AddResourceSettings method and expect errors back
    finally:
        st.TestCleanup()
    
    sys.exit(0)


