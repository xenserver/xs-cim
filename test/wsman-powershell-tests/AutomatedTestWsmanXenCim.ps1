# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the XenServer CIM interface.
# This PowerShell script is executed against the XenServer CIM WS-Management interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# This is an automated scipt that is written to simulate the 
# automated test core PowerShell script that is included
# with the XenServer PowerShell cmdlets.
#
# The Automated Test Core performs the following actions:
# Create a connection to a XenServer, create an NFS SR,
# provision a new VM from the included Debian Etch template to the new SR, 
# start the vm, shutdown the vm, delete the vm,
# destroy the NFS SR, disconnect from the XenServer.
#
# All required helper methods are contained within this script.

### Get an end point reference for a method ######
function EndPointReference {
	param ($cimClass)
	# This is the core to perform enumerations without sending a filter
	# All that is required to pass in is the CIM Class and an array is returned.
	
	# Set the return flag to an End Point Reference
	$enumFlags = $objWsmanAuto.EnumerationFlagReturnEPR()
	 
	# Form the URI String
	$cimResource = "http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/" + $cimClass
	
	# Perform the enumeration against the given CIM class - the two $NULLs are necessary
	$xenEnum = $objSession.Enumerate($cimResource, $NULL, $NULL, $enumFlags)

	$xenEnum = $xenEnum.ReadItem()
	
	# Return the reference	
	return $xenEnum
}
### End ##########################################

### Add NFS SR ###################################
function AddNfsSr {
	param ($newSrName, $server, $serverPath)
	
	$actionUri = EndPointReference "Xen_StoragePoolManagementService"

	$parameters = @"
	<CreateStoragePool_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService"
	xmlns:rasd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData">
		<ElementName>$newSrName</ElementName>
		<ResourceType>19</ResourceType>
		<Settings>
        	<rasd:CIM_ResourceAllocationSettingData xsi:type="CIM_ResourceAllocationSettingData_Type">
              <rasd:Connection>server=$server</rasd:Connection>
              <rasd:Connection>serverpath=$serverPath</rasd:Connection>
			  <rasd:ResourceSubType>nfs</rasd:ResourceSubType>
			</rasd:CIM_ResourceAllocationSettingData>
		</Settings>
	</CreateStoragePool_INPUT> 

"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("CreateStoragePool", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################

### Change VM State ##############################
function ChangeVMState {
param ($vmuuid, $state)

# $state can be:
# 2 ("Enabled") - Power On
# 3 ("Disabled")
# 4 ("Shut Down") - ACPI shutdown, requires Tools
# 6 ("Offline")
# 7 ("Test")
# 8 ("Defer")
# 9 ("Quiesce")
# 10 ("Reboot") - ACPI reboot, requires Tools
# 11 ("Reset") - equal to pressing reset button
# 32768 ("Hard Shutdown") - Equal to setting power switch to off
# 32769 ("Hard Reboot") - power off + power on

$actionUri = "http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem?CreationClassName=Xen_ComputerSystem+Name=$vmuuid"

   if ($vmUuid -ne $null)
    {
		$parameters = @"
		<RequestStateChange_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem">
				<RequestedState>$state</RequestedState>
		</RequestStateChange_INPUT>
"@
            $response = [xml]$objSession.Invoke("RequestStateChange", $actionUri, $parameters)
        }
return $response
}
### End ##########################################

### Destroy a VM #################################
function DestroyVM {
	param ($vmName)
	
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
	<DestroySystem_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
        <AffectedSystem xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
          <wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
          <wsa:ReferenceParameters> 
          <wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</wsman:ResourceURI> 
          <wsman:SelectorSet> 
                <wsman:Selector Name="Name">$vmName</wsman:Selector>
				<wsman:Selector Name="CreationClassName">Xen_ComputerSystem</wsman:Selector> 
          </wsman:SelectorSet> 
          </wsa:ReferenceParameters> 
        </AffectedSystem>
	</DestroySystem_INPUT>
"@
	
	$output = [xml]$objSession.Invoke("DestroySystem", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################

### Remove Any SR ################################
function DeleteSr {
# Detach, Forget - pass the InstanceId
param ($sr)

	if ($sr.GetType().Name -ne "XmlDocument") {
		$sr = [xml]$sr
	}

	$InstanceID = $sr.Xen_StoragePool.InstanceID

	$actionUri = EndPointReference "Xen_StoragePoolManagementService"

    $parameters = @"
	<DeleteResourcePool_INPUT
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService">
	<Pool>
		<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
		<a:ReferenceParameters xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
			<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_StoragePool</w:ResourceURI>
			<w:SelectorSet>
				<w:Selector Name="InstanceID">$InstanceID</w:Selector>
			</w:SelectorSet>
		</a:ReferenceParameters>
	</Pool>
	</DeleteResourcePool_INPUT>
"@

$response = [xml]$objSession.Invoke("DeleteResourcePool", $actionUri, $parameters)

   return $response
}
### End #########################################

### Clean up a job ###############################
function JobCleanUp {
# This does not take input, it simply cleans and reports incomplete jobs

# Get the endpoint Reference for each job
$enumFlags = $objWsmanAuto.EnumerationFlagReturnEPR()
	 
# Perform the enumeration - the two $NULLs are necessary for an EPR
$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_Job", $NULL, $NULL, $enumFlags)

# Read out each returned EPR object as a member of an array
$allJobs = @()
while (!$xenEnum.AtEndOfStream) {
	$elementRec = $xenEnum.ReadItem()
	$allJobs += $elementRec
}

# Loop through the array, see if each job is complete and if it is delete it.
	foreach ($jobEpr in $allJobs) {
		$job = $objsession.Get($jobEpr)
		$jobXml = [xml]$job
		# Check for an error status other than 0 and dump it
		$errorCode = $jobXml.GetElementsByTagName("p:ErrorCode")
		# $errorCode is returned and treated as an Array.
		foreach ($element in $errorCode){
				$errorValue = $element."#Text" 
		}
		if ($errorValue -ne "0") {
			"$job"
		} 
		else {
		$objsession.Delete($jobEpr)
		}
	}

}
### End ##########################################

# the host Xenserver is set with two management interfaces.
# One management interface is on the 192.168.1.0 subnet
# the second management interface is physically dedicated for storage traffic on 192.168.104.0

# The variables
$xenServer = "192.168.1.33"
$userName = "root"
$password = "K33p0ut"
$nfsServer = "192.168.1.29"
$nfsServerPath = "/R"

# Comment the set variables and un-comment the following
# to drive the script by sending arguments.
# $xenServer = $Args[0]
# $userName = $Args[1]
# $password = $Args[2]
# $nfsServer = $Args[3]
# $nfsServerPath = $Args[4]

$dialect = "http://schemas.microsoft.com/wbem/wsman/1/WQL"

$objWsmanAuto = New-Object -ComObject wsman.automation    # Create the WSMAN session object

$connOptions = $objWsmanAuto.CreateConnectionOptions()    # set the connection options of username and password
$connOptions.UserName = $userName
$connOptions.Password = $password

# set the session flags required for XenServer
$iFlags = ($objWsmanAuto.SessionFlagNoEncryption() -bor $objWsmanAuto.SessionFlagUTF8() -bor $objWsmanAuto.SessionFlagUseBasic() -bor $objWsmanAuto.SessionFlagCredUsernamePassword())

$target = "http://" + $xenServer + ":5988"                # The XenServer unsecure connection string
$objSession = $objWsmanAuto.CreateSession($target, $iflags, $connOptions)     # Open the WSMAN session
$objSession.Timeout = 3000000                             # Increase the timeout to 5 minutes for long queries
$objSession.Identify()                                    # WSMAN session identify

# Begin the tasks, now that the session is established.

# Add NFS SR
$addNfsSrResult = AddNfsSr "Test NFS SR" $nfsServer $nfsServerPath 
$nfsSr = [xml]$objSession.Get($addNfsSrResult.CreateStoragePool_OUTPUT.Pool.outerxml)

# Get the InstanceID of the SR from the result
	$newItem = $addNfsSrResult.GetElementsByTagName("wsman:Selector")
	foreach ($element in $newItem){
		if ($element.Name -eq "InstanceID") {
			$srInstanceID = $element."#Text"
		} 
	}
	
	Remove-Variable -Name newItem, addNfsSrResult
	
# Create the new VM from the Demo Linux VM template (this requires the Linux template supplimental pack)

# Find the Template
	$filter = "SELECT * FROM Xen_ComputerSystemTemplate where ElementName like `"%Demo Linux VM%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemTemplate", $filter, $dialect)
	$xenEnumXml = [xml]$xenEnum.ReadItem()
	$refVmInstanceId = $xenEnumXml.Xen_ComputerSystemTemplate.InstanceID

# Create the XML
	$parameters = @"
		<CopySystem_INPUT
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
		xmlns:xsd="http://www.w3.org/2001/XMLSchema"
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService"
		xmlns:cssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSettingData">
		<SystemSettings>
         <cssd:Xen_ComputerSystemSettingData  
             xsi:type="Xen_ComputerSystemSettingData_Type"> 
              <cssd:Description>Created by AutomatedTestWsmanXenCim.ps1</cssd:Description>
              <cssd:ElementName>Automated Test Core VM</cssd:ElementName>
           </cssd:Xen_ComputerSystemSettingData>
		</SystemSettings>
        <ReferenceConfiguration xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
              <wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
              <wsa:ReferenceParameters> 
              <wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemTemplate</wsman:ResourceURI> 
              <wsman:SelectorSet> 
                    <wsman:Selector Name="InstanceID">$refVmInstanceId</wsman:Selector> 
              </wsman:SelectorSet> 
              </wsa:ReferenceParameters> 
        </ReferenceConfiguration>
        <StoragePool xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
              <wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
              <wsa:ReferenceParameters> 
              <wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePool</wsman:ResourceURI> 
              <wsman:SelectorSet> 
                    <wsman:Selector Name="InstanceID">$srInstanceId</wsman:Selector> 
              </wsman:SelectorSet> 
              </wsa:ReferenceParameters> 
		</StoragePool>
	</CopySystem_INPUT>
"@

Remove-Variable -Name xenEnumXml, xenEnum, filter


# Begin the copy / provision operation
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$copyResult = [xml]$objSession.Invoke("CopySystem", $actionURI, $parameters)
"Copy"
	if ($copyResult.copysystem_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($copyResult.copysystem_output.job.outerxml)
			$jobPercentComplete = $jobResult.Xen_VirtualSystemCreateJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}
	}	
	Remove-Variable -Name jobPercentComplete, copyResult
	
# query for the new VM
	$jobVmName = $jobResult.Xen_VirtualSystemCreateJob.ElementName
	$filter = "SELECT * FROM Xen_ComputerSystem where ElementName like `"%$jobVmName%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
	$newVm = [xml]$xenEnum.ReadItem()
	$newVmUuid = $newVm.Xen_ComputerSystem.Name
	Remove-Variable -Name newVm, xenEnum, filter, jobVmName, jobResult

# the VM is copied but not running
# Boot the VM
$changeStateOn = ChangeVmState $newVmUuid "2"   # 2 = enable or PowerOn
"On"
# check that Xen Tools are running in the VM before continuing
$filter = "SELECT * FROM Xen_ComputerSystem where Name like `"%$newVmUuid%`""
$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
$xenEnumXml = [xml]$xenEnum.ReadItem()

while ($xenenumxml.Xen_ComputerSystem.AvailableRequestedStates -notcontains "4") {
	sleep 2
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
	$xenEnumXml = [xml]$xenEnum.ReadItem()
}
Remove-Variable -Name xenEnumXml, xenEnum, filter
	
# Give a breather for the VM OS to settle a bit
sleep 30

# Clean shutdown the Vm
$changeStateOff = ChangeVmState $newVmUuid "4"   # 4 = ACPI shutdown, requires that the vm tools is running
"Off"
	if ($changestateoff.RequestStateChange_OUTPUT.ReturnValue -ne 0) {
	# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($changestateoff.RequestStateChange_OUTPUT.job.outerxml)
			$jobPercentComplete = $jobResult.Xen_VirtualSystemManagementServiceJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}
	}

sleep 3

# Destroy the VM
$destroyResult = DestroyVm $newVmUuid     # This requires a UUID
"Destroy"
	if ($destroyResult.DestroySystem_OUTPUT.ReturnValue -ne 0) {
	# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($destroyResult.DestroySystem_OUTPUT.job.outerxml)
			$jobPercentComplete = $jobResult.Xen_VirtualSystemManagementServiceJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}
	}

# Remove the NFS SR
$forgetSr = DeleteSr $nfsSr
"Forget"
	if ($forgetSr.DeleteResourcePool_OUTPUT.ReturnValue -ne 0) {
	# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($forgetSr.DeleteResourcePool_OUTPUT.job.outerxml)
			$jobPercentComplete = $jobResult.Xen_StoragePoolManagementServiceJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}
	}

JobCleanUp

"End"

