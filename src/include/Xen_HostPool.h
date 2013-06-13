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

#ifndef __XEN_HOSTPOOL_H__
#define __XEN_HOSTPOOL_H__

/* Values for the various properties of the class */


typedef enum _Xen_HostPool_ListHosts{
    Xen_HostPool_ListHosts_Completed_with_No_Error=0,
    Xen_HostPool_ListHosts_Not_Supported=1,
    Xen_HostPool_ListHosts_Failed=2,
    Xen_HostPool_ListHosts_Timeout=3,
    Xen_HostPool_ListHosts_Invalid_Parameter=4,
    /*Xen_HostPool_ListHosts_DMTF_Reserved=..,*/
}Xen_HostPool_ListHosts;

typedef enum _Xen_HostPool_DisableHighAvailability{
    Xen_HostPool_DisableHighAvailability_Completed_with_No_Error=0,
    Xen_HostPool_DisableHighAvailability_Not_Supported=1,
    Xen_HostPool_DisableHighAvailability_Failed=2,
    Xen_HostPool_DisableHighAvailability_Timeout=3,
    Xen_HostPool_DisableHighAvailability_Invalid_Parameter=4,
    /*Xen_HostPool_DisableHighAvailability_DMTF_Reserved=..,*/
}Xen_HostPool_DisableHighAvailability;

typedef enum _Xen_HostPool_Create{
    Xen_HostPool_Create_Completed_with_No_Error=0,
    Xen_HostPool_Create_Not_Supported=1,
    Xen_HostPool_Create_Failed=2,
    Xen_HostPool_Create_Timeout=3,
    Xen_HostPool_Create_Invalid_Parameter=4,
    /*Xen_HostPool_AddHost_DMTF_Reserved=..,*/
}Xen_HostPool_Create;

typedef enum _Xen_HostPool_AddHost{
    Xen_HostPool_AddHost_Completed_with_No_Error=0,
    Xen_HostPool_AddHost_Not_Supported=1,
    Xen_HostPool_AddHost_Failed=2,
    Xen_HostPool_AddHost_Timeout=3,
    Xen_HostPool_AddHost_Invalid_Parameter=4,
    /*Xen_HostPool_AddHost_DMTF_Reserved=..,*/
}Xen_HostPool_AddHost;

typedef enum _Xen_HostPool_EnableHighAvailability{
    Xen_HostPool_EnableHighAvailability_Completed_with_No_Error=0,
    Xen_HostPool_EnableHighAvailability_Not_Supported=1,
    Xen_HostPool_EnableHighAvailability_Failed=2,
    Xen_HostPool_EnableHighAvailability_Timeout=3,
    Xen_HostPool_EnableHighAvailability_Invalid_Parameter=4,
    /*Xen_HostPool_EnableHighAvailability_DMTF_Reserved=..,*/
}Xen_HostPool_EnableHighAvailability;

typedef enum _Xen_HostPool_RemoveHost{
    Xen_HostPool_RemoveHost_Completed_with_No_Error=0,
    Xen_HostPool_RemoveHost_Not_Supported=1,
    Xen_HostPool_RemoveHost_Failed=2,
    Xen_HostPool_RemoveHost_Timeout=3,
    Xen_HostPool_RemoveHost_Invalid_Parameter=4,
    /*Xen_HostPool_RemoveHost_DMTF_Reserved=..,*/
}Xen_HostPool_RemoveHost;

typedef enum _Xen_HostPool_SetDefaultStoragePool{
    Xen_HostPool_SetDefaultStoragePool_Completed_with_No_Error=0,
    Xen_HostPool_SetDefaultStoragePool_Not_Supported=1,
    Xen_HostPool_SetDefaultStoragePool_Failed=2,
    Xen_HostPool_SetDefaultStoragePool_Timeout=3,
    Xen_HostPool_SetDefaultStoragePool_Invalid_Parameter=4,
    /*Xen_HostPool_SetDefaultStoragePool_DMTF_Reserved=..,*/
}Xen_HostPool_SetDefaultStoragePool;


#endif /*__XEN_HOSTPOOL_H__*/
