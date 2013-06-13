// ***** Generated by Codegen *****
// Copyright (C) 2008-2009 Citrix Systems Inc
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef __XEN_DISKSETTINGDATA_H__
#define __XEN_DISKSETTINGDATA_H__

/* Values for the various properties of the class */


typedef enum _Xen_DiskSettingData_Access{
    Xen_DiskSettingData_Access_Unknown=0,
    Xen_DiskSettingData_Access_Readable=1,
    Xen_DiskSettingData_Access_Writeable=2,
    Xen_DiskSettingData_Access_Read_Write_Supported=3,
    /*Xen_DiskSettingData_Access_DMTF_Reserved=..,*/
}Xen_DiskSettingData_Access;

typedef enum _Xen_DiskSettingData_ChangeableType{
    Xen_DiskSettingData_ChangeableType_Not_Changeable___Persistent=0,
    Xen_DiskSettingData_ChangeableType_Changeable___Transient=1,
    Xen_DiskSettingData_ChangeableType_Changeable___Persistent=2,
    Xen_DiskSettingData_ChangeableType_Not_Changeable___Transient=3,
}Xen_DiskSettingData_ChangeableType;

typedef enum _Xen_DiskSettingData_ConsumerVisibility{
    Xen_DiskSettingData_ConsumerVisibility_Unknown=0,
    Xen_DiskSettingData_ConsumerVisibility_Passed_Through=2,
    Xen_DiskSettingData_ConsumerVisibility_Virtualized=3,
    Xen_DiskSettingData_ConsumerVisibility_Not_represented=4,
    /*Xen_DiskSettingData_ConsumerVisibility_DMTF_reserved=..,*/
    /*Xen_DiskSettingData_ConsumerVisibility_Vendor_Reserved=32767..65535,*/
}Xen_DiskSettingData_ConsumerVisibility;

typedef enum _Xen_DiskSettingData_HostExtentNameFormat{
    Xen_DiskSettingData_HostExtentNameFormat_Unknown=0,
    Xen_DiskSettingData_HostExtentNameFormat_Other=1,
    Xen_DiskSettingData_HostExtentNameFormat_SNVM=7,
    Xen_DiskSettingData_HostExtentNameFormat_NAA=9,
    Xen_DiskSettingData_HostExtentNameFormat_EUI64=10,
    Xen_DiskSettingData_HostExtentNameFormat_T10VID=11,
    Xen_DiskSettingData_HostExtentNameFormat_OS_Device_Name=12,
    /*Xen_DiskSettingData_HostExtentNameFormat_DMTF_Reserved=..,*/
}Xen_DiskSettingData_HostExtentNameFormat;

typedef enum _Xen_DiskSettingData_HostExtentNameNamespace{
    Xen_DiskSettingData_HostExtentNameNamespace_Unknown=0,
    Xen_DiskSettingData_HostExtentNameNamespace_Other=1,
    Xen_DiskSettingData_HostExtentNameNamespace_VPD83Type3=2,
    Xen_DiskSettingData_HostExtentNameNamespace_VPD83Type2=3,
    Xen_DiskSettingData_HostExtentNameNamespace_VPD83Type1=4,
    Xen_DiskSettingData_HostExtentNameNamespace_VPD80=5,
    Xen_DiskSettingData_HostExtentNameNamespace_NodeWWN=6,
    Xen_DiskSettingData_HostExtentNameNamespace_SNVM=7,
    Xen_DiskSettingData_HostExtentNameNamespace_OS_Device_Namespace=8,
    /*Xen_DiskSettingData_HostExtentNameNamespace_DMTF_Reserved=..,*/
}Xen_DiskSettingData_HostExtentNameNamespace;

typedef enum _Xen_DiskSettingData_MappingBehavior{
    Xen_DiskSettingData_MappingBehavior_Unknown=0,
    Xen_DiskSettingData_MappingBehavior_Not_Supported=2,
    Xen_DiskSettingData_MappingBehavior_Dedicated=3,
    Xen_DiskSettingData_MappingBehavior_Soft_Affinity=4,
    Xen_DiskSettingData_MappingBehavior_Hard_Affinity=5,
    /*Xen_DiskSettingData_MappingBehavior_DMTF_Reserved=..,*/
    /*Xen_DiskSettingData_MappingBehavior_Vendor_Reserved=32767..65535,*/
}Xen_DiskSettingData_MappingBehavior;

typedef enum _Xen_DiskSettingData_ResourceType{
    Xen_DiskSettingData_ResourceType_Other=1,
    Xen_DiskSettingData_ResourceType_Computer_System=2,
    Xen_DiskSettingData_ResourceType_Processor=3,
    Xen_DiskSettingData_ResourceType_Memory=4,
    Xen_DiskSettingData_ResourceType_IDE_Controller=5,
    Xen_DiskSettingData_ResourceType_Parallel_SCSI_HBA=6,
    Xen_DiskSettingData_ResourceType_FC_HBA=7,
    Xen_DiskSettingData_ResourceType_iSCSI_HBA=8,
    Xen_DiskSettingData_ResourceType_IB_HCA=9,
    Xen_DiskSettingData_ResourceType_Ethernet_Adapter=10,
    Xen_DiskSettingData_ResourceType_Other_Network_Adapter=11,
    Xen_DiskSettingData_ResourceType_I_O_Slot=12,
    Xen_DiskSettingData_ResourceType_I_O_Device=13,
    Xen_DiskSettingData_ResourceType_Floppy_Drive=14,
    Xen_DiskSettingData_ResourceType_CD_Drive=15,
    Xen_DiskSettingData_ResourceType_DVD_drive=16,
    Xen_DiskSettingData_ResourceType_Disk_Drive=17,
    Xen_DiskSettingData_ResourceType_Tape_Drive=18,
    Xen_DiskSettingData_ResourceType_Storage_Extent=19,
    Xen_DiskSettingData_ResourceType_Other_storage_device=20,
    Xen_DiskSettingData_ResourceType_Serial_port=21,
    Xen_DiskSettingData_ResourceType_Parallel_port=22,
    Xen_DiskSettingData_ResourceType_USB_Controller=23,
    Xen_DiskSettingData_ResourceType_Graphics_controller=24,
    Xen_DiskSettingData_ResourceType_IEEE_1394_Controller=25,
    Xen_DiskSettingData_ResourceType_Partitionable_Unit=26,
    Xen_DiskSettingData_ResourceType_Base_Partitionable_Unit=27,
    Xen_DiskSettingData_ResourceType_Power=28,
    Xen_DiskSettingData_ResourceType_Cooling_Capacity=29,
    Xen_DiskSettingData_ResourceType_Ethernet_Switch_Port=30,
    Xen_DiskSettingData_ResourceType_Logical_Disk=31,
    Xen_DiskSettingData_ResourceType_Storage_Volume=32,
    Xen_DiskSettingData_ResourceType_Ethernet_Connection=33,
    /*Xen_DiskSettingData_ResourceType_DMTF_reserved=..,*/
    /*Xen_DiskSettingData_ResourceType_Vendor_Reserved=0x8000..0xFFFF,*/
}Xen_DiskSettingData_ResourceType;


#endif /*__XEN_DISKSETTINGDATA_H__*/
