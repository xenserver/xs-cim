# This was origionally developed by:
# Brian Ehlert Senior Test Engineer, Citrix Labs, Redmond WA, USA
# for the XenServer CIM interface.
# This PowerShell script is executed against the XenServer CIM WS-Management interface
#
# Note: the Windows Managment Framework 2.0 or higher is required to be installed where this 
# script runs to properly handle WinRM returns and calls due to updates to the wsman interface.
#
# This is an automated scipt that is written to simulate
# the actions of creating a new VM and installing an
# operating system in the VM, then converting the VM
# to a XenServer template (golden image)
#
# This script performs the following actions:
# Create a new VM without using a template, adding a 
# virtual disk to the VM, creating an ISO Storage Repository
# that is a CIFS/SMB share, attaching an ISO to the VM, 
# booting the VM, connecting to the console, allowing the 
# installation to be manually performed, power off the vm,
# detach the ISO, then convert the VM to a XenServer template.
# 
# A CIFS/SMB share is required with installation media (ISOs)

# dot source reference the script that contains all of the helper functions
# The full path needs to be sent if the prompt run location is the same as the script location.
. ".\XenWsMan.Functions.ps1"

# The variables
$xenServer = "10.60.2.56"
$userName = "root"
$password = "K33p0ut"

$newSrName = "Test ISO SR"
$location = "//10.60.2.133/NFS"		# location=//reddfs/images
$isoPath = ""						# iso_path=media/NT/Win7RTM/VL
$cifsUser = "localhost/administrator"
$cifsPass = "K33p0ut"

# $xenServer = $Args[0]
# $userName = $Args[1]
# $password = $Args[2]
# $newSrName = $Args[3]
# $location = $Args[4]
# $isoPath = $Args[5]
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

# Get the host information back
$xenHost = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_HostComputerSystem")
$xenHostXml = [xml]($xenHost.ReadItem())

# Begin the tasks, now that the session is established.

# Create the new HVM VM from scratch
$createVmResult = CreateVm "Demo VM" 768 2 "HVM"

# Get the VM object
$vm = [xml]$objSession.Get($createvmresult.DefineSystem_OUTPUT.ResultingSystem.outerxml)

# Find the Storage Repositories that a VDI can be stored on
$xenEnumXml = EnumClass "Xen_StoragePool"
$vdiSr = @()
foreach ($element in $xenEnumXml) {
	$storage = [xml]$element
	if (($storage.Xen_StoragePool.ResourceSubType -notlike "udev") -and ($storage.Xen_StoragePool.ResourceSubType -notlike "iso")) {
	$vdiSr += $storage
	}
}
Remove-Variable -Name xenEnumXml

# Randomly select one of the suitable SRs
$sr = $vdiSr | Get-Random

# Create an 16Gb virtual disk attached to a VM
$createVdiResult = CreateVmVdi "Demo Vm Disk" "0" 16 $vm.Xen_ComputerSystem.Name $sr.Xen_StoragePool.PoolID

# Get the VBD object - this is depricated due to a method change
# $vbd = [xml]$objSession.Get($createVdiResult.AddResourceSetting_OUTPUT.ResultingResourceSetting.outerxml)
# Translate the VBD to a VDI UUID
# $vdiUuid = $vbd.Xen_DiskSettingData.HostExtentName

# Get the VDI object
$vdi = [xml]$objSession.Get($createvdiresult.AddResourceSetting_OUTPUT.ResultingResourceSetting.OuterXml)

# Attach an ISO SR
$addIsoSrResult = AddCifsIsoSr $newSrName $location $isoPath $cifsUser $cifsPass

# Find all the disk images catalogued by the new ISO SR
# Get the SR object
$isoSr = [xml]$objSession.Get($addIsoSrResult.CreateStoragePool_OUTPUT.Pool.OuterXml)

# Give XenServer a chance to query the SR
sleep 3

# Find the Server 2008 R2 ISO
$filter = "SELECT * FROM Xen_DiskImage where ElementName like `"%PEx86%`""
$installIso = [xml](EnumClassFilter "Xen_DiskImage" $filter)

# Attach the ISO to the VM
$attachIsoResult = AttachIso $vm.Xen_ComputerSystem.Name $installIso

# Power On the VM
$changeStateOnResult = ChangeVmState $vm.Xen_ComputerSystem.Name "2"   # 2 = enable or PowerOn

#######
# This section still needs work as the helper method is not quite right.
#######
# Connect to the VM console
#	$consoleConnectionResult = ConnectToVmConsole $xenServer $vm.Xen_ComputerSystem.Name $username $password

# manually perform the installation / test the buttons
sleep 60

# PowerOff the VM
# $changeStateOff = ChangeVmState $vm.Xen_ComputerSystem.Name "4"   # 4 = ACPI shutdown, requires that the vm tools is running
$changeStateOff = ChangeVmState $vm.Xen_ComputerSystem.Name "32768"   # 32768 ("Hard Shutdown") - Equal to setting power switch to off

# Detach the ISO
$detachIsoResult = DetachIso $vm.Xen_ComputerSystem.Name
	
# Remove the ISO SR
$removeIsoSrResult = DeleteSr $isoSr

# Turn VM into XenTemplate - this is not an action that SCVMM will be doing that we know of
$convertToTemplateResult = VmToTemplate $vm.Xen_ComputerSystem.Name

JobCleanUp



