# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the Xen-CIM Project.
# This PowerShell script is executed against the XenServer CIM WSMAN interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# This script simply and quickly takes a single VM
# and then duplicates that
# vm into XXX virtual machines to the default Storage Repository.
# 
# This is used in testing in Citrix Labs Redmond to build out
# a test environment when stress requires a large number
# of elements in the environment.
# The standard build uses DamnSmallLinux running in an HVM virtual machine.
# DSL detects hardware on each boot and thus serves as a good
# candidate for this.  The only limitation is the absence of the XenTools.

# It is the responsibility of the person executing this script to
# create a VM, install it, convert it to a template and to 
# name add the template name to the script so it can be used for duplication.

# Currently this requires creating a new vm from the TransferVm hidden template.
# Then in xe 'clear' the other-config parameters

# The variables involved
$xenServer = "192.168.2.99"
$userName = "root"
$password = "Citrix`$2"
$source = "Sample"           # Name of the source
$vmCount = "1000"            # The desired count

# Constants
$dialect = "http://schemas.microsoft.com/wbem/wsman/1/WQL"

# Arguments
# $xenServer = $Args[0]
# $userName = $Args[1]
# $password = $Args[2]
# $sourceTemplate = $Args[3]
# $vmCount = $Args[4] 

# Helper functions
###################################
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

	return $output
	
}
### End ##########################################

# Create a WSMAN session object with the XenServer
	$objWsmanAuto = New-Object -ComObject wsman.automation
	# set the connection options of username and password
	$connOptions = $objWsmanAuto.CreateConnectionOptions()
	$connOptions.UserName = $userName
	$connOptions.Password = $password
	# set the session flags required for the connection to work
	$iFlags = ($objWsmanAuto.SessionFlagNoEncryption() -bor $objWsmanAuto.SessionFlagUTF8() -bor $objWsmanAuto.SessionFlagUseBasic() -bor $objWsmanAuto.SessionFlagCredUsernamePassword())
	# The target system
	$target = "http://" + $xenServer + ":5988"
	# Open the session
	$objSession = $objWsmanAuto.CreateSession($target, $iflags, $connOptions)
	# Increase the timeout to 5 minutes
	$objSession.Timeout = 3000000
	# Identify the interface
	$objSession.Identify()


# Find the source VM 
 	$filter = "SELECT * FROM Xen_ComputerSystemSettingData where ElementName like `"%$source%`""
  	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSettingData", $filter, $dialect)
	# We are only expecting one item back.
	$sourceVm = [xml]$xenEnum.ReadItem()
	$refVmInstanceId = $sourceVm.Xen_ComputerSystemSettingData.InstanceID

# Find the source Template 
#  	$filter = "SELECT * FROM Xen_ComputerSystemTemplate where ElementName like `"%$source%`""
#   	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemTemplate", $filter, $dialect)
# 	# We are only expecting one item back.
# 	$sourceVm = [xml]$xenEnum.ReadItem()
# 	$refVmInstanceId = $sourceVm.Xen_ComputerSystemTemplate.InstanceID
# 
# 	$parameters = @"
# 	<CopySystem_INPUT
# 		xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
# 		xmlns:xsd="http://www.w3.org/2001/XMLSchema"
# 		xmlns="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService"
# 		xmlns:cssd="http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSettingData">
# 		<SystemSettings>
#          <cssd:Xen_ComputerSystemSettingData  
#              xsi:type="Xen_ComputerSystemSettingData_Type"> 
#               <cssd:Description>This is a script created system</cssd:Description>
#               <cssd:ElementName>$newVmName</cssd:ElementName>
#               <cssd:Other_Config>"HideFromXenCenter=false; transfervm=false"</cssd:Other_Config>
#            </cssd:Xen_ComputerSystemSettingData>
# 		</SystemSettings>
#         <ReferenceConfiguration xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
#               <wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
#               <wsa:ReferenceParameters> 
#               <wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemTemplate</wsman:ResourceURI> 
#               <wsman:SelectorSet> 
#                     <wsman:Selector Name="InstanceID">$refVmInstanceId</wsman:Selector> 
#               </wsman:SelectorSet> 
#               </wsa:ReferenceParameters> 
#         </ReferenceConfiguration>
#         <StoragePool xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsman="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
#               <wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address> 
#               <wsa:ReferenceParameters> 
#               <wsman:ResourceURI>http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePool</wsman:ResourceURI> 
#               <wsman:SelectorSet> 
#                     <wsman:Selector Name="InstanceID">$xenSrInstanceId</wsman:Selector> 
#               </wsman:SelectorSet> 
#               </wsa:ReferenceParameters> 
# 		</StoragePool>
# 	</CopySystem_INPUT>
# "@
	
# Find "Local Storage"
	$filter = "SELECT * FROM Xen_StoragePool where Name like `"%Local storage%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePool", $filter, $dialect)
	$localSr = [xml]$xenEnum.ReadItem()
	$xenSrInstanceId = $localSr.Xen_StoragePool.InstanceID
	
# Create new virtual machines from the source until we reach the total count passed.	
	$a = 0
	while ($a -lt $vmCount) {
		$vmName = $source + $a
		$capture = CopyVm $vmName $refVmInstanceId $xenSrInstanceId
		$a++;$a
		}
	# If the source is a template then the helper method CreateVmFromTemplate must
	# be used as the XML is different
		
# Clean up the jobs that were spawned for each create.
	# $enumFlags = $objWsmanAuto.EnumerationFlagReturnEPR()
	# $xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_Job", $NULL, $NULL, $enumFlags)
	
	# $allJobs = @()
	# while (!$xenEnum.AtEndOfStream) {
	#	$elementRec = $xenEnum.ReadItem()
	#	$allJobs += $elementRec
	#}
	
	# foreach ($jobEpr in $allJobs) {
	#	$job = $objsession.Get($jobEpr)
	#	$jobXml = [xml]$job
	#	# Check for an error status other than 0 and dump it
	#	$errorCode = $jobXml.GetElementsByTagName("p:ErrorCode")
	#	# $errorCode is returned and treated as an Array.
	#	foreach ($element in $errorCode){
	#			$errorValue = $element."#Text" 
	#	}
	#	if ($errorValue -ne "0") {
	#		"$job"
	#	} 
	#	else {
	#	$objsession.Delete($jobEpr)
	#	}
	#}

#     "End"