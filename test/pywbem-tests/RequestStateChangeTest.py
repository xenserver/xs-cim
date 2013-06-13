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
Exercises the RequestStateChange method of the Xen_ComputerSystem class.
Allows caller to change the states of a VM (running, stopped etc)
'''
class RequestStateChange(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password)
        self.testVM = self.GetTargetVM()
        association_class = 'Xen_ComputerSystemElementCapabilities' # association to traverse via Xen_ComputerSystem
        result_class      = 'Xen_ComputerSystemCapabilities' # class we are looking for
        in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
        elements = self.conn.AssociatorNames(self.testVM, **in_params)
        self.supported_states = None
        for element in elements:
            element_inst = self.conn.GetInstance(element)
            self.supported_states = element_inst['RequestedStatesSupported']

##############################################################################        

    def state_is_supported (self, state):
        for item in self.supported_states:
            if item == state:
                print 'State %d is supported by VM.' % state
                return True
        print 'State %d is not supported by VM.' % state
        return False

    def getCurrentState(self, vm_ref):
        vm_inst = self.conn.GetInstance(vm_ref)
        return vm_inst['EnabledState']
    
    def changeVMState_PowerOn(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(2):
            self.VM_PowerOn()
            state = self.getCurrentState(self.testVM)
            result = 0
            if (str(state) == '2'):
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def xen_tools_are_up(self, testVM):
        # get the IP address of the VM, whih is available only when the tools are up
        network_ports = GetNetworkPortsForVM (self.conn, testVM)
        for network_port in network_ports:
            inst = self.conn.GetInstance(network_port)
            if 'NetworkAddresses' in inst:
                if inst['NetworkAddresses'] != None:
                    if len(inst['NetworkAddresses']) != 0:
                        print 'found networkAddress: %s, xen-tools must be up and running...' % inst['NetworkAddresses'][0]
                        break # finding one IP address is good enough

    def VM_PowerOn(self):
        state = self.getCurrentState(self.testVM)
        if (str(state) == '2'):
            print "Already started"
            time.sleep(5)
        else:
            print 'starting the VM'
            in_params = {'RequestedState':'2'} # Start the VM
            ChangeVMState(self.conn, self.testVM, in_params, True, '2')
            i = 1
            while i < 12:
                time.sleep(5) # xen-tools services take a while to start up
                if (self.xen_tools_are_up(self.testVM)):
                    break
                i = i + 1
    
    def changeVMState_Shutdown(self):
        self.TestBegin()        
        result = 1
        if self.state_is_supported(4):
            self.VM_PowerOn()
            in_params = {'RequestedState':'4'} # safe shutdown
            ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            #state = self.getCurrentState(self.testVM)
            vm_inst = self.conn.GetInstance(self.testVM)
            result = 0
            if (vm_inst['status'] == 'Stopped'):
                result = 1
            else:
                result = 0
        self.TestEnd(result)
        
    def changeVMState_Reboot(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(10):
            self.VM_PowerOn()
            in_params = {'RequestedState':'10'} # safe reboot
            ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            state = self.getCurrentState(self.testVM)
            result = 0
            if (str(state) == '2'):
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def changeVMState_Queisce(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(9):
            self.VM_PowerOn()
            in_params = {'RequestedState':'9'} # pause the VM
            ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            state = self.getCurrentState(self.testVM)
            result = 0
            if (str(state) == '9'):
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def changeVMState_Disabled(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(3):
            self.VM_PowerOn()
            in_params = {'RequestedState':'3'}  # shut it down
            ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            state = self.getCurrentState(self.testVM)
            result = 0
            if (str(state) == '3'):
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def changeVMState_Reset(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(11):
            self.VM_PowerOn()
            in_params = {'RequestedState':'11'} # hard shutdown/power reset
            ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            state = self.getCurrentState(self.testVM)
            result = 0
            if (str(state) == '2'):
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def changeVMState_HardShutdown(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(32768):
            self.VM_PowerOn()
            in_params = {'RequestedState':'32768'}  # hard shutdown/power reset
            ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            state = self.getCurrentState(self.testVM)
            result = 0
            if (str(state) != '2'):
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def changeVMState_HardReboot(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(32769):
            self.VM_PowerOn()
            in_params = {'RequestedState':'32769'} # hard reboot
            ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            state = self.getCurrentState(self.testVM)
            result = 0
            if (str(state) == '2'):
                result = 1
            else:
                result = 0
        self.TestEnd(result)
        
    def changeVMState_Defer(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(8):
            self.VM_PowerOn()
            in_params = {'RequestedState':'8'} # not supported
            n = ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            state = self.getCurrentState(self.testVM)
            result = 0
            if ((str(state) == '8') and (n == 1)):
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def changeVMState_Test(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(7):
            self.VM_PowerOn()
            in_params = {'RequestedState':'7'} # not supported
            n = ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            result = 0
            state = self.getCurrentState(self.testVM)
            if ((str(state) == '7') and (n == 1)):
                result = 1
            else:
                result = 0
        self.TestEnd(result)

    def changeVMState_Offline(self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(6):
            self.VM_PowerOn()
            in_params = {'RequestedState':'6'} # suspend
            n = ChangeVMState(self.conn, self.testVM, in_params)
            time.sleep(5)
            result = 0
            state = self.getCurrentState(self.testVM)
            if ((str(state) == '6') and (n == 1)):
                result = 1
            else:
                result = 0
        self.TestEnd(result)
        
    def changeVMState_NoParams (self):
        self.TestBegin()
        result = 1
        if self.state_is_supported(7):
            self.VM_PowerOn()
            in_params = {} 
            n = ChangeVMState(self.conn, self.testVM, in_params)
            if n == 1:
                print 'Success returned while expecting failure'
                result = 1
        self.TestEnd(result)
        

    def GetTargetVM(self):
        vssd = CIMInstance('Xen_ComputerSystemSettingData')
        vssd['ElementName'] = 'RequestStateChangeTestCommonTargetVM'
        vssd['Description'] = "VM to test state changes"
        vssd['Other_Config'] = ['HideFromXenCenter=false']
        return CreateVMBasedOnTemplateName(self.conn, self.vsms[0], "XenServer Transfer VM", vssd)

    def LocalCleanup (self):
        print 'Deleting local VM' + str(self.testVM.items())
        DeleteVM(self.conn, self.vsms[0], self.testVM)

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
    cd = RequestStateChange(Ip, username, password)
    try:
        # Exercises the states of the VM using the DMTF specified states.
        cd.changeVMState_PowerOn()      # Power on a VM
        cd.changeVMState_Queisce()      # Pause a VM
        cd.changeVMState_Disabled()     # Disable a VM
        cd.changeVMState_Shutdown()     # Shutdown a VM
        cd.changeVMState_Reboot()       # Reboot the VM
        cd.changeVMState_Reset()        # Reset the VM
        cd.changeVMState_Offline()      # Suspend the VM
        cd.changeVMState_HardReboot()   # Xen specific: HardReboot the VM
        cd.changeVMState_HardShutdown() # Xen specific: HardShutdown the VM

        #Error scenarios
        cd.changeVMState_Defer()        # Deferred is not a state Xen supports
        cd.changeVMState_Test()         # Test is not a state Xen supports
        cd.changeVMState_NoParams()     # Test for erros when no parameters are sent
        print ''

    #+++++++++++++++++++++++++++++++++++++++
    finally:
        cd.LocalCleanup()
        cd.TestCleanup()

    sys.exit(0)
    
