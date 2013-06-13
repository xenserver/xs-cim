# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the XenServer CIM interface.
# This PowerShell script is executed against the XenServer CIM WS-Management interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# The ENUM everything through WINRM batch script in PoSh.
#
# The output of this is designed to be parsed into Silk using the existing Visual Basic script
# The VB Script is designed to look through a log file that is composed of indvidual testts, parse
# the individual tests into individual output.xml files and return that detail to Silk Central.
#
# Write-Host was removed from the entire script to facilitate redirecting the output to file.

# The params for the script are the target XenServer, username, password
$xenServer = "http://" + $Args[0] + ":5988"
$userName = $Args[1]
$password = $Args[2]

# Static params for testing and debugging
# $xenServer = "http://192.168.1.71:5988"
# $userName = "root"
# $password = "K33p0ut"

# The name of the running script to create an output file in the current running directory
$MyInvocation.MyCommand.Name

##################################################
################### Functions ####################
##################################################



# This is a simple test that the responce returned was from the WSMAN service.  
# This passes where the WSMAN service responds with anything, regardless of the content.
# This fails if the responce is empty or is not wrapped in the WSMAN envelope
function WsmanResult ($xml)
{
$xml = [xml]$xml
if ($xml.Results.wsman -like "http://schemas.dmtf.org/wbem/wsman/1/wsman/results")
	{"Test Passed"}
else
	{"Test Failed"}

Clear-Variable -Name xml
Clear-Variable -Name cimXmlObject
}




Function WsmanEnumerate ($cimClass)

# This is the core function of this entire script and performs enumerations.
# This was set apart as a function for future flexibility and to reduce script size.

{
 "########################################"
 "Start: $cimClass"

# the enum string
$cimUri = "http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/" + $cimClass
$cimUri

$cimXmlObject  = [xml](winrm enum $cimUri -r:$xenServer -encoding:utf-8 -a:basic -u:$userName -p:$password -format:pretty)

$cimXmlObject.DocumentElement.$cimClass 

WsmanResult ($cimXmlObject)

 "End: $cimClass"
 "."

}



##################################################
#### XenServerCIM Classes that are under test ####
##################################################
# All tested classes must be listed here.
 
  $xenCimClasses = @(
    "Xen_HostComputerSystem",
    "Xen_HostComputerSystemCapabilities",
    "Xen_HostPool",
    "Xen_VirtualizationCapabilities",
    "Xen_MemoryCapabilitiesSettingData",
    "Xen_ProcessorCapabilitiesSettingData",
    "Xen_StorageCapabilitiesSettingData",
    "Xen_NetworkConnectionCapabilitiesSettingData",
    "Xen_VirtualSystemManagementService",
    "Xen_VirtualSystemManagementCapabilities",
    "Xen_VirtualSystemMigrationService",
    "Xen_VirtualSystemMigrationCapabilities",
    "Xen_VirtualSystemSnapshotService",
    "Xen_VirtualSystemSnapshotCapabilities",
    "Xen_VirtualSystemSnapshotServiceCapabilities",
    "Xen_VirtualSwitchManagementService",
    "Xen_StoragePoolManagementService",
    "Xen_VirtualSystemManagementServiceJob",
    "Xen_VirtualSystemModifyResourcesJob",
    "Xen_VirtualSystemCreateJob",
    "Xen_ConnectToDiskImageJob",
    "Xen_VirtualSystemMigrationServiceJob",
    "Xen_ComputerSystem",
    "Xen_ComputerSystemCapabilities",
    "Xen_VirtualSwitch",
    "Xen_HostNetworkPort",
    "Xen_HostProcessor",
    "Xen_HostMemory",
    "Xen_DiskImage",
    "Xen_MemoryState",
    "Xen_ProcessorPool",
    "Xen_MemoryPool",
    "Xen_StoragePool",
    "Xen_NetworkConnectionPool",
    "Xen_Processor",
    "Xen_Memory",
    "Xen_Disk",
    "Xen_DiskDrive",
    "Xen_NetworkPort",
    "Xen_VirtualSwitchLANEndpoint",
    "Xen_ComputerSystemLANEndpoint",
    "Xen_VirtualSwitchPort",
    "Xen_Console",
    "Xen_ComputerSystemSettingData",
    "Xen_ComputerSystemTemplate",
    "Xen_ComputerSystemSnapshot",
    "Xen_VirtualSwitchSettingData",
    "Xen_ProcessorSettingData",
    "Xen_MemorySettingData",
    "Xen_DiskSettingData",
    "Xen_NetworkPortSettingData",
    "Xen_HostNetworkPortSettingData",
    "Xen_ConsoleSettingData",
    "Xen_MemoryAllocationCapabilities",
    "Xen_ProcessorAllocationCapabilities",
    "Xen_StorageAllocationCapabilities",
    "Xen_NetworkConnectionAllocationCapabilities",
    "Xen_MetricService",
    "Xen_HostProcessorUtilization",
    "Xen_HostNetworkPortReceiveThroughput",
    "Xen_HostNetworkPortTransmitThroughput",
    "Xen_ProcessorUtilization",
    "Xen_DiskReadThroughput",
    "Xen_DiskWriteThroughput",
    "Xen_DiskReadLatency",
    "Xen_DiskWriteLatency",
    "Xen_NetworkPortReceiveThroughput",
    "Xen_NetworkPortTransmitThroughput"
    )


#################################################
################# The Tests #####################
#################################################

###### Probe to see if WSMAN is listening:  #####

# This is not supported with openPegasus
#### Anonymously:
# $anonProbe = $xenServer + "/wsman-anon/identify"
#  "########################################"
#  "Start: Anonymous_Probe"
# $XenProbeAnon = (winrm id -r:$anonProbe -encoding:utf-8 -a:none)
# $XenProbeAnon
# If ($XenProbeAnon -like 'IdentifyResponse*')
#     {
#      "Test Passed"
#     }
#     else
#     {
#      "Test Failed"
#     }
#  "End: Anonymous_Probe"


#### With authentication:
 "########################################"
 "Start: Authenticated_Probe"
$XenProbeAuth = (winrm id -r:$xenServer -encoding:utf-8 -a:basic -u:$userName -p:$password)
$XenProbeAuth
If ($XenProbeAuth -like 'IdentifyResponse*')
    {
     "Test Passed"
    }
    else
    {
     "Test Failed"
    }
 "End: Authenticated_Probe"


###### Loop through each XenServer CIM and test for return  #####
 
 foreach ($xenClass in $xenCimClasses)
 {
 
  WsmanEnumerate ($xenClass)
 
 }


####  End Script  