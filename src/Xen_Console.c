// Copyright (C) 2008 - 2009 Citrix Systems Inc
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

/*#include other header files required by the provider */
#include <assert.h>
#include "providerinterface.h"
#include "RASDs.h"

static const char *con_cn = "Xen_Console";      
static const char *con_keys[] = {"SystemName","SystemCreationClassName","CreationClassName","DeviceID"}; 
static const char *con_key_property = "DeviceID";
static const char *rasd_keys[] = {"InstanceID"}; 
static const char *rasd_key_property = "InstanceID";

/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if(xen_utils_class_is_subclass_of(broker, con_cn, classname))
        return con_key_property;
    else
        return rasd_key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if(xen_utils_class_is_subclass_of(broker, con_cn, classname))
        return con_keys;
    else
        return rasd_keys;
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
    xen_console_set *all_consoles = NULL;
    if(!xen_console_get_all(session->xen, &all_consoles))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = all_consoles;
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
    if(resources && resources->ctx)
        xen_console_set_free((xen_console_set *)resources->ctx);
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
    provider_resource_list *resources_list, /* in */
    xen_utils_session *session,             /* in */
    provider_resource *prov_res             /* in, out */
    )
{
    if(resources_list->ctx == NULL)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_console_set *console_set = resources_list->ctx;
    while(resources_list->current_resource < console_set->size)
    {
        xen_console_record *console_rec = NULL;
        if(!xen_console_get_record(
            session->xen,
            &console_rec,
            console_set->contents[resources_list->current_resource]
            ))
        {
            xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
            return CMPI_RC_ERR_FAILED;
        }
        prov_res->ctx = console_rec;
        return CMPI_RC_OK;
    }

    return CMPI_RC_ERR_NOT_FOUND;

}
/*****************************************************************************
 * Function to cleanup the resource
 *
 * @param - provider_resource to be freed
 * @return CMPIrc error codes
****************************************************************************/
CMPIrc xen_resource_record_cleanup(provider_resource *prov_res)
{
    if(prov_res->ctx)
        xen_console_record_free((xen_console_record *)prov_res->ctx);
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
    xen_console console = NULL;
    xen_console_record *console_rec = NULL;
    char buf[MAX_INSTANCEID_LEN];

    _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf)/sizeof(buf[0]));
    if(!xen_console_get_by_uuid(session->xen, &console, buf))
        return CMPI_RC_ERR_NOT_FOUND;

    xen_console_get_record(session->xen, &console_rec, console);
    xen_console_free(console);
    if(!session->xen->ok) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    prov_res->ctx = console_rec;
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
static CMPIrc console_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    xen_console_record *con_rec = (xen_console_record *)resource->ctx;
    char *dom_name = "NoHost";
    char *dom_desc = "NoHost";
    char *dom_uuid = "NoHost";

    if(con_rec == NULL)
        return CMPI_RC_ERR_FAILED;

    if(CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;

    xen_vm_record *dom_rec = NULL;
    if(!con_rec->vm->is_record)
        xen_vm_get_record(resource->session->xen, &dom_rec, con_rec->vm->u.handle);
        /* If this fails, it measn the console doesnt have a VM associated with it?? */
    else
        dom_rec = con_rec->vm->u.record;

    if(dom_rec) {
        dom_name = dom_rec->name_label;
        dom_desc = dom_rec->name_description;
        dom_uuid = dom_rec->uuid;
    }

    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "SystemCreationClassName", (CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName", (CMPIValue *)dom_uuid, CMPI_chars);
    CMSetProperty(inst, "CreationClassName", (CMPIValue *)"Xen_Console", CMPI_chars);
    char buf[MAX_INSTANCEID_LEN];
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf), dom_uuid, con_rec->uuid);
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)dom_name, CMPI_chars);
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    DMTF_HealthState health_state = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&health_state, CMPI_uint16);
    DMTF_OperationalStatus op_status = DMTF_OperationalStatus_OK;
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&op_status, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Console for Xen Domain", CMPI_chars);
    CMSetProperty(inst, "Description", (CMPIValue *)dom_desc, CMPI_chars);
    CMSetProperty(inst, "Status", (CMPIValue *)DMTF_Status_OK, CMPI_chars);
    CMSetProperty(inst, "Protocol", (CMPIValue *)&(con_rec->protocol), CMPI_uint16);
    if(con_rec->location && con_rec->location[0] != '\0')
        CMSetProperty(inst, "URI", (CMPIValue *)con_rec->location, CMPI_chars);

    if(!con_rec->vm->is_record)
        xen_vm_record_free(dom_rec);

    return CMPI_RC_OK;
}

static CMPIrc console_rasd_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    if(resource == NULL)
        return CMPI_RC_ERR_FAILED;
    xen_console_record *con_rec = (xen_console_record *)resource->ctx;
    xen_console_rec_to_console_rasd(resource->broker, resource->session, inst, con_rec);
    return CMPI_RC_OK;
}

static CMPIrc xen_resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    if(xen_utils_class_is_subclass_of(resource->broker, con_cn, resource->classname))
        return console_set_properties(resource, inst);
    else
        return console_rasd_set_properties(resource, inst);
}

/*****************************************************************************
 * METHOD PROVIDER
******************************************************************************/

/*******************************************************************************
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 ******************************************************************************/
static CMPIStatus xen_resource_invoke_method(
                CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
                const CMPIBroker *broker,       /* [in] Broker to handle all CMPI calls */
                const CMPIContext * context,    /* [in] Additional context info, if any */
                const CMPIResult * results,     /* [out] Results of this operation */
                const CMPIObjectPath * reference, /* [in] Contains the CIM namespace, classname and desired object path */
                const char * methodname,        /* [in] Name of the method to apply against the reference object */
                const CMPIArgs * argsin,        /* [in] Method input arguments */
                CMPIArgs * argsout)             /* [in] Method output arguments */
{
    CMPIStatus status = {CMPI_RC_OK, NULL};      /* Return status of CIM operations. */
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    unsigned long rc = 0;
    CMPIData argdata;
    xen_utils_session * session = NULL;
    struct xen_call_context *ctx = NULL;
    
    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\"%s\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));

    if(!xen_utils_get_call_context(context, &ctx, &status)){
         goto Exit;
    }

    if (!xen_utils_validate_session(&session, ctx)) {
        CMSetStatusWithChars(broker, &status, 
            CMPI_RC_ERR_METHOD_NOT_AVAILABLE, "Unable to connect to Xen");
        goto Exit;
    }

    argdata = CMGetKey(reference, con_key_property, &status);
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt find UUID of the VM to invoke method on"));
        goto Exit;
    }

    if(strcmp(methodname, "Login") == 0) {
        /* Method to create a xen session ID to be used with the Console's VNC control */
        xen_utils_session* console_session = NULL;
        if(xen_utils_get_session(&console_session, ctx->user, ctx->pw) && (console_session != NULL)) {
            rc = CMPI_RC_OK;
            CMAddArg(argsout, "SessionIdentifier", (CMPIValue *)console_session->xen->session_id, CMPI_chars);

            /* free the memory used up by the session - dont use xen_cleanup_session since it logs the session out */
            xen_utils_free_session(console_session);
        }
    }
    else if(strcmp(methodname, "Logout") == 0) {
        //
        //if(xen_utils_get_session(&session, ctx->user, ctx->pw)) {
        //   CMAddArg(argsout, "SessionIdentifier", (CMPIValue *)&session->xen->session_id, CMPI_chars);
        //}
        rc = CMPI_RC_OK;
    }
    else
        status.rc = CMPI_RC_ERR_METHOD_NOT_FOUND;
Exit:

    if(session)
        xen_utils_cleanup_session(session);
    if(ctx)
        xen_utils_free_call_context(ctx);

    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);
    _SBLIM_RETURNSTATUS(status);
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_Console)

/* Setup the method function table */
XenMethodMIStub(Xen_Console)

