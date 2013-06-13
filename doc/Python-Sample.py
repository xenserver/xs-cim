# import all the relevant python modules we will be using
import pywbem   # this needs to have been installed separately
import sys
import time

#
# Method to get the singleton Virtual System Management Service class 
# that allows one to operate on VMs (create/delete/modify)
#
def get_virtual_system_management_service (cim_conn):
    # get CIM 'reference' to the singleton service class
    vsms = None
    vsms_references = cim_conn.EnumerateInstanceNames("Xen_VirtualSystemManagementService")
    for vsms_ref in vsms_references:
        # get the CIM instance
        vsms_instance = cim_conn.GetInstance(vsms_ref)
        # print all the properties and values for the CIM class
        vsms = vsms_ref
        print '*** Found Virtual System Management Service: "' + vsms_instance['Name'] + '"'
        #print vsms_instance.items()
    return vsms

#
# Helper method to wait for a job to complete
#
def wait_for_job_to_complete (conn, job_ref):
    job = conn.GetInstance(job_ref);
    while (job['JobState'] == 4) or (job['JobState'] == 3) or (job['JobState'] == 5):
        print '    Waiting for job... state is now %d' % job['JobState']
        time.sleep(3)
        job = conn.GetInstance(job_ref);

    if job['JobState'] != 7:
        print '    Job failed: job state:%d' % job['JobState']
        print '          : Job error code :%d' % job['ErrorCode']
        print '          : description :' + job['ErrorDescription']
    else:
        print '    Job completed successfully'
    return job

#
# Method to create a virtual system
#
# the VM settings are provided in the 'SystemSettings' parameter
# the VM resource settings are provided in the 'ResourceSettings' parameter
# Any XenServer template information is provided 'ReferenceConfiguration' parameter
#
def create_vm_helper (conn, vsms, in_params):
    new_vm_ref = None
    job_ref = None
    try:
        (rval, out_params) = conn.InvokeMethod('DefineSystem', vsms, **in_params)
        # DefineSystem could return a job in which case return value 4096 is returned
        if((rval != 4096) and (rval != 0)):
            sys.stderr.write('%s returned error: %s\n' % (sys._getframe(0).f_code.co_name, rval))
            return None
        new_vm_ref = out_params['ResultingSystem']
        if rval == 4096:
            job_ref = out_params['Job']
            job = wait_for_job_to_complete(conn, job_ref) # poll until job state changes from 7
            if job['JobState'] != 7:
                new_vm = None;
            conn.DeleteInstance(job_ref)
    except pywbem.CIMError, arg:
        print 'Caught exception when calling %s' % (sys._getframe(0).f_code.co_name)
        if arg[0] != pywbem.CIM_ERR_NOT_SUPPORTED:
            print 'InvokeMethod(instancename): %s' % arg[1]
            return None

    return new_vm_ref

#
# Helper method to create the VMs we want
#
def create_new_PV_and_HVM_vms (conn, vsms):
    #
    # the metadata for a XenServer Paravirtualized VM we are going to create
    pv_virtualsystemsettingdata = pywbem.CIMInstance ("Xen_ComputerSystemSettingData")
    pv_virtualsystemsettingdata['Caption']              = "This is a PV VM"
    pv_virtualsystemsettingdata['ElementName']          = "test-pv-vm"
    pv_virtualsystemsettingdata['VirtualSystemType']    = "DMTF:xen:PV"     
    pv_virtualsystemsettingdata['AutomaticShutdownAction'] = pywbem.Uint8(0)
    pv_virtualsystemsettingdata['AutomaticStarupAction']   = pywbem.Uint8(1)
    pv_virtualsystemsettingdata['AutomaticRecoveryAction'] = pywbem.Uint8(2)
    # the following are XenServer specific CIM extensions
    pv_virtualsystemsettingdata['PV_Bootloader']        = "pygrub"      # use pygrub as the bootloader
    pv_virtualsystemsettingdata['PV_Bootloader_Args']   = ""
    pv_virtualsystemsettingdata['PV_Args']              = "Term=xterm"

    # We shall also base the PV VM on an existing XenServer template
    # This automatically allocates default proc/mem/disk/network resources specified by the template
    pv_template_list = conn.ExecQuery("WQL", "SELECT * FROM Xen_ComputerSystemTemplate WHERE ElementName LIKE \"%Debian Lenny%\"", "root/cimv2")
    reference_configuration = pywbem.CIMInstanceName(classname=pv_template_list[0].classname, keybindings={"InstanceID":pv_template_list[0]["InstanceID"]})

    # The metadata settings for a XenServer HVM VM (Hardware Virtualized) we will create
    hvm_virtualsystemsettingdata = pywbem.CIMInstance("Xen_ComputerSystemSettingData")
    hvm_virtualsystemsettingdata['Caption']             = 'This is an HVM VM'
    hvm_virtualsystemsettingdata['ElementName']         = 'test-hvm-vm'
    hvm_virtualsystemsettingdata['VirtualSystemType']   = 'DMTF:xen:HVM'
    hvm_virtualsystemsettingdata['AutomaticShutdownAction'] = pywbem.Uint8(0)
    hvm_virtualsystemsettingdata['AutomaticStarupAction']   = pywbem.Uint8(1)
    hvm_virtualsystemsettingdata['AutomaticRecoveryAction'] = pywbem.Uint8(2)
    # the following are XenServer specific CIM extensions
    hvm_virtualsystemsettingdata['HVM_Boot_Params']     = ['order=dc']      # boot order is cd drive and then hard drive
    hvm_virtualsystemsettingdata['HVM_Boot_Policy']     = 'BIOS order'      # boot based on the BIOS boot order specified above
    hvm_virtualsystemsettingdata['Platform']            = ['acpi=true','apic=true','pae=true'] # use ACPI, APIC, PAE emulation

    #
    # define all the resource settings (processor, memory, disk, network)
    # via ResourceAllocationSettingData instances
    #
    # processor
    proc_resource = pywbem.CIMInstance('CIM_ResourceAllocationSettingData')
    proc_resource['ResourceType']     = pywbem.Uint16(3)
    proc_resource['VirtualQuantity']  = pywbem.Uint64(1)
    proc_resource['AllocationUnits']  = "count"
    proc_resource['Limit']            = pywbem.Uint32(100)
    proc_resource['Weight']           = pywbem.Uint32(512)

    # memory
    mem_resource = pywbem.CIMInstance('CIM_ResourceAllocationSettingData')
    mem_resource['ResourceType']    = pywbem.Uint16(4)
    mem_resource['VirtualQuantity'] = pywbem.Uint64(512)
    mem_resource['AllocationUnits'] = 'byte*2^20'           # DMTF way of specifying MegaBytes

    # disks  
    sr_to_use = None
    # find all SRs available to us
    srs = conn.EnumerateInstances("Xen_StoragePool") 
    for sr in srs:
        if sr['Name'] == 'Local storage':
            sr_to_use = sr
    disk_resource = pywbem.CIMInstance('Xen_DiskSettingData')
    disk_resource['Elementname']       = 'my_vm_disk'
    disk_resource['ResourceType']      = pywbem.Uint16(19)
    disk_resource['ResourceSubType']   = "Disk"  # as opposed to "CD"
    disk_resource['VirtualQuantity']   = pywbem.Uint64(2048)
    disk_resource['AllocationUnits']   = "byte*2^20"        # DMTF way of specifying MegaBytes
    disk_resource['Access']            = pywbem.Uint8(3)
    disk_resource['Bootable']          = False
    disk_resource['PoolID']            = sr_to_use['PoolID']# XenServer SR to allocate the disk out of

    # nic 
    network_to_use = None
    # find all network switch connection pools available to us
    networks = conn.EnumerateInstances("Xen_NetworkConnectionPool") 
    for network in networks:
        if network['Name'].find('eth0') != -1:
            network_to_use = network

    # only RASDs of type NetworkConnection (33) are supported
    nic_resource = pywbem.CIMInstance('Xen_NetworkPortSettingData')
    nic_resource['ResourceType']   = pywbem.Uint16(33)
    nic_resource['ElementName']    = "0"
    nic_resource['PoolID']         = network_to_use['PoolID']# Virtual Switch to connect to

    rasds = [proc_resource, mem_resource, 
             disk_resource, nic_resource]

    hvm_params = {'SystemSettings': hvm_virtualsystemsettingdata, 
                  'ResourceSettings': rasds }

    pv_params =  {'SystemSettings': pv_virtualsystemsettingdata, 
                  'ReferenceConfiguration': reference_configuration}

    print '*** Creating PV VM %s' % (pv_virtualsystemsettingdata['ElementName'])
    new_pv_vm_reference     = create_vm_helper(conn, vsms, pv_params)
    print '*** Creating HVM VM %s' % (hvm_virtualsystemsettingdata['ElementName'])
    new_hvm_vm_reference    = create_vm_helper(conn, vsms, hvm_params)
    if new_pv_vm_reference == None:
        print 'PV VM was not created'
        sys.exit(1)
    if new_hvm_vm_reference == None:
        print 'HVM VM was not created'
        sys.exit(1)
    return [new_pv_vm_reference, new_hvm_vm_reference]

# 
# Method to delete a VM 
#
def delete_vm (conn, vsms, vm_ref):
    print '*** Deleting VM %s' % vm_ref['Name']
    in_params = {'AffectedSystem': vm_ref}
    try:
        (rval, out_params) = conn.InvokeMethod('DestroySystem', vsms, **in_params)
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (sys._getframe(0).f_code.co_name, e)) 
        return 0

    if rval != 0:
        sys.stderr.write('%s returned error: %s\n' % (sys._getframe(0).f_code.co_name, rval))
        return 0
    return 1

#
# Method to add more resources (proc, memory, disk, nic) to a VM
#
def add_vm_resources (conn, vsms, vm_ref):
    try:
        print '*** Adding processor and CD resources to VM %s ' % vm_ref['Name']
        # AddResourceSettings requires the Xen_ComputerSystemSettingData reference, find it
        # We'll use the association class for that
        association_class = 'CIM_SettingsDefineState'      # association to traverse via Xen_ComputerSystem
        result_class      = 'CIM_VirtualSystemSettingData' # result class we are looking for
        in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
        affected_config_arr  = conn.AssociatorNames(vm_ref, **in_params)
        affected_config = affected_config_arr[0]

        cd_drive = pywbem.CIMInstance('Xen_DiskSettingData')
        cd_drive['Elementname']       = 'my_cd_drive'
        cd_drive['ResourceType']      = pywbem.Uint16(15)
        cd_drive['VirtualQuantity']   = pywbem.Uint64(1)
        cd_drive['AllocationUnits']   = "count"
        cd_drive['Access']            = pywbem.Uint8(1)
        cd_drive['Bootable']          = True

        more_mem = pywbem.CIMInstance('CIM_ResourceAllocationSettingData')
        more_mem['ResourceType']    = pywbem.Uint16(4)
        more_mem['VirtualQuantity'] = pywbem.Uint64(512)
        more_mem['AllocationUnits'] = 'byte*2^20'

        rasds = [cd_drive, more_mem]
        in_params = {'ResourceSettings': rasds, 'AffectedConfiguration':affected_config}
        [rval, out_params] = conn.InvokeMethod('AddResourceSettings', vsms, **in_params)
        if rval != 4096:
            sys.stderr.write('%s returned error: %s\n' % (sys._getframe(0).f_code.co_name, rval))
            return 0
        job_ref = out_params['Job']
        job = wait_for_job_to_complete(conn, job_ref)
        conn.DeleteInstance(job_ref)
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (sys._getframe(0).f_code.co_name, e))
        return 0
    return 1

#
# Method to start or stop a VM
#
def change_vm_state (conn, vsms, vm_ref, state):
    in_params = {'RequestedState': state} # Start the VM
    print '*** Changing VM state for' + vm_ref['Name']
    try:
        (rval, out_params) = conn.InvokeMethod('RequestStateChange', vm_ref, **in_params)
    except Exception, e:
        sys.stderr.write('Exception caught in %s: %s\n' % (sys._getframe(0).f_code.co_name, e))
        return 0
    if rval != 4096:
        sys.stderr.write('%s returned error: %s\n' % (sys._getframe(0).f_code.co_name, rval))
        return 0
    else:
        job_ref = out_params['Job']
        wait_for_job_to_complete(conn, job_ref)
        conn.DeleteInstance(job_ref)
    return 1

#
# Method to locate all devices assocated with a VM
# This method makes use of association classes
#
def find_vm_devices (conn, vm_ref):
    # The following uses the DMTF CIM base classes for the association and result classes. 
    # One can also use the XenServer derived classes
    association_class = 'CIM_SystemDevice'      # association to traverse via Xen_ComputerSystem
    result_class      = 'CIM_LogicalDevice'     # result class we are looking for
    in_params         = {'ResultClass': result_class, 
                         'AssocClass': association_class }
    system_device_refs  = conn.AssociatorNames(vm_ref, **in_params)
    print '*** Finding devices associated with VM ' + vm_ref['Name']
    for device_ref in system_device_refs:
        system_device = conn.GetInstance(device_ref)
        print '    Found device: %s (%s)' % (system_device.classname, system_device['DeviceID'])
        #print system_device.items()

#################################################################################
# Main script
##################################################################################
# make a CIM Connection the server
cim_conn = pywbem.WBEMConnection("http://192.168.1.100:5988", ("root", "mypass")) 
vsms = get_virtual_system_management_service(cim_conn)      # find the virtual system mgmt service
[pv_vm, hvm_vm] = create_new_PV_and_HVM_vms(cim_conn, vsms) # create the test VMs
add_vm_resources(cim_conn, vsms, hvm_vm)                    # add a processor and a CD drive
find_vm_devices(cim_conn, pv_vm)                            # find all the devices associated with the PV VM
change_vm_state(cim_conn, vsms, hvm_vm, '2')                # start the VM
change_vm_state(cim_conn, vsms, hvm_vm, '32768')            # hard shutdown of the VM
delete_vm (cim_conn, vsms, pv_vm)                           # delete the PV VM
delete_vm (cim_conn, vsms, hvm_vm)                          # delete the HVM VM



