#!/bin/bash

usage()
{
    echo "wrong usage of script: enable-ssl.sh -d <openwsman_conf_dir> -s <cimstart_script_dir>"
    exit 1
}
enable_non_SSL()
{
    cat >> ${scriptsdir}/cimstart <<EOFC
echo "Starting: openwsmand"
# Do not start the SSL version of openwsmand
# openwsmand -d -S> \$openwsman_ssl_debug_log 2>&1 &
# start the non-SSL version of openwsmand
openwsmand -d> \$openwsman_nonssl_debug_log 2>&1 &
sleep 3
echo "-------------------------------------------------------------------"
ps -eaf | grep openwsmand
ps -eaf | grep sfcbd
echo "-------------------------------------------------------------------"
EOFC
}

enable_SSL()
{
    cat >> ${scriptsdir}/cimstart <<EOFC
echo "Starting: openwsmand"
# start the SSL version of openwsmand
openwsmand -d -S> \$openwsman_ssl_debug_log 2>&1 &
# Do not start the non-SSL version of openwsmand
# openwsmand -d> \$openwsman_nonssl_debug_log 2>&1 &
sleep 3
echo "-------------------------------------------------------------------"
ps -eaf | grep openwsmand
ps -eaf | grep sfcbd
echo "-------------------------------------------------------------------"
EOFC
}

enable_both()
{
    cat >> ${scriptsdir}/cimstart <<EOFC
echo "Starting: openwsmand"
# start the SSL version of openwsmand
openwsmand -d -S> \$openwsman_ssl_debug_log 2>&1 &
# start the non-SSL version of openwsmand
openwsmand -d> \$openwsman_nonssl_debug_log 2>&1 &
sleep 3
echo "-------------------------------------------------------------------"
ps -eaf | grep openwsmand
ps -eaf | grep sfcbd
echo "-------------------------------------------------------------------"
EOFC
}

confdir="/etc/openwsman"
scriptsdir="/usr/sbin"

while getopts "d:s" opt; do
   case $opt in
      d) confdir="${OPTARG}";;
      s) scriptsdir="${OPTARG}";;
      *) usage;;
   esac
done

#openwsman requires libssl.so, which is not on the system
#provide a symbolic link to the version that exists on the system
if ! test -f /lib/libssl.so
then
    echo "create symbolic link for libssl"
    ln -s /lib/libssl.so.6 /lib/libssl.so
fi
   
echo "Do you want to :"
echo "1) Enable only SSL encrypted CIM access"
echo "2) Enable only unencrypted CIM access"
echo "3) Enable both encrypted and unencrypted CIM access"
echo ">"
read -n 1 CHOICE
echo ""
if [ "$CHOICE" == "1" ]; then
    enable_SSL
elif [ "$CHOICE" == "2" ]; then
    enable_non_SSL
elif [ "$CHOICE" == "3" ]; then
    enable_both
fi 
