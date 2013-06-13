# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the XenServer-CIM Project.
# This PowerShell script is executed against the XenServer CIM WSMAN interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# This script is designed to simulate the actions of exporting
# a VM from a host to a folder and then importing that
# same VM back to the host again.
# This is fundamentally similar to a Hyper-V Export and Import 
#
# dot source reference the script that contains all of the helper functions
# The full path needs to be sent if the prompt run location is NOT the same as the script location.
# . ".\XenWsMan.Functions.ps1"
# . "F:\brianeh.CITRITE\Documents\Amano\AmanoPoShPegasus\XenWsMan.Functions.ps1"

# The variables
$xenServer = "10.60.2.56"  # The XenServer exported from
$userName = "root"
$password = "K33p0ut"
$xenServer2 = "10.60.2.56"  # The XenServer imported to
$userName2 = "root"
$password2 = "K33p0ut"
$downloadPath = "F:\Test2\"
$testVm = "DemoLinuxVM"

# Constants
$dialect = "http://schemas.microsoft.com/wbem/wsman/1/WQL"

# Functions
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
### End ##########################################

function ExportSnapshotTree {
	param ($vmName, $xenServer, $downloadPath)
	
	$actionUri = EndPointReference "Xen_VirtualSystemSnapshotService"

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

	$startExport = [xml]$objSession.Invoke("StartSnapshotForestExport", $actionURI, $parameters)
	
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
		$jobResult = [xml]$objSession.Get($startExport.StartSnapshotForestExport_OUTPUT.job.outerxml)
	}

	$connectionHandle = $jobResult.Xen_StartSnapshotForestExportJob.ExportConnectionHandle
 	$metadataUri = $jobResult.Xen_StartSnapshotForestExportJob.MetadataURI

	# Download the Metadata file (this is an HTTP file download)
 	$downloadClient = New-Object System.Net.WebClient
 	$downloadClient.DownloadFile($metadataUri,($downloadPath + "export.xva"))

	# Capture the virtual disk image URIs to pass to BITS
	$vDisksToDownload = @()
	$vDisksToDownload = $jobResult.Xen_StartSnapshotForestExportJob.DiskImageURIs

	# Begin the download of all virtual disks at the same time
	
	foreach ($element in $vDisksToDownload) {
		$file = $element.Split('/')
		$file = $file[($file.length - 1)]
		$destination = $downloadPath + $file
        
		$transferJob = Start-BitsTransfer -Source $element -destination $destination -Asynchronous -DisplayName SnapshotDiskExportAsync
		"-Source $element -destination $destination"
    }

	$bitsJobs = Get-BitsTransfer -Name "SnapshotDiskExportAsync"

	while ($bitsJobs.Count -ne $null) {
		foreach ($element in $bitsJobs) {
# 			while (($element.JobState -eq "Transferring") -or ($element.JobState -eq "Connecting")) `
#  				{ sleep 5;} # Poll for status, sleep for 5 seconds, or perform an action.

			switch($element.JobState)
			{
				"Connecting" { Write-Host " Connecting " }
				"Transferring" { Write-Host "$element.JobId has progressed to " + ((($element.BytesTransferred / 1Mb) / ($element.BytesTotal / 1Mb)) * 100) + " Percent Complete" }
				"Transferred" {Complete-BitsTransfer -BitsJob $element}
				"Error" {
					$transferJob | Format-List
					"BITS Error Condition: " + $transferJob.ErrorCondition
					"BITS Error Description: " + $transferJob.ErrorDescription
					"BITS Error Context: " + $transferJob.ErrorContext
					"BITS Error Context Description: " + $transferJob.ErrorContextDescription
					pause
					Remove-BitsTransfer $transferJob
					}
				"TransientError" {
					$element | Format-List
					pause
					# Resume-BitsTransfer $element # This should attempt a resume-bitstransfer but that is currently not supported with the TransferVM.
					Remove-BitsTransfer $element
					} 
			}
		}
	sleep 10
	$bitsJobs = Get-BitsTransfer -Name "SnapshotDiskExportAsync" -ErrorAction SilentlyContinue
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
	$endExport = [xml]$objSession.Invoke("EndSnapshotForestExport", $actionURI, $parameters)
	
	# Check for Job Status
	if ($endExport.EndSnapshotForestExport_OUTPUT.ReturnValue -ne 0) {
	$jobPercentComplete = 0
	while ($jobPercentComplete -ne 100) {
		$jobResult = [xml]$objSession.Get($endExport.EndSnapshotForestExport_OUTPUT.job.outerxml)
		$jobPercentComplete = $jobresult.Xen_EndSnapshotForestExportJob.PercentComplete
		sleep 10
		}
	}
	
	return $endExport
}
### End ##########################################

function ImportSnapshotTree {
	param ($targetSR, $xenServer, $downloadPath)
	
	if ($targetSR.GetType().Name -ne "XmlDocument") {
		$targetSR = [xml]$targetSR
	}

	$actionUri = EndPointReference "Xen_VirtualSystemSnapshotService"

	# Import the Metadata file that describes the VM snapshot tree
	# Find the Export.xva file and copy it in
	$importFiles = Get-ChildItem $downloadPath
	foreach ($element in $importFiles) {
		if ($element.Extension -like ".xva") {
			# Create a VDI
			$createMetadataVdi = CreateVdi $element.BaseName 1 $targetSR.Xen_StoragePool.PoolID
			$metadataVdi = [xml]$objSession.Get($createMetadataVdi.CreateDiskImage_OUTPUT.ResultingDiskImage.outerxml)

			# Copy the export.xva to the VDI endpoint
			$transferVm = ConnectToDiskImage $metadataVdi "bits" "0"
			$source =  $downloadPath + $element.Name
			# This is a RAW disk copy using BITS
			$transferJob = Start-BitsTransfer -Source $source -destination $transferVm.Xen_ConnectToDiskImageJob.TargetURI -Asynchronous -DisplayName ImportSnapshotTreeMetadataUpload -TransferType Upload
			"-Source " + $source + " -destination " + $transferVm.Xen_ConnectToDiskImageJob.TargetURI

			while (($transferJob.JobState -eq "Transferring") -or ($transferJob.JobState -eq "Connecting"))
				{ sleep 5 }
			
			switch($transferJob.JobState)
			{
				"Connecting" { Write-Host " Connecting " }
				"Transferring" { Write-Host "$transferJob.JobId has progressed to " + ((($transferJob.BytesTransferred / 1Mb) / ($transferJob.BytesTotal / 1Mb)) * 100) + " Percent Complete" }
				"Transferred" {Complete-BitsTransfer -BitsJob $transferJob}
				"Error" {
					$transferJob | Format-List
					"BITS Error Condition: " + $transferJob.ErrorCondition
					"BITS Error Description: " + $transferJob.ErrorDescription
					"BITS Error Context: " + $transferJob.ErrorContext
					"BITS Error Context Description: " + $transferJob.ErrorContextDescription
					pause
					Remove-BitsTransfer $transferJob
					}
				"TransientError" {
					$transferJob | Format-List
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

	$prepareImport = [xml]$objSession.Invoke("PrepareSnapshotForestImport", $actionURI, $parameters)
	
	# Start the Import
	$importContext = $prepareImport.PrepareSnapshotForestImport_OUTPUT.ImportContext
	$InstanceID = $targetSr.Xen_StoragePool.InstanceID

	# Set the namespace once beore entering the loop
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

		$diskImport = [xml]$objSession.Invoke("CreateNextDiskInImportSequence", $actionURI, $parameters)
		
		$diskImport.CreateNextDiskInImportSequence_OUTPUT
		
		$diskToImport = $diskImport.CreateNextDiskInImportSequence_OUTPUT.OldDiskID
		$importContext = $diskImport.CreateNextDiskInImportSequence_OUTPUT.ImportContext
		$diskImageMap = $diskImport.CreateNextDiskInImportSequence_OUTPUT.DiskImageMap
	
		# little loop above until the parameter OldDiskID is returned if OldDiskID is present then go below
		if ((Select-Xml -Xml $diskImport -Xpath "//n1:OldDiskID" -Namespace $namespace) -ne $null) {
			foreach ($element in $importFiles) {
				if ($element.Name -match $diskToImport) {
					
					$newVdi = [xml]$objSession.Get($diskImport.CreateNextDiskInImportSequence_OUTPUT.NewDiskImage.outerxml)
					
					$transferVm = ConnectToDiskImage $newVdi "bits" "0"
					
					$source =  $downloadPath + $element.Name
					$destination = $transferVm.Xen_ConnectToDiskImageJob.TargetURI + ".vhd"
					 
					$transferJob = Start-BitsTransfer -Source $source -destination $destination -Asynchronous -DisplayName ImportSnapshotVirtualDiskUpload -TransferType Upload
					"-Source " + $source + " -destination " + $destination
					
					while (($transferJob.JobState -eq "Transferring") -or ($transferJob.JobState -eq "Connecting"))
						{ sleep 5 }
					
					switch($transferJob.JobState)
					{
						"Connecting" { Write-Host " Connecting " }
						"Transferring" { Write-Host "$transferJob.JobId has progressed to " + ((($transferJob.BytesTransferred / 1Mb) / ($transferJob.BytesTotal / 1Mb)) * 100) + " Percent Complete" }
						"Transferred" {Complete-BitsTransfer -BitsJob $transferJob}
						"Error" {
							$transferJob | Format-List
							"BITS Error Condition: " + $transferJob.ErrorCondition
							"BITS Error Description: " + $transferJob.ErrorDescription
							"BITS Error Context: " + $transferJob.ErrorContext
							"BITS Error Context Description: " + $transferJob.ErrorContextDescription
							pause
							Remove-BitsTransfer $transferJob
							}
						"TransientError" {
							$transferJob | Format-List
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

	$importFinalize = [xml]$objSession.Invoke("FinalizeSnapshotForestImport", $actionURI, $parameters)
	
	# Get the imported VM back to pass back out
	$vmImportResult = [xml]$objSession.Get($importFinalize.FinalizeSnapshotForestImport_OUTPUT.VirtualSystem.outerxml)

	return $vmImportResult
}
### End ##########################################


function Pause ($Message="Press any key to continue...")
{
Write-Host -NoNewLine $Message

$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
Write-Host ""
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
#  	$filter = "SELECT * FROM Xen_ComputerSystem where ElementName like `"%$testVm%`""
 	$filter = "SELECT * FROM Xen_ComputerSystem where ElementName = `"$testVm`""
  	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
	# We are only expecting one item back.
	$sourceVm = [xml]$xenEnum.ReadItem()
	$vmName = $sourceVm.Xen_ComputerSystem.Name
	$vmName

# Export the VM
	$exportResult = ExportSnapshotTree $vmName $xenServer $downloadPath

# Create a WSMAN session object with the XenServer2
	$objWsmanAuto = New-Object -ComObject wsman.automation
	# set the connection options of username and password
	$connOptions = $objWsmanAuto.CreateConnectionOptions()
	$connOptions.UserName = $userName2
	$connOptions.Password = $password2
	# set the session flags required for the connection to work
	$iFlags = ($objWsmanAuto.SessionFlagNoEncryption() -bor $objWsmanAuto.SessionFlagUTF8() -bor $objWsmanAuto.SessionFlagUseBasic() -bor $objWsmanAuto.SessionFlagCredUsernamePassword())
	# The target system
	$target = "http://" + $xenServer2 + ":5988"
	# Open the session
	$objSession = $objWsmanAuto.CreateSession($target, $iflags, $connOptions)
	# Increase the timeout to 5 minutes
	$objSession.Timeout = 3000000
	# Identify the interface
	$objSession.Identify()

# Find the Local Storage SR
	$filter = "SELECT * FROM Xen_StoragePool where Name like `"%Local storage%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_StoragePool", $filter, $dialect)
	$localSr = [xml]$xenEnum.ReadItem()

# Import the VM
	$importResult = ImportSnapshotTree $localSr $xenServer2 $downloadPath
	
	