@echo off
setlocal

REM winrm doesnt support spaces in key names. hence the fix in winrm-1 which replaces 0x20 with a ' ' before calling winrm com object.

REM set server="http://<ip_addr>:8889"
REM set xenhostname="<hostname>"
REM set user="<user>"
REM set pass="<pass>"

echo "Get VirtualSwitchManagementService object"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% | grep -i " Name ="

echo "Create internal network...."
call winrm invoke DefineSystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService?CreationClassName=Xen_VirtualSwitchManagementService+Name=Xen0x20Virtual0x20Switch0x20Management0x20Service+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\network_tests\create_internal_network.xml | grep -i " Name = " | cut -d"=" -f 4 | cut -d" " -f 2 > internal_network_id.txt
FOR /F %%K IN (internal_network_id.txt) DO set INTERNAL_NETWORK_UUID=%%K
IF '%INTERNAL_NETWORK_UUID%'=='' GOTO ERROR

echo "Create external network...."
call batchSubstitute.bat MYVLANID 2 .\network_tests\create_external_network.xml > createexternal.txt
call winrm invoke DefineSystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService?CreationClassName=Xen_VirtualSwitchManagementService+Name=Xen0x20Virtual0x20Switch0x20Management0x20Service+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\createexternal.txt | grep -i " Name = " | cut -d"=" -f 4 | cut -d" " -f 2 > external_network_id.txt
FOR /F %%K IN (external_network_id.txt) DO set EXTERNAL_NETWORK_UUID1=%%K
IF '%EXTERNAL_NETWORK_UUID1%'=='' GOTO ERROR

echo "Create a second external network on the same interface with a differnt VLANID ...."
call batchSubstitute.bat MYVLANID 3 .\network_tests\create_external_network.xml > createexternal2.txt
call winrm invoke DefineSystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService?CreationClassName=Xen_VirtualSwitchManagementService+Name=Xen0x20Virtual0x20Switch0x20Management0x20Service+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\createexternal2.txt | grep -i " Name = " | cut -d"=" -f 4 | cut -d" " -f 2 > external_network_id2.txt
FOR /F %%K IN (external_network_id2.txt) DO set EXTERNAL_NETWORK_UUID2=%%K
IF '%EXTERNAL_NETWORK_UUID2%'=='' GOTO ERROR

echo "Convert internal network to an external network"
call batchSubstitute.bat NETWORK_UUID %INTERNAL_NETWORK_UUID% .\network_tests\convert_internal_to_external_network.xml > convert.txt
call winrm invoke AddResourceSettings http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService?CreationClassName=Xen_VirtualSwitchManagementService+Name=Xen0x20Virtual0x20Switch0x20Management0x20Service+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\convert.txt | grep -i "ReturnValue = 0"

echo "Destroying internal network....%INTERNAL_NETWORK_UUID%"
call batchSubstitute.bat NETWORK_UUID %INTERNAL_NETWORK_UUID% .\network_tests\delete_network.xml > delete_internal.txt
call winrm invoke DestroySystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService?CreationClassName=Xen_VirtualSwitchManagementService+Name=Xen0x20Virtual0x20Switch0x20Management0x20Service+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:delete_internal.txt | grep -i "ReturnValue = 0"

echo "Destroying external network....%EXTERNAL_NETWORK_UUID1%"
call batchSubstitute.bat NETWORK_UUID %EXTERNAL_NETWORK_UUID1% .\network_tests\delete_network.xml > delete_external.txt
call winrm invoke DestroySystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService?CreationClassName=Xen_VirtualSwitchManagementService+Name=Xen0x20Virtual0x20Switch0x20Management0x20Service+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:delete_external.txt | grep -i "ReturnValue = 0"

echo "Destroying external network....%EXTERNAL_NETWORK_UUID2%"
call batchSubstitute.bat NETWORK_UUID %EXTERNAL_NETWORK_UUID2% .\network_tests\delete_network.xml > delete_external.txt
call winrm invoke DestroySystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService?CreationClassName=Xen_VirtualSwitchManagementService+Name=Xen0x20Virtual0x20Switch0x20Management0x20Service+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:delete_external.txt | grep -i "ReturnValue = 0"

del *.txt

echo "SUCCESS"
goto END

:ERROR
echo "ERROR OCCURRED!!!"

:END

