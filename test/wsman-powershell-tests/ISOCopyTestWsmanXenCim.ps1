# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the Xen-CIM Project.
# This PowerShell script is executed against the XenServer CIM WSMAN interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# This is an automated scipt that is written to simulate
# the actions of importing an ISO to an ISO SR,
# attaching it to a VM, then booting from the ISO, 
# then performing a clean shutdown and clean up.
#
# This script performs the following actions:
# Create a new VM without using a template, create an ISO Storage Repository
# that is a CIFS/SMB share, create a virtual disk on the SR, 
# import an ISO into the virtual disk, attach the ISO to the VM, 
# boot the VM, wait for tools to come up within the ISO image, power off the vm,
# detach the ISO, delete the ISO, then remove the SR from teh XenServer
# 
# A CIFS/SMB share and the xenserver-linuxfixup-disk.iso from XenCenter are required

# dot source reference the script that contains all of the helper functions
# The full path needs to be sent if the prompt run location is the same as the script location.
. ".\XenWsMan.Functions.ps1"
# . "F:\brianeh.CITRITE\Documents\Amano\AmanoPoShPegasus\XenWsMan.Functions.ps1"

# The variables
$xenServer = "192.168.1.39"
$userName = "root"
$password = "K33p0ut"

$newSrName = "Test ISO SR"
$location = "//192.168.104.100/ImportExportISOSR"		# location=//reddfs/images
$isoFolderPath = ""								# iso_path=media/NT/Win7RTM/VL
$cifsUser = "localhost/administrator"
$cifsPass = "K33p0ut"

$loopCount = 1000	# The number of time to loop throught he test for repeated stress.

# $xenServer = $Args[0]
# $userName = $Args[1]
# $password = $Args[2]
# $newSrName = $Args[3]
# $location = $Args[4]
# $isoFolderPath = $Args[5]
# $cifsUser = $Args[6]
# $cifsPass = $Args[7]

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

# Get the sample ISO
if ($Env:PROCESSOR_ARCHITECTURE -eq "AMD64") {
	$isoPath = ${Env:ProgramFiles(x86)} + "\Citrix\XenCenter\Plugins\Citrix\XenAppliance\External Tools"
}
else {
	$isoPath = ${Env:ProgramFiles} + "\Citrix\XenCenter\Plugins\Citrix\XenAppliance\External Tools"
}
$isoFile = Get-ChildItem $isoPath | where {$_.Extension -eq ".iso"}
$isoSizeMb = $isoFile.Length / 1MB

# round up the MB of the VDI we will create
$vdiMb = [math]::round($isoSizeMb) 

# Use a while loop to set the number of cycles and allow a way to break out.	
$aLoop = 0
while ($aLoop -lt $loopCount) {

	# Create the new HVM VM from scratch
	$vm = CreateVm "Demo VM" 768 2 "HVM"
	

	# Add the ISO SR to the XenServer
	$addIsoSrResult = AddCifsIsoSr $newSrName $location $isoFolderPath $cifsUser $cifsPass
	
	if ($addIsoSrResult.CreateStoragePool_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($addIsoSrResult.CreateStoragePool_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_StoragePoolManagementServiceJob.PercentComplete
			$jobPercentComplete
			sleep 3
		}
	}
	
	# Find the ISO SR
    $isoSr = [xml]$objSession.Get($addIsoSrResult.CreateStoragePool_OUTPUT.Pool.outerxml)
	
	# Create a virtual disk image
	$createVdiResult = CreateVdi "Demo Vm ISO" $vdiMb $isosr.Xen_StoragePool.PoolID
	
	if ($createVdiResult.CreateDiskImage_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($createVdiResult.CreateDiskImage_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobResult.Xen_StoragePoolManagementServiceJob.PercentComplete
			sleep 1
		}
	}

	# Get the VDI object back
	$vdi = $objSession.Get($createvdiresult.CreateDiskImage_OUTPUT.ResultingDiskImage.outerxml)

	# Load the BITS module (PowerShell 2.0 is required)
	Import-Module BitsTransfer
	
	# Copy the Test ISO to the SR using BITS and the TransferVM
	$transferVm = ConnectToDiskImage $vdi "bits" "0"
	
	# $transferVm.Xen_ConnectToDiskImageJob
	
	# By default the TransferVM component uses the same external virtual network as the 
	# management interface and therefore requires that DHCP be running on this
	# segment to obtain an IP address.
	
	# No need to test for the status of starting the TransferVm as the helper method does that
	
	$source =  $isofile.FullName
	$transferJob = Start-BitsTransfer -Source $source -destination $transferVm.Xen_ConnectToDiskImageJob.TargetURI -Asynchronous -DisplayName XenISOTransfer -TransferType Upload
	"-Source " + $source + " -destination " + $transferVm.Xen_ConnectToDiskImageJob.TargetURI
	
	while ($jobStatus.JobState -ne "transferred"){
		$jobStatus = Get-BitsTransfer -JobId $transferJob.JobId
		# $jobstatus.BytesTransferred.ToString() + " / " + $jobStatus.BytesTotal.ToString() + " = " + (($jobstatus.BytesTransferred / $jobStatus.BytesTotal)*100) + "%"
        Write-Progress -activity "BITS Transfer Upload" -status "copying.. " -PercentComplete ((($jobstatus.BytesTransferred / 1Mb) / ($jobStatus.BytesTotal / 1Mb)) * 100)
            if ($jobStatus.JobState -eq "TransientError") {
			 $jobstatus
			 "upload is paused due to TransientError from BITS"
			 pause
             Resume-BitsTransfer -BitsJob $transferJob
			}
	sleep 1
	}
	
    Write-Progress -activity "BITS Transfer Upload" -status "copying.. " -completed
    
	$bitsTime = $jobstatus.TransferCompletionTime - $jobstatus.CreationTime
    $bitsTime.TotalSeconds.ToString() + " Seconds"
	
	Complete-BitsTransfer $transferJob.JobID
    
	$vdiDisconnect = DisconnectFromDiskImage $transferVm.Xen_ConnectToDiskImageJob.ConnectionHandle
	$jobPercentComplete = 0
	while ($jobPercentComplete -ne 100) {
		$jobResult = [xml]$objSession.Get($vdiDisconnect.DisconnectFromDiskImage_OUTPUT.Job.outerxml)
		$jobPercentComplete = $jobresult.Xen_DisconnectFromDiskImageJob.PercentComplete
		sleep 3
	}

	# Attach the ISO to the VM
	$attachIsoResult = AttachIso $vm.Xen_ComputerSystem.Name $vdi
		
	# Power On the VM
	$changeStateOnResult = ChangeVmState $vm.Xen_ComputerSystem.Name "2"   # 2 = enable or PowerOn
    
	if ($changeStateOnResult.RequestStateChange_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($changeStateOnResult.RequestStateChange_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_SystemStateChangeJob.PercentComplete
			# $jobPercentComplete
			sleep 2
		}
	}	
	
	# Wait for the VM to automatically power itself back off.
	$vmName = $vm.Xen_ComputerSystem.Name
	$filter = "SELECT * FROM Xen_ComputerSystem where Name like `"%$vmName%`""
	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
	$xenEnumXml = [xml]$xenEnum.ReadItem()
	
	while ($xenenumxml.Xen_ComputerSystem.Status -ne "Stopped") {
		sleep 4
		$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
		$xenEnumXml = [xml]$xenEnum.ReadItem()
		# $xenenumxml.Xen_ComputerSystem.Status
	}		
	
	# Detach the ISO
	$detachIsoResult = DetachIso $vmName

	if ($detachIsoResult.ModifyResourceSettings_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($detachIsoResult.ModifyResourceSettings_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_SystemModifyResourcesJob.PercentComplete
			# $jobPercentComplete
			sleep 3
		}
	}
	
	# Delete the Test ISO from the SR
	$deleteVdiResult = DeleteVdi $vdi

	if ($deleteVdiResult.DeleteDiskImage_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($deleteVdiResult.DeleteDiskImage_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_StoragePoolManagementServiceJob.PercentComplete
			# $jobPercentComplete
			sleep 3
		}
	}
	
	# Remove the ISO SR
	$removeIsoSrResult = DeleteSr $isoSr

	if ($removeIsoSrResult.DeleteResourcePool_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($removeIsoSrResult.DeleteResourcePool_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_StoragePoolManagementServiceJob.PercentComplete
			# $jobPercentComplete
			sleep 3
		}
	} 
	
	# Delete the VM
	$destroyVmResult = DestroyVM $vm.Xen_ComputerSystem.Name
	
	if ($destroyVmResult.DestroySystem_OUTPUT.ReturnValue -ne 0) {
		# check for a job status of finished
		$jobPercentComplete = 0
		while ($jobPercentComplete -ne 100) {
			$jobResult = [xml]$objSession.Get($destroyVmResult.DestroySystem_OUTPUT.Job.outerxml)
			$jobPercentComplete = $jobresult.Xen_VirtualSystemManagementServiceJob.PercentComplete
			# $jobPercentComplete
			sleep 3
		}
	}
	
	# Clean up the jobs so not to cause problems by filling the job queue
	JobCleanUp
		
$aLoop++;$aLoop
sleep 5
}





