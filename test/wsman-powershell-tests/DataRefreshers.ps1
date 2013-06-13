# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the XenServer CIM interface.
# This PowerShell script is executed against the XenServer CIM WS-Management interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# This script is designed to simulate the actions of the 
# SCVMM refreshers.  The refreshers represent constant background 
# noise and work against the environment, even during out-of-hours times.
# The SCVMM refreshers are background
# functions that continuously (and on demand) enumerate
# various aspects of the virtualization environment and 
# it elements.
# All of the refreshers run on a regular schedule, but are
# performend when items are selected in the UI or can be forced
# through the SCVMM PowerShell cmdlets.
# http://technet.microsoft.com/en-us/library/dd221389.aspx
#
# This script performs the following actions:
# Host Refresher:
# Every 30 minutes - Gather Host properties, Storage, virtual networking, physical adapters, physical disks
# VM Heavy Refresher:
# Every 30 minutes - gather all vm properties, associated pools, cluster information, snapshots
# VM Light Refresher:
# Every 2 minutes - gather host hypervisor status, vm and host status, looks for new VMs
# Performance Refresher:
# Every 9 minutes - gather perf information for host and VMs
#
# The script does nothing with the returned information except probe that something
# was returned.  The goal is to provide the continuous background load.


# The variables
$xenServer = "192.168.1.73"
$userName = "root"
$password = "K33p0ut"

# $xenServer = Args[0]
# $userName = Args[1]
# $password = Args[2]

$global:vmFilter = "SELECT * FROM Xen_ComputerSystem where Caption != `"XenServer Host`""
$global:allVms = @()
$global:allHosts = @()

# The core functions
### Enumerate without filter #####################
function EnumClass {
	param ($cimClass)
	$cimUri = "http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/" + $cimClass
	$xenEnum = $objSession.Enumerate($cimUri)
	$xenEnumXml = @()
	while (!$xenEnum.AtEndOfStream) {
		$elementRec = $xenEnum.ReadItem()
		$xenEnumXml += $elementRec
	}
	return $xenEnumXML
}
### End ##########################################

### Enumerate with WQL filter ####################
function EnumClassFilter {
param ($cimClass, $filter)
	$cimUri = "http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/" + $cimClass
	$xenEnum = $objSession.Enumerate($cimUri, $filter, "http://schemas.microsoft.com/wbem/wsman/1/WQL")
	$xenEnumXml = @()
	while (!$xenEnum.AtEndOfStream) {
		$elementRec = $xenEnum.ReadItem()
		$xenEnumXml += $elementRec
	}
	return $xenEnumXml
}
### End ##########################################

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

# The WSMAN Session Object
$objWsmanAuto = New-Object -ComObject wsman.automation # Create the WSMAN session object

$connOptions = $objWsmanAuto.CreateConnectionOptions() # set the connection options of username and password
$connOptions.UserName = $userName
$connOptions.Password = $password

# set the session flags required for XenServer
$iFlags = ($objWsmanAuto.SessionFlagNoEncryption() -bor $objWsmanAuto.SessionFlagUTF8() -bor $objWsmanAuto.SessionFlagUseBasic() -bor $objWsmanAuto.SessionFlagCredUsernamePassword())

$target = "http://" + $xenServer + ":5988" # The XenServer unsecure connection string
$objSession = $objWsmanAuto.CreateSession($target, $iflags, $connOptions) # Open the WSMAN session
$objSession.Timeout = 3000000 # Increase the timeout to 5 minutes for long queries
$objSession.Identify() # WSMAN session identify

# Gather the references to the methods we will use:
$metricService = EndPointReference "Xen_MetricService"


# The Task functions

# Host Refresher
function HostRefresher {
	"Host Refresher started"
	# query for all hosts in a pool
	$global:allHosts = (EnumClass Xen_HostComputerSystem)
	$virtualizationCapabilities = (EnumClass Xen_VirtualizationCapabilities)
	$resourcePool = (EnumClass Xen_HostPool)
	
	# for each host - query its attributes
	foreach ($xenServer in $allHosts) {
		foreach ($element in $virtualizationCapabilities) {
			$capability = [xml]$element
			$resourceType = $capability.Xen_VirtualizationCapabilities.ResourceType
			$enumClass = "Xen_Host" + $resourceType
				if ($enumClass -eq "Xen_HostDisk") {
				$enumClass = "Xen_StoragePool"
				}
			$output = (EnumClass $enumClass)
		}
	# Don't forget virtual networks
	$output = (EnumClass Xen_VirtualSwitch)
	}
	"Host Refresher finished"
}


# Light VM Refresher
# Due to lack of support for association classes I have to be creative
# This job assumes that processing will happen on the client side
function VmLightRefresher {
	"VM Light Refresher started"
	# Check the VSMS is running
	$vsms = [xml](EnumClass Xen_VirtualSystemManagementService)
	$vsms.Xen_VirtualSystemManagementService.Started
	
	# query for all domains in the pool
	$global:allDomains = (EnumClassFilter Xen_ComputerSystem $global:vmFilter)
	$allVmsDetail = (EnumClassFilter Xen_ComputerSystemSettingData $global:vmFilter)
	# The client would sort and parse the response
	"VM Light Refresher finished"
}


# Heavy VM Refresher
# Due to lack of support for association classes I have to be creative
# This job assumes that processing will happen on the client side
function VmHeavyRefresher {
	"VM Heavy Refresher started"
	# query for only VMs in the pool
	$global:allVms = (EnumClassFilter Xen_ComputerSystem $global:vmFilter)
	$allVmsDetail = (EnumClassFilter Xen_ComputerSystemSettingData $global:vmFilter)
	$allVif = (EnumClass Xen_NetworkPort)
	$allVdb = (EnumClass Xen_DiskSettingData)
	$allVdi = (EnumClass Xen_Disk)  # Disk Images
	$allProcessor = (EnumClass Xen_Processor)  # Processor Allocation
	$allMemory = (EnumClass Xen_Memory)  # RAM Allocation
	$allSnapshots = (EnumClass Xen_ComputerSystemSnapshot)  #Snapshots
	$resourcePool = (EnumClass Xen_HostPool)  # Cluster
	
	# It might be easier to know the VMs and to query each one individually 
	# by Get-ting the object, however that does not tell the entire story 
	# and actually increases the overhead on the host.
	"VM Heavy Refresher finished"
}


# Performance refresher
# This runs each 9 minutes, so I assume that it does not need 
# all the history - just the last 9 minutes
function PerformanceRefresher {
	"Performance Refresher started"
	# SCVMM shows the following performance data:
	# Host: CPU use average
	# VM: CPU use, Disk IO, Network IO
	# We will gather CPU, Disk, and network for all

	# Get all hosts in the Pool - this should be provided by the Host refresher which should run first
	# $allHosts = (EnumClass Xen_HostComputerSystem)
	# use $global:allHosts
	
	# Get all VMs in the Pool - this should be provided by the Heavy VM refresher which should run first 
	# $filter = "SELECT * FROM Xen_ComputerSystem where Caption != 'XenServer Host'"
	# $allVms = (EnumClassFilter Xen_ComputerSystem $filter)
	# use $global:allVms
	
	$startTimeWindow = ((Get-Date) - $9Minutes)
	$objScriptTime = New-Object -ComObject WbemScripting.SWbemDateTime
	$objScriptTime.SetVarDate($startTimeWindow)
	$cimStartTime = $objScriptTime.Value
	
	$vmPerformance = @()
	
	# Due to requirement for InstanceID Xen_ComputerSystem needs to e used as Xen_HostComputerSystem does not return the InstanceID
	foreach ($element in $global:allVMs) {
		$xenDomain = [xml]$element
		$name = $xenDomain.Xen_ComputerSystem.Name
		$creationClassName = $xenDomain.Xen_ComputerSystem.CreationClassName
		
		$parameters = @"
		<GetPerformanceMetricsForSystem_INPUT 
			xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
			xmlns:xsd="http://www.w3.org/2001/XMLSchema"
			xmlns="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_MetricService">
			<StartTime>$cimStartTime</StartTime>
			<System>
				<a:Address xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>
				<a:ReferenceParameters 
				xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" 
				xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
					<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>
					<w:SelectorSet>
						<w:Selector Name="CreationClassName">$creationClassName</w:Selector>
						<w:Selector Name="Name">$name</w:Selector>
					</w:SelectorSet>
				</a:ReferenceParameters>
			</System>
		</GetPerformanceMetricsForSystem_INPUT>
"@
		# Put this into an arry as if this was actually useful
		$vmPerformance += [xml]$objSession.Invoke("GetPerformanceMetricsForSystem", $metricService, $parameters)
    }
	# return $vmPerformance
	"Performance Refresher finished"
}


# The big loop - the actual work

# The Timer is set to run for 2 days
$9Minutes = New-TimeSpan -Minutes 9
$2Minutes = New-TimeSpan -Minutes 2
$30Minutes = New-TimeSpan -Minutes 30
$48Hours = New-TimeSpan -Hours 48
$startTime = Get-Date
$ahead2 = ($startTime + $2Minutes)
$ahead9 = ($startTime + $9Minutes)
$ahead30 = ($startTime + $30Minutes)

do {
	
	switch (Get-Date) {
		{$_ -ge $ahead2} {
			# $time
			$runTime = Measure-Command {VmLightRefresher}
			"VmLightRefresher; $runTime.TotalSeconds"
			$ahead2 = ($time + $2Minutes)
		}
		{$_ -ge $ahead9} {
			# $time
			$runTime = Measure-Command {PerformanceRefresher}
			"PerformanceRefresher; $runTime.TotalSeconds"
			$ahead9 = ($time + $9Minutes)
		}
		{$_ -ge $ahead30} {
			# $time
			$runTime = Measure-Command {HostRefresher}
			"HostRefresher; $runTime.TotalSeconds"
			$runTime = Measure-Command {VmHeavyRefresher}
			"VmHeavyRefresher; $runTime.TotalSeconds"
		$ahead30 = ($time + $30Minutes)
		}
	}
	
	$time = Get-Date
	sleep 30
	# $time
	
} until ($time -ge ($startTime + $48Hours))





