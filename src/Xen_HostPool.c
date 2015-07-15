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

#include <Xen_HostPool.h>
#include "providerinterface.h"

static const char * classname = "Xen_HostPool";    
static const char *keys[] = {"InstanceID"}; 
static const char *key_property = "InstanceID";

#if NOT_IMPLEMENTED
static CMPIrc ListHosts(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    );
#endif
static CMPIrc CreatePool(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    );
static CMPIrc AddHost(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    struct xen_call_context *xen_ctx,      /* in - contains caller's creds */
    CMPIStatus *status          /* out - Report CMPI status of method */
    );
static CMPIrc RemoveHost(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    );
static CMPIrc EnableHighAvailability(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    );
static CMPIrc DisableHighAvailability(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    );

static CMPIrc SetDefaultStoragePool(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIObjectPath *ref,  /* in - Xen_pool reference */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    );

/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
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
    xen_pool_set *pool_set = NULL;
    if (!xen_pool_get_all(session->xen, &pool_set))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = pool_set;
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
    if (resources->ctx)
        xen_pool_set_free((xen_pool_set *)resources->ctx);
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
    xen_pool_set *pool_set = (xen_pool_set *)resources_list->ctx;
    if (pool_set == NULL || 
        resources_list->current_resource == pool_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_pool_record *pool_rec = NULL;
    if (!xen_pool_get_record(
        session->xen,
        &pool_rec,
        pool_set->contents[resources_list->current_resource]
        )) {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    prov_res->ctx = pool_rec;
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
    if (prov_res->ctx)
        xen_pool_record_free((xen_pool_record *)prov_res->ctx);
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
    char buf[MAX_INSTANCEID_LEN];
    _CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf));
    xen_pool pool;
    xen_pool_record *pool_rec = NULL;
    if (!xen_pool_get_by_uuid(session->xen, &pool, buf) || 
        !xen_pool_get_record(session->xen, &pool_rec, pool)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    xen_pool_free(pool);
    prov_res->ctx = pool_rec;
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
    char *uuid = NULL;
    char buf[MAX_INSTANCEID_LEN];

    xen_pool_record *pool_rec = (xen_pool_record *)resource->ctx;
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Host Pool", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)pool_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)pool_rec->name_label, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    bool ha_enabled = pool_rec->ha_enabled;
    CMSetProperty(inst, "HighAvailabilityEnabled",(CMPIValue *)&ha_enabled, CMPI_boolean);

    xen_sr_record_opt *sr_opt = pool_rec->default_sr;
    if (sr_opt->is_record)
        uuid = sr_opt->u.record->uuid;
    else
        xen_sr_get_uuid(resource->session->xen, &uuid, sr_opt->u.handle);
    CMSetProperty(inst, "DefaultStoragePoolID", (CMPIValue *)uuid, CMPI_chars);
    if (!sr_opt->is_record) {
        free(uuid);
        uuid = NULL;
    }
    RESET_XEN_ERROR(resource->session->xen);

    xen_host_record_opt *host_opt = pool_rec->master;
    if (host_opt->is_record)
        uuid = host_opt->u.record->uuid;
    else
        xen_host_get_uuid(resource->session->xen, &uuid, host_opt->u.handle);
    CMSetProperty(inst, "Master", (CMPIValue *)uuid, CMPI_chars);
    if (!host_opt->is_record) {
        free(uuid);
        uuid = NULL;
    }
    RESET_XEN_ERROR(resource->session->xen);

    _CMPICreateNewSystemInstanceID(buf, MAX_INSTANCEID_LEN, pool_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    return CMPI_RC_OK;
}

/*******************************************************************************
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 ******************************************************************************/
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,       /* [in] CMPI factory broker */
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

    if (!xen_utils_get_call_context(context, &ctx, &status)) {
        goto Exit;
    }

    if (!xen_utils_validate_session(&session, ctx)) {
        CMSetStatusWithChars(broker, &status, 
            CMPI_RC_ERR_METHOD_NOT_AVAILABLE, "Unable to connect to Xen");
        goto Exit;
    }

    if (strcmp(nameSpace, HOST_INSTRUMENTATION_NS) == 0) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("--- \"%s\" is not a valid namespace for %s", nameSpace, classname));
        CMSetStatusWithChars(broker, &status, CMPI_RC_ERR_INVALID_NAMESPACE, 
            "Invalid namespace specified for Xen_ComputerSystem");
        goto Exit;
    }

    int argcount = CMGetArgCount(argsin, NULL);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- argsin=%d", argcount));

    argdata = CMGetKey(reference, key_property, &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt find UUID of the VM to invoke method on"));
        goto Exit;
    }

    /* Check that the method has the correct number of arguments. */
#if NOT_IMPLEMENTED
    if (strcmp(methodname, "ListHosts") == 0) {
        rc = ListHosts(broker, context, argsin, argsout, session, &status);
    }
    else
#endif
        if (strcmp(methodname, "Create") == 0) {
        rc = CreatePool(broker, context, argsin, argsout, session, &status);
    }
    else if (strcmp(methodname, "AddHost") == 0) {
        rc = AddHost(broker, context, argsin, argsout, session, ctx, &status);
    }
    else if (strcmp(methodname, "RemoveHost") == 0) {
        rc = RemoveHost(broker, context, argsin, argsout, session, &status);
    }
    else if (strcmp(methodname, "EnableHighAvailability") == 0) {
        rc = EnableHighAvailability(broker, context, argsin, argsout, session, &status);
    }
    else if (strcmp(methodname, "DisableHighAvailability") == 0) {
        rc = DisableHighAvailability(broker, context, argsin, argsout, session, &status);
    }
    else if (strcmp(methodname, "SetDefaultStoragePool") == 0) {
        rc = SetDefaultStoragePool(broker, context, reference, argsin, argsout, session, &status);
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

#if NOT_IMPLEMENTED
static CMPIrc ListHosts(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    )
{
    CMPIData data;
    int i=0;
    int rc = Xen_HostPool_ListHosts_Failed;

    CMPIObjectPath *op = CMNewObjectPath(broker, "root/cimv2", "Xen_HostComputerSystem", status);
    CMPIEnumeration *host_refs = CBEnumInstanceNames(broker, ctx, op, status);
    CMPIArray *enum_arr = CMToArray(host_refs, status);
    CMPIArray *arr = CMNewArray(broker, CMGetArrayCount(enum_arr, status), CMPI_ref, NULL);
    while (CMHasNext(host_refs, status)) {
        data = CMGetNext(host_refs, status);
        CMSetArrayElementAt(arr, i, (CMPIValue *)&data.value.ref, CMPI_ref);
        i++;
    }
    CMAddArg(argsout, "Hosts", (CMPIValue *)&arr, CMPI_refA);
    rc = Xen_HostPool_ListHosts_Completed_with_No_Error;

    return rc;
}
#endif

CMPIrc CreatePool(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,          /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    )
{
    CMPIData argdata;
    int rc = Xen_HostPool_Create_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    char *name_label = NULL, *name_description = NULL;

    /* Mandatory Name argument */
    if (!_GetArgument(broker, argsin, "Name", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: Couldn't find the 'Name' parameter";
        goto Exit;
    }
    name_label = CMGetCharPtr(argdata.value.string);

    /* Its okay if description is not present */
    if (_GetArgument(broker, argsin, "Description", CMPI_string, &argdata, status))
        name_description = CMGetCharPtr(argdata.value.string);

    rc = Xen_HostPool_Create_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    xen_pool_set *pool_set = NULL;
    if (!xen_pool_get_all(session->xen, &pool_set))
        goto Exit;

    if (pool_set && pool_set->size > 0) {
        xen_pool_set_name_label(session->xen, pool_set->contents[0], name_label);
        xen_pool_set_name_description(session->xen, pool_set->contents[0], name_description);
        rc = Xen_HostPool_Create_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
    }

    Exit:
    if (pool_set)
        xen_pool_set_free(pool_set);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

static CMPIrc AddHost(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    struct xen_call_context *xen_ctx,  /* in - contains user creds of the caller */
    CMPIStatus *status          /* out - Report CMPI status of method */
    )
{
    CMPIData argdata;
    char *hostname=NULL, *username=NULL, *password=NULL;
    char *error_msg = "ERROR: Unknown error";
    int rc = Xen_HostPool_AddHost_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    xen_utils_session *remote_session = NULL;
    xen_host_record *host_rec = NULL;

    if (!_GetArgument(broker, argsin, "HostName", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: Missing the 'HostName' parameter. Needs to be an IP address or hostname";
        goto Exit;
    }
    hostname = CMGetCharPtr(argdata.value.string);

    if (!_GetArgument(broker, argsin, "Username", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: Missing the 'Username' parameter";
        goto Exit;
    }
    username = CMGetCharPtr(argdata.value.string);

    if (!_GetArgument(broker, argsin, "Password", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: Missing the Password parameter";
        goto Exit;
    }
    password = CMGetCharPtr(argdata.value.string);

    /* This call is directed at the host that wants to join this pool, initiate a xen session with it */
    if (!xen_utils_get_remote_session(&remote_session, hostname, username, password)) {
        error_msg = "ERROR: Failed to establish a xen session. Check hostname, username and password.";
        goto Exit;
    }

    if (!xen_host_get_record(session->xen, &host_rec, session->host))
        goto Exit;

    /* xen_pool_join causes a JOINING_HOST_CANNOT_CONTAIN_SHARED_SRS failure when the host has shared SRs */
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Adding %s to %s's pool", hostname, host_rec->address));

    rc = Xen_HostPool_AddHost_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    if (xen_pool_join(remote_session->xen, host_rec->address, xen_ctx->user, xen_ctx->pw)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("%s joined pool successfully", hostname));
        rc = Xen_HostPool_AddHost_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
    }

    Exit:
    xen_utils_set_status(broker, status, rc, error_msg, (remote_session != NULL ? remote_session->xen : NULL));

    if (remote_session)
        xen_utils_cleanup_session(remote_session);
    if (host_rec)
        xen_host_record_free(host_rec);

    return rc;
}

static CMPIrc RemoveHost(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    )
{
    CMPIData argdata;
    int rc = Xen_HostPool_RemoveHost_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = NULL;

    if (!_GetArgument(broker, argsin, "Host", CMPI_ref, &argdata, status)) {
        error_msg = "ERROR: Missing 'Host' parameter";
        goto Exit;
    }
    CMPIData keydata = CMGetKey(argdata.value.ref, "Name", status);
    char *host_uuid = CMGetCharPtr(keydata.value.string);
    if (host_uuid) {
        rc = Xen_HostPool_RemoveHost_Failed;
        statusrc = CMPI_RC_ERR_FAILED;
        xen_host host = NULL;
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("removing Host %s", host_uuid));
        if (xen_host_get_by_uuid(session->xen, &host, host_uuid)) {
            if (xen_pool_eject(session->xen, host)) {
                rc = Xen_HostPool_RemoveHost_Completed_with_No_Error;
                statusrc = CMPI_RC_OK;
            }
            xen_host_free(host);
        }
    }
    else {
        error_msg = "ERROR: 'Host' parameter is not a valid CIM reference";
    }
    Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

static CMPIrc EnableHighAvailability(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    )
{
    int rc = Xen_HostPool_EnableHighAvailability_Failed;
    CMPIrc statusrc = CMPI_RC_ERR_FAILED;
    xen_pool_set *pool_set = NULL;
    char *error_msg = "ERROR: Unknown error";

    if (xen_pool_get_all(session->xen, &pool_set) && pool_set && (pool_set->size > 0)) {
        /* The HA feature enablement via the CIM  uses the deatault SR for the heartbeat data */
        xen_sr default_sr = NULL;
        if (xen_pool_get_default_sr(session->xen, &default_sr, pool_set->contents[0])) {
            xen_sr_set* heartbeat_srs = xen_sr_set_alloc(1);
            if (heartbeat_srs) {
                heartbeat_srs->size = 1;
                heartbeat_srs->contents[0] = default_sr;
                xen_string_string_map *ha_config = xen_string_string_map_alloc(0);

                if (xen_pool_enable_ha(session->xen, heartbeat_srs, ha_config, "")); {
                    rc = Xen_HostPool_EnableHighAvailability_Completed_with_No_Error;
                    statusrc = CMPI_RC_OK;
                }
		xen_string_string_map_free(ha_config);
                xen_sr_set_free(heartbeat_srs);
            }
            // xen_sr_free(default_sr); gets freed above
        }
        xen_pool_set_free(pool_set);
    }

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

static CMPIrc DisableHighAvailability(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    )
{
    int rc = Xen_HostPool_DisableHighAvailability_Failed;
    CMPIrc statusrc = CMPI_RC_ERR_FAILED;
    char *error_msg = "ERROR: Unknown error";

    if (xen_pool_disable_ha(session->xen)) {
        rc = Xen_HostPool_DisableHighAvailability_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
    }

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

static CMPIrc SetDefaultStoragePool(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *ctx,     /* in - CMPI context for this call */
    const CMPIObjectPath *reference, /* in - reference to the Xen_HostPool object */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    CMPIStatus *status          /* out - Report CMPI status of method */
    )
{
    CMPIData argdata;
    char buf[MAX_INSTANCEID_LEN];
    int rc = Xen_HostPool_SetDefaultStoragePool_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";

    xen_pool pool = NULL;
    xen_sr sr = NULL;

    argdata = CMGetKey(reference, key_property, status);
    if ((status->rc != CMPI_RC_OK) || CMIsNullValue(argdata))
        goto Exit;

    _CMPIStrncpySystemNameFromID(buf, CMGetCharPtr(argdata.value.string), sizeof(buf));
    if (!xen_pool_get_by_uuid(session->xen, &pool, buf)) {
        error_msg = "ERROR: Could not find the specified Pool";
        goto Exit;
    }

    if (!_GetArgument(broker, argsin, "StoragePool", CMPI_ref, &argdata, status)) {
        error_msg = "ERROR: Could not find the specified StoragePool";
        goto Exit;
    }

    CMPIData property = CMGetKey(argdata.value.ref, "InstanceID", NULL);
    if (CMIsNullValue(property)) {
        error_msg = "ERROR: InstanceID on the StoragePool parameter is null";
        goto Exit;
    }

    _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(property.value.string), sizeof(buf));
    if (xen_sr_get_by_uuid(session->xen, &sr, buf)) {
        if (xen_pool_set_default_sr(session->xen, pool, sr)) {
            rc = Xen_HostPool_SetDefaultStoragePool_Completed_with_No_Error;
            statusrc = CMPI_RC_OK;
        }
    }

    Exit:
    if (pool)
        xen_pool_free(pool);
    if (sr)
        xen_sr_free(sr);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}


/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_HostPool)

/* Setup the method function table */
XenMethodMIStub(Xen_HostPool)

