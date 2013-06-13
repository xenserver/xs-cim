@echo off
setlocal

REM winrm doesnt support spaces in key names. hence the fix in winrm-1 which replaces 0x20 with a ' ' before calling winrm com object.

REM set server="http://<ip_addr>:8889"
REM set xenhostname="<hostname>"
REM set user="<user>"
REM set pass="<pass>"
echo "Get StoragePoolManagementService object"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | grep -i "Name"

REM
REM Start with normal VDI disks
REM
echo "Get Storage Pool to allocate disk out of (hopefully there's only 1 NFS SR in the pool)"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePool -filter:"SELECT InstanceID FROM Xen_StoragePool WHERE ResourceSubType = \"nfs\""  -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | grep -i "InstanceID" | cut -d"=" -f 2 | cut -d" " -f 2 > nfssr.txt
FOR /F "tokens=2 delims=/" %%K IN (nfssr.txt) DO set SRUUID=%%K
FOR /F "tokens=1 delims=" %%K IN (nfssr.txt) DO set SRID=%%K

echo "New Disk %DISK_NAME% created from pool %SRID% (%SRUUID%)"
set DISK_NAME=winrm_created_disk123
call batchSubstitute.bat SR_UUID %SRUUID% .\disk_tests\create_disk_image.xml > create_disk_image1.txt
call batchSubstitute.bat DISK_NAME %DISK_NAME% .\create_disk_image1.txt > create_disk_image.txt
call winrm invoke CreateDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:create_disk_image.txt | findstr -i "ReturnValue = 0"

echo "Find newly created disk....."
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskImage -filter:"SELECT * FROM Xen_DiskImage WHERE ElementName = \"%DISK_NAME%\""  -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | grep -i "DeviceID" | cut -d"=" -f 2 | cut -d" " -f 2 > diskid.txt
FOR /F %%K IN (diskid.txt) DO set DISK_ID=%%K
echo "New Disk created %DISK_ID%"
IF '%DISK_ID%'=='' GOTO ERROR

call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitch -filter:"SELECT Name FROM Xen_VirtualSwitch WHERE ElementName = \"Pool-wide network associated with eth0\""  -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | grep -i "Name" | cut -d"=" -f 2 | cut -d" " -f 2 > vswitchid.txt
FOR /F %%K IN (vswitchid.txt) DO set VIRTUALSWITCH_UUID=%%K
echo "Using Virtual Switch %VIRTUALSWITCH_UUID% to connect to it.."
IF '%VIRTUALSWITCH_UUID%'=='' GOTO ERROR

REM make changes to the XML
echo "connecting to disk %DISK_ID%"
call batchSubstitute.bat VDI_ID %DISK_ID% .\disk_tests\connect_disk_image.xml > connect_disk_tmp1.txt
call batchSubstitute.bat SR_UUID %SRUUID% .\connect_disk_tmp1.txt > connect_disk_tmp.txt
call batchSubstitute.bat NETWORK_UUID %VIRTUALSWITCH_UUID% connect_disk_tmp.txt > connect_disk.txt
call winrm invoke ConnectToDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -timeout:60000 -file:connect_disk.txt | findstr -i "InstanceID" | cut -d"=" -f 3 | cut -d " " -f 2 > jobid.txt
echo "PAUSE until transfer vm comes up and then some (until VM has an IP address)"
pause

REM connect returns a job handle, which we will use to query the connection handle
FOR /F %%K IN (jobid.txt) DO set JOBID=%%K
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ConnectToDiskImageJob -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -filter:"SELECT ConnectionHandle FROM Xen_ConnectoToDiskImageJob WHERE InstanceID = \"%JOBID%\"" | findstr -i "ConnectionHandle" | cut -d"=" -f 2 | cut -d" " -f 2 > target.txt
FOR /F %%K IN (target.txt) DO set TGT=%%K
IF "%TGT%"=="" GOTO ERROR

REM transfervm is up, can use BITSadmin or iSCSIInitiator to export/import the disk
echo "TransferVM is up.. use BITSAdmin to import export the following uri 
echo "------------------"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ConnectToDiskImageJob -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -filter:"SELECT TargetURI FROM Xen_ConnectoToDiskImageJob WHERE InstanceID = \"%JOBID%\"" | findstr -i "TargetURI" | cut -d"=" -f 2 | cut -d" " -f 2
echo "------------------"

echo "disconnecting from disk with connect handle %TGT%"
call batchSubstitute.bat CONNECT_HANDLE %TGT% .\disk_tests\disconnect_disk_image.xml > disconnect_disk.txt
call winrm invoke DisconnectFromDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:disconnect_disk.txt | findstr -i "ReturnValue = 0"

echo "Deleteing disk image %NEW_DISK_UUID%"
call batchSubstitute.bat VDI_ID %DISK_ID% .\disk_tests\delete_disk_image.xml > delete_disk.txt
call winrm invoke DeleteDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:delete_disk.txt | findstr -i "ReturnValue = 0"

pause

REM
REM Try with ISO SR and ISO images
REM
echo "Create a CIFS ISO SR"
call winrm invoke CreateStoragePool http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20SR0x20Management0x20Service+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\disk_tests\create_iso_sr.xml

echo "Get Storage Pool to allocate ISO out of (Look for the new CIFS ISO SR)"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePool -filter:"SELECT InstanceID FROM Xen_StoragePool WHERE Name = \"WinrmTestCIFSISOSR\""  -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | findstr -i "InstanceID" | cut -d"=" -f 2 | cut -d" " -f 2 > isosrid.txt
FOR /F %%K IN (isosrid.txt) DO set ISOSRID=%%K
FOR /F "tokens=2 delims=/" %%K IN (isosrid.txt) DO set ISOPOOLID=%%K

echo "Create disk image create_vdi_test123.... from pool %ISOSRID%"
call batchSubstitute.bat SR_UUID %ISOPOOLID% .\disk_tests\create_iso.xml > create_iso.txt
call winrm invoke CreateDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:create_iso.txt

echo "Find newly created disk....."
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskImage -filter:"SELECT DeviceID FROM Xen_DiskImage WHERE ElementName = \"winrm-test-created.iso\""  -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | findstr -i "DeviceID" | cut -d"=" -f 2 | cut -d" " -f 2 > isoid.txt
FOR /F %%K IN (isoid.txt) DO set ISO_ID=%%K
echo "New ISO created %ISO_ID%"
IF '%ISO_ID%'=='' GOTO ERROR

echo "connecting to disk %ISO_ID%"
call batchSubstitute.bat VDI_ID %ISO_ID% .\disk_tests\connect_disk_images.xml > connect_iso_tmp.txt
call batchSubstitute.bat SR_UUID %ISOPOOLID% .\connect_iso_tmp.txt > connect_iso.txt
call winrm invoke ConnectToDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -timeout:60000 -file:connect_iso.txt | findstr -i "InstanceID" | cut -d"=" -f 3 | cut -d " " -f 2  > jobid2.txt
echo "PAUSE until transfer vm comes up and then some (until VM has an IP address)"

FOR /F %%K IN (jobid2.txt) DO set JOB_ID2=%%K
IF "%JOB_ID2%"=="" GOTO ERROR
pause

echo "Find Connect job %JOB_ID2%"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ConnectToDiskImageJob -filter:"SELECT ConnectionHandle FROM Xen_ConnectToDiskImageJob WHERE InstanceID = \"%JOB_ID2%\""  -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | findstr -i "ConnectionHandle" | cut -d"=" -f 2 | cut -d" " -f 2 > target2.txt
FOR /F %%K IN (target2.txt) DO set TGT2=%%K
IF "%TGT2%"=="" GOTO ERROR

REM transfervm is up, can use BITSadmin or iSCSIInitiator to export/import the disk
echo "TransferVM is up.. use BITSAdmin to import export the following uri 
echo "------------------"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ConnectToDiskImageJob -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -filter:"SELECT TargetURI FROM Xen_ConnectoToDiskImageJob WHERE InstanceID = \"%JOB_ID2%\"" | findstr -i "TargetURI" | cut -d"=" -f 2 | cut -d" " -f 2
echo "------------------"

echo "disconnecting from disk with handle %TGT2%"
call batchSubstitute.bat CONNECT_HANDLE %TGT2% .\disk_tests\disconnect_disk_images.xml > disconnect_iso.txt
call winrm invoke DisconnectFromDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:disconnect_iso.txt | findstr -i "ReturnValue = 0"

echo "Deleteing disk image %ISO_ID%"
call batchSubstitute.bat VDI_ID %ISO_ID% .\disk_tests\delete_disk_image.xml > delete_iso.txt
call winrm invoke DeleteDiskImage http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20Storage0x20Repository+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:delete_iso.txt | findstr -i "ReturnValue = 0"

echo "Detaching ISO SR %ISOSRID%"
call batchSubstitute.bat SR_ID %ISOSRID% .\disk_tests\detach_iso_sr.xml > detach_iso_sr.txt
call winrm invoke DeleteResourcePool http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService?CreationClassName=Xen_StoragePoolManagementService+Name=Xen0x20SR0x20Management0x20Service+SystemCreationClassName=CIM_ResourcePoolConfigurationService+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\detach_iso_sr.txt

del *.txt

echo "SUCCESS"
goto END

:ERROR
echo "ERROR OCCURRED!!!"

:END

