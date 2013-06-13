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
import datetime
import time
import getpass
import os
from xen_cim_operations import *
from TestSetUp import *

'''
Exercises the methods in the Xen_MetricsService class to gather metrics.
This allows caller to get the historical metrics of VMs and hosts in Xport XML format
or instantaneous metrics for devices as CIM objects.
'''
class MetricTests(TestSetUp):

    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, False, True)
        self.mss = self.conn.EnumerateInstanceNames("Xen_MetricService")
        # start the PV VM, so we get some metrics
        in_params = {'RequestedState':'2'} 
        ChangeVMState(self.conn, self.pv_test_vm, in_params, True, '2')

    def get_historical_host_metrics (self):
        self.TestBegin()
        interval = 0
        rc = 1
        hosts = self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        for host in hosts:
            in_params = { "System": host, "TimeDuration":pywbem.Uint32(60) } # last 1 hour of metrics
            print 'Getting Metrics for host %s from the last 60 mins' % (host['Name'])
            try:
                [rc, out_params] = self.conn.InvokeMethod("GetPerformanceMetricsForSystem", self.mss[0], **in_params)
            except pywbem.cim_operations.CIMError:
                print 'Exception caught getting metrics'
            if rc == 0:
                print '    Metrics: %s' % out_params["Metrics"]
            else:
                print '    NO METRICS AVAILABLE'
        self.TestEnd2(rc)

    def get_historical_vm_metrics (self):
        self.TestBegin()
        vms = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        rc = 1
        for vm in vms:
            one_hour_delta = timedelta(hours=1)
            one_hour_ago = datetime.now() - one_hour_delta
            starttime = CIMDateTime(one_hour_ago)
            in_params = {"System": vm, "StartTime" : starttime} # 'EndTime' defaults to 'Now'
            print 'Getting Metrics for VM %s' % vm['Name']
            try:
                [rc, out_params] = self.conn.InvokeMethod("GetPerformanceMetricsForSystem", self.mss[0], **in_params)
            except pywbem.cim_operations.CIMError:
                print 'Exception caught getting metrics'
            if rc == 0:
                outxml = out_params["Metrics"]
                print '    Metrics: %s' % outxml
            else:
                print '    NO METRICS AVAILABLE'
        self.TestEnd2(rc)

    def test_instantaneous_metrics (self):
        self.TestBegin()
        rc = 0
        proc_utilizations = self.conn.EnumerateInstances("Xen_ProcessorUtilization")
        print 'Xen_ProcessorUtilization (VM)'
        for metric in proc_utilizations:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        host_proc_utilizations = self.conn.EnumerateInstances("Xen_HostProcessorUtilization")
        print 'Xen_HostProcessorUtilization (hosts)'
        for metric in host_proc_utilizations:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        network_port_tx_thrput = self.conn.EnumerateInstances("Xen_NetworkPortTransmitThroughput")
        print 'Xen_NetworkPortTransmitThroughput (VM NICs)'
        for metric in network_port_tx_thrput:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        network_port_rx_thrput = self.conn.EnumerateInstances("Xen_NetworkPortReceiveThroughput")
        print 'Xen_NetworkPortReceiveThroughput (VM NICs)'
        for metric in network_port_rx_thrput:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        host_network_port_tx_thrput = self.conn.EnumerateInstances("Xen_HostNetworkPortTransmitThroughput")
        print 'Xen_HostNetworkPortTransmitThroughput (Host NICs)'
        for metric in host_network_port_tx_thrput:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        host_network_port_rx_thrput = self.conn.EnumerateInstances("Xen_HostNetworkPortReceiveThroughput")
        print 'Xen_HostNetworkPortReceiveThroughput (Host NICs)'
        for metric in host_network_port_rx_thrput:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        disk_reads = self.conn.EnumerateInstances("Xen_DiskReadThroughput")
        print 'Xen_DiskReadThroughput (VM Disks)'
        for metric in disk_reads:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        disk_writes = self.conn.EnumerateInstances("Xen_DiskWriteThroughput")
        print 'Xen_DiskWriteThroughput (VM Disks)'
        for metric in disk_writes:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        disk_read_latencies = self.conn.EnumerateInstances("Xen_DiskReadLatency")
        print 'Xen_DiskReadLatency (VM Disks)'
        for metric in disk_read_latencies:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])
        disk_write_latencies = self.conn.EnumerateInstances("Xen_DiskWriteLatency")
        print 'Xen_DiskWriteLatency (VM Disks)'
        for metric in disk_write_latencies:
            print '    InstanceID: %s, util:%s at %s' % (metric['InstanceID'], metric['MetricValue'], metric['TimeStamp'])

        # Test is successful if we got at least one object out of enumerating these classes
        if(len(proc_utilizations) > 0 and
           len(host_proc_utilizations) > 0 and
           len(network_port_tx_thrput) > 0 and
           len(network_port_rx_thrput) > 0 and
           len(host_network_port_tx_thrput) > 0 and
           len(host_network_port_rx_thrput) > 0 and
           len(disk_reads) > 0 and
           len(disk_writes) > 0 and
           len(disk_read_latencies) > 0 and
           len(disk_write_latencies) > 0):
            rc = 1
        self.TestEnd(rc)

    def LocalCleanup (self):
        in_params = {'RequestedState':'4'} 
        ChangeVMState(self.conn, self.pv_test_vm, in_params, True, '4')

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username, and password as arguments"
            print "Count is "+str(count)        
            sys.exit(0)
    mt = MetricTests(sys.argv[1], sys.argv[2], sys.argv[3])
    try:
        mt.get_historical_host_metrics()   # Get historical metrics for a Host, in Xport form
        mt.get_historical_vm_metrics()     # get historical metrics for a VM, in Xport form
        mt.test_instantaneous_metrics()   # Test all classes that represent instantaneous metrics (proc utilization, nic reads and writes/s etc)
    finally:
        mt.LocalCleanup()
        mt.TestCleanup()
    
    sys.exit(0)

