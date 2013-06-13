#!/bin/sh

set -eu

PEGASUS_HOME="/opt/openpegasus"
REGISTER="$PEGASUS_HOME/share/provider-register.sh"

thisdir=$(dirname "$0")
libdir="$thisdir/../lib"
sharedir="$thisdir/xs-cim-cmpi"

XEN_MOFS="@XEN_MOFS@"
XEN_REGS="$sharedir/Xen_DefaultNamespace.regs"
INTEROP_MOFS="$sharedir/Xen_RegisteredProfiles.mof"
INTEROP_REGS="$sharedir/Xen_InteropNamespace.regs"

if ! pidof /opt/openpegasus/bin/cimserver >/dev/null
then
  echo "Saving install of xs-cim-cmpi until next boot."
  cp "$thisdir/67-install-xs-cim-cmpi" "/etc/firstboot.d"
  chmod a+x "/etc/firstboot.d/67-install-xs-cim-cmpi"
  exit 0
fi

"$REGISTER" -n "root/cimv2" -r "$XEN_REGS" -m $XEN_MOFS
"$REGISTER" -n "root/cimv2" -r "$INTEROP_REGS" -m $INTEROP_MOFS

for file in "$libdir/cmpi/"*
do
  ln -s "$file" "$PEGASUS_HOME/providers/"
done

/sbin/ldconfig

# Configure IP tables
/opt/xs-cim-cmpi/share/xs-cim-cmpi/cimiptables

# Start the KVP Daemon in Dom0
chkconfig --add kvpd
/etc/init.d/kvpd start