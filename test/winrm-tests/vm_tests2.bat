@echo off
setlocal

REM set server="http://<ip_add>:8889"
REM set xenhostname="<hostname>"
REM set user="<user>"
REM set pass="<pass>"

echo "Find XenServer Transfer VM template"
call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemTemplate -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -filter:"SELECT InstanceID FROM Xen_ComputerSystemTemplate WHERE ElementName LIKE \"%%XenServer Transfer VM%%\"" | findstr -i "InstanceID" | cut -d"=" -f 2 | cut -d" " -f 2 > templateid.txt
FOR /F %%K IN (templateid.txt) DO set TEMPLATE_ID=%%K

echo "Copy VM from template %TEMPLATE_ID%"
call batchSubstitute.bat TEMPLATE_ID %TEMPLATE_ID% .\vm_tests\copy_system_args.xml > copy_system.txt
call winrm invoke CopySystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\copy_system.txt | findstr -i "InstanceID" | cut -d"=" -f 3 | cut -d" " -f 2 > jobid.txt

FOR /F %%K IN (jobid.txt) DO set JOBID=%%K
echo "Pause until the VM is fully created"
pause

call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemCreateJob -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -filter:"SELECT ResultingSystem FROM Xen_VirtualSystemCreateJob WHERE InstanceID = \"%JOBID%\"" | findstr -i "ResultingSystem" | cut -d"=" -f 4 | cut -d" " -f 2 | cut -c 2-37 > vmuuid.txt
FOR /F %%K IN (vmuuid.txt) DO set VMUUID=%%K

echo "Start VM %VMUUID%"
call batchSubstitute.bat REQSTATE 2 .\vm_tests\request_state_change_args.xml > start_vm.txt
call winrm invoke RequestStateChange http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem?CreationClassName=Xen_ComputerSystem+Name=%VMUUID% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\start_vm.txt

echo "Get performance metrics for %VMUUID%"
call batchSubstitute.bat SYSTEMCLASSNAME Xen_ComputerSystem .\vm_tests\perf_metrics_gathering.xml > perf_gather1.txt
call batchSubstitute.bat SYSTEM_UUID %VMUUID% .\perf_gather1.txt > perf_gather.txt
call winrm invoke GetPerformanceMetricsForSystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_MetricService?CreationClassName=Xen_MetricsService+Name=Xen0x20Metrics0x20Service -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:perf_gather.txt

pause 

REM echo "Get performance metrics for %HOSTUUID%"
REM call batchSubstitute.bat SYSTEMCLASSNAME Xen_HostComputerSystem .\vm_tests\perf_metrics_gathering.xml > perf_gather1.txt
REM call batchSubstitute.bat SYSTEMUUID %HOSTUUID% .\vm_tests\perf_metrics_gathering.xml > perf_gather.txt
REM call winrm invoke GetPerformanceMetricsForSystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_MetricService?CreationClassName=Xen_MetricsService+Name=Xen0x20Metrics0x20Service -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:perf_gather.txt

echo "Stop VM"
call batchSubstitute.bat REQSTATE 4 .\vm_tests\request_state_change_args.xml > stop_vm.txt
call winrm invoke RequestStateChange http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem?CreationClassName=Xen_ComputerSystem+Name=%VMUUID% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:.\stop_vm.txt

echo "Destroying VM %VMUUID%"
call batchSubstitute.bat VM_UUID %VMUUID% .\vm_tests\destroy_system_args.xml > destroy.txt
call winrm invoke DestroySystem http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService?CreationClassName=Xen_VirtualSystemManagementService+Name=Xen0x20Hypervisor+SystemCreationClassName=CIM_ComputerSystem+SystemName=%xenhostname% -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -file:destroy.txt | grep -i "ReturnValue = 0"


REM del *.txt
echo "SUCCESS"
