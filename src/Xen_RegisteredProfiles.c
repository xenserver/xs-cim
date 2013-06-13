// Copyright (C) 2007 Novell, Inc.
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
// ============================================================================
// Authors:       Jim Fehlig, <jfehlig@novell.com>
// Contributors:
// Description:   Implements all CIM_RegisteredProfile classes
//                (DSP1042 System Virtualizaiton Profile)
//                (DSP1057 Virtual System Profile)
//                (  Generic Device Virtualization Profile)
//                (  Processor Allocation Profile)
//                (  Memory Allocation Profile)
//                (  Storage Allocation Profile)
// ============================================================================

/* Common declarations for each CMPI "Cimpler" instance provider */
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#include <assert.h>
#include <stdbool.h>

#include <cmpidt.h>
#include <cmpimacs.h>

#include "cmpilify.h"


static const CMPIInstanceMI* mi;


#define _BROKER (((CMPILIFYInstance1ROMI*)(mi->hdl))->brkr)
#define _CLASS (((CMPILIFYInstance1ROMI*)(mi->hdl))->cn)
#define _KEYS (((CMPILIFYInstance1ROMI*)(mi->hdl))->kys)

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

#include <string.h>

#include "cmpitrace.h"
#include "provider_common.h"


/* Class keys */
static const char *keys[] = {"InstanceID", NULL};


static CMPIrc load()
{
   return CMPI_RC_OK;
}


static CMPIrc unload(const int terminating)
{
   (void)terminating;
   
   return CMPI_RC_OK;
}


/* Nothing to get, all static properties. */
static CMPIrc get(struct xen_call_context *id, void **res, const char** properties)
{
    (void)id;
    (void)res;
    (void)properties;
   
    return CMPI_RC_OK;
}


static void release(void *res)
{
    return;
}


/* Set CMPIInstance properties from the resource data. */
static CMPIrc setproperties(CMPIInstance *inst, const void *res,
                            const char **properties)
{
    CMPIStatus status;
    if (CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;

    /* Setup a filter to only return the desired properties. */
    CMPIObjectPath *object_path = CMGetObjectPath(inst, &status);
    if(status.rc != CMPI_RC_OK)
        return status.rc;

    CMPIString* class_name = CMGetClassName(object_path, &status);
    if(status.rc != CMPI_RC_OK)
        return status.rc;

    char *inst_id, *reg_name;
    if(strcmp(CMGetCharPtr(class_name), "Xen_RegisteredSystemVirtualizationProfile") == 0) {
        inst_id = "Xen:SystemVirtualizationProfile";
        reg_name = "System Virtualization";
    } else if(strcmp(CMGetCharPtr(class_name), "Xen_RegisteredVirtualSystemProfile") == 0) {
        inst_id = "Xen:VirtualSystemProfile";
        reg_name = "Virtual System Profile";
    } else if(strcmp(CMGetCharPtr(class_name), "Xen_RegisteredGenericDeviceAllocationProfile") == 0) {
        inst_id = "Xen:GenericDeviceAllocationProfile";
        reg_name = "Generic Device Allocation Profile";
    } else if(strcmp(CMGetCharPtr(class_name), "Xen_RegisteredMemoryAllocationProfile") == 0) {
        inst_id = "Xen:MemoryAllocationProfile";
        reg_name = "Memory Allocation Profile";
    } else if(strcmp(CMGetCharPtr(class_name), "Xen_RegisteredProcessorAllocationProfile") == 0) {
        inst_id = "Xen:ProcessorAllocationProfile";
        reg_name = "Processor Allocation Profile";
    } else if(strcmp(CMGetCharPtr(class_name), "Xen_RegisteredStorageVirtualizationProfile") == 0) {
        inst_id = "Xen:StorageResourceVirtualizationProfile";
        reg_name = "Storage Resource Virtualization Profile";
    } else if(strcmp(CMGetCharPtr(class_name), "Xen_RegisteredEthernetPortVirtualizationProfile") == 0) {
        inst_id = "Xen:EthernetPortResourceVirtualizationProfile";
        reg_name = "Ethernet Port Resource Virtualization Profile";
    }
    else {
        assert(false);
    }

    CMSetPropertyFilter(inst, properties, keys);
    CMSetProperty(inst, "InstanceID",
                 (CMPIValue *)inst_id, CMPI_chars);
    CMSetProperty(inst, "RegisteredName",
                 (CMPIValue *)reg_name, CMPI_chars);
   
    int registeredOrg = 2; // "DMTF"
    CMSetProperty(inst, "RegisteredOrganization",
                 (CMPIValue *)&registeredOrg, CMPI_uint16);

    CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    int advert_type = 3; /* SLP */
    CMSetArrayElementAt(arr, 0, &advert_type, CMPI_uint16);
    CMSetProperty(inst, "AdvertiseTypes",
                 (CMPIValue *)&arr, CMPI_uint16A);

    CMSetProperty(inst, "RegisteredVersion", (CMPIValue *)"1.0.0", CMPI_chars);

    return CMPI_RC_OK;
}


/* Setup CMPILIFY function tables and instance provider entry point.*/
CMPILIFYInstance1ROMIStub(Xen_RegisteredProfiles, mi)
