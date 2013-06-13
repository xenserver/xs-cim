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

#include "Xen_Job.h"
#include "providerinterface.h"

static const char *classname = "Xen_Job";    
static const char *keys[] = {"InstanceID"}; 
static const char *key_property = "InstanceID";

/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classnamestr
    )
{
    return key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classnamestr
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
    xen_task_set *task_set_all = NULL;
    xen_task_set *task_set = NULL;
    int num_resources = 0;
    if (!xen_task_get_all(session->xen, &task_set_all))
        return CMPI_RC_ERR_FAILED;
    /* filter based on task name-label */
    if(task_set_all != NULL) {
        int i=0;
        while(i<task_set_all->size) {
            char *name= NULL;
            xen_task_get_name_label(session->xen, &name, task_set_all->contents[i]);
            if(name) {
                if(strcmp(name, resources->classname) == 0)
                    num_resources++;
                free(name);
            }
            RESET_XEN_ERROR(session->xen);
            i++;
        }
        task_set = xen_task_set_alloc(num_resources);
        i=0;
        int j=0;
        while(i<task_set_all->size) {
            char *name= NULL;
            xen_task_get_name_label(session->xen, &name, task_set_all->contents[i]);
            if(name) { 
                if(strcmp(name, resources->classname) == 0) {
                    task_set->contents[j++] = task_set_all->contents[i];
                    task_set_all->contents[i] = NULL;
                }
                free(name);
            }
            RESET_XEN_ERROR(session->xen);
            i++;
        }
        xen_task_set_free(task_set_all);
    }
    resources->ctx = task_set;
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
        xen_task_set_free((xen_task_set *)resources->ctx);
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
    xen_task_set *task_set = (xen_task_set *)resources_list->ctx;
    if (task_set == NULL || resources_list->current_resource == task_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_task_record *task_rec = NULL;
    if (!xen_task_get_record(session->xen, &task_rec, task_set->contents[resources_list->current_resource]
        )) {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    prov_res->ctx = task_rec;
    return CMPI_RC_OK;
}


/*****************************************************************************
 * Function to cleanup the resource
 *
 * @param - provider_resource to be freed
 * @return CMPIrc error codes
****************************************************************************/
static CMPIrc xen_resource_record_cleanup(provider_resource *prov_res)
{
    if (prov_res->ctx)
        xen_task_record_free((xen_task_record *)prov_res->ctx);
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
    xen_task task;
    xen_task_record *task_rec = NULL;
    char buf[MAX_INSTANCEID_LEN];
    _CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf)/sizeof(buf[0]));
    if (!xen_task_get_by_uuid(session->xen, &task, buf) || 
        !xen_task_get_record(session->xen, &task_rec, task)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    xen_task_free(task);
    if(strcmp(task_rec->name_label, prov_res->classname) == 0)
    {
        /* This task matches what's on xen */
        prov_res->ctx = task_rec;
        return CMPI_RC_OK;
    }
    else
    {
        xen_task_record_free(task_rec);
        return CMPI_RC_ERR_INVALID_PARAMETER;
    }
}
/*****************************************************************************
 * This function is called from add in the Xen_ProviderGeneric.c
 * The code specific to a provider may be implemented below.
 *
 * @param None
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_add(
    const CMPIBroker *broker,
    xen_utils_session *session,
    const void *res_id
    )
{
    return CMPI_RC_ERR_NOT_SUPPORTED;
}
/*****************************************************************************
 * Delete a provider specific resource identified by inst_id.
 *
 * @param session - handle to the xen_utils_session object
 * @param inst_id - resource identifier for the provider specific resource
 * @return CMPIrc error codes
****************************************************************************/
static CMPIrc xen_resource_delete(
    const CMPIBroker *broker,
    xen_utils_session *session, 
    const char *inst_id
    )
{
    xen_task task = NULL;
    char buf[MAX_INSTANCEID_LEN];
    CMPIrc rc = CMPI_RC_OK;
    _CMPIStrncpySystemNameFromID(buf, inst_id, sizeof(buf)/sizeof(buf[0]));
    if (xen_task_get_by_uuid(session->xen, &task, buf)) {
        xen_task_destroy(session->xen, task);
        xen_task_free(task);
    }
    if (!session->xen->ok) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        rc = CMPI_RC_ERR_FAILED;
    }
    return rc;
}

/*****************************************************************************
 * Modify a provider specific resource identified by inst_id.
 *
 * @param res_id - pointer to a CMPIInstance that represents the CIM object
 *                 being modified
 * @param modified_res -
 * @param properties - list of properties to be used while modifying
 * @param session - handle to the xen_utils_session object
 * @param inst_id - resource identifier for the provider specific resource
 * @return CMPIrc error codes
*****************************************************************************/
static CMPIrc xen_resource_modify(
    const CMPIBroker *broker,
    const void *res_id, 
    const void *modified_res,
    const char **properties, 
    CMPIStatus status, 
    char *inst_id, 
    xen_utils_session *session
    )
{
    return CMPI_RC_ERR_NOT_SUPPORTED;
}
/************************************************************************
 * Function to extract resources
 *
 * @param res - provider specific resource to get values from
 * @param inst - CIM object whose properties are being set
 * @param properties - list of properties to be used while modifying
 * @return CMPIrc return values
*************************************************************************/
static CMPIrc xen_resource_extract(
    void **res, 
    const CMPIInstance *inst, 
    const char **properties
    )
{
    /* Following is the default implementation*/
    (void)res;
    (void)inst;
    (void)properties;
    return CMPI_RC_ERR_NOT_SUPPORTED;  /* unsupported */
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
    CMPIInstance *inst)
{
    CMPIUint16 jobstate = 0, errorcode = 0, percentcomplete=0;
    char buf[MAX_INSTANCEID_LEN];
    xen_task_record *task_rec = (xen_task_record *)resource->ctx;
    char *jobstatestr = xen_utils_get_from_string_string_map(task_rec->other_config, "CIMJobState");
    if (jobstatestr)
        jobstate = atoi(jobstatestr);
    char *errorcodestr = xen_utils_get_from_string_string_map(task_rec->other_config, "ErrorCode");
    if (errorcodestr)
        errorcode = atoi(errorcodestr);
    char *percentcompletestr = xen_utils_get_from_string_string_map(task_rec->other_config, "PercentComplete");
    if (percentcompletestr)
        percentcomplete = atoi(percentcompletestr);
    char *errordesc = xen_utils_get_from_string_string_map(task_rec->other_config, "ErrorDescription");
    char *desc = xen_utils_get_from_string_string_map(task_rec->other_config, "Description");

    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Task", CMPI_chars);
    //CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "DeleteOnCompletion",(CMPIValue *)&<value>, CMPI_boolean);
    CMSetProperty(inst, "Description",(CMPIValue *)desc, CMPI_chars);
    //CMSetProperty(inst, "DetailedStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "ElapsedTime",(CMPIValue *)&date_time, CMPI_dateTime);
    CMSetProperty(inst, "ElementName",(CMPIValue *)task_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ErrorCode",(CMPIValue *)&errorcode, CMPI_uint16);
    CMSetProperty(inst, "ErrorDescription",(CMPIValue *)errordesc, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "HealthState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    _CMPICreateNewSystemInstanceID(buf, sizeof(buf)/sizeof(buf[0]), task_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "JobRunTimes",(CMPIValue *)&<value>, CMPI_uint32);
    CMSetProperty(inst, "JobState",(CMPIValue *)&jobstate, CMPI_uint16);
    //CMSetProperty(inst, "JobStatus",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "LocalOrUtcTime",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "Name",(CMPIValue *)task_rec->name_label, CMPI_chars);
    //CMSetProperty(inst, "Notify",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OperatingStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherRecoveryAction",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Owner",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "PercentComplete",(CMPIValue *)&percentcomplete, CMPI_uint16);
    //CMSetProperty(inst, "PrimaryStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Priority",(CMPIValue *)&<value>, CMPI_uint32);
    //CMSetProperty(inst, "RecoveryAction",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "RunDay",(CMPIValue *)&<value>, CMPI_sint8);
    //CMSetProperty(inst, "RunDayOfWeek",(CMPIValue *)&<value>, CMPI_sint8);
    //CMSetProperty(inst, "RunMonth",(CMPIValue *)&<value>, CMPI_uint8);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "RunStartInterval",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "ScheduledStartTime",(CMPIValue *)&date_time, CMPI_dateTime);
    CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, task_rec->created);
    CMSetProperty(inst, "StartTime",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "Status",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "TimeBeforeRemoval",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "TimeSubmitted",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "UntilTime",(CMPIValue *)&date_time, CMPI_dateTime);

    /* The Xen_ConnectToDiskImageJob has extra properties, set them here */
    if(xen_utils_class_is_subclass_of(resource->broker, resource->classname, "Xen_ConnectToDiskImageJob")) {
        char *target_uri = xen_utils_get_from_string_string_map(task_rec->other_config, "TargetURI");
        if(target_uri)
            CMSetProperty(inst, "TargetURI",(CMPIValue *)target_uri, CMPI_chars);
        char *server_cert = xen_utils_get_from_string_string_map(task_rec->other_config, "SSLCertificate");
        if(server_cert)
            CMSetProperty(inst, "SSLCertificate",(CMPIValue *)server_cert, CMPI_chars);
        char *connect_handle = xen_utils_get_from_string_string_map(task_rec->other_config, "ConnectionHandle");
        if(connect_handle)
            CMSetProperty(inst, "ConnectionHandle",(CMPIValue *)connect_handle, CMPI_chars);
        char *user = xen_utils_get_from_string_string_map(task_rec->other_config, "Username");
        if(user)
            CMSetProperty(inst, "Username",(CMPIValue *)user, CMPI_chars);
        char *pass = xen_utils_get_from_string_string_map(task_rec->other_config, "Password");
        if(pass)
            CMSetProperty(inst, "Password",(CMPIValue *)pass, CMPI_chars);

    }
    else if(xen_utils_class_is_subclass_of(resource->broker, resource->classname, "Xen_VirtualSystemModifyResourcesJob")) {
        /* VSMS jobs could have added resources in a job, we need to update the job object with the object paths of the resources */
        char *affected_resources = xen_utils_get_from_string_string_map(task_rec->other_config, "AffectedResources");
        if(affected_resources) {
            xen_string_set *obj_paths =xen_utils_copy_to_string_set(affected_resources, ";");
            if(obj_paths) {
                CMPIArray *arr = xen_utils_convert_string_set_to_CMPIArray(resource->broker, obj_paths);
                if(arr)
                    CMSetProperty(inst, "AffectedResources", (CMPIValue *)&arr, CMPI_charsA);
                xen_string_set_free(obj_paths);
            }
        }
    }
    else if(xen_utils_class_is_subclass_of(resource->broker, resource->classname, "Xen_VirtualSystemCreateJob")) {
        /* Create jobs could have the resulting system in a job, we need to update the job object with the object paths of the new system */
        char *resulting_system = xen_utils_get_from_string_string_map(task_rec->other_config, "ResultingSystem");
        if(resulting_system)
            CMSetProperty(inst, "ResultingSystem", (CMPIValue *)resulting_system, CMPI_chars);
    }
    else if(xen_utils_class_is_subclass_of(resource->broker, resource->classname, "Xen_StartSnapshotForestExportJob")) {
        /* Create jobs could have the resulting system in a job, we need to update the job object with the object paths of the new system */
        char *diskuris = xen_utils_get_from_string_string_map(task_rec->other_config, "DiskImageURIs");
        if(diskuris){
            xen_string_set *disk_uri_set = xen_utils_copy_to_string_set(diskuris, ",");
            if(disk_uri_set && disk_uri_set->size > 0) {
                CMPIArray *arr = CMNewArray(resource->broker, disk_uri_set->size, CMPI_chars, NULL);
                int i;
                for(i=0; i<disk_uri_set->size; i++)
                    CMSetArrayElementAt(arr, i, disk_uri_set->contents[i], CMPI_chars);
                CMSetProperty(inst, "DiskImageURIs", (CMPIValue *)&arr, CMPI_charsA);
            }
            xen_string_set_free(disk_uri_set);
        }
        char *ssl_certs = xen_utils_get_from_string_string_map(task_rec->other_config, "SSLCertificates");
        if(ssl_certs) {
            xen_string_set *cert_set = xen_utils_copy_to_string_set(ssl_certs, ",");
            if(cert_set && cert_set->size > 0) {
                CMPIArray *certarr = CMNewArray(resource->broker, cert_set->size, CMPI_chars, NULL);
                int i;
                for(i=0; i<cert_set->size; i++)
                    CMSetArrayElementAt(certarr, i, cert_set->contents[i], CMPI_chars);
                CMSetProperty(inst, "SSLCertificates", (CMPIValue *)&certarr, CMPI_charsA);
            }
            xen_string_set_free(cert_set);
        }

        char *metadata_uri = xen_utils_get_from_string_string_map(task_rec->other_config, "MetadataURI");
        if(metadata_uri)
            CMSetProperty(inst, "MetadataURI", (CMPIValue *)metadata_uri, CMPI_chars);

        char *handle = xen_utils_get_from_string_string_map(task_rec->other_config, "ExportConnectionHandle");
        if(handle)
            CMSetProperty(inst, "ExportConnectionHandle", (CMPIValue *)handle, CMPI_chars);
    }
    else if(xen_utils_class_is_subclass_of(resource->broker, resource->classname, "Xen_EndSnapshotForestExportJob")) {
    }

    return CMPI_RC_OK;
}

/*******************************************************************************
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 ******************************************************************************/
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,       /* [in] CMPI Factory services */
    const CMPIContext * context,    /* [in] Additional context info, if any */
    const CMPIResult * results,     /* [out] Results of this operation */
    const CMPIObjectPath * reference, /* [in] Contains the CIM namespace, classname and desired object path */
    const char * methodname,          /* [in] Name of the method to apply against the reference object */
    const CMPIArgs * argsin,          /* [in] Method input arguments */
    CMPIArgs * argsout)             /* [in] Method output arguments */
{
    CMPIStatus status = {CMPI_RC_OK, NULL};      /* Return status of CIM operations. */
    const char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    unsigned long rc = 0;
    CMPIData argdata;
    xen_utils_session * session = NULL;

    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\"%s\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));

    struct xen_call_context *ctx = NULL;
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
    const char *res_id = CMGetCharPtr(argdata.value.string);


    /* TODO : Find the backend resource based on the resource id */
    /* Check that the method has the correct number of arguments. */
#if 0
    if (strcmp(methodname, "RequestStateChange") == 0) {
        if (!_GetArgument(broker, argsin, "RequestedState", CMPI_uint16, &argdata, &status)) {
            /* return an error */
            goto Exit;
        }
        if (!_GetArgument(broker, argsin, "TimeoutPeriod", CMPI_dateTime, &argdata, &status)) {
            /* return an error */
            goto Exit;
        }
    }
    else if (strcmp(methodname, "GetError") == 0) {
        CMPIObjectPath* error_instance_op = NULL;
        CMAddArg(argsout, "Error", (CMPIValue *)&error_instance_op, CMPI_chars);
    }
    else 
#endif 
    if (strcmp(methodname, "KillJob") == 0) {
        //if (_GetArgument(broker, argsin, "DeleteOnKill", CMPI_boolean, &argdata, &status)) {
        //    /* return an error */
        //    goto Exit;
        //}
        /* Delete the job object */
        xen_resource_delete(broker, session, res_id);
    }
    else
        status.rc = CMPI_RC_ERR_METHOD_NOT_FOUND;

    Exit:

    if(ctx)
        xen_utils_free_call_context(ctx);
    if(session)
        xen_utils_cleanup_session(session);

    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);

    _SBLIM_RETURNSTATUS(status);
}

/* Setup the function table for the instance provider */
XenFullInstanceMIStub(Xen_Job)

/* CMPI Method provider function table setup */
XenMethodMIStub(Xen_Job);

