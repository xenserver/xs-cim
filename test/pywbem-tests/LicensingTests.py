#!/usr/bin/env python

'''Copyright (C) 2009 Citrix Systems Inc.

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
from TestSetUp import *

'''
Exercises methods in the Xen_HostPool class that require licensing to have been configurued. 
Allows caller to configure a XenServer host pool for high availability
'''

class LicensingTest(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, need_shared_storage=False, create_vms=False)
        self.pool_user = userName
        self.pool_pass = password
        pools = self.conn.EnumerateInstanceNames("Xen_HostPool")
        self.pool_ref = pools[0]

    def get_pool_inst (self):
        # print 'getting instance of [%s]' % str(self.pool_ref)
        pool_inst = self.conn.GetInstance(self.pool_ref)
        return pool_inst

    def get_ha(self):
        pool_inst = self.get_pool_inst()
        return pool_inst['HighAvailabilityEnabled']

    ###########################################################################
    # Test requirement: 
    #    1) A non-default iSCSI SR is available to host the HA statefile.
    #    2) The servers are licensed properly
    ########################################################################### 
    def enable_ha (self):
        self.TestBegin()
        isSuccess = 0
        pool_inst = self.get_pool_inst()
        print 'TestRequirement: Availability of a non-default iSCSI SR on this pool. (Required to host the HA statefile).'
        print 'TestRequirement: Proper licenses assigned to the pool hosts.'
        in_params = {}
        try:
            (rval, out_params) = self.conn.InvokeMethod('EnableHighAvailability', self.pool_ref, **in_params)
            if rval == 0:
                pool_inst = self.conn.GetInstance(self.pool_ref)
                ha_enabled_after = pool_inst["HighAvailabilityEnabled"]
                if ha_enabled_after == True:
                    isSuccess = 1
                    print "HA is turned on for pool %s" % self.pool_ref['InstanceID']
                else:
                    print "Turning on HA failed. No error info available."
            else:
                print "Turning on HA failed with return value %d. Perhaps no non-default iSCSI SR is available to host the HA statefile ? " % rval
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        self.TestEnd(isSuccess)

    def disable_ha (self):
        self.TestBegin()
        isSuccess = 0
        pool_inst = self.get_pool_inst()
        ha_enabled_before = pool_inst["HighAvailabilityEnabled"]
        in_params = {}
        try:
            (rval, out_params) = self.conn.InvokeMethod('DisableHighAvailability', self.pool_ref, **in_params)
            if rval == 0:
                pool_inst = self.conn.GetInstance(self.pool_ref)
                ha_enabled_after = pool_inst["HighAvailabilityEnabled"]
                if ha_enabled_after == False:
                    isSuccess = 1
                    print "HA is turned off for pool %s" % self.pool_ref['InstanceID']
                else:
                    print "HA turn off failed"
            else:
                print "HA turn off failed with return value %d" % rval
        except Exception, e:
            sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        self.TestEnd(isSuccess)

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username and password as arguments "
            print "Count is "+str(count)        
            sys.exit(0)
    Ip = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    lictest = LicensingTest(Ip, username, password)
    print "Pool Licensing Tests"
    try:
        #
        # The HA Tests require hosts to be licensed
        # 
        ha_enabled = lictest.get_ha()        # Get the current High Availability setting of the pool
        if ha_enabled:              
            lictest.disable_ha()             # Disable it (starting point)
        lictest.enable_ha()                  # Enable HA
        lictest.disable_ha()                 # Disable HA
    finally:
        lictest.TestCleanup()
    sys.exit(0)
