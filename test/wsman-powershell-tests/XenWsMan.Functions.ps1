# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the Xen-CIM Project.
# This PowerShell script is executed against the XenServer CIM WSMAN interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# The functions contained within this script are designed to serve as 
# examples of using the WSMAN interface of XenServer (5.6 and above) from 
# PowerShell.  This uses the WSMAN Automation COM object and its resulting
# session to perform all actions.

# The params for the script are the target XenServer, username, password
#$xenServer = "http://" + $Args[0] + ":5988"
#$userName = $Args[1]
#$password = $Args[2]

# Static params for testing and debugging
# $xenServer = "192.168.1.58"
# $userName = "root"
# $password = "K33p0ut"

##################################################
############### Helper Functions #################
##################################################

### Open an HTTP WSMAN session with a XenServer ##

############### WARNING ALL SCRIPTS MUST HAVE THIS ###############
# Due to script behavior each script must include the following. #
# When calling this as a function a session object is not        #
# returned, simply the output of the function is captured.       #
##################################################################

# function WsmanSession {
# param ($xenServer, $userName, $password)
# # ip address or machine name of the target XenServer
# # a user logon name ('root' by default) with permissions to perform all required actions
# # the password of the user logon
# 
# # Create a WSMAN object
# 	$objWsmanAuto = New-Object -ComObject wsman.automation
# 
# 	# set the connection options of username and password
# 	$connOptions = $objWsmanAuto.CreateConnectionOptions()
# 	$connOptions.UserName = $userName
# 	$connOptions.Password = $password
# 
# 	# set the session flags required for the connection to work
# 	$iFlags = ($objWsmanAuto.SessionFlagNoEncryption() -bor $objWsmanAuto.SessionFlagUTF8() -bor $objWsmanAuto.SessionFlagUseBasic() -bor $objWsmanAuto.SessionFlagCredUsernamePassword())
# 
# 	# The target system
# 	$target = "http://" + $xenServer + ":5988"
# 
# 	# Open the session
# 	$objSession = $objWsmanAuto.CreateSession($target, $iflags, $connOptions)
# 
# 	# Increase the timeout to 5 minutes
# 	$objSession.Timeout = 3000000
# 
# 	# Identify the interface
# 	$objSession.Identify()
# 
# 	# Return the session object output - not the session itself.
# 	# return $objSession
# 
# }
### End ##########################################



### Pause ########################################
# Do I need to say more?
function Pause ($Message="Press any key to continue...")
{
Write-Host -NoNewLine $Message

$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
Write-Host ""
}
### End ##########################################



### Enumerate without filter #####################
function EnumClass {
	param ($cimClass)
	# This is the core to perform enumerations without sending a filter
	# All that is required to pass in is the CIM Class and an array is returned.

	# Form the URI String
	$cimUri = "http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/" + $cimClass

	# Perform the enumeration against the given CIM class
	$xenEnum = $objSession.Enumerate($cimUri)

	# This returns an object that contains all elements in $cimUri

	# Declare an empty, generic array with no specific type
	$xenEnumXml = @()

	# Read out each returned element as a member of the array
	while (!$xenEnum.AtEndOfStream) {
		$elementRec = $xenEnum.ReadItem()
		$xenEnumXml += $elementRec
	}

# 	# A simple foreach to iterate through the entire array, to return some item from each element, or each element
#	# By default an array of blobs is returned if the enumeration has more than one
# 	foreach ($element in $xenEnumXml) {
# 		$item = [xml]$element
# 		# Print the output for each item of the array
# 		$item.$cimClass
# 	}

	# Return the array	
	return $xenEnumXML
}
### End ##########################################



### Enumerate with filter ########################
function EnumClassFilter {
param ($cimClass, $filter)
	# This is the core to perform enumerations wit a filter
	# All that is required to pass in is the CIM Class and an array is returned.

	# Form the URI String
	$cimUri = "http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/" + $cimClass

	# Perform the enumeration against the given CIM class
	# the command should resemble this:
	# $objSession.Enumerate($cimUri, "SELECT * FROM Xen_DiskImage where ElementName = 'create_vdi_test123'")

	$xenEnum = $objSession.Enumerate($cimUri, $filter, "http://schemas.microsoft.com/wbem/wsman/1/WQL")

	# This returns an object that contains all elements in $cimUri

	# Declare an empty, generic array with no specific type
	$xenEnumXml = @()

	# Read out each returned element as a member of the array
	while (!$xenEnum.AtEndOfStream) {
		$elementRec = $xenEnum.ReadItem()
		$xenEnumXml += $elementRec
	}

	# Return the array	
	return $xenEnumXml
}
### End ##########################################



### Get an end point reference for a method ######
# this can be used to replace the $actionUri found throughout the scripts
# it would be used to "get" a method.
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



### Create a Virtual Machine #####################
function CreateVm {
	param ($vmName, $vmRam, $vmProc, $vmType)
	# $vmName is the Name of the VM
	# $vmRam is the RAM / memory setting of the VM
	# $vmProc is the number of virtual processors
	# $vmType is the Xen VP or HVM type
	
	# Defaults for Error handling
	if ($vmName -eq $null) {$vmName = "TestVirtualMachine"}
	if ($vmRam -eq $null) {$vmRam = 256}
	if ($vmProc -eq $null) {$vmProc = 1}
	if ($vmType -eq $null) {$vmType = "HVM"}

	# Create the VM
	# Get a reference for the VirtualSystemManagementService
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"
	
	$parameters = @"
	<DefineSystem_INPUT 
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
     xmlns:xsd="http://www.w3.org/2001/XMLSchema"  
	 xmlns:cssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSettingData" 
     xmlns:msd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_MemorySettingData" 
     xmlns="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService"> 
     <SystemSettings>
         <cssd:Xen_ComputerSystemSettingData xsi:type="Xen_ComputerSystemSettingData_Type"> 
              <cssd:HVM_Boot_Policy>BIOS order</cssd:HVM_Boot_Policy>
              <cssd:HVM_Boot_Params>order=dc</cssd:HVM_Boot_Params>
              <cssd:Platform>nx=false</cssd:Platform>
              <cssd:Platform>acpi=true</cssd:Platform>
              <cssd:Platform>apic=true</cssd:Platform>
              <cssd:Platform>pae=true</cssd:Platform>
              <cssd:Platform>viridian=true</cssd:Platform>
              <cssd:AutomaticShutdownAction>0</cssd:AutomaticShutdownAction>
              <cssd:AutomaticStartupAction>1</cssd:AutomaticStartupAction>
              <cssd:AutomaticRecoveryAction>2</cssd:AutomaticRecoveryAction>
              <cssd:VirtualSystemType>DMTF:xen:$vmType</cssd:VirtualSystemType>
              <cssd:Caption>My test VM description goes here</cssd:Caption>
              <cssd:ElementName>$vmName</cssd:ElementName>
         </cssd:Xen_ComputerSystemSettingData>
     </SystemSettings>
     <ResourceSettings>
         <msd:Xen_MemorySettingData> 
             <msd:ResourceType>4</msd:ResourceType>
             <msd:VirtualQuantity>$vmRam</msd:VirtualQuantity>
             <msd:AllocationUnits>MegaBytes</msd:AllocationUnits>
         </msd:Xen_MemorySettingData>
     </ResourceSettings>
	</DefineSystem_INPUT>
"@

    $vmOutput = [xml]$objSession.Invoke("DefineSystem", $actionUri, $parameters)

	if ($vmOutput.DefineSystem_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($vmOutput.DefineSystem_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobResult.Xen_VirtualSystemModifyResourcesJob.PercentComplete
			sleep 1
		}
	}

	# Get the VM object to return to the calling method
	$newVm = [xml]$objSession.Get($vmOutput.DefineSystem_OUTPUT.ResultingSystem.outerxml)

	# The InstanceID of the VM we just created
	$newVmInstanceId = $newVm.Xen_ComputerSystem.InstanceID

	if ($vmProc -gt 1) {
		# Modify the number of processors to match the desired if more than 1 is sent or accept the default of 1
		$parameters = @"
		<ModifyResourceSettings_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    	xmlns:psd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ProcessorSettingData"
		xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
		<ResourceSettings>
			<psd:Xen_ProcessorSettingData > 
				<psd:ResourceType>3</psd:ResourceType>
				<psd:VirtualQuantity>$vmProc</psd:VirtualQuantity>
				<psd:AllocationUnits>true</psd:AllocationUnits>
				<psd:InstanceID>$newVmInstanceId</psd:InstanceID>
			</psd:Xen_ProcessorSettingData>
		</ResourceSettings>
		</ModifyResourceSettings_INPUT>
"@

		$output = [xml]$objSession.Invoke("ModifyResourceSettings", $actionURI, $parameters)
		
		if ($output.ModifyResourceSettings_OUTPUT.ReturnValue -ne 0) {
			# check for a job status of finished
			$jobPercentComplete = 0
			while ($jobPercentComplete -ne 100) {
				$jobResult = [xml]$objSession.Get($output.ModifyResourceSettings_OUTPUT.Job.outerxml)
				$jobPercentComplete = $jobresult.Xen_VirtualSystemModifyResourcesJob.PercentComplete
				sleep 1
			}
		}

	}

	# Add a Virtual CD / DVD ROM device to the VM in the state of <Empty>
	$parameters = @"
		<AddResourceSettings_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
    	xmlns:dsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData"
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
		<ResourceSettings>
			<dsd:Xen_DiskSettingData 
			xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData"
			xsi:type="Xen_DiskSettingData_Type">
				<dsd:ElementName>MyCDRom</dsd:ElementName>
				<dsd:ResourceType>15</dsd:ResourceType>
				<dsd:ResourceSubType>CD</dsd:ResourceSubType>
				<dsd:Bootable>true</dsd:Bootable>
				<dsd:Access>1</dsd:Access>
				<dsd:AddressOnParent>3</dsd:AddressOnParent>
			</dsd:Xen_DiskSettingData>
		</ResourceSettings>
		<AffectedConfiguration> 
			<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
			<a:ReferenceParameters 
			  xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
			  xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
				<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
				<w:SelectorSet>
					<w:Selector Name="InstanceID">$newVmInstanceId</w:Selector>
				</w:SelectorSet>
			</a:ReferenceParameters>
		</AffectedConfiguration>
		</AddResourceSettings_INPUT>
"@
	
	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("AddResourceSettings", $actionURI, $parameters)

	if ($output.AddResourceSettings_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($output.AddResourceSettings_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_VirtualSystemModifyResourcesJob.PercentComplete
			sleep 1
		}
	}

	return $newVm
	
}
### End ##########################################


### Convert a VM to a Template ###################
function VmToTemplate {
	param ($vmName)

	# Create the VM
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
		<ConvertToXenTemplate_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema"
		xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
		<System> 
			<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
			<a:ReferenceParameters 
			  xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
			  xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
				<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
				<w:SelectorSet>
					<w:Selector Name="Name">$vmName</w:Selector>
				</w:SelectorSet>
			</a:ReferenceParameters>
		</System>
		</ConvertToXenTemplate_INPUT>
"@

		$output = [xml]$objSession.Invoke("ConvertToXenTemplate", $actionURI, $parameters)

}
### END ##########################################



### Create a new VM from a virtual machine #######
function CopyVm {
	param ($newVmName, $refVmInstanceId, $xenSrInstanceId)

	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
	<CopySystem_INPUT
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
		xmlns:xsd="http://www.w3.org/2001/XMLSchema"
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService"
		xmlns:cssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSettingData">
		<SystemSettings>
         <cssd:Xen_ComputerSystemSettingData  
             xsi:type="Xen_ComputerSystemSettingData_Type"> 
              <cssd:Description>This is a script created system</cssd:Description>
              <cssd:ElementName>$newVmName</cssd:ElementName>
              <cssd:Other_Config>HideFromXenCenter=false</cssd:Other_Config>
              <cssd:Other_Config>transfervm=false</cssd:Other_Config>
           </cssd:Xen_ComputerSystemSettingData>
		</SystemSettings>
        <ReferenceConfiguration xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
              <wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
              <wsa:ReferenceParameters> 
              <wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSettingData</wsman:ResourceURI> 
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
                    <wsman:Selector Name="InstanceID">$xenSrInstanceId</wsman:Selector> 
              </wsman:SelectorSet> 
              </wsa:ReferenceParameters> 
		</StoragePool>
	</CopySystem_INPUT>
"@

	$output = [xml]$objSession.Invoke("CopySystem", $actionURI, $parameters)

	return $output
	
}
### End ##########################################



### Create a new VM from a template ##############
function CreateVmFromTemplate {
	param ($newVmName, $refVmInstanceId, $xenSrInstanceId)

	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
	<CopySystem_INPUT
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
		xmlns:xsd="http://www.w3.org/2001/XMLSchema"
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService"
		xmlns:cssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSettingData">
		<SystemSettings>
         <cssd:Xen_ComputerSystemSettingData  
             xsi:type="Xen_ComputerSystemSettingData_Type"> 
              <cssd:Description>This is a script created system</cssd:Description>
              <cssd:ElementName>$newVmName</cssd:ElementName>
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
                    <wsman:Selector Name="InstanceID">$xenSrInstanceId</wsman:Selector> 
              </wsman:SelectorSet> 
              </wsa:ReferenceParameters> 
		</StoragePool>
	</CopySystem_INPUT>
"@

	$output = [xml]$objSession.Invoke("CopySystem", $actionURI, $parameters)

# StoragePool is not defined, thererfore the default SR should be used by the method

	return $output
	
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



### Get the VDI UUIDs for a single VM ############
function GetVmVdi {
	param ($vmUUID)

	$filter = "SELECT * FROM Xen_Disk where DeviceID like `"%$vmUuid%`""
	$vmVdi = EnumClassFilter "Xen_Disk" $filter

	return $vmVdi
}
### End ##########################################



### Get the VBD(s) of a particular VM ############
function GetVmVbd {
	param ($vmName)
	
	$filter = "SELECT * FROM Xen_DiskSettingData where InstanceID like `"%$vmName%`""
	$xenEnumXml = EnumClassFilter "Xen_DiskSettingData" $filter

	return $xenEnumXml
	
	# ElementName = Name of the VDI (Xen_DiskImage)
	# HostExtentName = UUID of the VDI
	# HostResource = DeviceID of the VDI = Xen_DiskImage.DeviceID="Xen:<SR Pool UUID>\<VDI UUID>"
	# InstanceID = Xen:<VM UUID>\<VBD UUID>
	# PoolID = the SR the VDI is stored on
}
### End ##########################################



### Create a VDI attached to a VM ################
# this method returns the VBD since the VDI is created and attached to a VM via a VBD.
function CreateVmVdi {
	param ($vdiName, $addressOnParent, $vdiMb, $vmName, $srPoolId)
	
	# This can create an empty VDI and attach it to a VM with one Invoke.
	# This uses the default SR
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
	<AddResourceSetting_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/root/cimv2/Xen_VirtualSystemManagementService">
	<ResourceSetting>
		<dsd:Xen_DiskSettingData
			xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData" 
			xsi:type="Xen_DiskettingData_Type">
			<dsd:PoolID>$srPoolId</dsd:PoolID>
			<dsd:ElementName>$vdiName</dsd:ElementName>
			<dsd:ResourceType>19</dsd:ResourceType>
			<dsd:VirtualQuantity>$vdiMb</dsd:VirtualQuantity>
			<dsd:AllocationUnits>MegaBytes</dsd:AllocationUnits>
			<dsd:Bootable>true</dsd:Bootable>
			<dsd:Access>3</dsd:Access>
			<dsd:AddressOnParent>$addressOnParent</dsd:AddressOnParent>
		</dsd:Xen_DiskSettingData>
	</ResourceSetting>
	<AffectedSystem> 
		<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
		<a:ReferenceParameters xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
			<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
			<w:SelectorSet>
				<w:Selector Name="Name">$vmName</w:Selector>
				<w:Selector Name="CreationClassName">Xen_ComputerSystem</w:Selector>
			</w:SelectorSet>
		</a:ReferenceParameters>
	</AffectedSystem>
	</AddResourceSetting_INPUT>
"@
	
	# $objSession.Get($actionURI)
	
	$output = [xml]$objSession.Invoke("AddResourceSetting", $actionURI, $parameters)
	

# 	$newItem = $output.GetElementsByTagName("wsman:Selector")
# 	# $newItem is returned and treated as an Array.
# 
# 		foreach ($element in $newItem){
# 			if ($element.Name -match "InstanceID") {
# 				$xenVbdInstanceId = $element."#Text"
# 			} 
# 		}
# 	}
# 	
# 	return $xenVbdInstanceId

	return $output
	
}
### End ##########################################



### Create a VDI #################################
function CreateVdi {
	param ($vdiName, $vdiMb, $srPoolId)
	
	$actionUri = EndPointReference "Xen_StoragePoolManagementService"
	
	$parameters = @"
		<CreateDiskImage_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService">
		<ResourceSetting>
			<dsd:Xen_DiskSettingData 
			xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData"
			xsi:type="Xen_DiskSettingData_Type">
				<dsd:ElementName>$vdiName</dsd:ElementName>
				<dsd:ResourceType>19</dsd:ResourceType>
				<dsd:PoolID>$srPoolId</dsd:PoolID>
				<dsd:Bootable>true</dsd:Bootable>
				<dsd:VirtualQuantity>$vdiMb</dsd:VirtualQuantity>
				<dsd:AllocationUnits>MegaBytes</dsd:AllocationUnits>
				<dsd:Access>3</dsd:Access>
			</dsd:Xen_DiskSettingData>
		</ResourceSetting>
		</CreateDiskImage_INPUT>
"@

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("CreateDiskImage", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################


### Delete a VDI from a SR #######################
function DeleteVdi {
	param ($vdi)
	
	if ($vdi.GetType().Name -ne "XmlDocument") {
		$vdi = [xml]$vdi
	}

	$DeviceID = $vdi.Xen_DiskImage.DeviceID
	$CreationClassName = $vdi.Xen_DiskImage.CreationClassName
    $SystemCreationClassName = $vdi.Xen_DiskImage.SystemCreationClassName
	$SystemName = $vdi.Xen_DiskImage.SystemName

	$actionUri = EndPointReference "Xen_StoragePoolManagementService"
	
	$parameters = @"
		<DeleteDiskImage_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService">
			<DiskImage>
				<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
				<a:ReferenceParameters xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
					<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskImage</w:ResourceURI>
					<w:SelectorSet>
						<w:Selector Name="DeviceID">$DeviceID</w:Selector>
						<w:Selector Name="CreationClassName">$CreationClassName</w:Selector>
						<w:Selector Name="SystemCreationClassName">$SystemCreationClassName</w:Selector>
						<w:Selector Name="SystemName">$SystemName</w:Selector>
					</w:SelectorSet>
				</a:ReferenceParameters>
			</DiskImage>
		</DeleteDiskImage_INPUT>
"@
	
	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("DeleteDiskImage", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################



### Attach a VDI to a VM #########################
function AttachVdi {
	param ($vmName, $vdiDeviceId)
	# The HostResource string needs to look like: Xen_DiskImage.DeviceID="Xen:e8f3881c-0c11-2e8c-a5d0-9af3652e0852\472589a7-0f31-40cb-a8e6-76687603becc"
	# Attach the disk to the VM
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
		<AddResourceSettings_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
			<ResourceSettings>
				<dsd:Xen_DiskSettingData xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData" xsi:type="Xen_DiskSettingData_Type">
					<dsd:HostResource>Xen_DiskImage.DeviceID=$vdiDeviceId</dsd:HostResource>
					<dsd:ResourceType>19</dsd:ResourceType>
				</dsd:Xen_DiskSettingData>
			</ResourceSettings> 
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
		</AddResourceSettings_INPUT>
"@
	
	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("AddResourceSettings", $actionURI, $parameters)
	
	return $output

}
### End ##########################################



### Detach a VBD from a VM #######################
function DetachVbd {
	param ($xenVbd)
	# Detach the disk from the VM using the InstanceID of the VBD
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
		<RemoveResourceSettings_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
    	xmlns:dsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData"
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
		<ResourceSettings>
			<dsd:Xen_DiskSettingData xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData" xsi:type="Xen_DiskSettingData_Type">
				<dsd:InstanceID>$xenVbd</dsd:InstanceID>
				<dsd:ResourceType>16</dsd:ResourceType>
			</dsd:Xen_DiskSettingData>
		</ResourceSettings>
		</RemoveResourceSettings_INPUT>
"@
	
	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("RemoveResourceSettings", $actionURI, $parameters)
	
	return $output

}
### End ##########################################



### Create an Empty Virtual CD device ############
function AddCdDevice {
	param ($vmName)
	
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
	<AddResourceSettings_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns:dsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData"
	xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
	<ResourceSettings>
		<dsd:Xen_DiskSettingData 
		xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData"
		xsi:type="Xen_DiskSettingData_Type">
			<dsd:ElementName>CD / DVD ROM</dsd:ElementName>
			<dsd:ResourceType>15</dsd:ResourceType>
			<dsd:ResourceSubType>CD</dsd:ResourceSubType>
		</dsd:Xen_DiskSettingData>
	</ResourceSettings>
	<AffectedConfiguration> 
		<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
		<a:ReferenceParameters 
		  xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
		  xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
			<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
			<w:SelectorSet>
				<w:Selector Name="InstanceID">Xen:$vmName</w:Selector>
			</w:SelectorSet>
		</a:ReferenceParameters>
	</AffectedConfiguration>
	</AddResourceSettings_INPUT>
"@
	
	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("AddResourceSettings", $actionURI, $parameters)
	
	return $output

}
### End ##########################################



### Create a VBD and attach an ISO ###############
function AddVbdIso {
	param ($vmName, $isoDeviceId)
	# The HostResource string needs to look like: Xen_DiskImage.DeviceID="Xen:e8f3881c-0c11-2e8c-a5d0-9af3652e0852\472589a7-0f31-40cb-a8e6-76687603becc"
	# Attach the disk to the VM
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
		<AddResourceSettings_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
			<ResourceSettings>
				<dsd:Xen_DiskSettingData xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData" xsi:type="Xen_DiskSettingData_Type">
					<dsd:HostResource>Xen_DiskImage.DeviceID=$isoDeviceId</dsd:HostResource>
					<dsd:ResourceType>15</dsd:ResourceType>
					<dsd:ResourceSubType>CD</dsd:ResourceSubType>
				</dsd:Xen_DiskSettingData>
			</ResourceSettings> 
			<AffectedConfiguration> 
				<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
				<a:ReferenceParameters 
				xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
				xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
					<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
					<w:SelectorSet>
						<w:Selector Name="InstanceID">Xen:$vmName</w:Selector>
					</w:SelectorSet>
				</a:ReferenceParameters>
			</AffectedConfiguration>
		</AddResourceSettings_INPUT>
"@
	
	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("AddResourceSettings", $actionURI, $parameters)
	
	return $output

}
### End ##########################################



### Attach ISO to Existing VBD ###################
function AttachISO {
	param ($vmName, $vdi)
	
	if ($vdi.GetType().Name -ne "XmlDocument") {
		$vdi = [xml]$vdi
	}
	
	# find the (last if there are many, assume one) virtual CD device (VBD) of a VM
	$filter = "SELECT * FROM Xen_DiskSettingData WHERE InstanceID LIKE `"%$vmName%`" AND ResourceType = 16"
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData", $filter, $dialect)
	$xenEnumXml = [xml]$xenEnum.ReadItem()
	$vbdInstanceID = $xenEnumXml.Xen_DiskSettingData.InstanceID

	# Build the HostResource string
	$DeviceID = $vdi.Xen_DiskImage.DeviceID
	$hostResource = "root/cimv2:Xen_DiskImage.CreationClassName=`"Xen_DiskImage`",DeviceID=`"$DeviceID`",SystemCreationClassName=`"Xen_StoragePool`",SystemName=`"$vdi.Xen_DiskImage.SystemName`""

	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
		<ModifyResourceSettings_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
			<ResourceSettings>
				<dsd:Xen_DiskSettingData
					xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData" 
					xsi:type="Xen_DiskettingData_Type">
					<dsd:InstanceID>$vbdInstanceID</dsd:InstanceID>
					<dsd:HostResource>$hostResource</dsd:HostResource>
					<dsd:ResourceType>16</dsd:ResourceType>
				</dsd:Xen_DiskSettingData>
			</ResourceSettings>
		</ModifyResourceSettings_INPUT>
"@
	
	$output = [xml]$objSession.Invoke("ModifyResourceSettings", $actionURI, $parameters)

	return $output
	
}
### End ##########################################



### Detach an ISO leaving the VBD ################
### This assumes one virtual CD device it will fail if more than 1 CD / DVD
function DetachIso {
	param ($vmName)
		
	$filter = "SELECT * FROM Xen_DiskSettingData WHERE InstanceID LIKE `"%$vmName%`" AND ResourceType = 16"
	$xenEnumXml = [xml](EnumClassFilter "Xen_DiskSettingData" $filter)

	$vbdInstanceID = $xenEnumXml.Xen_DiskSettingData.InstanceID
	$hostResource = $xenenumxml.Xen_DiskSettingData.HostResource

	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
		<ModifyResourceSettings_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
			<ResourceSettings>
				<dsd:Xen_DiskSettingData
					xmlns:dsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData" 
					xsi:type="Xen_DiskettingData_Type">
					<dsd:InstanceID>$vbdInstanceID</dsd:InstanceID>
					<dsd:HostResource>$hostResource</dsd:HostResource>
					<dsd:ResourceType>16</dsd:ResourceType>
				</dsd:Xen_DiskSettingData>
			</ResourceSettings>
		</ModifyResourceSettings_INPUT>
"@
	
	$output = [xml]$objSession.Invoke("ModifyResourceSettings", $actionURI, $parameters)

	return $output
	
}
### End ##########################################



### Connect to TransferVM disk Image #############
# This assumes you are connectiong using the management network
function ConnectToDiskImage {
	param ($vdi, $protocol, $ssl)
	
	if ($vdi.GetType().Name -ne "XmlDocument") {
		$vdi = [xml]$vdi
	}

	$DeviceID = $vdi.Xen_DiskImage.DeviceID
	$CreationClassName = $vdi.Xen_DiskImage.CreationClassName
	$SystemCreationClassName = $vdi.Xen_DiskImage.SystemCreationClassName
	$SystemName = $vdi.Xen_DiskImage.SystemName

	$actionUri = EndPointReference "Xen_StoragePoolManagementService"

	$parameters = @"
	<ConnectToDiskImage_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService">
			<DiskImage xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
				<wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
				<wsa:ReferenceParameters> 
				<wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskImage</wsman:ResourceURI> 
				<wsman:SelectorSet> 
						<wsman:Selector Name="DeviceID">$DeviceID</wsman:Selector> 
						<wsman:Selector Name="CreationClassName">$CreationClassName</wsman:Selector> 
						<wsman:Selector Name="SystemCreationClassName">$SystemCreationClassName</wsman:Selector> 
						<wsman:Selector Name="SystemName">$SystemName</wsman:Selector> 
				</wsman:SelectorSet> 
				</wsa:ReferenceParameters> 
			</DiskImage>
			<Protocol>$protocol</Protocol>
			<UseSSL>$ssl</UseSSL>
	</ConnectToDiskImage_INPUT>
"@

	# $objSession.Get($actionURI)
	$startTransfer = [xml]$objSession.Invoke("ConnectToDiskImage", $actionURI, $parameters)
	
	if ($startTransfer.RequestStateChange_OUTPUT.ReturnValue -ne 0) {
	$jobPercentComplete = 0
	while ($jobPercentComplete -ne 100) {
		$jobResult = [xml]$objSession.Get($startTransfer.ConnectToDiskImage_OUTPUT.job.outerxml)
		$jobPercentComplete = $jobResult.Xen_ConnectToDiskImageJob.PercentComplete
		sleep 3
		}
	}
	
	# $output = [xml]$objSession.Get($startTransfer.ConnectToDiskImage_OUTPUT.job.outerxml)

	return $jobResult
	
}
### End ##########################################



### Disconnect from TransferVM disk Image ########
function DisconnectFromDiskImage {
	param ($connectionHandle)
	
	$actionUri = EndPointReference "Xen_StoragePoolManagementService"

	$parameters = @"
	<DisconnectFromDiskImage_INPUT 
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService">
				<ConnectionHandle>$connectionHandle</ConnectionHandle>
	</DisconnectFromDiskImage_INPUT>
"@	

	$output = [xml]$objSession.Invoke("DisconnectFromDiskImage", $actionURI, $parameters)
	
	return $output
	
}

	# The following can be used to check for a completed job
	# $jobPercentComplete = 0
	# while ($jobPercentComplete -ne 100) {
	# 	$jobResult = [xml]$objSession.Get($vdiDisconnect.DisconnectFromDiskImage_OUTPUT.Job.outerxml)
	# 	$jobPercentComplete = $jobresult.Xen_DisconnectFromDiskImageJob.PercentComplete
	# 	$jobPercentComplete
	# 	sleep 3
	# }
	
### End ##########################################



### Create Internal Network ######################
function CreateInternalNet {
	param ($netName)
	
	$actionUri = EndPointReference "Xen_VirtualSwitchManagementService"

	$parameters = @"
	<DefineSystem_INPUT 
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
     xmlns:xsd="http://www.w3.org/2001/XMLSchema"  
	 xmlns:vssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSettingData" 
     xmlns="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService"> 
		<SystemSettings>
			<vssd:Xen_VirtualSystemSettingData xsi:type="Xen_VirtualSystemSettingData_Type"> 
				<vssd:ElementName>$netName</vssd:ElementName>
				<vssd:Description>Internal network created by the test script</vssd:Description>
			</vssd:Xen_VirtualSystemSettingData>
		</SystemSettings>
	</DefineSystem_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("DefineSystem", $actionURI, $parameters)
	
	return $output
}
### End ##########################################



### Create External Network ######################
function CreateExternalNet {
	param ($netName, $ethAdapter)
	
	$actionUri = EndPointReference "Xen_VirtualSwitchManagementService"

	$parameters = @"
	<DefineSystem_INPUT 
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"  
	xmlns:vssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSettingData"
   	xmlns:npsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
	xmlns="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService"> 
		<SystemSettings>
			<vssd:Xen_VirtualSystemSettingData xsi:type="Xen_VirtualSystemSettingData_Type"> 
				<vssd:ElementName>$netName</vssd:ElementName>
				<vssd:Description>External network created by the test script</vssd:Description>
			</vssd:Xen_VirtualSystemSettingData>
		</SystemSettings>
		<ResourceSettings>
			<npsd:Xen_NetworkPortSettingData 
			xmlns:npsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
			xsi:type="Xen_HostNetworkPortSettingData_Type">
				<npsd:Connection>$ethAdapter</npsd:Connection>
				<npsd:VlanTag></npsd:VlanTag>
			</npsd:Xen_NetworkPortSettingData>
		</ResourceSettings>
	</DefineSystem_INPUT>
"@	
	# $ethAdapter is a simple string such as "eth0" or "eth1"
	# <npsd:VlanTag></npsd:VlanTag> - sending a VlanTag is recommended however not required.  The tag must not be blank.
	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("DefineSystem", $actionURI, $parameters)
	
	return $output

}
### End ##########################################


### Create External Bonded Network ###############
function CreateBondedNet {
	param ($netName, $ethAdapter, $ethAdapter2)
	
	$actionUri = EndPointReference "Xen_VirtualSwitchManagementService"

	$parameters = @"
	<DefineSystem_INPUT 
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"  
	xmlns:vssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSettingData"
   	xmlns:npsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
	xmlns="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService"> 
		<SystemSettings>
			<vssd:Xen_VirtualSystemSettingData xsi:type="Xen_VirtualSystemSettingData_Type"> 
				<vssd:ElementName>$netName</vssd:ElementName>
				<vssd:Description>External network created by the test script</vssd:Description>
			</vssd:Xen_VirtualSystemSettingData>
		</SystemSettings>
		<ResourceSettings>
			<npsd:Xen_NetworkPortSettingData 
			xmlns:npsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
			xsi:type="Xen_HostNetworkPortSettingData_Type">
				<npsd:Connection>$ethAdapter</npsd:Connection>
				<npsd:Connection>$ethAdapter2</npsd:Connection>
			</npsd:Xen_NetworkPortSettingData>
		</ResourceSettings>
	</DefineSystem_INPUT>
"@	

	# $ethAdapter is a simple string such as "eth0" or "eth1"
	# <npsd:VlanTag></npsd:VlanTag> - sending a VlanTag is recommended however not required.  The tag must not be blank.
	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("DefineSystem", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################



### Add a Physical NIC to an existing Network ####
# This could create a bond or convert an Internal to an External
function AddNicToNet {
	param ($vSwitchName, $ethAdapter)
	
	$actionUri = EndPointReference "Xen_VirtualSwitchManagementService"

	$parameters = @"
	<AddResourceSettings_INPUT 
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"  
   	xmlns:npsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
	xmlns="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService"> 
		<AffectedConfiguration> 
			<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
			<a:ReferenceParameters 
			xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
			xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
				<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchSettingData</w:ResourceURI>
				<w:SelectorSet>
					<w:Selector Name="InstanceID">Xen:$vSwitchName</w:Selector>
				</w:SelectorSet>
			</a:ReferenceParameters>
		</AffectedConfiguration>
		<ResourceSettings>
			<npsd:Xen_NetworkPortSettingData 
			xmlns:npsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
			xsi:type="Xen_HostNetworkPortSettingData_Type">
				<npsd:Connection>$ethAdapter</npsd:Connection>
				<npsd:VlanTag>99</npsd:VlanTag>
			</npsd:Xen_NetworkPortSettingData>
		</ResourceSettings>
	</AddResourceSettings_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("AddResourceSettings", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################



### UN-Bond Existing Network #####################
function RemoveNicFromExternalNet {
	param ($vSwitchName, $ethAdapter)
	
	# Find the HostNetworkPortSettingData that is associated with the virtual switch.
	$filter = "SELECT * FROM Xen_HostNetworkPortSettingData where VirtualSwitch like `"%$vSwitchName%`""
	$hostNetPortsd = EnumClassFilter "Xen_HostNetworkPortSettingData" $filter

	foreach ($element in $hostNetPortsd) {
		$nic = [xml]$element
		if ($nic.Xen_HostNetworkPortSettingData.Connection -like "$ethAdapter") {
		$hostNetPort = $nic.Xen_HostNetworkPortSettingData.InstanceID
		}
	}
	$actionUri = EndPointReference "Xen_VirtualSwitchManagementService"

	$parameters = @"
	<RemoveResourceSettings_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
   	xmlns:hnpsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_HostNetworkPortSettingData"
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService">
		<ResourceSettings>
			<hnpsd:Xen_HostNetworkPortSettingData 
			xmlns:npsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_HostNetworkPortSettingData"
			xsi:type="Xen_HostNetworkPortSettingData_Type">
				<hnpsd:InstanceID>$hostNetPort</hnpsd:InstanceID>
			</hnpsd:Xen_HostNetworkPortSettingData>
		</ResourceSettings>
	</RemoveResourceSettings_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("RemoveResourceSettings", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################


### Attach VM to Virtual Network #################
function VmToNetwork {
	param ($vSwitchName, $vmName)
	
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
	<AddResourceSetting_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
   	xmlns:npsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
	xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
		<AffectedSystem> 
			<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
			<a:ReferenceParameters 
			xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
			xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
				<w:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
				<w:SelectorSet>
					<w:Selector Name="Name">$vmName</w:Selector>
					<w:Selector Name="CreationClassName">Xen_ComputerSystem</w:Selector>
				</w:SelectorSet>
			</a:ReferenceParameters>
		</AffectedSystem>
		<ResourceSetting>
			<npsd:Xen_NetworkPortSettingData 
			xmlns:npsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
			xsi:type="Xen_NetworkPortSettingData_Type">
				<npsd:PoolID>$vSwitchName</npsd:PoolID>
				<npsd:ResourceType>33</npsd:ResourceType>
			</npsd:Xen_NetworkPortSettingData>
		</ResourceSetting>
	</AddResourceSetting_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("AddResourceSetting", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################


### Detach VM from Virtual Network ###############
function VmFromNetwork {
	param ($vifInstanceId)
	# Xen_NetworkPortSettingData.InstanceID
	
	$actionUri = EndPointReference "Xen_VirtualSystemManagementService"

	$parameters = @"
	<RemoveResourceSettings_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
   	xmlns:npsd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService">
		<ResourceSettings>
			<npsd:Xen_NetworkPortSettingData 
			xmlns:npsd="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData"
			xsi:type="Xen_NetworkPortSettingData_Type">
				<npsd:InstanceID>$vifInstanceId</npsd:InstanceID>
				<npsd:ResourceType>33</npsd:ResourceType>
			</npsd:Xen_NetworkPortSettingData>
		</ResourceSettings>
	</RemoveResourceSettings_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("RemoveResourceSettings", $actionURI, $parameters)
	
	return $output

}
### End ##########################################



### Destroy Virtual Network ######################
function DestroyNet {
	param ($xenNetName)
	
	$actionUri = EndPointReference "Xen_VirtualSwitchManagementService"

	$parameters = @"
	<DestroySystem_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitchManagementService">
        <AffectedSystem xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
          <wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
          <wsa:ReferenceParameters> 
          <wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSwitch</wsman:ResourceURI> 
          <wsman:SelectorSet> 
                <wsman:Selector Name="Name">$xenNetName</wsman:Selector>
				<wsman:Selector Name="CreationClassName">Xen_VirtualSwitch</wsman:Selector> 
          </wsman:SelectorSet> 
          </wsa:ReferenceParameters> 
        </AffectedSystem>
	</DestroySystem_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("DestroySystem", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################



### Connect to VM Console ########################
function ConnectToVmConsole {
	param ($xenServer, $vmUuid, $username, $password)


	# The link for the ActiveX Control: http://<XenServer>/VNCControl.msi - this must be installed
	# The VM must be powered on to allow connection.
	# The ActiveX control is not marked as safe for scripting therefore anytime that the contorl is invoked
	# from a script the Control is blocked.
	# The user must manually launch the console.hta that is produced.
	# More information:  http://msdn.microsoft.com/en-us/library/aa751977(VS.85).aspx
	
	# Create the HTML page
	$consoleHtml = @"
	<html>
		<body color=white>
			<hr>  
				<form name="frm" id="frm">
				Server IP: <input type="text" name="server" value="$xenServer"> 
				Port: <input type="text" name="port" value="80"> 
				<br />
				VM UUID: <input type="text" name="vmuuid" value="$vmUuid">
				<br />
				Credentials: 
				<input type="text" name="user" value="$username">
				<input type="password" name="pass" value="$password">
				<br />
					Width: <input type="text" name="wdth" value="800">
					Height: <input type="text" name="hgt" value="600">
				<br />
				Excercise Methods: 
					<input type=button value="Connect" onClick="doConnect();">
					<input type=button value="CanConnect" onClick="doCanConnect();">
					<input type=button value="IsConnected" onClick="doIsConnected();">
					<input type=button value="Disconnect" onClick="doDisconnect();">
					<input type=button value="Send Ctrl+Alt+Del" onClick="doSendCtrlAltDel();">
				</form>
				<font face=arial size=1>
				<OBJECT classid="clsid:D52D9588-AB6E-425b-9D8C-74FBDA46C4F8" id="myControl1" name="myControl1" width=300 height=100 OnDisconnectedCallback="myDisconnectedCallback(eventid, msg);">
				</OBJECT>
				</font>  
			
			<hr>
		</body> 
		<script language="javascript">
			function myDisconnectedCallback(eventid, msg)
			{
				alert("error occured");
			}
			
			function doConnect()
			{
				//myControl1.add_OnDisconnectedCallbackEvent(myDisconnectedCallback);
				fret = myControl1.Connect(frm.server.value, frm.port.value, frm.vmuuid.value, frm.user.value, frm.pass.value, frm.wdth.value, frm.hgt.value);
				alert(fret);
			}
			function doCanConnect()
			{
				fret = myControl1.CanConnect();
				alert(fret);
			}
			function doIsConnected()
			{
				fret = myControl1.IsConnected();
				alert(fret);
			}
			function doDisconnect()
			{
				fret = myControl1.Disconnect();
				alert(fret);
			}
			function doSendCtrlAltDel()
			{
			myControl1.SendCtrlAltDel();
			}
		</script>
	</html>
"@

	# get the ActiveX control directly
	# This will only work in a 32-bit environment (x86 PowerShell session) because the control is 32-bit only.
	# Running the following command in an x64 session yields the error: 80040154
	# $vncActivexControl = [Activator]::CreateInstance([Type]::GetTypeFromCLSID([Guid]"{D52D9588-AB6E-425b-9D8C-74FBDA46C4F8}")) 

	# Output the HTML page to the disk
# 	$consoleHtml > Console.html
# 	$console = $pwd.ToString() + "\Console.html"
	$consoleHtml > Console.hta
	$console = $pwd.ToString() + "\Console.hta"

	# Open the current folder in Explorer to allow manuall running of the .hta
	explorer $pwd

	
# 	$ie = New-Object -ComObject "InternetExplorer.Application"
# 	$ie.visible = $true
# 	$ie.navigate("$console")
# 	# Need to allow blocked ActiveX content
# 	
# 	$ie | Get-Member | more
# 	$doc = $ie.document
# 	
# 	$btnConnect = $doc.getElementByID("Connect")
# 	$btnCanConnect = $doc.getElementByID("CanConnect")
# 	$btnIsConnected = $doc.getElementByID("IsConnected")
# 	$btnDisconnect = $doc.getElementByID("Disconnect")
# 	$btnCAD = $doc.getElementByID("Send Ctrl+Alt+Del")
# 	
# 	$btnConnect.Click()
		
}
### End ##########################################



### Add CIFS ISO SR ##############################
function AddCifsIsoSr {
	# All of the conneciton information must be supplied in the input sctring, example follows
	# "location=//reddfs/images, iso_path=/media/NT/Win7RTM/VL,username=eng/reduser, cifspassword=Citrix$2"
	# if the password contains a "$" it needs to be in single quites when passed in instead of double quotes
	# AddCifsIsoSr "newSR" "//reddfs/images" "media/NT/Win7RTM/VL" "eng/reduser" 'Citrix$2'
	
	param ($newSrName, $location, $isoPath, $cifsUser, $cifsPass)

	$actionUri = EndPointReference "Xen_StoragePoolManagementService"
	
	$parameters = @"
	<CreateStoragePool_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService"
	xmlns:rasd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData">
		<ElementName>$newSrName</ElementName>
		<ResourceType>16</ResourceType>
		<Settings>
        	<rasd:CIM_ResourceAllocationSettingData xsi:type="CIM_ResourceAllocationSettingData_Type">
              <rasd:Connection>location=$location</rasd:Connection>
              <rasd:Connection>iso_path=$isoPath</rasd:Connection>
              <rasd:Connection>username=$cifsUser</rasd:Connection>
              <rasd:Connection>cifspassword=$cifsPass</rasd:Connection>
			  <rasd:ResourceSubType>iso</rasd:ResourceSubType>
			</rasd:CIM_ResourceAllocationSettingData>
		</Settings>
	</CreateStoragePool_INPUT> 
"@		

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("CreateStoragePool", $actionURI, $parameters)
	
	
	
	return $output

}
### End ##########################################



### Add NFS ISO SR ###############################
function AddNfsIsoSr {
	# Location is passed as an NFS location that the host has permissions to "statler:/NFS"
	# location needs to be in the format of servername:/NFSSharename
	param ($newSrName, $location)
	$actionUri = EndPointReference "Xen_StoragePoolManagementService"
	
	$parameters = @"
	<CreateStoragePool_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePoolManagementService"
	xmlns:rasd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData">
		<ElementName>$newSrName</ElementName>
		<ResourceType>16</ResourceType>
		<Settings>
        	<rasd:CIM_ResourceAllocationSettingData xsi:type="CIM_ResourceAllocationSettingData_Type">
              <rasd:Connection>location=$location</rasd:Connection>
			  <rasd:ResourceSubType>iso</rasd:ResourceSubType>
			</rasd:CIM_ResourceAllocationSettingData>
		</Settings>
	</CreateStoragePool_INPUT> 
"@		

	$output = [xml]$objSession.Invoke("CreateStoragePool", $actionURI, $parameters)
	
	return $output
	
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



### Add iSCSI SR #################################
function AddIscsiSr {

	param ($name, $target, $targetIqn, $chapUser, $chapPassword, $scsiId)

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
              <rasd:Connection>target=$target</rasd:Connection>
              <rasd:Connection>targetIQN=$targetIqn</rasd:Connection>
              <rasd:Connection>chapuser=$chapUser</rasd:Connection>
              <rasd:Connection>chappassword=$chapPassword</rasd:Connection>
              <rasd:Connection>SCSIid=$scsiId</rasd:Connection>
			  <rasd:ResourceSubType>lvmoiscsi</rasd:ResourceSubType>
			</rasd:CIM_ResourceAllocationSettingData>
		</Settings>
	</CreateStoragePool_INPUT> 
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("CreateStoragePool", $actionURI, $parameters)
	
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

	# The following can be used to check for a completed job
	# $jobPercentComplete = 0
	# while ($jobPercentComplete -ne 100) {
	# 	$jobResult = [xml]$objSession.Get($vdiDisconnect.DisconnectFromDiskImage_OUTPUT.Job.outerxml)
	# 	$jobPercentComplete = $jobresult.Xen_DisconnectFromDiskImageJob.PercentComplete
	# 	$jobPercentComplete
	# 	sleep 3
	# }

### End ##########################################



### Clean up Jobs ################################
# I have learned that it is best to clean up a CIM job after a create, modify, destroy, etc.
function DirtyJobCleanUp {
# This does not take input, it simply cleans jobs that do not have errors
# This will literally clean all jobs from the system, leaving only those jobs with an ErrorCode
# As an alternative, use the more intelligent "JobCleanUp" method

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



function JobCleanUp {
# This does not take input, it simply cleans jobs that do not have errors

	$enumFlags = $objWsmanAuto.EnumerationFlagReturnEPR()

	# Make sure this does not go into an infinite loop
	$30Minutes = New-TimeSpan -Minutes 30
	$startTime = Get-Date
	$ahead30 = ($startTime + $30Minutes)

	do {
		$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_Job", $NULL, $NULL, $enumFlags)
		
		$allJobs = @()
		while (!$xenEnum.AtEndOfStream) {
			$elementRec = $xenEnum.ReadItem()
			$allJobs += $elementRec
		}
		
		"Job Count: " + $allJobs.Count
		foreach ($jobEpr in $allJobs) {
			$job = $objsession.Get($jobEpr)
			$jobXml = [xml]$job
			# Check for the state of the Job "JobState"
			$jobState = $jobXml.GetElementsByTagName("p:JobState")
			# $errorCode is returned and treated as an Array.
			foreach ($element in $jobState){
					$stateValue = $element."#Text" 
			}
			# Get the InstanceID as well
			$jobInstance = $jobXml.GetElementsByTagName("p:InstanceID")
			foreach ($element in $jobInstance){
					$instanceValue = $element."#Text" 
			}

			switch ($stateValue) {
				3 {"Starting: " + $instanceValue}
				4 {"Running: " + $instanceValue}
				5 {"Suspended: " + $instanceValue}
				6 {"Shutting Down: " + $instanceValue}
				7 {$null = $objsession.Delete($jobEpr)}
				8 {"Terminated: " + $instanceValue}
				9 {"Killed: " + $instanceValue}
				10 {"Exception: " + $instanceValue}
				default {"Stuck?: " + $instanceValue + " The State: " + $stateValue}
			}
		}	
		sleep 5
		
		# The break out if the clean up takes longer than 30 minutes
		# most likely something went wrong or the jobs are taking an incredibly long time
		$time = Get-Date
        if ($time -ge $ahead30)
        {
            break
        }    

	} until ($allJobs.Count -eq 0)
	
}	
### End ##########################################



### Create a snapshot of a VM ####################
# this needs to have the snapshot type added.
function SnapshotVm {
	param ($vmName, $snapshotName)
	
	$actionUri = EndPointReference "Xen_VirtualSystemSnapshotService"

	$parameters = @"
	<CreateSnapshot_INPUT 
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
	xmlns:vssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSettingData" 
	xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService">
		<AffectedSystem> 
			<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
			<a:ReferenceParameters 
			xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
			xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
				<w:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
				<w:SelectorSet>
					<w:Selector Name="Name">$vmName</w:Selector>
					<w:Selector Name="CreationClassName">Xen_ComputerSystem</w:Selector>
				</w:SelectorSet>
			</a:ReferenceParameters>
		</AffectedSystem>
		<SnapshotSettings>
			<vssd:Xen_VirtualSystemSettingData xsi:type="Xen_VirtualSystemSettingData_Type"> 
				<vssd:ElementName>$snapshotName</vssd:ElementName>
				<vssd:Description>This is the description for this test snapshot</vssd:Description>
			</vssd:Xen_VirtualSystemSettingData>
		</SnapshotSettings>
	</CreateSnapshot_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("CreateSnapshot", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################



### Export the snapshot tree of a VM #############
# $downloadPath = "F:\Test\"
# $vmName = Xen_ComputerSystem.Name
function ExportSnapshotTree {
	param ($vmName, $xenServer, $downloadPath)
	
	$actionUri = EndPointReference "Xen_VirtualSystemSnapshotService"
	$exportOutput = $downloadPath + "exportSnapshotTree.txt"

	Import-Module BitsTransfer

	# Start the Export
	$parameters = @"
	<StartSnapshotForestExport_INPUT
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService">
		<System> 
			<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
			<a:ReferenceParameters 
			xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
			xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
				<w:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
				<w:SelectorSet>
					<w:Selector Name="Name">$vmName</w:Selector>
					<w:Selector Name="CreationClassName">Xen_ComputerSystem</w:Selector>
				</w:SelectorSet>
			</a:ReferenceParameters>
		</System>
	</StartSnapshotForestExport_INPUT>
"@	

	$startExport = $objSession.Invoke("StartSnapshotForestExport", $actionURI, $parameters)
	"StartSnapshotForestExport" | Out-File $exportOutput 
	$startExport | Out-File $exportOutput -Append
	$startExport = [xml]$startExport
	
	# Check for Job Status
	if ($startExport.StartSnapshotForestExport_OUTPUT.ReturnValue -ne 0) {
		$jobResult = [xml]$objSession.Get($startExport.StartSnapshotForestExport_OUTPUT.job.outerxml)
		if ($jobresult.Xen_StartSnapshotForestExportJob.PercentComplete -ne 100) {
			$jobPercentComplete = $jobresult.Xen_StartSnapshotForestExportJob.PercentComplete
			while ($jobPercentComplete -ne 100) {
				$jobResult = [xml]$objSession.Get($startExport.StartSnapshotForestExport_OUTPUT.job.outerxml)
				$jobPercentComplete = $jobresult.Xen_StartSnapshotForestExportJob.PercentComplete
				sleep 3
			}
		}
	}
	else {
		$jobResult = $objSession.Get($startExport.StartSnapshotForestExport_OUTPUT.job.outerxml)
		"StartExport Job Result" | Out-File $exportOutput -Append
		$jobResult | Out-File $exportOutput -Append
		$jobResult = [xml]$jobResult
	}

	$connectionHandle = $jobResult.Xen_StartSnapshotForestExportJob.ExportConnectionHandle
 	$metadataUri = $jobResult.Xen_StartSnapshotForestExportJob.MetadataURI
	
	if ($jobResult.Xen_StartSnapshotForestExportJob.DiskImageURIs -eq $null) {
		"No URI's were returned, go look" | Out-File $exportOutput -Append
		pause
	}
	# Download the Metadata file (this is an HTTP file download)
 	$downloadClient = New-Object System.Net.WebClient
 	$downloadClient.DownloadFile($metadataUri,($downloadPath + "export.xva"))

	# Capture the virtual disk image URIs to pass to BITS
	$vDisksToDownload = @()
	$vDisksToDownload = $jobResult.Xen_StartSnapshotForestExportJob.DiskImageURIs
	"The URIs for the disks that will be downloaded" | Out-File $exportOutput -Append
	$vDisksToDownload | Out-File $exportOutput -Append
		
	# download each disk one at a time
	foreach ($element in $vDisksToDownload) {
		$file = $element.Split('/')
		$file = $file[($file.length - 1)]
		$destination = $downloadPath + $file
        
		$transferJob = Start-BitsTransfer -Source $element -destination $destination -DisplayName SnapshotDiskExport -asynchronous
		"-Source $element -destination $destination" | Out-File $exportOutput -Append
        
		while (($transferJob.JobState -eq "Transferring") -or ($transferJob.JobState -eq "Connecting"))
			{ sleep 5 }
		
			switch($transferJob.JobState)
			{
				"Connecting" { Write-Host " Connecting " }
				"Transferring" { Write-Host "$transferJob.JobId has progressed to " + ((($transferJob.BytesTransferred / 1Mb) / ($transferJob.BytesTotal / 1Mb)) * 100) + " Percent Complete"}
				"Transferred" {Complete-BitsTransfer -BitsJob $transferJob}
				"Error" {
					$transferJob | Format-List | Out-File $exportOutput -Append
					"BITS Error Condition: " + $transferJob.ErrorCondition | Out-File $exportOutput -Append
					"BITS Error Description: " + $transferJob.ErrorDescription | Out-File $exportOutput -Append
					"BITS Error Context: " + $transferJob.ErrorContext | Out-File $exportOutput -Append
					"BITS Error Context Description: " + $transferJob.ErrorContextDescription | Out-File $exportOutput -Append
					pause
					Remove-BitsTransfer $transferJob
					}
				"TransientError" {
					$transferJob | Format-List | Out-File $exportOutput -Append
					pause
					# Resume-BitsTransfer $element # This should attempt a resume-bitstransfer but that is currently not supported with the TransferVM.
					Remove-BitsTransfer $transferJob
					} 
			}	
	}
	
	# End the entire process to tear down the Transfer VM
	$parameters = @"
	<EndSnapshotForestExport_INPUT
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
		xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
		xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService">
				<ExportConnectionHandle>$connectionHandle</ExportConnectionHandle>
	</EndSnapshotForestExport_INPUT>
"@	

	# $objSession.Get($actionURI)
	$endExport = $objSession.Invoke("EndSnapshotForestExport", $actionURI, $parameters)
	$endExport | Out-File $exportOutput -Append
	$endExport = [xml]$endExport
	
	# Check for Job Status
	if ($endExport.EndSnapshotForestExport_OUTPUT.ReturnValue -ne 0) {
	$jobPercentComplete = 0
	while ($jobPercentComplete -ne 100) {
		$jobResult = [xml]$objSession.Get($endExport.EndSnapshotForestExport_OUTPUT.job.outerxml)
		$jobPercentComplete = $jobresult.Xen_EndSnapshotForestExportJob.PercentComplete
		sleep 3
		}
	}
	
	return $endExport
}
### End ##########################################



### Import the snapshot tree of a VM #############
# $targetSR = this should be the Xen_StoragePool object
function ImportSnapshotTree {
	param ($targetSR, $xenServer, $downloadPath)
	
	if ($targetSR.GetType().Name -ne "XmlDocument") {
		$targetSR = [xml]$targetSR
	}

	# setup an output file
	$importOutput = $downloadPath + "importSnapshotTree.txt"

	Import-Module BitsTransfer

	$actionUri = EndPointReference "Xen_VirtualSystemSnapshotService"

	# Import the Metadata file that describes the VM snapshot tree
	# Find the Export.xva file and copy it in
	$importFiles = Get-ChildItem $downloadPath
	"0 - The items found in the import folder" | Out-File $importOutput   # note -append nor -noclobber is used, thus the file is reset
	$importFiles | Out-File -append $importOutput
	
	foreach ($element in $importFiles) {
		if ($element.Extension -like ".xva") {
			# Create a VDI
			$createMetadataVdi = CreateVdi $element.BaseName 1 $targetSR.Xen_StoragePool.PoolID
			$metadataVdi = $objSession.Get($createMetadataVdi.CreateDiskImage_OUTPUT.ResultingDiskImage.outerxml)
			$metadataVdi | Out-File -append $importOutput
			$metaDataVdi = [xml]$metaDataVdi

			# Copy the export.xva to the VDI endpoint
			$transferVm = ConnectToDiskImage $metadataVdi "bits" "0"
			$source =  $downloadPath + $element.Name
			# This is a RAW file copy using BITS as the transport
			$transferJob = Start-BitsTransfer -Source $source -destination $transferVm.Xen_ConnectToDiskImageJob.TargetURI -DisplayName ImportSnapshotTreeMetadataUpload -TransferType Upload -Asynchronous
			"-Source " + $source + " -destination " + $transferVm.Xen_ConnectToDiskImageJob.TargetURI | Out-File -append $importOutput

			while (($transferJob.JobState -eq "Transferring") -or ($transferJob.JobState -eq "Connecting"))
				{ sleep 5 }
			
			switch($transferJob.JobState)
			{
				"Connecting" { Write-Host " Connecting " }
				"Transferring" { Write-Host "$transferJob.JobId has progressed to " + ((($transferJob.BytesTransferred / 1Mb) / ($transferJob.BytesTotal / 1Mb)) * 100) + " Percent Complete" }
				"Transferred" {Complete-BitsTransfer -BitsJob $transferJob}
				"Error" {
					$transferJob | Format-List | Out-File -append $importOutput
					"BITS Error Condition: " + $transferJob.ErrorCondition | Out-File -append $importOutput
					"BITS Error Description: " + $transferJob.ErrorDescription | Out-File -append $importOutput
					"BITS Error Context: " + $transferJob.ErrorContext | Out-File -append $importOutput
					"BITS Error Context Description: " + $transferJob.ErrorContextDescription | Out-File -append $importOutput
					pause
					Remove-BitsTransfer $transferJob
					}
				"TransientError" {
					$transferJob | Format-List | Out-File -append $importOutput
					pause
					# Resume-BitsTransfer $transferJob # This should attempt a resume-bitstransfer but that is currently not supported with the TransferVM.
					Remove-BitsTransfer $transferJob
					} 
			}
	
			$metadataVdiDisconnect = DisconnectFromDiskImage $transferVm.Xen_ConnectToDiskImageJob.ConnectionHandle
			$jobPercentComplete = 0
			while ($jobPercentComplete -ne 100) {
				$jobResult = [xml]$objSession.Get($metadataVdiDisconnect.DisconnectFromDiskImage_OUTPUT.Job.outerxml)
				$jobPercentComplete = $jobresult.Xen_DisconnectFromDiskImageJob.PercentComplete
				sleep 10
			}
		}
	}	
		
	# Parse out $metadataVdi
	$DeviceID = $metadataVdi.Xen_DiskImage.DeviceID
	$CreationClassName = $metadataVdi.Xen_DiskImage.CreationClassName
	$SystemCreationClassName = $metadataVdi.Xen_DiskImage.SystemCreationClassName
	$SystemName = $metadataVdi.Xen_DiskImage.SystemName

	$parameters = @"
	<PrepareSnapshotForestImport_INPUT
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService">
		<MetadataDiskImage 
		xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
		xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
			<wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
			<wsa:ReferenceParameters> 
			<wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskImage</wsman:ResourceURI> 
			<wsman:SelectorSet> 
					<wsman:Selector Name="DeviceID">$DeviceID</wsman:Selector> 
					<wsman:Selector Name="CreationClassName">$CreationClassName</wsman:Selector> 
					<wsman:Selector Name="SystemCreationClassName">$SystemCreationClassName</wsman:Selector> 
					<wsman:Selector Name="SystemName">$SystemName</wsman:Selector> 
			</wsman:SelectorSet> 
			</wsa:ReferenceParameters> 
		</MetadataDiskImage>
	</PrepareSnapshotForestImport_INPUT>
"@	

	$prepareImport = $objSession.Invoke("PrepareSnapshotForestImport", $actionURI, $parameters)
	"PrepareImport" | Out-File -append $importOutput
	$prepareImport  | Out-File -append $importOutput
	$prepareImport = [xml]$prepareImport
	
	# Start the Import
	$importContext = $prepareImport.PrepareSnapshotForestImport_OUTPUT.ImportContext
	$InstanceID = $targetSr.Xen_StoragePool.InstanceID

	# Set the namespace once before entering the loop
	$namespace = @{n1="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService"}

	# The big loop needs to begin here and needs to Loop Until ImportContext is missing
	do {
		# This parameter needs to be set each time because $diskImageMap and $importContext need to be fed back in for processing
		$parameters = @"
		<CreateNextDiskInImportSequence_INPUT
		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
		xmlns:xsd="http://www.w3.org/2001/XMLSchema"
		xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService">
			<StoragePool>
				<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
				<a:ReferenceParameters xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
					<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_StoragePool</w:ResourceURI>
					<w:SelectorSet>
						<w:Selector Name="InstanceID">$InstanceID</w:Selector>
					</w:SelectorSet>
				</a:ReferenceParameters>
			</StoragePool>
			<ImportContext>
				$importContext
			</ImportContext>
			<DiskImageMap>
				$diskImageMap
			</DiskImageMap>
		</CreateNextDiskInImportSequence_INPUT>
"@	

		$diskImport = $objSession.Invoke("CreateNextDiskInImportSequence", $actionURI, $parameters)
		"DiskImport" | Out-File -append $importOutput
		$diskImport  | Out-File -append $importOutput
		$diskImport = [xml]$diskImport
		
		$diskImport.CreateNextDiskInImportSequence_OUTPUT
		
		$diskToImport = $diskImport.CreateNextDiskInImportSequence_OUTPUT.OldDiskID
		$importContext = $diskImport.CreateNextDiskInImportSequence_OUTPUT.ImportContext
		$diskImageMap = $diskImport.CreateNextDiskInImportSequence_OUTPUT.DiskImageMap
	
		# little loop above until the parameter OldDiskID is returned if OldDiskID is present then go below
		if ((Select-Xml -Xml $diskImport -Xpath "//n1:OldDiskID" -Namespace $namespace) -ne $null) {
			foreach ($element in $importFiles) {
				if ($element.Name -match $diskToImport) {
					
					$newVdi = $objSession.Get($diskImport.CreateNextDiskInImportSequence_OUTPUT.NewDiskImage.outerxml)
					"DiskImportNewVdi" | Out-File -append $importOutput
					$newVdi  | Out-File -append $importOutput
					$newVdi = [xml]$newVdi
					
					$transferVm = ConnectToDiskImage $newVdi "bits" "0"
					"DiskImportTransferVm" | Out-File -append $importOutput
					$transferVm  | Out-File -append $importOutput
					
					$source =  $downloadPath + $element.Name
                    $destination = $transferVm.Xen_ConnectToDiskImageJob.TargetURI + ".vhd"
                    "-Source $source"  | Out-File -append $importOutput
                    "-Destination $destination"  | Out-File -append $importOutput
					
					$transferJob = Start-BitsTransfer -Source $source -destination $destination -DisplayName ImportSnapshotVirtualDiskUpload -TransferType Upload -Asynchronous
					"DiskImportTransferJob" | Out-File -append $importOutput
					$transferJob  | Out-File -append $importOutput

					while (($transferJob.JobState -eq "Transferring") -or ($transferJob.JobState -eq "Connecting"))
						{ sleep 5 }
					
					switch($transferJob.JobState)
					{
						"Connecting" { Write-Host " Connecting " }
						"Transferring" { Write-Host "$element.JobId has progressed to " + ((($transferJob.BytesTransferred / 1Mb) / ($transferJob.BytesTotal / 1Mb)) * 100) + " Percent Complete" }
						"Transferred" {Complete-BitsTransfer -BitsJob $transferJob}
						"Error" {
							$transferJob | Format-List | Out-File -append $importOutput
							"BITS Error Condition: " + $transferJob.ErrorCondition | Out-File -append $importOutput
							"BITS Error Description: " + $transferJob.ErrorDescription | Out-File -append $importOutput
							"BITS Error Context: " + $transferJob.ErrorContext | Out-File -append $importOutput
							"BITS Error Context Description: " + $transferJob.ErrorContextDescription | Out-File -append $importOutput
							pause
							Remove-BitsTransfer $transferJob
							}
						"TransientError" {
							$transferJob | Format-List | Out-File -append $importOutput
							pause
							# Resume-BitsTransfer $transferJob # This should attempt a resume-bitstransfer but that is currently not supported with the TransferVM.
							Remove-BitsTransfer $transferJob
							} 
					}	
					
					$uploadVdiDisconnect = DisconnectFromDiskImage $transferVm.Xen_ConnectToDiskImageJob.ConnectionHandle
					$jobPercentComplete = 0
					while ($jobPercentComplete -ne 100) {
						$jobResult = [xml]$objSession.Get($uploadVdiDisconnect.DisconnectFromDiskImage_OUTPUT.Job.outerxml)
						$jobPercentComplete = $jobresult.Xen_DisconnectFromDiskImageJob.PercentComplete
						sleep 3
					}
				}
			}	
		}
			
	} until ((Select-Xml -Xml $diskImport -Xpath "//n1:ImportContext" -Namespace $namespace) -eq $null)

	# When the loop is complete, finalize the import here.
	$parameters = @"
	<FinalizeSnapshotForestImport_INPUT
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService">
		<StoragePool>
			<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
			<a:ReferenceParameters xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
				<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_StoragePool</w:ResourceURI>
				<w:SelectorSet>
					<w:Selector Name="InstanceID">$InstanceID</w:Selector>
				</w:SelectorSet>
			</a:ReferenceParameters>
		</StoragePool>
		<MetadataDiskImage 
		xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
		xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
			<wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
			<wsa:ReferenceParameters> 
			<wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskImage</wsman:ResourceURI> 
			<wsman:SelectorSet> 
					<wsman:Selector Name="DeviceID">$DeviceID</wsman:Selector> 
					<wsman:Selector Name="CreationClassName">$CreationClassName</wsman:Selector> 
					<wsman:Selector Name="SystemCreationClassName">$SystemCreationClassName</wsman:Selector> 
					<wsman:Selector Name="SystemName">$SystemName</wsman:Selector> 
			</wsman:SelectorSet> 
			</wsa:ReferenceParameters> 
		</MetadataDiskImage>
		<DiskImageMap>
			$diskImageMap
		</DiskImageMap>
	</FinalizeSnapshotForestImport_INPUT>
"@	

	$importFinalize = $objSession.Invoke("FinalizeSnapshotForestImport", $actionURI, $parameters)
	"ImportFinalize" | Out-File -append $importOutput
	$importFinalize  | Out-File -append $importOutput
	$importFinalize = [xml]$importFinalize
	
	# Get the imported VM back to pass back out
	$vmImportResult = $objSession.Get($importFinalize.FinalizeSnapshotForestImport_OUTPUT.VirtualSystem.outerxml)
	"VmImportResult" | Out-File -append $importOutput
	$vmImportResult  | Out-File -append $importOutput
	$vmImportResult = [xml]$vmImportResult
	
	return $vmImportResult
}
### End ##########################################



### Apply a snapshot of a VM #####################
# You can also think of this as reverting to a previous moment in time
function ApplySnapshot {
	param ($newVmName, $snapshotInstanceId)
	
	$actionUri = EndPointReference "Xen_VirtualSystemSnapshotService"

    $parameters = @"
	<ApplySnapshot_INPUT
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService">
	<Snapshot>
		<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
		<a:ReferenceParameters xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
			<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSnapshot</w:ResourceURI>
			<w:SelectorSet>
				<w:Selector Name="InstanceID">$snapshotInstanceId</w:Selector>
			</w:SelectorSet>
		</a:ReferenceParameters>
	</Snapshot>
	</ApplySnapshot_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("ApplySnapshot", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################



### Delete a snapshot of a VM ####################
function DestroySnapshot {
	param ($snapshotInstanceId)
	
	$actionUri = EndPointReference "Xen_VirtualSystemSnapshotService"

    $parameters = @"
	<DestroySnapshot_INPUT
	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	xmlns ="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemSnapshotService">
	<AffectedSnapshot>
		<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
		<a:ReferenceParameters xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
			<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSnapshot</w:ResourceURI>
			<w:SelectorSet>
				<w:Selector Name="InstanceID">$snapshotInstanceId</w:Selector>
			</w:SelectorSet>
		</a:ReferenceParameters>
	</AffectedSnapshot>
	</DestroySnapshot_INPUT>
"@	

	# $objSession.Get($actionURI)
	$output = [xml]$objSession.Invoke("DestroySnapshot", $actionURI, $parameters)
	
	return $output
	
}
### End ##########################################
