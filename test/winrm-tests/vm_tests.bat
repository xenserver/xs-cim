@echo off
setlocal

REM winrm doesnt support spaces in key names. hence the fix in winrm-1 which replaces 0x20 with a ' ' before calling winrm com object.

REM set server="http://<ip_add>:8889"
REM set xenhostname="<hostname>"
REM set user="<user>"
REM set pass="<pass>"

echo "Get the instance of the virtual system management service class"
call winrm get http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | grep -i " Name"

REM Create a new VM
echo "Create a VM"
call winrm invoke DefineSystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\vm_tests\define_system_args.xml | grep -i "CreationClassName"  | cut -d"=" -f 4 | cut -d" " -f 2 > vmuuid.txt
FOR /F %%K IN (vmuuid.txt) DO set NEW_VM_UUID=%%K
if "%NEW_VM_UUID%" == "" goto ERROR

REM add a disk to a vm
echo "Adding a disk to VM %NEW_VM_UUID%, creating the disk on the default SR...."
call batchSubstitute.bat VM_UUID %NEW_VM_UUID% .\vm_tests\add_disk_settings_args.xml > add_disk.txt
call winrm invoke AddResourceSetting http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:add_disk.txt | findstr -i "InstanceID" | cut -d"=" -f 3 | cut -d" " -f 2 > vbduuid.txt
FOR /F "tokens=2 delims=/" %%K IN (vbduuid.txt) DO set NEW_VBD_UUID=%%K
IF "%NEW_VBD_UUID%"=="" GOTO DISK_ERROR
echo "Created VBD %NEW_VBD_UUID%"

REM find the default sr of the pool                                                                                                                                          
echo "Finding default SR in the pool"
call winrm enum  http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_HostPool -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | find "DefaultStoragePoolID" | cut -d"=" -f 2 | cut -d" " -f 2 > def_sr.txt
FOR /F %%K IN (def_sr.txt) DO set DEFSR_UUID=%%K
IF "%DEFSR_UUID%"=="" GOTO ERROR

REM add an existing VDI to the VM
set DISK_NAME=winrm_vm_test_created_disk
echo "Creating disk image create_delete_test_vdi123 on SR: %DEFSR_UUID%"
call batchSubstitute.bat SR_UUID %DEFSR_UUID% .\disk_tests\create_disk_image.xml > create_new_disk1.txt
call batchSubstitute.bat DISK_NAME %DISK_NAME% .\create_new_disk1.txt > create_new_disk.txt
call winrm invoke CreateDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:create_new_disk.txt | findstr -i "ReturnValue = 0"

echo "Finding new disk....."
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskImage -filter:"SELECT DeviceID FROM Xen_DiskImage WHERE ElementName = \"%DISK_NAME%\""  -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | findstr -i "DeviceID" | cut -d"=" -f 2 | cut -d" " -f 2 > newdiskid.txt
FOR /F %%K IN (newdiskid.txt) DO set DISK_ID=%%K
IF '%DISK_ID%'=='' GOTO DISK_ERROR
call batchSubstitute.bat DISK_ID %DISK_ID% .\vm_tests\add_existing_disk_settings_args.xml > add_existing_disk.txt
call batchSubstitute.bat VM_UUID %NEW_VM_UUID% add_existing_disk.txt > add_disk.txt

echo "Attaching the newly created disk to the VM"
call winrm invoke AddResourceSetting http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:add_disk.txt | findstr -i "InstanceID" | cut -d"=" -f 3 | cut -d" " -f 2 > adddiskid.txt
FOR /F %%K IN (adddiskid.txt) DO set NEW_VBD2_UUID=%%K
IF "%NEW_VBD2_UUID%"=="" GOTO DISK_ERROR
echo "Created 2nd VBD " %NEW_VBD2_UUID%

echo "modifying virtual processors to 2 and memory to 128MB"
call batchSubstitute.bat VM_UUID %NEW_VM_UUID% .\vm_tests\modify_resource_settings_args.xml > modify_resources.txt
call winrm invoke ModifyResourceSettings http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:modify_resources.txt | grep -i "ReturnValue = 0"

echo "adding 2 more virtual processors and 512 MB"
call batchSubstitute.bat VM_UUID %NEW_VM_UUID% .\vm_tests\add_resource_settings_args.xml > add_resources.txt
call winrm invoke AddResourceSettings http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:add_resources.txt | grep -i "ReturnValue = 0"t

echo "Find network pool to connect to"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_NetworkConnectionPool -filter:"SELECT PoolID FROM Xen_NetworkConnectionPool WHERE Description = \"Device=eth0,Bridge=xenbr0\"" -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | findstr -i "PoolID" | cut -d"=" -f 2 | cut -d" " -f 2 > networkinfo.txt
FOR /F %%K IN (networkinfo.txt) DO set NETWORK_UUID=%%K

echo "Adding NIC to Network %NETWORK_UUID%"
call batchSubstitute.bat VM_UUID %NEW_VM_UUID% .\vm_tests\add_network_settings_args.xml > tmp_add.txt
call batchSubstitute.bat NETWORK_UUID %NETWORK_UUID% tmp_add.txt > add_nic.txt
call winrm invoke AddResourceSetting http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:add_nic.txt > addnic_out.txt
FOR /F "tokens=6 delims=/,: " %%K IN (addnic_out.txt) DO set NEW_NIC_UUID=%%K
echo "New NIC UUID="%NEW_NIC_UUID%
IF '%NEW_NIC_UUID%'=='' goto NIC_ADD_ERR

REM Remove a bunch of resources from VM
REM echo "removing 1 virtual processor and 256MB"
REM call batchSubstitute.bat VM_UUID %NEW_VM_UUID% .\vm_tests\remove_resource_settings_args.xml > remove_resources.txt
REM call winrm invoke RemoveResourceSettings http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:remove_resources.txt | grep -i "ReturnValue = 0"


REM check if memory was updated to 128
REM call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ProcessorSettingData -filter:"SELECT VirtualQuantity FROM Xen_ProcessorSettingData WHERE InstanceID = \"Xen:%NEW_VM_UUID%/Processor\"" -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | findstr -i "PoolID" | cut -d"=" -f 2 | cut -d" " -f 2 > procs.txt
REM FOR /F %%K IN (procs.txt) DO set CNT=%%K
REM echo "Checking if processor count (%CNT%) is 3"
REM IF NOT '%CNT%'=='3' GOTO PROC_COUNT_ERR

REM call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_MemorySettingData -filter:"SELECT VirtualQuantity FROM Xen_MemorySettingData WHERE InstanceID = \"Xen:%NEW_VM_UUID%/Memory\"" -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | findstr -i "PoolID" | cut -d"=" -f 2 | cut -d" " -f 2 > memory.txt
REM FOR /F %%K IN (memory.txt) DO set CNT=%%K
REM echo "Checking if memory (%CNT%) is 384MB"
REM IF NOT '%CNT%'=='384' GOTO MEM_COUNT_ERR

echo "Destroying VM %NEW_VM_UUID%"
call batchSubstitute.bat VM_UUID %NEW_VM_UUID% .\vm_tests\destroy_system_args.xml > destroy.txt
call winrm invoke DestroySystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:destroy.txt | grep -i "ReturnValue = 0"


REM cleanup         
del *.txt

echo "SUCCESS"
goto end

REM Error handling
:PROC_COUNT_ERR 
  echo "ERROR: Updated Processor Count did not Match"
goto end

:MEM_COUNT_ERR
  echo "ERROR: Updated Memory Count did not Match"
goto end

:NIC_ADD_ERR
  echo "ERROR: Couldnt NIC"
goto end

:DISK_ERROR
  echo "ERROR: Couldnt Add Disk"
goto end

:ERROR
   echo "ERROR occured"
goto end

:end
