#!/bin/sh
# ======================================
# Install SFCB, openwsman, XenServer-CIM
# Copyright Citrix Systems Inc 2008
#=======================================
usage()
{
    echo "install.sh <-u for uninstall>"
}

add_library_path_to_profile()
{
    echo "LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/usr/lib/cmpi" >> ~/.bash_profile
    echo "export LD_LIBRARY_PATH" >> ~/.bash_profile
}

check_library_path()
{
    cmpi_path=`echo $LD_LIBRARY_PATH | grep -i "/usr/lib/cmpi"`
    if test -z $cmpi_path
    then
       add_library_path_to_profile
    fi
}

clear
while getopts "u" opt; 
do
    case $opt in
        #u) uninstall="${OPTARG}";;
        u)uninstall="true";;
        *) usage;;
    esac
done

if [ "${uninstall}" == "true" ]; 
then
    echo 'UNINSTALLING XENSERVER-CIM'
    rpm -e xs-cim-cmpi
    rpm -e tog-pegasus
    rpm -e bind-utils
    rpm -e bind-libs
else
    echo "Checking for previous install of XenServer-CIM...."

    exists=`rpm -q xen-cim-cmpi | grep -i "xen-cim-cmpi-1.5"`
    if [ -z $exists ]
    then
        echo "No Previous installation found..."
    else
        echo -e "\033[1mPrevious versions of package exists. Installation cannot continue.\033[0m"
        echo "Installed CIM package versions..."
        cimver
        exit 0
    fi

    # no previous versions istall, proceed
    echo "INSTALLING XENSERVER-CIM...."
    echo ""
    echo -e "\033[1mThe software you are about to install is \033[0m"
    echo -e "\033[1mcovered under the Lesser GNU Public license.\033[0m" 
    echo -e "\033[1mA copy of the license is included in the \033[0m"
    echo -e "\033[1minstallation folder.\033[0m"
    echo ""
    echo ""
    
    # bind-utils is required by tog-pegasus
    rpm -ivh bind-libs-9.3.6-4.P1.el5.i386.rpm
    rpm -ivh bind-utils-9.3.6-4.P1.el5.i386.rpm
    rpm -ivh tog-pegasus-2.10.0-1.i386.rpm
    # replace /etc/pam.d/wbem file owned by pegasus
    rpm -ivh xs-cim-cmpi-1.5.0-pegasus.i386.rpm --replacefiles
fi

