// Copyright (C) 2006 IBM Corporation
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
// 
//    Modified by: Citrix Systems Inc.

#ifndef _XEN_VM_METHODS_H_
#define _XEN_VM_METHODS_H_
typedef enum _VSMS_AddResourceSettings{
    VSMS_AddResourceSettings_Completed_with_No_Error=0,
    VSMS_AddResourceSettings_Not_Supported=1,
    VSMS_AddResourceSettings_Failed=2,
    VSMS_AddResourceSettings_Timeout=3,
    VSMS_AddResourceSettings_Invalid_Parameter=4,
    /*VSMS_AddResourceSettings_DMTF_Reserved=..,*/
    VSMS_AddResourceSettings_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_AddResourceSettings_Method_Reserved=4097..32767,*/
    /*VSMS_AddResourceSettings_Vendor_Specific=32768..65535,*/
}VSMS_AddResourceSettings;

typedef enum _VSMS_DefineSystem{
    VSMS_DefineSystem_Completed_with_No_Error=0,
    VSMS_DefineSystem_Not_Supported=1,
    VSMS_DefineSystem_Failed=2,
    VSMS_DefineSystem_Timeout=3,
    VSMS_DefineSystem_Invalid_Parameter=4,
    /*VSMS_DefineSystem_DMTF_Reserved=..,*/
    VSMS_DefineSystem_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_DefineSystem_Method_Reserved=4097..32767,*/
    /*VSMS_DefineSystem_Vendor_Specific=32768..65535,*/
}VSMS_DefineSystem;

typedef enum _VSMS_DestroySystem{
    VSMS_DestroySystem_Completed_with_No_Error=0,
    VSMS_DestroySystem_Not_Supported=1,
    VSMS_DestroySystem_Failed=2,
    VSMS_DestroySystem_Timeout=3,
    VSMS_DestroySystem_Invalid_Parameter=4,
    VSMS_DestroySystem_Invalid_State=5,
    /*VSMS_DestroySystem_DMTF_Reserved=..,*/
    VSMS_DestroySystem_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_DestroySystem_Method_Reserved=4097..32767,*/
    /*VSMS_DestroySystem_Vendor_Specific=32768..65535,*/
}VSMS_DestroySystem;

typedef enum _VSMS_RemoveResourceSettings{
    VSMS_RemoveResourceSettings_Completed_with_No_Error=0,
    VSMS_RemoveResourceSettings_Not_Supported=1,
    VSMS_RemoveResourceSettings_Failed=2,
    VSMS_RemoveResourceSettings_Timeout=3,
    VSMS_RemoveResourceSettings_Invalid_Parameter=4,
    VSMS_RemoveResourceSettings_Invalid_State=5,
    /*VSMS_RemoveResourceSettings_DMTF_Reserved=..,*/
    VSMS_RemoveResourceSettings_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_RemoveResourceSettings_Method_Reserved=4097..32767,*/
    /*VSMS_RemoveResourceSettings_Vendor_Specific=32768..65535,*/
}VSMS_RemoveResourceSettings;

typedef enum _VSMS_ImportSystem{
    VSMS_ImportSystem_Completed_with_No_Error=0,
    VSMS_ImportSystem_Not_Supported=1,
    VSMS_ImportSystem_Failed=2,
    VSMS_ImportSystem_Timeout=3,
    VSMS_ImportSystem_Invalid_Parameter=4,
    /*VSMS_ImportSystem_DMTF_Reserved=..,*/
    VSMS_ImportSystem_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_ImportSystem_Method_Reserved=4097..32767,*/
    /*VSMS_ImportSystem_Vendor_Specific=32768..65535,*/
}VSMS_ImportSystem;

typedef enum _VSMS_ExportDisk{
    VSMS_ExportDisk_Completed_with_No_Error=0,
    VSMS_ExportDisk_Not_Supported=1,
    VSMS_ExportDisk_Failed=2,
    VSMS_ExportDisk_Timeout=3,
    VSMS_ExportDisk_Invalid_Parameter=4,
    /*VSMS_ExportDisk_DMTF_Reserved=..,*/
    VSMS_ExportDisk_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_ExportDisk_Method_Reserved=4097..32767,*/
    /*VSMS_ExportDisk_Vendor_Specific=32768..65535,*/
}VSMS_ExportDisk;

typedef enum _VSMS_ModifySystemSettings{
    VSMS_ModifySystemSettings_Completed_with_No_Error=0,
    VSMS_ModifySystemSettings_Not_Supported=1,
    VSMS_ModifySystemSettings_Failed=2,
    VSMS_ModifySystemSettings_Timeout=3,
    VSMS_ModifySystemSettings_Invalid_Parameter=4,
    VSMS_ModifySystemSettings_Invalid_State=5,
    VSMS_ModifySystemSettings_Incompatible_Parameters=6,
    /*VSMS_ModifySystemSettings_DMTF_Reserved=..,*/
    VSMS_ModifySystemSettings_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_ModifySystemSettings_Method_Reserved=4097..32767,*/
    /*VSMS_ModifySystemSettings_Vendor_Specific=32768..65535,*/
}VSMS_ModifySystemSettings;

typedef enum _VSMS_ModifyResourceSettings{
    VSMS_ModifyResourceSettings_Completed_with_No_Error=0,
    VSMS_ModifyResourceSettings_Not_Supported=1,
    VSMS_ModifyResourceSettings_Failed=2,
    VSMS_ModifyResourceSettings_Timeout=3,
    VSMS_ModifyResourceSettings_Invalid_Parameter=4,
    VSMS_ModifyResourceSettings_Invalid_State=5,
    VSMS_ModifyResourceSettings_Incompatible_Parameters=6,
    /*VSMS_ModifyResourceSettings_DMTF_Reserved=..,*/
    VSMS_ModifyResourceSettings_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_ModifyResourceSettings_Method_Reserved=4097..32767,*/
    /*VSMS_ModifyResourceSettings_Vendor_Specific=32768..65535,*/
}VSMS_ModifyResourceSettings;

typedef enum _VSMS_ImportDisk{
    VSMS_ImportDisk_Completed_with_No_Error=0,
    VSMS_ImportDisk_Not_Supported=1,
    VSMS_ImportDisk_Failed=2,
    VSMS_ImportDisk_Timeout=3,
    VSMS_ImportDisk_Invalid_Parameter=4,
    /*VSMS_ImportDisk_DMTF_Reserved=..,*/
    VSMS_ImportDisk_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_ImportDisk_Method_Reserved=4097..32767,*/
    /*VSMS_ImportDisk_Vendor_Specific=32768..65535,*/
}VSMS_ImportDisk;

typedef enum _VSMS_AddResourceSetting{
    VSMS_AddResourceSetting_Completed_with_No_Error=0,
    VSMS_AddResourceSetting_Not_Supported=1,
    VSMS_AddResourceSetting_Failed=2,
    VSMS_AddResourceSetting_Timeout=3,
    VSMS_AddResourceSetting_Invalid_Parameter=4,
    /*VSMS_AddResourceSetting_DMTF_Reserved=..,*/
    VSMS_AddResourceSetting_Method_Parameters_Checked___Job_Started=4096,
    /*VSMS_AddResourceSetting_Method_Reserved=4097..32767,*/
    /*VSMS_AddResourceSetting_Vendor_Specific=32768..65535,*/
}VSMS_AddResourceSetting;

#endif //_XEN_VM_METHODS_H_

