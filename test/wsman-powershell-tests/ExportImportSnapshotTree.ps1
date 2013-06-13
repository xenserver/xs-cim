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
. "F:\brianeh.CITRITE\Documents\Amano\AmanoPoShPegasus\XenWsMan.Functions.ps1"

# The variables
$xenServer = "192.168.1.29"  # The XenServer exported from
# $xenServer = "10.60.2.56"  # The XenServer exported from
$userName = "root"
$password = "K33p0ut"
$xenServer2 = "192.168.1.29"  # The XenServer imported to
# $xenServer2 = "10.60.2.56"  # The XenServer imported to
$userName2 = "root"
$password2 = "K33p0ut"
$downloadPath = "F:\Test\"
$testVm = "DemoLinuxVM"

# Constants
$dialect = "http://schemas.microsoft.com/wbem/wsman/1/WQL"

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
 	$filter = "SELECT * FROM Xen_ComputerSystem where ElementName = `"$testVm`""
  	$xenEnum = $objSession.Enumerate("http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem", $filter, $dialect)
	# We are only expecting one item back.
	$sourceVm = [xml]$xenEnum.ReadItem()
	$vmName = $sourceVm.Xen_ComputerSystem.Name

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
	
	