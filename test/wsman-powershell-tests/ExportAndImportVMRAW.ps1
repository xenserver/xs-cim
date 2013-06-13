# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the XenServer-CIM Project.
# This PowerShell script is executed against the XenServer CIM WSMAN interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# This script is designed to simulate the actions of exporting
# a VM from a host to a share and then importing that
# same VM back to the host again.
# These are the basic actions that SCVMM performs when 
# saving a VM to the SCVMM library over the network.
#
# This script performs the following actions:
# Create a base VM, Boot the VM, gather the settings of the VM, export the VDI to VHD,
# wait for the Export to complete, tear down the transfer session, Create a VDI,
# Import the VHD, Create a VM, attach the VDI to the VM, Boot the VM.

# dot source reference the script that contains all of the helper functions
# The full path needs to be sent if the prompt run location is NOT the same as the script location.
#. ".\XenWsMan.Functions.ps1"
. "F:\brianeh.CITRITE\Documents\Amano\AmanoPoShPegasus\XenWsMan.Functions.ps1"

# This script is designed to use the BITS interface with a mechanism that uses a RAW copy process the output being RAW but in a file named <something>.vhd



# The variables
$xenServer = "192.168.1.29"
$userName = "root"
$password = "K33p0ut"

# This script has been edited with a local folder locatioon ignoring these settings
# Search for "F:\Test3" to find the lines to comment and uncomment.
$targetShare = "\\192.168.1.41\ImportExportISOSR"
$user = "localhost\administrator"
$pass = "K33p0ut"
# $pass = ConvertTo-SecureString "K33p0ut" -AsPlainText -Force
# $credentials = New-Object -TypeName System.Management.Automation.PSCredential -ArgumentList $user,$pass

$loopCount = 1	# The number of time to loop throught he test for repeated stress.

# using the script with arguments instead of constants
# $xenServer = $Args[0]
# $userName = $Args[1]
# $password = $Args[2]
# $targetShare = $Args[3]
# $cifsUser = $Args[4]
# $cifsPass = $Args[5]
$dialect = "http://schemas.microsoft.com/wbem/wsman/1/WQL"  # This is used for all WQL filters

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

# Begin the tasks, now that the session is established.

$aLoop = 0
while ($aLoop -lt $loopCount) {
	
	# Find "Local Storage"
	# this test is designed for a single host with local storage
	$filter = "SELECT * FROM Xen_StoragePool where Name like `"%Local storage%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePool", $filter, $dialect)
	$localSr = [xml]$xenEnum.ReadItem()
	
	# Create the new VM from a template named "Sample"
	# Find the Template
	$filter = "SELECT * FROM Xen_ComputerSystemTemplate where ElementName like `"%Sample%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemTemplate", $filter, $dialect)
	$sourceTemplate = [xml]$xenEnum.ReadItem()
	
	$createVmResult = CreateVmFromTemplate "Automated Test Core VM" $sourceTemplate.Xen_ComputerSystemTemplate.InstanceID $localSr.Xen_StoragePool.InstanceID
	
	if ($createVmResult.CopySystem_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($createVmResult.CopySystem_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_VirtualSystemCreateJob.PercentComplete
			sleep 3
		}
		
		# query for the new VM
		$jobVmName = $jobresult.Xen_VirtualSystemCreateJob.ElementName
		$filter = "SELECT * FROM Xen_ComputerSystem where ElementName like `"%$jobVmName%`""
		$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
		$vm = [xml]$xenEnum.ReadItem()
		$vmUuid = $vm.Xen_ComputerSystem.Name
	} else {
		# $vm = [xml]($objSession.Get($createVmResult.DefineSystem_OUTPUT.ResultingSystem.outerXML))
		$filter = "SELECT * FROM Xen_ComputerSystem where ElementName like `"%Automated Test Core VM%`""
		$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
		$vm = [xml]$xenEnum.ReadItem()
		$vmUuid = $vm.Xen_ComputerSystem.Name
	}
	
	# the VM is copied but not running
	# Boot the VM
	$changeStateOn = ChangeVmState $vmUuid "2" # 2 = enable or PowerOn
	
	if ($changeStateOn.RequestStateChange_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($changeStateOn.RequestStateChange_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_SystemStateChangeJob.PercentComplete
			sleep 2
		}
	}	

	
	# check that Xen Tools are running in the VM before continuing
	$filter = "SELECT * FROM Xen_ComputerSystem where Name like `"%$vmUuid%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
	$xenEnumXml = [xml]$xenEnum.ReadItem()
	
	while ($xenenumxml.Xen_ComputerSystem.AvailableRequestedStates -notcontains "4") {
		sleep 2
		$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
		$xenEnumXml = [xml]$xenEnum.ReadItem()
	}
	Remove-Variable -Name xenEnumXml, xenEnum, filter
	
	# Give some status
	"$vmUuid is running and the VM tools are up"
	
	# Give a breather for the VM OS to settle a bit
	sleep 15
	
	# Clean shutdown the Vm
	$changeStateOff = ChangeVmState $vmUuid "4" # 4 = ACPI shutdown, requires that the vm tools is running
	
	if ($changestateoff.RequestStateChange_OUTPUT.ReturnValue -ne 0) {
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($changestateoff.RequestStateChange_OUTPUT.job.outerxml)
			$jobPercentComplete = $jobResult.Xen_SystemStateChangeJob.PercentComplete
			sleep 3
		}
	}
	
	# Give a breather for the VM OS to settle a bit
	sleep 15
	
	# Find the VDI(s) of the VM - always assume there is more than 1
	$filter = "SELECT * FROM Xen_DiskSettingData where InstanceID like `"Xen:$vmUuid%`""
	# $xenEnumXml = EnumClassFilter "Xen_DiskSettingData" $filter
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData", $filter, $dialect)
	$vmVbd = @()
	while (!$xenEnum.AtEndOfStream) {
		$elementRec = $xenEnum.ReadItem()
		$vmVbd += [xml]$elementRec
	}
	
	# Load the BITS module (PowerShell 2.0 is required)
	Import-Module BitsTransfer # -Verbose
	# By default the TransferVM component uses the same external virtual network as the 
	# management interface and therefore requires that DHCP be running on this
	# segment to obtain an IP address.
	
	# Map the destination
	net use Q: $targetShare /user:$user $pass
	
	# Begin the export of the VDI to the share (sequentially)
	# The ResourceType of 19 is to focus only on the virtual disks, not all VBDs (as I could get an empty CD drive by accident)
	foreach ($element in $vmVbd) {
		if ($element.Xen_DiskSettingData.ResourceType -eq 19){
			# Parse the VBD into the Xen_DiskImage information needed
			$dsdHostResource = $element.Xen_DiskSettingData.HostResource
			$vDisk = @()
			$vDiskHash = @{}
			$vDisk = $dsdHostResource.split(",")
			foreach ($i in $vDisk) {
				$tempArr = $i.Split("=")
				$vdiskHash.Add($tempArr[0], $tempArr[1])
			}
			
			$deviceID = $vDiskHash.DeviceID.Replace('"','')
			$systemName = $vDiskHash.SystemName.Replace('"','')
			$systemCreationClassName = $vDiskHash.SystemCreationClassName.Replace('"','')
			$creationClassName = $vDiskHash.'root/cimv2:Xen_DiskImage.CreationClassName'.Replace('"','')
			
			$vdi = @"
			<Xen_DiskImage>
				<DeviceID>$DeviceID</DeviceID>
				<CreationClassName>$CreationClassName</CreationClassName>
				<SystemCreationClassName>$SystemCreationClassName</SystemCreationClassName>
				<SystemName>$SystemName</SystemName>
			</Xen_DiskImage>
"@
			
			$transferVM = ConnectToDiskImage $vdi "bits" "0"
			
			
			# The Address on parent is put here to hopefully put them in the right boot order
	#		$destination = "Q:\" + $element.Xen_DiskSettingData.AddressOnParent + "." + $element.Xen_DiskSettingData.HostExtentName + ".vhd"
			$destination = "F:\Test3\" + $element.Xen_DiskSettingData.AddressOnParent + "." + $element.Xen_DiskSettingData.HostExtentName + ".vhd"
			$source = $transferVm.Xen_ConnectToDiskImageJob.TargetURI
            
			$transferJob = Start-BitsTransfer -Source $source -destination $destination -Asynchronous -DisplayName XenExportImport
			"-Source $source -destination $destination"
            
			while ($jobStatus.JobState -ne "transferred"){
				$jobStatus = Get-BitsTransfer -JobId $transferJob.JobId
                Write-Progress -activity "BITS Transfer Download" -status "copying.. " -PercentComplete ((($jobstatus.BytesTransferred / 1Mb) / ($jobStatus.BytesTotal / 1Mb)) * 100)
				if ($jobStatus.JobState -eq "TransientError") {
					$jobstatus
					"download is paused due to TransientError from BITS"
					pause
                    Resume-BitsTransfer -BitsJob $transferJob
				}
				sleep 10
			}

            Write-Progress -activity "BITS Transfer Download" -status "copying.. " -completed
			
        	$bitsTime = $jobstatus.TransferCompletionTime - $jobstatus.CreationTime
            $bitsTime.TotalSeconds.ToString() + " Seconds"

			Complete-BitsTransfer $transferJob.JobId
			$vdiDisconnect = DisconnectFromDiskImage $transferVM.Xen_ConnectToDiskImageJob.ConnectionHandle
			
			# check for a job status of finished
			$jobPercentComplete = 0
			while ($jobPercentComplete -ne 100) {
				$jobResult = [xml]$objSession.Get($vdiDisconnect.DisconnectFromDiskImage_OUTPUT.Job.outerxml)
				$jobPercentComplete = $jobresult.Xen_DisconnectFromDiskImageJob.PercentComplete
				sleep 3
			}

		}
	}
	
	# Gather the settings of the VM
	$filter = "SELECT * FROM Xen_ComputerSystemSettingData where InstanceID like `"%$vmUuid%`""
	$vmData = [xml](EnumClassFilter "Xen_ComputerSystemSettingData" $filter)
	$vmData
	
	$filter = "SELECT * FROM Xen_MemorySettingData where InstanceID like `"Xen:$vmUuid\Memory%`""
	$vmMem = [xml](EnumClassFilter "Xen_MemorySettingData" $filter)
	$vmMem
	
	$filter = "SELECT * FROM Xen_ProcessorSettingData where InstanceID like `"Xen:$vmUuid\Processor%`""
	$vmProc = [xml](EnumClassFilter "Xen_ProcessorSettingData" $filter)
	$vmProc
	
	$filter = "SELECT * FROM Xen_NetworkPortSettingData where InstanceID like `"Xen:$vmUuid\%`""
	$vmNet = [xml](EnumClassFilter "Xen_NetworkPortSettingData" $filter)
	$vmNet
	
	
	# Create a second VM that is a duplicate of the first
	$vm2 = CreateVm "Automated Test VM 2" $vmMem.Xen_MemorySettingData.VirtualQuantity $vmProc.Xen_ProcessorSettingData.VirtualQuantity $vmData.Xen_ComputerSystemSettingData.VirtualSystemType
	
	$vm2Uuid = $vm2.Xen_ComputerSystem.Name
	$vm2Uuid
	
	# Begin the Import of the VHD files to VDI files
	# Find the VHD files on the share
#	$vhdFiles = Get-ChildItem Q:
	$vhdFiles = Get-ChildItem "F:\Test3"
	
	foreach ($element in $vhdFiles) {
		# Create a VDI
		$fileSplit = $element.Name.Split(".")
			
		$newVdi = CreateVmVdi $element.BaseName $fileSplit[0] (($element.Length/1024)/1024) $vm2.Xen_ComputerSystem.Name $localSr.Xen_StoragePool.PoolID
		
		if ($newVdi.AddResourceSetting_OUTPUT.ReturnValue -ne 0) {
			# check for a job status of finished
			$jobPercentComplete = 0
			while ($jobPercentComplete -ne 100) {
				$jobResult = [xml]$objSession.Get($newVdi.AddResourceSetting_OUTPUT.Job.outerxml)
				$jobPercentComplete = $jobresult.Xen_DisconnectFromDiskImageJob.PercentComplete
				sleep 1
			}
		}
		
		$vm2Vdi = [xml]($objSession.Get($newvdi.AddResourceSetting_OUTPUT.ResultingResourceSetting.outerXML))
		
		# Parse the VBD into the Xen_DiskImage information needed
		$dsdHostResource = $vm2vdi.Xen_DiskSettingData.HostResource
		$vDisk = @()
		$vDiskHash = @{}
		$vDisk = $dsdHostResource.split(",")
		foreach ($i in $vDisk) {
			$tempArr = $i.Split("=")
			$vdiskHash.Add($tempArr[0], $tempArr[1])
		}
		$deviceID = $vDiskHash.DeviceID.Replace('"','')
		$systemName = $vDiskHash.SystemName.Replace('"','')
		$systemCreationClassName = $vDiskHash.SystemCreationClassName.Replace('"','')
		$creationClassName = $vDiskHash.'root/cimv2:Xen_DiskImage.CreationClassName'.Replace('"','')
		
		$vdi = @"
		<Xen_DiskImage>
			<DeviceID>$DeviceID</DeviceID>
			<CreationClassName>$CreationClassName</CreationClassName>
			<SystemCreationClassName>$SystemCreationClassName</SystemCreationClassName>
			<SystemName>$SystemName</SystemName>
		</Xen_DiskImage>
"@
		$transferVm = ConnectToDiskImage $vdi "bits" "0"
	#	$source =  "Q:\" + $element.Name
		$source =  "F:\Test3\" + $element.Name

		$transferJob = Start-BitsTransfer -Source $source -destination $transferVm.Xen_ConnectToDiskImageJob.TargetURI -Asynchronous -DisplayName XenVdiTransfer -TransferType Upload
		"-Source " + $source + " -destination " + $transferVm.Xen_ConnectToDiskImageJob.TargetURI
		while ($jobStatus.JobState -ne "transferred"){
		$jobStatus = Get-BitsTransfer -JobId $transferJob.JobId
		# [string]($jobstatus.BytesTransferred / 1Mb) + " / " + [string]($jobStatus.BytesTotal / 1Mb) + " = " + [string]"{0,-10:p}" -f ((($jobstatus.BytesTransferred / 1Mb) / ($jobStatus.BytesTotal / 1Mb)))
                Write-Progress -activity "BITS Transfer Upload" -status "copying.. " -PercentComplete ((($jobstatus.BytesTransferred / 1Mb) / ($jobStatus.BytesTotal / 1Mb)) * 100)
				if ($jobStatus.JobState -eq "TransientError") {
					$jobstatus
					"upload is paused due to TransientError from BITS"
					pause
                    Resume-BitsTransfer -BitsJob $transferJob
				}
				sleep 10
			}

            Write-Progress -activity "BITS Transfer Upload" -status "copying.. " -completed
			
    	$bitsTime = $jobstatus.TransferCompletionTime - $jobstatus.CreationTime
        $bitsTime.TotalSeconds.ToString() + " Seconds"
	
        
		Complete-BitsTransfer $transferJob.JobId
        
		$vdiDisconnect = DisconnectFromDiskImage $transferVm.Xen_ConnectToDiskImageJob.ConnectionHandle
		
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($vdiDisconnect.DisconnectFromDiskImage_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_DisconnectFromDiskImageJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}

	} 
	
	# the VM is created but not running nor tested, it is only assembled
	# Boot the VM
	$changeStateOn = ChangeVmState $vm2Uuid "2" # 2 = enable or PowerOn
	
	if ($changeStateOn.RequestStateChange_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($changeStateOn.RequestStateChange_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_SystemStateChangeJob.PercentComplete
			$jobPercentComplete
			sleep 2
		}
	}	

	
	# check that Xen Tools are running in the VM before continuing
	$filter = "SELECT * FROM Xen_ComputerSystem where Name like `"%$vm2Uuid%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
	$xenEnumXml = [xml]$xenEnum.ReadItem()
	
	while ($xenenumxml.Xen_ComputerSystem.AvailableRequestedStates -notcontains "4") {
		# Assuming that the tools are not running becuase this information is not reported xe stores this as os-version
	# this is not reliable and needs a better way to determine the tools are running
		sleep 2
		$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
		$xenEnumXml = [xml]$xenEnum.ReadItem()
	}
	Remove-Variable -Name xenEnumXml, xenEnum, filter
	
	# Give a status
	"$vm2Uuid is running and the VM tools are up"
	
	# Give a breather for the VM OS to settle a bit
	sleep 15
	
	# Clean shutdown the Vm
	$changeStateOff = ChangeVmState $vm2Uuid "4" # 4 = ACPI shutdown, requires that the vm tools is running
	
	if ($changestateoff.RequestStateChange_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($changestateoff.RequestStateChange_OUTPUT.job.outerxml)
			$jobPercentComplete = $jobResult.Xen_SystemStateChangeJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}
	}
	
	# Give a breather for the VM OS to settle a bit
	sleep 15
	
	# The Clean Up
	"Cleaning up the execution"
	
	# Delete the VHD files
	foreach ($element in $vhdFiles) {
#		Remove-Item ("Q:\" + $element)
		Remove-Item ("F:\Test3\" + $element)
	}
	
	# Remove the mapped destination
	net use Q: /delete
	
	# Delete the VMs
	$destroyVm = DestroyVM $vmUuid
	
	if ($destroyVm.DestroySystem_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($destroyVm.DestroySystem_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobResult.Xen_VirtualSystemManagementServiceJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}
	}

	DestroyVM $vm2Uuid
	
	if ($destroyVm.DestroySystem_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($destroyVm.DestroySystem_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobResult.Xen_VirtualSystemManagementServiceJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}
	}

	# Clean up after myself
	JobCleanUp
	
$aLoop++;$aLoop
}
