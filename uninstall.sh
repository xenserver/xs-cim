#!/bin/bash

# Uninstall Script for the xs-cim-cmpi rpm

CIM_PROVIDER="/opt/openpegasus/bin/cimprovider"
CIM_WATCH_SWITCH="/opt/xs-cim-cmpi/watchdog-off"

PEGASUS_HOME="/opt/openpegasus"
REGISTER="$PEGASUS_HOME/share/provider-register.sh"

sharedir="/opt/xs-cim-cmpi/share/xs-cim-cmpi"
cmpi_libs="/opt/xs-cim-cmpi/lib/cmpi"

XEN_MOFS=$(ls $sharedir/Xen_*.mof)


XEN_REGS="$sharedir/Xen_DefaultNamespace.regs"
INTEROP_MOFS="$sharedir/Xen_RegisteredProfiles.mof"
INTEROP_REGS="$sharedir/Xen_InteropNamespace.regs"


if [ -x  $REGISTER ]; then
  "$REGISTER" -d "root/cimv2" -r "$XEN_REGS" -m $XEN_MOFS > /dev/null || true
  "$REGISTER" -d "root/cimv2" -r "$INTEROP_REGS" -m $INTEROP_MOFS > /dev/null || true
fi

$CIM_PROVIDER -l | xargs -ixx $CIM_PROVIDER -r -m xx

cd $cmpi_libs

for file in *; do
  rm -f $PEGASUS_HOME/providers/$file
done

rm -f $PEGASUS_HOME/repository/root#cimv2/instances/Xen_*


if [ $(pidof cimserver) != "" ]; then
  touch $CIM_WATCH_SWITCH
  /etc/init.d/openpegasus restart
  echo "CIM Server re-started."
  rm $CIM_WATCH_SWITCH
fi

# Delete iptables rules
/opt/xs-cim-cmpi/share/xs-cim-cmpi/cimiptables -c

# Stop the KVP service
/etc/init.d/kvpd stop

