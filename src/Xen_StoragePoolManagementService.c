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

#include <inttypes.h>
#include <uuid/uuid.h>

#include "RASDs.h"
#include "Xen_Job.h"
#include "Xen_StoragePoolManagementService.h"
#include "providerinterface.h"

#define JOB_NAME_CONNECTTODISK      "Xen_ConnectToDiskImageJob"
#define JOB_NAME_DISCONNECTFROMDISK "Xen_DisconnectFromDiskImageJob"

/* External functions used */
int disk_rasd_to_vbd(
    const CMPIBroker *broker,
    xen_utils_session* session,
    CMPIInstance *disk_rasd,
    xen_vbd_record **vbd_rec,
    xen_vdi_record **vdi_rec,
    xen_sr  *sr,
    CMPIStatus *status);
CMPIObjectPath *create_storage_pool_ref(
    const CMPIBroker *broker, 
    xen_utils_session *session, 
    xen_sr_record *sr_rec
    );
/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
int create_resource_pool(
    const CMPIBroker *broker,
    const CMPIContext *context,
    char *name_space,
    xen_utils_session *session, 
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status
    );
int delete_resource_pool(
    const CMPIBroker *broker,
    xen_utils_session *session, 
    const CMPIArgs *argsin,
    CMPIStatus *status,
    bool forget
    );
int connect_to_disk_image(
    const CMPIBroker *broker,
    const CMPIContext *context,
    char *name_space,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status);
int disconnect_from_disk_image(
    const CMPIBroker *broker,
    const CMPIContext *context,
    char *name_space,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status);
int discover_iscsi_targets(
    const CMPIBroker *broker,
    const CMPIContext *context,
    char *name_space,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status);
int create_disk_image(
    const CMPIBroker *broker,
    char *name_space,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status
    );
int delete_disk_image(
    const CMPIBroker *broker,
    xen_utils_session *session,
    char *vdi_uuid,
    CMPIStatus *status
    );
char *_get_disk_image_param(
    const CMPIBroker * broker, 
    const CMPIArgs *argsin,
    CMPIStatus *status);

/* 
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 */
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,       /* [in] CMPI Broker services */
    const CMPIContext * cmpi_context, /* [in] Additional context info, if any */
    const CMPIResult * results,     /* [out] Results of this operation */
    const CMPIObjectPath * reference, /* [in] Contains the CIM namespace, classname and desired object path */
    const char * methodname,        /* [in] Name of the method to apply against the reference object */
    const CMPIArgs * argsin,        /* [in] Method input arguments */
    CMPIArgs * argsout)             /* [in] Method output arguments */
{
    CMPIStatus status = {CMPI_RC_ERR_INVALID_PARAMETER, NULL};      /* Return status of CIM operations. */
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    unsigned long rc = Xen_StoragePoolManagementService_CreateResourcePool_Invalid_Parameter;
    CMPIData argdata;
    xen_utils_session * session = NULL;

    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\"%s\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));

    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(cmpi_context, &ctx, &status)) {
        goto Exit;
    }

    if (!xen_utils_validate_session(&session, ctx)) {
        CMSetStatusWithChars(broker, &status, 
            CMPI_RC_ERR_METHOD_NOT_AVAILABLE, "Unable to connect to Xen");
        goto Exit;
    }

    int argcount = CMGetArgCount(argsin, NULL);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- argsin=%d", argcount));

    argdata = CMGetKey(reference, "Name", &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("Couldnt find the Storage Pool Management Service to invoke method on"));
        goto Exit;
    }
#if 0
    /* Check that the method has the correct number of arguments. */
    if (strcmp(methodname, "RequestStateChange") == 0) {
        status.rc=CMPI_RC_ERR_NOT_SUPPORTED;
    }
    else if (strcmp(methodname, "StopService") == 0) {
        status.rc=CMPI_RC_ERR_NOT_SUPPORTED;
    }
    else if (strcmp(methodname, "ChangeAffectedElementsAssignedSequence") == 0) {
        status.rc=CMPI_RC_ERR_NOT_SUPPORTED;
    }
    else if (strcmp(methodname, "RemoveResourcesFromResourcePool") == 0) {
        status.rc=CMPI_RC_ERR_NOT_SUPPORTED;
    }
    else if (strcmp(methodname, "ChangeParentResourcePool") == 0) {
        status.rc=CMPI_RC_ERR_NOT_SUPPORTED;
    }
    else if (strcmp(methodname, "AddResourcesToResourcePool") == 0) {
        status.rc=CMPI_RC_ERR_NOT_SUPPORTED;
    }
    else if (strcmp(methodname, "StartService") == 0) {
        status.rc=CMPI_RC_ERR_NOT_SUPPORTED;
    }
    else if (strcmp(methodname, "CreateChildResourcePool") == 0) {
        status.rc=CMPI_RC_ERR_NOT_SUPPORTED;
    }
    else 
#endif
    if (strcmp(methodname, "CreateStoragePool") == 0) {
        /* A new CIM method has been created to allow passing in the SR device-config Settings */
        /* We cannot use the DMTF definition because it only allows host storage extents discoverable 
        via CIM to be aggregated into pools */
        rc = create_resource_pool(broker, cmpi_context, nameSpace, session, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "DeleteResourcePool") == 0) {
        rc = delete_resource_pool(broker, session, argsin, &status, false);
    }
    else if (strcmp(methodname, "DetachStoragePool") == 0) {
        rc = delete_resource_pool(broker, session, argsin, &status, true);
    }
    else if (strcmp(methodname, "CreateDiskImage") == 0) {
        rc = create_disk_image(broker, nameSpace, session, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "DeleteDiskImage") == 0) {
        char *vdi_uuid = _get_disk_image_param(broker, argsin, &status);
        if (!vdi_uuid)
            goto Exit;
        rc = delete_disk_image(broker, session, vdi_uuid, &status);
        free(vdi_uuid);
    }
    else if (strcmp(methodname, "ConnectToDiskImage") == 0) {
        rc = connect_to_disk_image(broker, cmpi_context, nameSpace, session, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "DisconnectFromDiskImage") == 0) {
        rc = disconnect_from_disk_image(broker, cmpi_context, nameSpace, session, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "DiscoveriSCSITargetInfo") == 0) {
        rc = discover_iscsi_targets(broker, cmpi_context, nameSpace, session, argsin, argsout, &status);
    }
    else {
        rc = Xen_StoragePoolManagementService_CreateResourcePool_Not_Supported;
        xen_utils_set_status(broker, &status, CMPI_RC_ERR_METHOD_NOT_FOUND, "ERROR: Method not supported", session->xen);
    }

Exit:
    if (ctx) xen_utils_free_call_context(ctx);

    if(session)
        xen_utils_cleanup_session(session);

    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);
    _SBLIM_RETURNSTATUS(status);
}

/* CMPI Method provider function table setup */
XenMethodMIStub(Xen_StoragePoolManagementService);

/******************************************************************************
* Method implementations 
*******************************************************************************/
/*
 * create_disk_image
 * Function that creates a VDI.
 *
 * @param session - xen_utils_session object to get to xen
 * @param rasd - rasd instance containing all settings required to create the VDI
 *
 * @return = one of the DMTF error codes specified in the MOF file 
 */
int create_disk_image(
    const CMPIBroker *broker,
    char *name_space,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status
    )
{
    xen_vdi_record *vdi_rec = NULL;
    xen_sr sr = NULL;
    xen_vbd_record *vbd_rec = NULL;
    CMPIObjectPath *objectpath = NULL;
    CMPIInstance *instance = NULL;
    CMPIData argdata;
    xen_vdi vdi = NULL;
    char *error_msg = "ERROR: Unknown Error";
    int rc = Xen_StoragePoolManagementService_ConnectToDiskImages_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;

    if (!_GetArgument(broker, argsin, "ResourceSetting", CMPI_string, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "ResourceSetting", CMPI_instance, &argdata, status)) {
            /* return an error */
            error_msg = "ERROR: Couldn't find the ResourceSetting parameter";
            goto Exit;
        }
    }
    if (!xen_utils_get_cmpi_instance(broker, &argdata, &objectpath, &instance) || !instance) {
        error_msg = "ERROR: ResourceSetting Instance is NULL";
        goto Exit;
    }

    if (!disk_rasd_to_vbd(broker, session, instance, &vbd_rec, &vdi_rec, &sr, status)) {
        error_msg = "ERROR: Couldnt' parse the 'ResourceSetting' parameter";
        goto Exit;
    }

    xen_sr_record_opt sr_record = {
        .u.handle = sr
    };
    vdi_rec->sr = &sr_record;

    rc = Xen_StoragePoolManagementService_ConnectToDiskImages_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    if (!xen_vdi_create(session->xen, &vdi, vdi_rec))
        goto Exit;

    /* Create a reference to a Xen_DiskImage obejct as output */
    char* vdi_uuid, *sr_uuid;
    char device_id[MAX_INSTANCEID_LEN];
    xen_sr_get_uuid(session->xen, &sr_uuid, sr);
    xen_vdi_get_uuid(session->xen, &vdi_uuid, vdi);

    CMPIObjectPath *result_setting = CMNewObjectPath(broker, name_space, "Xen_DiskImage", NULL);
    _CMPICreateNewDeviceInstanceID(device_id, MAX_INSTANCEID_LEN, sr_uuid, vdi_uuid);
    CMAddKey(result_setting, "DeviceID", (CMPIValue *)device_id, CMPI_chars);
    CMAddKey(result_setting, "CreationClassName",(CMPIValue *)"Xen_DiskImage", CMPI_chars);
    CMAddKey(result_setting, "SystemCreationClassName",(CMPIValue *)"Xen_StoragePool", CMPI_chars);
    CMAddKey(result_setting, "SystemName",(CMPIValue *)sr_uuid, CMPI_chars);
    CMAddArg(argsout, "ResultingDiskImage", (CMPIValue *)&result_setting, CMPI_ref);

    free(sr_uuid);
    free(vdi_uuid);

    rc = Xen_StoragePoolManagementService_ConnectToDiskImages_Completed_with_No_Error;
    statusrc = CMPI_RC_OK;
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Created Xen_DiskImage: %s", device_id));

    Exit:
    if (vdi_rec) {
        vdi_rec->sr = NULL;
        xen_vdi_record_free(vdi_rec);
    }
    if (vdi)
        xen_vdi_free(vdi);
    if (vbd_rec)
        xen_vbd_record_free(vbd_rec);
    if (sr)
        xen_sr_free(sr);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}
/*
 * create_disk_image
 * Function that creates a VDI.
 *
 * @param session - xen_utils_session object to get to xen
 * @param rasd - rasd instance containing all settings required to create the VDI
 *
 * @return = one of the DMTF error codes specified in the MOF file 
 */
int delete_disk_image(
    const CMPIBroker *broker,
    xen_utils_session *session,
    char *vdi_uuid,
    CMPIStatus *status
    )
{
    xen_vdi vdi = NULL;
    char *error_msg = "ERROR: Unknown error";
    CMPIrc statusrc= CMPI_RC_ERR_INVALID_PARAMETER;
    int rc= Xen_StoragePoolManagementService_ConnectToDiskImages_Invalid_Parameter;

    if (!xen_vdi_get_by_uuid(session->xen, &vdi, vdi_uuid)) {
        error_msg = "ERROR: Couldn't find the specified disk image";
        goto Exit;
    }
    statusrc= CMPI_RC_ERR_FAILED;
    rc = Xen_StoragePoolManagementService_ConnectToDiskImages_Failed;
    if (!xen_vdi_destroy(session->xen, vdi)) {
        error_msg = "ERROR: Couldn't delete the disk image.";
        goto Exit;
    }
    statusrc= CMPI_RC_OK;
    rc = Xen_StoragePoolManagementService_ConnectToDiskImages_Completed_with_No_Error;
    Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

char *_get_disk_image_param(
    const CMPIBroker *broker,
    const CMPIArgs* argsin,
    CMPIStatus *status)
{
    char *vdi_uuid = NULL;
    CMPIData argdata;

    /* VDI - mandatory argument - could be just be string form */
    if (!_GetArgument(broker, argsin, "DiskImage", CMPI_string, &argdata, status)) {
        /* Or it could be a reference to a Xen_DiskImage object */
        if (!_GetArgument(broker, argsin, "DiskImage", CMPI_ref, &argdata, status))
            goto Exit;
        else {
            char buf[MAX_INSTANCEID_LEN];
            CMPIData key = CMGetKey(argdata.value.ref, "DeviceID", status);
            if (CMIsNullValue(key))
                goto Exit;
            _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(key.value.string), sizeof(buf));
            vdi_uuid = strdup(buf);
        }
    }
    else
        vdi_uuid = strdup(CMGetCharPtr(argdata.value.string));
    Exit:
    return vdi_uuid;
}

typedef struct _connect_job_context {
    char *vdi_uuid;             /* UUID of the disk being connected to */
    bool use_ssl;               /* allow connect over SSL */
    xen_string_string_map *args;/* arguments to the transfer vm */
}connect_job_context;

/* 
 * ConnectToDiskImage asynchronous method 
 *
 * Calls the host function to spin up a transfer vm and attach the VDI to it 
 * The VDI is then made available to the outside world for import/export purposes 
 */
void _connect_task_callback(
    void* async_job
    )
{
    int state = JobState_Exception;
    int job_error_code = Xen_StoragePoolManagementService_ConnectToDiskImages_Failed;
    char *description = strdup("Error: Unknown error connecting to disk image");
    Xen_job *job = (Xen_job *)async_job;
    connect_job_context *ctx = (connect_job_context *)job->job_context;
    xen_utils_session *session = job->session;
    char *record_handle = NULL;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Connect job started"));
    state = JobState_Running;
    job_change_state(job, session, state, 0, 0, NULL);

    xen_host_call_plugin(session->xen, &record_handle, session->host, "transfer", "expose", ctx->args);
    /* 'expose' completed, get the output parameters (which contains the 'trasnfer_handle') */
    if (record_handle && *record_handle != '\0') {
        /* get the 'transfer_record' from the handle */
        xen_string_string_map *args = NULL;
        xen_utils_add_to_string_string_map("record_handle", record_handle, &args);
        xen_utils_add_to_string_string_map("vdi_uuid", ctx->vdi_uuid, &args);
        char *transfer_record = NULL;
        if (xen_host_call_plugin(session->xen, &transfer_record, session->host, "transfer", "get_record", args)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Get_Record succeeded :%s", transfer_record));
            char *uri = xen_utils_get_uri_from_transfer_record(transfer_record);
            if (uri) {
                /* add the record handle to the task other-config */
                xen_task_add_to_other_config(session->xen, job->task_handle, "ConnectionHandle", record_handle);
                /* Add the TargetURI to the xen task other-config so it can be picked up by the Xen_Job instance */    
                xen_task_add_to_other_config(session->xen, job->task_handle, "TargetURI", uri);
                free(uri);
                /* Add CHAP username and password to xen task */
                char *user = xen_utils_get_value_from_transfer_record(transfer_record, "username");
                if(user) {
                    xen_task_add_to_other_config(session->xen, job->task_handle, "Username", user);
                    free(user);
                }
                char *pass = xen_utils_get_value_from_transfer_record(transfer_record, "password");
                if(pass) {
                    xen_task_add_to_other_config(session->xen, job->task_handle, "Password", pass);
                    free(pass);
                }
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Added task info to other_config"));
                if (ctx->use_ssl) {
                    /* include the server certificate, if available */
                    char *cert = xen_utils_get_value_from_transfer_record(transfer_record, "ssl_cert");
                    if (cert) {
                        xen_task_add_to_other_config(session->xen, job->task_handle, "SSLCertificate", cert);
                        free(cert);
                    }
                }
                state = JobState_Completed;
                job_error_code = Xen_StoragePoolManagementService_ConnectToDiskImages_Completed_with_No_Error;
		if (description)
		  free(description);
                description = NULL;
            }
            free(transfer_record);
        }
        free(record_handle);
        xen_string_string_map_free(args);
        args = NULL;
    } else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("ERROR: Connect job Failed !"));
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    }

    if (job_error_code != Xen_StoragePoolManagementService_ConnectToDiskImages_Completed_with_No_Error) {
      if (description)
	free(description);
        description = xen_utils_get_xen_error(session->xen);
        state = JobState_Exception;
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Changing job state to %d", state));
    job_change_state(job, session, state, 100, job_error_code, description);

    if (ctx->args)
        xen_string_string_map_free(ctx->args);
    if (ctx->vdi_uuid)
        free(ctx->vdi_uuid);
    free(ctx);

    if (description) {
        free(description);
    }
}

/* Xen-API documentation for transfer VM available at 
  "http://scale.ad.xensource.com/confluence/display/eng/Transfer+VM+XenServer+Plugin+API+Specification" */
/*
 * connect_from_disk_images
 * Function that connects the given list of VDIs to the iSCSI VM.
 *
 * @param session - xen_utils_session object to get to xen
 * @param vdi_list - ';' separated list of vdi uuids to dettach from the iSCSI VM
 * @param out target_uri - URI of the LUNs to connect using iSCSI VM, in the form iscsi://ip/iqn/luns=lun1,lun2
 *    where the lun ids are in the same order as the vdi uuids.
 *
 * @return = one of the DMTF error codes specified in the MOF file 
 */
int connect_to_disk_image(
    const CMPIBroker *broker,
    const CMPIContext *context,
    char *name_space,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status
    )
{
    int rc = Xen_StoragePoolManagementService_ConnectToDiskImages_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    CMPIData argdata;
    char *vdi_uuid = NULL;
    char *protocol = "bits";
    char *network_uuid = NULL;
    char *ip_address = NULL, *subnet_mask = NULL, *ip_gateway = NULL;
    char *host_uuid = NULL;
    bool use_ssl = false;
    xen_vdi vdi = NULL;
    xen_host host = NULL;
    unsigned long timeout_in_minutes = 24*60;
    bool read_only = false;

    /* VDI - mandatory argument - could be just be string form */
    vdi_uuid = _get_disk_image_param(broker, argsin, status);
    if (!vdi_uuid) {
        error_msg = "ERROR: Couldn't get the 'DiskImage' parameter";
        goto Exit;
    }

    /* Network UUID - mandatory argument - could be in string form */
    if (!_GetArgument(broker, argsin, "VirtualSwitch", CMPI_string, &argdata, status)) {
        /* Or it could be a reference to a Xen_VirtualSwitch CIM object */
        if (!_GetArgument(broker, argsin, "VirtualSwitch", CMPI_ref, &argdata, status)) {
            /* default to the management network if not specified */
            network_uuid = "management";
        }
        else {
            CMPIData key = CMGetKey(argdata.value.ref, "Name", status);
            if (CMIsNullValue(key)) {
                error_msg = "ERROR: 'VirtualSwitch' parameter is missing the 'Name' property"; 
                goto Exit;
            }
            network_uuid = CMGetCharPtr(key.value.string);
        }
    }
    else
        network_uuid = CMGetCharPtr(argdata.value.string);

    if (_GetArgument(broker, argsin, "UseSSL", CMPI_boolean, &argdata, status))
        /* Should the TargetURI use SSL or not */
        use_ssl = argdata.value.boolean;

    if (_GetArgument(broker, argsin, "TimeoutInMinutes", CMPI_uint32, &argdata, status))
        /* Timeout for connect, after which disconnect automatically kicks in */
        timeout_in_minutes = argdata.value.uint32;

    if (_GetArgument(broker, argsin, "ReadOnly", CMPI_boolean, &argdata, status))
        /* Should the disk image be opened up in read-onyl mode or not */
        read_only = argdata.value.boolean;

    if (_GetArgument(broker, argsin, "NetworkConfiguration", CMPI_stringA, &argdata, status)) {
        /* IP configuration could have been specified */
        CMPIArray *arr = argdata.value.array;
        if(arr) {
            int elems = CMGetArrayCount(arr, NULL);
            if(elems == 3) {
                CMPIData arrelem = CMGetArrayElementAt(arr, 0, NULL);
                ip_address = CMGetCharPtr(arrelem.value.string);
                arrelem = CMGetArrayElementAt(arr, 1, NULL);
                subnet_mask = CMGetCharPtr(arrelem.value.string);
                arrelem = CMGetArrayElementAt(arr, 2, NULL);
                ip_gateway = CMGetCharPtr(arrelem.value.string);
            }
        }
    }

    if (_GetArgument(broker, argsin, "Protocol", CMPI_string, &argdata, status))
        protocol = CMGetCharPtr(argdata.value.string);

    /* Get the VDI and find out what hosts it is avilable on */
    if (xen_vdi_get_by_uuid(session->xen, &vdi, vdi_uuid)) {
        xen_sr sr = NULL;
        if (xen_vdi_get_sr(session->xen, &sr, vdi)) {
            xen_pbd_set *pbd_set = NULL;
            if (xen_sr_get_pbds(session->xen, &pbd_set, sr)) {
                /* Pick the host to start the VM on - one among the many possible will do */
                if (pbd_set->size > 0)
                    xen_pbd_get_host(session->xen, &host, pbd_set->contents[0]);
                xen_pbd_set_free(pbd_set);
            }
            xen_sr_free(sr);
        }
        xen_vdi_free(vdi);
    }
    else {
        error_msg = "ERROR: Specified DiskImage was not found";
        goto Exit;
    }

    rc = Xen_StoragePoolManagementService_ConnectToDiskImages_Failed;
    statusrc = CMPI_RC_ERR_FAILED;

    if (host && vdi_uuid && network_uuid) {
        xen_string_string_map *args = NULL;
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Connecting to VDI %s via network %s using %s (ssl=%s)", 
                                               vdi_uuid, network_uuid, protocol, (use_ssl ?"true":"false")));
        xen_utils_add_to_string_string_map("transfer_mode", protocol, &args); 
        if (use_ssl)
            xen_utils_add_to_string_string_map("use_ssl", "true", &args);
        if (read_only)
            xen_utils_add_to_string_string_map("read_only", "true", &args);
        xen_utils_add_to_string_string_map("vdi_uuid", vdi_uuid, &args);
        xen_utils_add_to_string_string_map("network_uuid", network_uuid, &args); /* could be 'management' */

        if(ip_address && subnet_mask && ip_gateway) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Exposing VDI using %s/%s/%s", 
                                                   ip_address, subnet_mask, ip_gateway)); 
            xen_utils_add_to_string_string_map("network_mode", "manual", &args);
            xen_utils_add_to_string_string_map("network_ip",   ip_address, &args);
            xen_utils_add_to_string_string_map("network_mask", subnet_mask, &args);
            xen_utils_add_to_string_string_map("network_gateway", ip_gateway, &args);
        }
        /* use a 24 hour 'timeout' for unexpose, if unexpose is not called withing that time */
        if(timeout_in_minutes != 0) {
            char timeout_in_minutes_str[100];
            snprintf(timeout_in_minutes_str, sizeof(timeout_in_minutes_str)/sizeof(timeout_in_minutes_str[0])-1, 
                     "%lu", timeout_in_minutes);
            xen_utils_add_to_string_string_map("timeout_minutes", timeout_in_minutes_str, &args);
        }

        /* Start an asynchronous task to connect to the disk image since the connect 
          could take a while (becuase it has to spin up the transfer vm) */
        CMPIObjectPath *job_instance_op = NULL;
        connect_job_context* job_context = calloc(1, sizeof(connect_job_context));
        if (job_context == NULL)
            goto Exit;
        job_context->args = args;
        job_context->use_ssl = use_ssl;
        job_context->vdi_uuid = strdup(vdi_uuid);
        if (!job_create(broker, context, session, JOB_NAME_CONNECTTODISK, vdi_uuid, 
                        _connect_task_callback, job_context, &job_instance_op, status))
            goto Exit;

        /* add the job reference to the output */
        CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
        rc = Xen_StoragePoolManagementService_ConnectToDiskImages_Method_Parameters_Checked___Job_Started;
        statusrc = CMPI_RC_OK;

    }
    Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);

    if (vdi_uuid)
        free(vdi_uuid);
    if (host)
        xen_host_free(host);
    if (host_uuid)
        free(host_uuid);

    return rc;
}

/*
 * _disconnect_task_callback
 * Async job that disconnects the given VDI from the transfer VM.
 * This shuts down the tranfer VM in the process.
 *
 * @param async_job - job parameters
 */
void _disconnect_task_callback(
    void* async_job_handle
    )
{
    int state = JobState_Exception;
    int rc = Xen_StoragePoolManagementService_DisconnectFromDiskImages_Failed;
    char *description = strdup("Error: Unknown error connecting to disk image");
    Xen_job *job = (Xen_job *)async_job_handle;
    char *connect_handle = (char *)job->job_context;
    char *result = NULL;
    xen_utils_session *session = job->session;
    xen_string_string_map *args = NULL;
    CMPIrc statusrc = CMPI_RC_ERR_FAILED;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Disconnect job started"));
    state = JobState_Running;
    job_change_state(job, session, state, 0, 0, NULL);

    xen_utils_add_to_string_string_map("record_handle", connect_handle, &args);
    if (xen_host_call_plugin(session->xen, &result, session->host, "transfer", "unexpose", args) && 
        strcmp(result, "OK") == 0) {
        statusrc = CMPI_RC_OK;
        rc = Xen_StoragePoolManagementService_DisconnectFromDiskImages_Completed_with_No_Error;
    }
    xen_string_string_map_free(args);

    /* update the job */
    if(rc != Xen_StoragePoolManagementService_DisconnectFromDiskImages_Completed_with_No_Error) {
        if(!session->xen->ok) {
            free(description);
            description = xen_utils_get_xen_error(session->xen);
        }
        state = JobState_Exception;
    } else {
        free(description);
        description = NULL;
        state = JobState_Completed;
    }

    job_change_state(job, session, state, 100, rc, description);

    if(result)
        free(result);
    if(description)
        free(description);
    if(connect_handle)
        free(connect_handle);
}
/*
 * disconnect_from_disk_images
 * Function that disconnects the given VDI from the transfer VM.
 *
 * @param session - xen_utils_session object to get to xen
 * @param argsin - 'vdi_uuid' of the disk being 'unexposed'
 *              - 'record_handle' of the transfer record being 'unexposed'
 *
 * @return = one of the DMTF error codes specified in the mof file
 */
int disconnect_from_disk_image(
    const CMPIBroker *broker,
    const CMPIContext *context,
    char *name_space,
    xen_utils_session *session, 
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status
    )
{
    int rc = Xen_StoragePoolManagementService_DisconnectFromDiskImages_Invalid_Parameter;
    CMPIData argdata;
    int statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    char *connect_handle = NULL;

    if (!_GetArgument(broker, argsin, "ConnectionHandle", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: ConnectionHandle parameter was not specified";
        goto Exit;
    }
    else
        /* connection handle from the ConnectToDiskImage call above */
        connect_handle = strdup(CMGetCharPtr(argdata.value.string));

    /* Connection handle has to be specified */
    if(connect_handle) {
        CMPIObjectPath *job_instance_op = NULL;
        if (job_create(broker, context, session, "Xen_DisconnectFromDiskImageJob", 
                       connect_handle, _disconnect_task_callback, connect_handle, &job_instance_op, status)) {
            if(job_instance_op) {
                CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
                statusrc = CMPI_RC_OK;
                rc = Xen_StoragePoolManagementService_DisconnectFromDiskImages_Method_Parameters_Checked___Job_Started;
                error_msg = NULL;
            }
        }
    }

    Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}
/*
 * create_resource_pool
 * Function that creates a Xen Storage Repository .
 *
 * @param broker    - CMPIBroker instance to be able to call CMPI functions
 * @param session   - xen_utils_session object to get to xen
 * @param resource_type - type of SR being created (block or iso)
 * @param element_name - name of the SR to be created
 * @param setting   - RASd instance that contains the settings to create the SR
 * @param[out] poolinstance_op - CIM reference to the newly created pool instance 
 *
 * @return = one of the DMTF error codes specified in the mof file
 */
int create_resource_pool(
    const CMPIBroker *broker,
    const CMPIContext *context,
    char *name_space,
    xen_utils_session *session, 
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status
)
{
    int rc = Xen_StoragePoolManagementService_CreateResourcePool_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    char *name_description = "";
    char * content_type = "user";
    char * type = "lvm";
    bool shared = true;
    uint64_t size = -1;
    xen_string_string_map *device_config = NULL;
    xen_string_string_map *sm_config = NULL;
    xen_sr sr = NULL;
    DMTF_ResourceType resource_type = DMTF_ResourceType_Storage_Extent;
    char *element_name = NULL;
    CMPIInstance *setting_inst = NULL;
    CMPIObjectPath *op = NULL;
    CMPIData argdata;

    //if(!_GetArgument(broker, argsin, "ResourceType", CMPI_string, &argdata, status))
    //     goto Exit;
    resource_type = argdata.value.uint16;
    if (!_GetArgument(broker, argsin, "ElementName", CMPI_string, &argdata, status))
        goto Exit;
    element_name = CMGetCharPtr(argdata.value.string);

    if (_GetArgument(broker, argsin, "Settings", CMPI_string, &argdata, status)) {
        /* The setting could be in MOF string form, parse it and get a CIM instance */
        if (!xen_utils_get_cmpi_instance(broker, &argdata, &op, &setting_inst)) {
            error_msg = "ERROR: 'Setting' parameter is not in the correct MOF string form";
            goto Exit;
        }
    }
    else if (_GetArgument(broker, argsin, "Settings", CMPI_instance, &argdata, status)) {
        setting_inst = argdata.value.inst;
    }
    else {
        /* mandatory parameter setting was not passed */
        error_msg = "ERROR: 'Setting' parameter was not passed in (needs to be in embedded instance form).";
        goto Exit;
    }

    /* Collect all the SR related settings from the 'setting' parameter */
    CMPIData property = CMGetProperty(setting_inst, "Connection", status);
    if (status->rc != CMPI_RC_OK || 
        CMIsNullValue(property)  || 
        (CMGetArrayCount(property.value.array, status) == 0) || (property.type != CMPI_stringA)) {
        error_msg = "ERROR: 'Connection' property couldnt not be found";
        goto Exit;
    }
    else {
        device_config = xen_utils_convert_CMPIArray_to_string_string_map(property.value.array);
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("device_Config has %d items", device_config->size));
    }
    property = CMGetProperty(setting_inst, "ResourceSubType", status);
    if (status->rc != CMPI_RC_OK || CMIsNullValue(property) || (property.type != CMPI_string)) {
        error_msg = "ERROR: The Required 'ResourceSubType' property could not be found";
        goto Exit;
    }
    else {
        type = CMGetCharPtr(property.value.string);
    }
    property = CMGetProperty(setting_inst, "Description", status);
    if (status->rc == CMPI_RC_OK && !CMIsNullValue(property) && (property.type == CMPI_string))
        name_description = CMGetCharPtr(property.value.string);
    property = CMGetProperty(setting_inst, "VirtualQuantity", status);
    if (status->rc == CMPI_RC_OK && !CMIsNullValue(property) && (property.type & CMPI_INTEGER))
        size = property.value.uint64;

    /* setup the rest of the xen parameters for the SR */
    sm_config = xen_string_string_map_alloc(0);
    if (strcmp(type, "nfs") == 0) {
        content_type = "disk";
    }
    else if ((strcmp(type, "lvmoiscsi") == 0) ||
        (strcmp(type, "lvmohba")   == 0)) {
        //No longer anything to do here.
    }
    else if (strcmp(type, "iso") == 0) {
        content_type = "iso";
    }

    /* Create sr */
    rc = Xen_StoragePoolManagementService_CreateResourcePool_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    if (!xen_sr_create(session->xen, &sr, session->host, device_config, 
        size, element_name, name_description, 
        type, content_type, shared, sm_config))
        goto Exit;

    /* Create the output parameter containing the new SR reference */
    xen_sr_record *sr_rec = NULL;
    if (xen_sr_get_record(session->xen, &sr_rec, sr)) {
        CMPIObjectPath *op = create_storage_pool_ref(broker, session, sr_rec);
        if (op) {
            statusrc =  CMPI_RC_OK;
            rc = Xen_StoragePoolManagementService_CreateResourcePool_Job_Completed_with_No_Error;
            CMAddArg(argsout, "Pool", (CMPIValue *)&op, CMPI_ref);
        }
        xen_sr_record_free(sr_rec);
    }

Exit:
    if (device_config)
        xen_string_string_map_free(device_config);
    if (sm_config)
        xen_string_string_map_free(sm_config);
    if (sr)
        xen_sr_free(sr);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    /* unwind here */
    return rc;
}
/*
 * delete_resource_pool
 * Function that forgets or destroys a Xen Storage Repository.
 * If destroy is not allowed on the pool it forgets it.
 *
 * @param broker    - CMPIBroker instance to be able to call CMPI functions
 * @param session   - xen_utils_session object to get to xen
 * @param poolinstance_op - CIM reference to the pool instance 
 *
 * @return = one of the DMTF error codes specified in the mof file
 */
int delete_resource_pool(
    const CMPIBroker *broker,
    xen_utils_session *session, 
    const CMPIArgs *argsin,
    CMPIStatus *status,
    bool forget
    )
{
    int rc = Xen_StoragePoolManagementService_DeleteResourcePool_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    CMPIData argdata, key_property;
    char *pool_uuid = NULL;
    char buf[MAX_INSTANCEID_LEN];

    if (!_GetArgument(broker, argsin, "Pool", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "Pool", CMPI_string, &key_property, status)) {
            error_msg = "ERROR: 'Pool' parameter is not set";
            goto Exit;
        }
    }
    else {
        key_property = CMGetKey(argdata.value.ref, "InstanceID", status);
        if (status->rc != CMPI_RC_OK || CMIsNullValue(key_property)) {
            error_msg = "ERROR: Pool parameter is missing the 'InstanceID' property";
            goto Exit;
        }
    }

    /* Check if the string is in the InstanceID form */
    pool_uuid = _CMPIStrncpyDeviceNameFromID(buf,
                    CMGetCharPtr(key_property.value.string),
                    sizeof(buf));
    if (pool_uuid == NULL)
        /* use the string as is */
        pool_uuid = CMGetCharPtr(key_property.value.string);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Deleting SR %s", pool_uuid));

    rc = Xen_StoragePoolManagementService_DeleteResourcePool_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    xen_sr sr = NULL;
    if (xen_sr_get_by_uuid(session->xen, &sr, pool_uuid)) {
        /* if this is an ISO SR, just forget it, if its a non-ISO SR, destroy it if possible */
        char *content_type = NULL;
        xen_sr_get_content_type(session->xen, &content_type, sr);
        if(content_type) {
            /* unplug all pbds prior to checking what operations we can execute on the SR, as being plugged influences the result */
            xen_pbd_set *pbd_set = NULL;
            if (xen_sr_get_pbds(session->xen, &pbd_set, sr)) {
                int i = 0;
                for (i=0; i<pbd_set->size; i++) {
                    xen_pbd_unplug(session->xen, pbd_set->contents[i]);
                }
                RESET_XEN_ERROR(session->xen); /* reset any erors */
                xen_pbd_set_free(pbd_set);
                pbd_set=NULL;
            }
            RESET_XEN_ERROR(session->xen); /* PBDs not found errors are possible, reset it */

            /* check to see if destroy is allowed on this SR */
            bool destroy_allowed = false;
            if(strcmp(content_type, "iso") != 0) {
                xen_storage_operations_set *allowed_ops = NULL;
                xen_sr_get_allowed_operations(session->xen, &allowed_ops, sr);
                if(allowed_ops && allowed_ops->size > 0) {
                    int i = 0;
                    while(i < allowed_ops->size) {
                        if(allowed_ops->contents[i] == XEN_STORAGE_OPERATIONS_DESTROY){
                            destroy_allowed = true;
                            break;
                        }
                        i++;
                    }
                    xen_storage_operations_set_free(allowed_ops);
                }
            }

            if(!forget && destroy_allowed) {
                /* destroy the SR, as it is allowed and forget was not requested */
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Destroying SR %s", pool_uuid));
                xen_sr_destroy(session->xen, sr);
            }
            else {
                /* forget is always allowed */

                /* destroy the PBDs */
                if (xen_sr_get_pbds(session->xen, &pbd_set, sr)) {
                    int i = 0;
                    for (i=0; i<pbd_set->size; i++) {
                        xen_pbd_destroy(session->xen, pbd_set->contents[i]);
                        RESET_XEN_ERROR(session->xen); /* reset any erors */
                    }
                    xen_pbd_set_free(pbd_set);
                }
                RESET_XEN_ERROR(session->xen); /* PBDs not found errors are possible, reset it */

                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Forgetting SR %s", pool_uuid));
                xen_sr_forget(session->xen, sr);
            }
            free(content_type);
        }
        if(session->xen->ok) {
            rc = Xen_StoragePoolManagementService_DeleteResourcePool_Job_Completed_with_No_Error;
            statusrc = CMPI_RC_OK;
        }
        xen_sr_free(sr);
    }
    Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

/*
 * discover_iscsi_targets
 * Function that discovers the iscsi target IQNs or LUNs that the host knows of
 *
 * @param broker    - CMPIBroker instance to be able to call CMPI functions
 * @param session   - xen_utils_session object to get to xen
 * @param argsin    - 'TargetHost' - iSCSI target host IP address 
 * @param argsin    - 'Port' - iSCSI filer port
 * @param argsin    - 'TargetIQN' - IQN whose LUNs are to be discovered
 * @param argsout   - 'IQNs' - if IQN is not specified in the input, all possible IQNs are discovered and returned as output
 * @param argsout   - 'LUNs' - LUNs associated with IQNs, if one was specifed in the input
 *
 * @return = one of the DMTF error codes specified in the mof file
 */
int discover_iscsi_targets(
    const CMPIBroker *broker,
    const CMPIContext *context,
    char *name_space,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    CMPIArgs *argsout,
    CMPIStatus *status)
{
    CMPIData argdata;
    char *error_msg = "ERROR: Could not get the target IQNs, unknown error";
    char *iscsi_server = NULL, *iscsi_port = "3260", *target_iqn = NULL;
    xen_string_string_map *device_config = NULL, *sm_config = NULL;
    xen_sr sr = NULL;
    int rc = Xen_StoragePoolManagementService_DeleteResourcePool_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;

    if (!_GetArgument(broker, argsin, "TargetHost", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: Target iSCSI Server was not specified in the 'TargetHost' parameter.";
        goto Exit;
    }
    else
        iscsi_server = CMGetCharPtr(argdata.value.string);

    if (_GetArgument(broker, argsin, "Port", CMPI_string, &argdata, status))
        iscsi_port = CMGetCharPtr(argdata.value.string);

    if (_GetArgument(broker, argsin, "TargetIQN", CMPI_string, &argdata, status)) 
        target_iqn = CMGetCharPtr(argdata.value.string);

    /* The way we discover iSCSI target is by trying to create an SR based on the 
       partial info passed in. We expect to get a SR_BANKEND_FAILURE_96 error
       in the xen error_description with target IQNs specified in XML in the 
       form below.
       <iscsi-target-iqns>
         <TGT>
           <Index>0</Index>
           <IPAddress>192.168.5.10</IPAddress>
           <TargetIQN>iqn.1992-08.com.netapp:sn.135049502</TargetIQN>
         </TGT>
         <TGT>
            ....
         </TGT>
       <iscsi-target-iqns> */
    xen_utils_add_to_string_string_map("target", iscsi_server, &device_config);
    xen_utils_add_to_string_string_map("port",   iscsi_port,   &device_config);
    if(target_iqn)
        xen_utils_add_to_string_string_map("targetIQN", target_iqn, &device_config);
    sm_config = xen_string_string_map_alloc(0);
    int64_t size = 2 << 30;
    if (!xen_sr_create(session->xen, &sr, session->host, 
                       device_config, size, "discoverscsiiqns", 
                       "Fake SR to inspect target IQNs", "lvmoiscsi", 
                       "user", true, sm_config)) 
    {
        /* we expect to get the SR_BANKEND_FAILURE_96 error */
        if(session->xen->ok || 
           session->xen->error_description == NULL || 
           session->xen->error_description_count < 4 ||
           (strcmp(session->xen->error_description[0], "SR_BACKEND_FAILURE_96") != 0 &&
           strcmp(session->xen->error_description[0], "SR_BACKEND_FAILURE_107") != 0 &&
           strcmp(session->xen->error_description[0], "SR_BACKEND_FAILURE_87") != 0))
            goto Exit;
        /* Get the target IQNs */
        char *xml = xen_utils_get_xen_error(session->xen);
        if(xml) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Target information: %s", xml));
            char *xmlout = strstr(xml, "<?xml version");
            if(xmlout) {
                rc = Xen_StoragePoolManagementService_DeleteResourcePool_Job_Completed_with_No_Error;
                statusrc = CMPI_RC_OK;
                CMAddArg(argsout, "TargetInfo", xmlout, CMPI_chars);
            }
            free(xml);
        }
    }
    else {
        /* oops, the sr create actually worked */
        xen_sr_destroy(session->xen, sr);
        xen_sr_free(sr);
    }
        
    Exit:
    if(device_config)
        xen_string_string_map_free(device_config);
    if(sm_config)
        xen_string_string_map_free(sm_config);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

