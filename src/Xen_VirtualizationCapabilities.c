// Copyright (C) 2008-2009 CitrixSystems Inc
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

#include <string.h>
#include "cmpiutil.h"
#include "providerinterface.h"

typedef struct {
    char instanceid[1024];
    char resourcetype[128];
    int shared;
    int mutable;
} local_virt_cap_resource;

//static const char * classname = "Xen_VirtualizationCapabilities";         
static const char *keys[] = {"InstanceID"}; 
static const char *key_property = "InstanceID";

static CMPIrc populate_resource(
    provider_resource *prov_res,
    int capnum
    )
{
    _SBLIM_ENTER("populate_resource");

    local_virt_cap_resource *resource = (local_virt_cap_resource *)
                                    calloc(1, sizeof(local_virt_cap_resource));
    if(resource == NULL)
        return CMPI_RC_ERR_FAILED;

    switch (capnum) {
    case 0: /* Processor (shared) */
        strcpy(resource->resourcetype, "Processor");
        sprintf(resource->instanceid, "Xen_%sVirtualizationCapability", resource->resourcetype);
        resource->shared = 1;
        resource->mutable = 0;
        break;

    case 1: /* Memory (dedicated) */
        strcpy(resource->resourcetype, "Memory");
        sprintf(resource->instanceid, "Xen_%sVirtualizationCapability", resource->resourcetype);
        resource->shared = 0;
        resource->mutable = 1;
        break;

    case 2: /* Disk (dedicated) */
        strcpy(resource->resourcetype, "Disk");
        sprintf(resource->instanceid, "Xen_%sVirtualizationCapability", resource->resourcetype);
        resource->shared = 1;
        resource->mutable = 0;
        break;

    case 3: /* NetworkPort (dedicated) */
        strcpy(resource->resourcetype, "NetworkPort");
        sprintf(resource->instanceid, "Xen_%sVirtualizationCapability", resource->resourcetype);
        resource->shared = 0;
        resource->mutable = 0;
        break;

    default:
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_WARNING, ("Default -- not supported"));
        free(resource);
        return CMPI_RC_ERR_FAILED;
    }
    prov_res->ctx = resource;
    _SBLIM_RETURN(CMPI_RC_OK);
}

/******************************************************************************
 ************ Provider Export functions ***************************************
 *****************************************************************************/
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    return key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    return keys;
}
/********************************************************
 * Function to enumerate provider specific resource
 *
 * @param session - handle to a xen_utils_session object
 * @param resources - pointer to the provider_resource_list
 *   object, the provider specific resource defined above
 *   is a member of this struct
 * @return CMPIrc error codes
 ********************************************************/
static CMPIrc xen_resource_list_enum(
    xen_utils_session *session, 
    provider_resource_list *resources
    )
{
    return CMPI_RC_OK;
}

/*******************************************************************
 * Function to cleanup provider specific resource, this function is
 * called at various places in Xen_ProviderGeneric.c
 *
 * @param resources - handle to the provider_resource_list to be
 *    be cleaned up. Clean up the provider specific part of the
 *    resource.
 * @return CMPIrc error codes
 *******************************************************************/
static CMPIrc xen_resource_list_cleanup(
    provider_resource_list *resources
    )
{
    return CMPI_RC_OK;
}

/*****************************************************************************
 * Function to get the next provider specific resource in the resource list
 *
 * @param resources_list - handle to the provide_resource_list object
 * @param session - handle to the xen_utils_session object
 * @param prov_res - handle to the next provider_resource to be filled in.
 * @return CMPIrc error codes
 *****************************************************************************/

static CMPIrc xen_resource_record_getnext(
    provider_resource_list *resources_list,/* in */
    xen_utils_session *session,/* in */
    provider_resource *prov_res /* in , out */
    )
{
  /* Return 'NOT_FOUND' when at the end of our list of caps*/
    CMPIrc rc = CMPI_RC_ERR_NOT_FOUND;
    if (resources_list->current_resource == 4) 
        return rc;

    rc = populate_resource(prov_res, resources_list->current_resource);
    return CMPI_RC_OK;
}

/*****************************************************************************
 * Function to cleanup the resource
 *
 * @param - provider_resource to be freed
 * @return CMPIrc error codes
****************************************************************************/
static CMPIrc xen_resource_record_cleanup(
    provider_resource *prov_res
    )
{
    if(prov_res->ctx)
        free(prov_res->ctx);
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Function to get a provider specific resource identified by an id
 *
 * @param res_uuid - resource identifier for the provider specific resource
 * @param session - handle to the xen_utils_session object
 * @param prov_res - provide_resource object to be filled in with the provider
 *                   specific resource
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_record_get_from_id(
    char *res_uuid, /* in */
    xen_utils_session *session, /* in */
    provider_resource *prov_res /* in , out */
    )
{
    int capnum;
    /* Generate raw data for the single resource. */
    if (strcmp(res_uuid, "Xen_ProcessorVirtualizationCapability") == 0) {
        /* Processor (shared) */
        capnum = 0;
    }
    else if (strcmp(res_uuid, "Xen_MemoryVirtualizationCapability") == 0) {
        /* Memory (dedicated) */
        capnum = 1;
    }
    else if (strcmp(res_uuid, "Xen_DiskVirtualizationCapability") == 0) {
        /* Disk (dedicated) */
        capnum = 2;
    }
    else if (strcmp(res_uuid, "Xen_NetworkPortVirtualizationCapability") == 0) {
        /* NetworkPort (dedicated) */
        capnum = 3;
    }
    else {
        return CMPI_RC_ERR_FAILED;
    }
    populate_resource(prov_res, capnum);

    return CMPI_RC_OK;
}
/************************************************************************
 * Function that sets the properties of a CIM object with values from the
 * provider specific resource.
 *
 * @param resource - provider specific resource to get values from
 * @param inst - CIM object whose properties are being set
 * @return CMPIrc return values
*************************************************************************/
static CMPIrc xen_resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    _SBLIM_ENTER("resource_set_properties");
    local_virt_cap_resource *ctx = resource->ctx;

    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "InstanceID",(CMPIValue *)ctx->instanceid, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)ctx->instanceid, CMPI_chars);
    CMSetProperty(inst, "ResourceType",(CMPIValue *)ctx->resourcetype, CMPI_chars);
    CMSetProperty(inst, "Shared",(CMPIValue *)&(ctx->shared), CMPI_boolean);
    CMSetProperty(inst, "Mutable",(CMPIValue *)&(ctx->mutable), CMPI_boolean);

    _SBLIM_RETURN(CMPI_RC_OK);
}

XenInstanceMIStub(Xen_VirtualizationCapabilities)
