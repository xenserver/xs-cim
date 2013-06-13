#!/usr/bin/env python

# Copyright (C) 2008 Citrix Systems Inc
#
#    This library is free software; you can redistribute it and/or
#    modify it under the terms of the GNU Lesser General Public
#    License as published by the Free Software Foundation; either
#    version 2.1 of the License, or (at your option) any later version.
#
#    This library is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#    Lesser General Public License for more details.
#
#    You should have received a copy of the GNU Lesser General Public
#    License along with this library; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USAimport sys

import sys
import pywbem
from pywbem.cim_obj import *
import time

##########################################################################
# Subroutines
# 
# 
#
def GetHost (conn):
    profile_refs = conn.EnumerateInstanceNames("Xen_RegisteredSVProfile", 
                                               "root/interop")
    for profile_ref in profile_refs:
        profile = conn.GetInstance(profile_ref)
        print 'Elements conforming to profile:' + profile_ref['InstanceID']
        association_class = 'CIM_ElementConformsToProfile'  # association to traverse 
        result_class      = 'CIM_System'                    # result class we are looking for
        in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
        elements = conn.AssociatorNames(profile_ref, **in_params)
        for element in elements:
           print element.items()
    return elements


def WaitForJobCompletion (conn, job_ref):
    print 'waiting for job %s' % job_ref['InstanceID']
    job = conn.GetInstance(job_ref);
    while (job['JobState'] == 4) or (job['JobState'] == 3) or (job['JobState'] == 5):
        print 'waiting for job... state is now %d' % job['JobState']
        time.sleep(3)
        job = conn.GetInstance(job_ref);

    if job['JobState'] != 7:
        print 'Job failed: job state:%d' % job['JobState']
        print '          : Job error code :%d' % job['ErrorCode']
        print '          : description :' + job['ErrorDescription']
    else:
        print 'Job completed successfully'
    return job

def DeleteJob(conn, job_ref):
    print 'Deleting job %s' % job_ref['InstanceID']
    # 
    # Either a DeleteInstance or KillJob should work
    # conn.DeleteInstance(job_ref)
    # 
    in_params = { "DeleteOnKill": True }
    conn.InvokeMethod('KillJob', job_ref, **in_params)

def ConvertCIMInstanceToMofFormat (classname, inst):

    mof_string = inst.tomof()
    #mof_string = '''instance of %s {
#<PROPERTYLIST>};''' % classname
#    property_list_str = ''
#    for k,v in inst.items():
#        # just use the string and integer types for now
#        if type(v) == type(u'string'):
#            property_list_str += '%s="%s";\n' % (k,v)
#        elif type(v) == type('string'):
#            property_list_str += '%s="%s";\n' % (k,v)
#        elif (type(v) == pywbem.cim_types.Uint16):
#            property_list_str += '%s=%d;\n' % (k,v)
#        elif (type(v) == pywbem.cim_types.Uint32):
#            property_list_str += '%s=%d;\n' % (k,v)
#        elif (type(v) == pywbem.cim_types.Uint64):
#            property_list_str += '%s=%d;\n' % (k,v)
#        elif (type(v) == pywbem.cim_types.Real64):
#            property_list_str += '%s=%f;\n' % (k,v)
#        elif (type(v) == type([])):
#            property_list_str += '%s="%s";\n' % (k, v[0])
#        else:
#            print 'skipping %s of type %s' % (v, type(v))
        #else:
        #    print 'skipping property %s of type %s' % (k, type(v))
#    mof_string = mof_string.replace('<PROPERTYLIST>', property_list_str)
    return mof_string

def ConvertWBEMURIToCIMInstanceName (wbem_uri):
    #WBEM URI is of te form 'namespace:Classname.propertyname="value",propertyname="value"'
    [ns, colon, classandproperties] = wbem_uri.partition(':')
    [classname, dot, propertylist] = classandproperties.partition('.')
    keys = {}
    while propertylist != None and propertylist.find('=') != -1:
        [name, equals, propertylist] = propertylist.partition('=')
        [value, comma, propertylist] = propertylist.partition(',')
        keys[name] = value.strip('"')
    new_vm =  CIMInstanceName(classname, keys)
    return new_vm

##########################################################################
# Invoke DefineSystem on Virtual System Management Serive to define a new VM,
# providing the new VM settings and its resource settings.
def CreateVM (conn, vsms, in_params):
    new_vm = None
    job_ref = None
    try:
        (rval, out_params) = conn.InvokeMethod('DefineSystem', vsms, **in_params)
    
        if((rval != 4096) and (rval != 0)):
            sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
            return None
    
        new_vm = out_params['ResultingSystem']
        if rval == 4096:
            job_ref = out_params['Job']
            job = WaitForJobCompletion(conn, job_ref)
            if job['JobState'] != 7:
                new_vm = None;
            DeleteJob(conn, job_ref)
            time.sleep(2)
    
    except pywbem.CIMError, arg:
        print 'Caught exception when calling %s' % (__name__)
        if arg[0] != pywbem.CIM_ERR_NOT_SUPPORTED:
            print 'InvokeMethod(instancename): %s' % arg[1]
            return None
    
    except ValueError, e:
        pass
    
    return new_vm

# Invoke CopySystem on Virtual System Management Serive to define a 
# new VM based on an existing template or a vm.
def CopyVM (conn, vsms, vm_name, in_params):
    new_vm = None
    job_ref = None
    try:
        (rval, out_params) = conn.InvokeMethod('CopySystem', vsms, **in_params)
        if((rval != 4096) and (rval != 0)):
            sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
            return None

        if rval == 4096:
            job_ref = out_params['Job']
            job = WaitForJobCompletion(conn, job_ref)
            if job['JobState'] != 7:
                new_vm = None;
            else:
                vm_wbem_uri = job['ResultingSystem'].encode('ascii', 'ignore') # convert unicode string to ascii
                new_vm = ConvertWBEMURIToCIMInstanceName(vm_wbem_uri)
                print 'CopySystem returned %s' % str(new_vm)
            DeleteJob(conn, job_ref)
            time.sleep(2)
    except pywbem.CIMError, arg:
        print 'Caught exception when calling %s' % (__name__)
        if arg[0] != pywbem.CIM_ERR_NOT_SUPPORTED:
            print 'InvokeMethod(instancename): %s' % arg[1]
            return None
    except ValueError, e:
        pass
    return new_vm

# Setup a VM for KVP communication

def SetupKVPCommunication(conn, vm, in_params={}):
    try:
        (rval, out_params) = conn.InvokeMethod('SetupKVPCommunication', vm, **in_params)

        if((rval != 4096) and (rval != 0)):
            sys.stderr.write('% returned error: %s\n' % (__name__, rval))
            return None
    except Exception, e:
        sys.stderr.write("exception returned %s" % str(e))
           
           

def ConvertVMToTemplate (conn, vsms, in_params):
    vm = None
    template_ref = None
    try:
        (rval, out_params) = conn.InvokeMethod('ConvertToXenTemplate', vsms, **in_params)
        if rval == 0:
            template_ref = out_params['ResultingTemplate']
    except pywbem.CIMError, arg:
        print 'Caught exception when calling %s' % (__name__)
    return template_ref


##########################################################################
# Delete a VM
# 
def DeleteVM(conn, vsms, vm):
    
    in_params = {'AffectedSystem': vm}
    try:
        (rval, out_params) = conn.InvokeMethod('DestroySystem', vsms, **in_params)
        time.sleep(2)
    except Exception, e:
         sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
         return 0
    
    if rval != 0:
        sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
        return 0

    return 1

##########################################################################
# Modify a VM's settings
# 
def ModifyVM(conn, vsms, in_params):
    try:
        (rval, out_params) = conn.InvokeMethod('ModifySystemSettings', vsms, **in_params)
        time.sleep(2)
    except Exception, e:
         sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
         return 0
    
    if rval != 0:
        sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
        return 0

    return 1

##########################################################################
# Create VM snapshot
# 
def CreateVMSnapshot (conn, vsss, in_params):
    try:
        (rval, out_params) = conn.InvokeMethod('CreateSnapshot', vsss, **in_params)
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return None
    if rval != 0:
        sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
        return None
    new_snapshot = out_params['ResultingSnapshot']
    return new_snapshot

##########################################################################
# Apply VM snapshot
# 
def RevertVMToSnapshot (conn, vsss, in_params):
    try:
        (rval, out_params) = conn.InvokeMethod('ApplySnapshot', vsss, **in_params)
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return 0
    if rval != 0:
        sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
        return 0
    return 1


##########################################################################
# Destroy VM snapshot
# 
def DestroyVMSnapshot (conn, vsss, in_params):
    try:
        (rval, out_params) = conn.InvokeMethod('DestroySnapshot', vsss, **in_params)
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return 0
    if rval != 0:
        sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
        return 0
    return 1

##########################################################################
# Export a VM
# 
def ExportDisk (conn, vsms, in_params, wait_immediately=True):
    job_ref = None
    try:
        [rval, out_params] = conn.InvokeMethod('ExportDisk', vsms, **in_params)
        if rval != 4096:
            sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
            return (0, job_ref)
    
        job_ref = out_params['Job']
        if wait_immediately == True:
            job = WaitForJobCompletion(conn, job_ref)
            DeleteJob(conn, job_ref)
            job_ref=None

    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return (0, job_ref)

    return (1, job_ref)

##########################################################################
# Import a disk to an existing VM
# 
def ImportDisk (conn, vsms, in_params, wait_immediately=True):
    job_ref = None
    try:
        [rval, out_params] = conn.InvokeMethod('ImportDisk', vsms, **in_params)
        if rval != 4096:
            sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
            return (0, job_ref)
    
        job_ref = out_params['Job']
        print 'started job..' 
        print job_ref.items()
        if wait_immediately == True:
            job = WaitForJobCompletion(conn, job_ref)
            DeleteJob(conn, job_ref)
            job_ref = None

    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return (0, job_ref)

    return (1, job_ref)

def CreateDiskImage (conn, spms, sr_to_use, size_in_bytes, name):
    print sr_to_use.items()
    [system_id, sep, pool_id] = sr_to_use['InstanceID'].partition('/')
    print 'PoolID - %s' % pool_id
    new_disk_sasd = CIMInstance('Xen_DiskSettingData')
    new_disk_sasd['ElementName'] = name
    new_disk_sasd['ResourceType'] = pywbem.Uint16(19)
    new_disk_sasd['ResourceSubType'] = "Disk" 
    new_disk_sasd['VirtualQuantity'] = pywbem.Uint64(size_in_bytes)
    new_disk_sasd['AllocationUnits'] = "Bytes"
    new_disk_sasd['Bootable'] = False
    new_disk_sasd['Access'] = pywbem.Uint8(3)
    new_disk_sasd['PoolID'] = pool_id
    in_params = {"ResourceSetting": new_disk_sasd}
    [rc, out_params] = conn.InvokeMethod("CreateDiskImage", spms, **in_params)
    new_disk = out_params["ResultingDiskImage"]
    print "Created Disk"
    print new_disk.items()
    return [rc, new_disk]

def DeleteDiskImage (conn, spms, disk):
    in_params = {"DiskImage": disk}
    [rc, out_params] = conn.InvokeMethod("DeleteDiskImage", spms[0], **in_params)
    return rc

def ConnectToDisk (conn, spms, disk_image, protocol, transfer_vm_network_config = None, network_config=None, use_ssl=False, use_management_net=False):
    rc = 0
    connect_handle = None
    disk_uri = None
    ssl_cert = None
    chapuser = None
    chappass = None
    print 'connecting to %s' % str(disk_image)
    [system_id, sep, disk_id] = disk_image['DeviceID'].partition('/')
    network_to_use = None
    if use_management_net == False:
        network_refs = conn.EnumerateInstanceNames("Xen_VirtualSwitch")
        for network_ref in network_refs:
            network = conn.GetInstance(network_ref)
            if network['ElementName'] == 'Pool-wide network associated with eth0':
                network_to_use = network_ref
    print 'Connecting to disk %s' % disk_id
    #if protocol == 'bits':
    #    in_params = {"DiskImage": disk_id, "VirtualSwitch": network_to_use, "Protocol":protocol, "UseSSL": True}
    #else:
    in_params = {"DiskImage": disk_id, "Protocol":protocol}
    if network_to_use != None:
        in_params["VirtualSwitch"] = network_to_use
    if use_ssl == True:
        in_params["UseSSL"] = use_ssl
    if network_config != None:
        in_params["NetworkConfiguration"] = network_config
    [rval, out_params] = conn.InvokeMethod("ConnectToDiskImage", spms, **in_params)
    job = None
    if rval == 4096:
        rc = 1
        job_ref = out_params['Job']
        job = WaitForJobCompletion(conn, job_ref)
        disk_uri = job["TargetURI"]
        connect_handle = job['ConnectionHandle']
        if protocol == 'iscsi':
            chapuser = job['Username']
            chappass = job['Password']
            print 'CHAP creds (user:%s, pass:%s)' % (chapuser, chappass)
        if disk_uri == '':
            rc = 0
        print 'Connected Disk URI: "%s"' % disk_uri
        if use_ssl == True:
            ssl_cert = job["SSLCertificate"]
            print 'SSL cert: "%s"' % ssl_cert
            if ssl_cert == '':
                rc = 0 # found cert, success
        if network_config != None:
            if disk_uri.find(transfer_vm_network_config[0]) == -1:
                rc = 0 # if we cannot find the static ip in the disk uri, then return error
        conn.DeleteInstance(job_ref)
    if rc == 1:
        connect_handle = job['ConnectionHandle']
    return [connect_handle, disk_uri, ssl_cert, chapuser, chappass]

def DisconnectFromDisk (conn, spms, connect_handle):
    print 'Disconnecting from disk connection: %s' % connect_handle
    in_params = {"ConnectionHandle": connect_handle}
    [rc, out_params] = conn.InvokeMethod("DisconnectFromDiskImage", spms, **in_params)
    if rc != 4096:
        print 'DisconnectFromDiskImages returned error %d' % rc
    else:
        job_ref = out_params['Job']
        job = WaitForJobCompletion(conn, job_ref)
        if job['JobState'] != 7:
            rc = 1
        else:
            rc = 0
        conn.DeleteInstance(job_ref)
    return rc

#########################################################################
# Add a VM resource
# 
def AddVMResource (conn, vsms, in_params):
    try:
        [rval, out_params] = conn.InvokeMethod('AddResourceSetting', vsms, **in_params)
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return 0

    if rval != 0:
        sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
        return 0
    return 1

##########################################################################
# Add resources to a VM
# 
def AddResourcesToVM (conn, vsms, in_params):
    try:
        [rval, out_params] = conn.InvokeMethod('AddResourceSettings', vsms, **in_params)
        if rval != 4096:
            sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
            return 0
    
        job_ref = out_params['Job']
        job = WaitForJobCompletion(conn, job_ref)
        DeleteJob(conn, job_ref)

    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return 0

    return 1

##########################################################################
# Delete resources from a VM
# 
def DeleteVMResources (conn, vsms, in_params):
    try:
        [rval, out_params] = conn.InvokeMethod('RemoveResourceSettings', vsms, **in_params)
        if rval != 0:
            sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
            return 0
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return 0

    return 1
##########################################################################
# Add resources to a VM
# 
def ModifyVMResources (conn, vsms, in_params):
    #try:
    [rval, out_params] = conn.InvokeMethod('ModifyResourceSettings', vsms, **in_params)
    if rval != 0:
        sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
        return 0
    #except Exception, e:
    #    sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
    #    return 0

    return 1

##########################################################################
# Start/Stop a VM 
# 
def ChangeVMState (conn, vm, in_params, check_for_state=False, state_to_check_for=0):
    print 'Changing VM state for ' + vm['Name']
    try:
        job = None
        (rval, out_params) = conn.InvokeMethod('RequestStateChange', vm, **in_params)
        if rval == 4096:
            print 'StateChange requested with job %s' % out_params['Job']['InstanceID']
            job = out_params['Job']
        # Make sure the state change has occured
        if check_for_state:
            i=0
            while i<20:
                vm_inst = conn.GetInstance(vm)
                if vm_inst['EnabledState'] == state_to_check_for:
                    break
                time.sleep(3)
                i = i+1

        if job != None:
            conn.DeleteInstance(job)
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (__name__, e))
        return 0
    
    if rval != 0 and rval != 4096:
        sys.stderr.write('%s returned error: %s\n' % (__name__, rval))
        return 0
    return 1

def VerifyVMResources (conn, vm_ref, mem_size_in_mb, proc_count, disk_count, nic_count):
    # Verify total memory is 1.5 G
    memory_list = conn.EnumerateInstanceNames("Xen_MemorySettingData")
    for memory_ref in memory_list:
        instance_id = memory_ref['InstanceID'].encode('utf-8')
        vm_uuid = vm_ref['Name'].encode('utf-8')
        if instance_id.find(vm_uuid) == -1:
            continue
        memory_inst = conn.GetInstance(memory_ref)
        size_in_mb = memory_inst['VirtualQuantity']
        if memory_inst['AllocationUnits'] == u'Bytes':
            size_in_mb = size_in_mb / (1024*1024)
        if str(size_in_mb) != str(mem_size_in_mb): #this is of type pywbem.cim_obj.uint64 and not uint64
            print 'MEMORY for %s IS NOT %s (its %s)' % (vm_ref['Name'], mem_size_in_mb, size_in_mb)
    all_processors = conn.EnumerateInstanceNames("Xen_ProcessorSettingData")
    for proc in all_processors:
        instance_id = proc['InstanceID'].encode('utf-8')
        vm_uuid = vm_ref['Name'].encode('utf-8')
        if instance_id.find(vm_uuid) == -1:
            continue
        processor_inst = conn.GetInstance(proc)
        if str(processor_inst['VirtualQuantity']) != str(proc_count):
            print 'PROCESSOR COUNT FOR %s IS NOT %d (its %d)' % (vm_ref['Name'], proc_count, processor_inst['VirtualQuantity'])
    network_list = GetNetworkPortsForVM(conn, vm_ref)
    if len(network_list) != nic_count:
        print 'NETWORK COUNT FOR %s IS NOT %d (its %d)' % (vm_ref['Name'], nic_count, len(network_list))
    disk_list = GetDisksForVM(conn, vm_ref)
    if len(disk_list) != disk_count:
        print 'DISK COUNT FOR %s IS NOT %d (its %d)' % (vm_ref['Name'], disk_count, len(disk_list))

def CreateVMBasedOnTemplateName(conn, vsms, template_name, vssd):
    # create a test VM based on the  template, that we can boot from
    query_str = 'SELECT * FROM Xen_ComputerSystemTemplate WHERE ElementName LIKE "%' + template_name + '%"'
    target_vm_template = None
    templates = conn.ExecQuery("WQL", query_str, "root/cimv2")
    keys = {'CreationClassName':templates[0].classname, 'InstanceID': templates[0]['InstanceID']}
    target_vm_template = CIMInstanceName(templates[0].classname, keys)
    print target_vm_template.items()
    in_params = {'SystemSettings':vssd, 'ReferenceConfiguration':target_vm_template}
    target_vm = CreateVM(conn, vsms, in_params)
    time.sleep(3)
    return target_vm

##########################################################################
# Get a VM based on UUID
def GetVMRefFromUUID (conn, host_uuid, vm_uuid):
    keys = {'CreationClassName':'Xen_ComputerSystem', 'Name': '%s' % (vm_uuid) }
    vm_ref = CIMInstanceName("Xen_ComputerSystem", keys)
    return vm_ref

def GetVMFromUUID (conn, vm_uuid):
    vm_ref = GetVMRefFromUUID(conn, vm_uuid)
    vm_inst = conn.GetInstance(vm_ref)
    return vm_inst

def GetVSSDsForVM (conn, vm_ref):
    association_class = 'Xen_ComputerSystemSettingsDefineState' # association to traverse via Xen_ComputerSystem
    result_class      = 'Xen_ComputerSystemSettingData'  # result class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    vssds             = conn.AssociatorNames(vm_ref, **in_params)
    return vssds

def GetRASDsFromVSSD (conn, vssd_ref):
    association_class = 'CIM_VirtualSystemSettingDataComponent' # association to traverse via Xen_ComputerSystemSettingData
    result_class      = 'CIM_ResourceAllocationSettingData' # class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    rasds = conn.Associators(vssd_ref, **in_params)
    return rasds

def GetMemoryForVM (conn, vm_ref):
    association_class = 'Xen_ComputerSystemMemory' # association to traverse via Xen_ComputerSystem
    result_class      = 'Xen_Memory' # class we are looking for
    in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
    memory_list = conn.AssociatorNames(vm_ref, **in_params)
    return memory_list

def GetMemoryRASDs (conn, memory_ref):
    association_class = 'Xen_MemorySettingsDefineState' # Memory RASD
    result_class      = 'Xen_MemorySettingData'
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    memory_rasds = conn.AssociatorNames(memory_ref, **in_params)
    return memory_rasds

def GetProcessorsForVM (conn, vm_ref):
    association_class = 'Xen_ComputerSystemProcessor' # association to traverse via Xen_ComputerSystem
    result_class      = 'Xen_Processor' # class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    processor_list = conn.AssociatorNames(vm_ref, **in_params)
    return processor_list

def GetProcessorRASDs (conn, processor_ref):
    association_class = 'Xen_ProcessorSettingsDefineState' # Processor RASD
    result_class      = 'Xen_ProcessorSettingData'
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    processor_rasds = conn.AssociatorNames(processor_ref, **in_params)
    return processor_rasds

def GetNetworkPortsForVM (conn, vm_ref):
    association_class = 'Xen_ComputerSystemNetworkPort' # association to traverse via Xen_ComputerSystem
    result_class      = 'Xen_NetworkPort' # class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    network_list = conn.AssociatorNames(vm_ref, **in_params)
    return network_list

def GetNetworkPortRASDs (conn, np_ref):
    association_class = 'Xen_NetworkPortSettingsDefineState'  # Network RASD
    result_class      = 'Xen_NetworkPortSettingData'
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    np_rasds = conn.AssociatorNames(np_ref, **in_params)
    return np_rasds

def GetDisksForVM (conn, vm_ref) :
    association_class = 'Xen_ComputerSystemDisk' # association to traverse via Xen_ComputerSystem
    result_class      = 'Xen_Disk'  # result class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    disk_list  = conn.AssociatorNames(vm_ref, **in_params)
    return disk_list

def GetDiskDrivesForVM (conn, vm_ref) :
    association_class = 'Xen_ComputerSystemDiskDrive' # association to traverse via Xen_ComputerSystem
    result_class      = 'Xen_DiskDrive'  # result class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    disk_list  = conn.AssociatorNames(vm_ref, **in_params)
    return disk_list

def GetDiskRASDs (conn, disk_ref):
    association_class = 'Xen_DiskSettingsDefineState' # Disk RASD
    result_class      = 'Xen_DiskSettingData'  # result class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    disk_rasds  = conn.Associators(disk_ref, **in_params)
    return disk_rasds

def GetConsoleForVM (conn, vm_ref) :
    association_class = 'Xen_ComputerSystemConsole' # association to traverse via Xen_ComputerSystem
    result_class      = 'Xen_Console'  # result class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    con_list  = conn.AssociatorNames(vm_ref, **in_params)
    return con_list

def GetConsoleRASDs (conn, con_ref):
    association_class = 'Xen_ConsoleSettingsDefineState' # Disk RASD
    result_class      = 'Xen_ConsoleSettingData'  # result class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    con_rasds  = conn.Associators(con_ref, **in_params)
    return con_rasds

def GetVMCapabilities (conn, vm_ref):
    association_class = 'Xen_ComputerSystemElementCapabilities' # association to traverse via Xen_ComputerSystem
    result_class      = 'Xen_ComputerSystemCapabilities' # class we are looking for
    in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
    vm_cap_list = conn.AssociatorNames(vm_ref, **in_params)
    return vm_cap_list

def GetDefaultPool (conn, host_ref, res_type):
    # use case 9.2.4 from storage profile
    print 'getting default pool for host %s' % str(host_ref)

    def_host_alloc_cap = None
    result_class = 'CIM_ElementCapabilities' # association class 
    in_params    = {'ResultClass': result_class}
    el_caps = conn.References(host_ref, **in_params)
    for el_cap in el_caps:
        if el_cap['Characteristics'][0] == 2: # check for default characteristic
            host_alloc_cap = conn.GetInstance(el_cap['Capabilities'])
            if host_alloc_cap['ResourceType'] == res_type: # matches the right resource type too
                def_host_alloc_cap = el_cap['Capabilities']
                break

    # we have a default allocation capabilities instance that refers to the host. 
    # Get the one that refers to the resource pool
    # use case 9.2.5 from storage profile
    print def_host_alloc_cap.items()
    in_params = {'ResultClass': result_class}
    el_caps = conn.References(def_host_alloc_cap, **in_params)
    for el_cap in el_caps:
        print el_cap.items()
        if el_cap['Characteristics'] == '2': # check for default characteristic
            print el_cap['ManagedElement']
            if el_cap['ManagedElement'] != host_ref: 
                # this capability does not refer to the one for the host.. it must
                # refer to the one for the pool
                return el_cap['ManagedElement']

    return None
        
