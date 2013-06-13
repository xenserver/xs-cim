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

#include "xen_utils.h"
#include "Xen_VirtualSystemSnapshotService.h"
#include "Xen_VirtualSystemSettingData.h"
#include "providerinterface.h"
#include "Xen_Job.h"

/* external functions used */
CMPIObjectPath *disk_image_create_ref(
    const CMPIBroker *broker,
    const char *name_space,
    xen_utils_session *session,
    char* sr_uuid,
    char* vdi_uuid);
CMPIObjectPath *vssd_create_ref(
    const CMPIBroker *broker,
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec
    );

/* argument for the snapshot tree export */
typedef struct _snapshot_forest_export_context {
    xen_string_string_map *plugin_args;
    bool use_ssl;
    xen_vm vm;
} snapshot_forest_export_context;

/******************************************************************************
 * Internal functions that implement the asynchronous tasks that some of the
 * methods initiate
 *****************************************************************************/
static int _get_metadata_url_for_vm (
    CMPIContext *call_context,
    xen_utils_session *session, 
    xen_vm vm,
    bool use_ssl,
    char **metadata_url_path,
    char **metadata_server_cert,
    char **session_id
    )
{
    CMPIStatus status;
    int rc = 0;
    struct xen_call_context *ctx = NULL;
    if(!xen_utils_get_call_context(call_context, &ctx, &status)){
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt get the call context"));
        return 0;
    }
    /* The session passed in gets logged out when the provider is done with this call. 
       We need a new session  to return back to the caller */
    xen_utils_session* new_session = NULL;
    if(xen_utils_get_session(&new_session, ctx->user, ctx->pw) && (new_session != NULL)) {
        /* The disk snapshot tree metadata is available for download as an xva under the following URL */
        char *ip_address = NULL;
        char *metadata_url_format = "http://%s:80/export_metadata?session_id=%s&amp;ref=%s&amp;include_vhd_parents=true";
        if(use_ssl)
            metadata_url_format = "https://%s:443/export_metadata?session_id=%s&amp;ref=%s&amp;include_vhd_parents=true";

        xen_host_get_address(session->xen, &ip_address, session->host);
        xen_host_get_server_certificate(session->xen, metadata_server_cert, session->host);

        if(ip_address == NULL || (metadata_server_cert == NULL)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt get the server certificate"));
        }
        int buflen = strlen(metadata_url_format) + strlen(ip_address) + (strlen("OpaqueRef:")+GUID_STRLEN)*2 + 1;
        *metadata_url_path = calloc(1, buflen);
        if(*metadata_url_path) {
            snprintf(*metadata_url_path, buflen-1, metadata_url_format, ip_address, new_session->xen->session_id, (char *)vm);
            *session_id = strdup(new_session->xen->session_id);
        }

        /* free the memory used up by the session - dont use xen_cleanup_session since it logs the session out */
        free(ip_address);
        xen_utils_free_session(new_session);
        rc = 1;
    } 
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt get Xen Session"));
    }
    xen_utils_free_call_context(ctx);
    return rc;
}

int _get_disk_urls(
    char *transfer_record, 
    xen_string_set **disk_url_set
    )
{
    xen_string_string_map *tr_rec_map = 
        xen_utils_convert_transfer_record_to_string_map(transfer_record);
    if(tr_rec_map && tr_rec_map->size > 0) {
        int i;
        for(i=0; i<tr_rec_map->size; i++) {
            if(strstr(tr_rec_map->contents[i].key, "url_full_")) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Found DiskURL: %s", tr_rec_map->contents[i].val));
                /* Add a .vhd to the end to get the delta vhd file rather than the raw disk */
                /* NOTE: WE ALWAYS RETURN THE VHD FILE AND NOT THE RAW DISK */
                char *raw_disk_url = tr_rec_map->contents[i].val;
                int len = strlen(raw_disk_url) + strlen(".vhd") + 1;
                char *disk_url = calloc(1, len);
                snprintf(disk_url, len, "%s.vhd", raw_disk_url); /* adding a VHD to the end returns vhd */
                xen_utils_add_to_string_set(disk_url, disk_url_set);
                free(disk_url);
            }
        }
        xen_string_string_map_free(tr_rec_map);
    }
    return 1;
}

/* 
 * StartSnapshotForestExport asynchronous method 
 *
 * Calls the host plugin to spin up a transfer vm to start the snapshot forest 
 * export process. All the VDIs in the forest are then made available
 * to the outside world for import/export purposes 
 */
void _start_snapshot_forest_export_task(
    void* async_job
    )
{
    int state = JobState_Exception;
    int job_error_code = Xen_VirtualSystemSnapshotService_StartSnapshotForestExport_Failed;
    char *description = strdup("Error: Unknown error starting snapshot forest export");

    Xen_job *job = (Xen_job *)async_job;
    snapshot_forest_export_context *exp_ctx = (snapshot_forest_export_context *)job->job_context;
    xen_string_string_map *args = exp_ctx->plugin_args;
    xen_utils_session *session = job->session;
    xen_vm vm = exp_ctx->vm;
    char *session_id = NULL, *metadata_server_cert = NULL, *metadata_url_path = NULL;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("SnapshotForestExport job started"));
    state = JobState_Running;
    job_change_state(job, session, state, 0, 0, NULL);

    state = JobState_Exception;
    job_error_code = Xen_VirtualSystemSnapshotService_StartSnapshotForestExport_Failed;

    /* Make the metadata available for download (export.xva) */
    /* create a new session to be used with the metadata URL, it needs to persist even after this call completes.
      It gets logged out in the end_export call */
    if(_get_metadata_url_for_vm(job->call_context, session, vm, exp_ctx->use_ssl,
                         &metadata_url_path, &metadata_server_cert, &session_id)) {
        xen_task_add_to_other_config(session->xen, job->task_handle, "MetadataURI", metadata_url_path);
    }
    /* expose all the snapshot's VDIs in one shot */
    if(args) {
        char *record_handles = NULL;
        xen_host_call_plugin(session->xen, &record_handles, session->host, "transfer", "expose_forest", args);
        if (record_handles && *record_handles != '\0') {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("RecordHandles from expose_forest: %s", record_handles));
            xen_string_set *disk_url_set = NULL;
            xen_string_set *record_handle_set = xen_utils_copy_to_string_set(record_handles, ",");
            xen_string_set *ssl_cert_set = NULL;

            /* start off by adding the metadata server cert to the server-cert-set*/
            if(exp_ctx->use_ssl)
                xen_utils_add_to_string_set(metadata_server_cert, &ssl_cert_set);

            int i=0;
            /* record handles is a ',' delimited string of record handles for each VDI, get the actual record for each */
            for (;i<record_handle_set->size; i++) {
                xen_string_string_map *newargs = NULL;
                xen_utils_add_to_string_string_map("record_handle", record_handle_set->contents[i], &newargs);
                char *transfer_record = NULL;
                if(xen_host_call_plugin(session->xen, &transfer_record, session->host, "transfer", "get_record", newargs) 
                   && transfer_record) {
                    /* The transfer record contains more than 1 VDI that is part of the snapshot tree branch 
                       The full url to each exposed VDI is to be found as 'url_full_<vdi_uuid>="<url>"*/
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("TransferRecord for handle: %s: ##%s##", 
                                                           record_handle_set->contents[i],
                                                           transfer_record));

                    /* get the server certificate, if available, to go with the disk url */
                    char *cert = xen_utils_get_value_from_transfer_record(transfer_record, "ssl_cert");
                    if (cert) {
                        xen_utils_add_to_string_set(cert, &ssl_cert_set);
                        free(cert);
                    }
                    _get_disk_urls(transfer_record, &disk_url_set);
                    free(transfer_record);
                }
                xen_string_string_map_free(newargs);
            }
            if(disk_url_set && disk_url_set->size > 0) {
                /* Add the output args to the CIM_Job object by setting the right property in the xen_task object */
                char *disk_uris = xen_utils_flatten_string_set(disk_url_set, ",");
                if(disk_uris) {
                    xen_task_add_to_other_config(session->xen, job->task_handle, "DiskImageURIs", disk_uris);
                    free(disk_uris);
                }
                xen_string_set_free(disk_url_set);

                if(ssl_cert_set) {
                    char *certs = xen_utils_flatten_string_set(ssl_cert_set, ",");
                    xen_task_add_to_other_config(session->xen, job->task_handle, "SSLCertificates", certs);
                    free(certs);
                    xen_string_set_free(ssl_cert_set);
                }

                /* we need to return the ExportConnectionHandles so the caller can call us back for cleanup */
                /* NOTE: we'll slide the session id in here, since it needs to get logged out in EndExport */
                int len = strlen(record_handles) + 1 + strlen(session_id) + 1;
                char *export_conn_handle = calloc(1, len);
                snprintf(export_conn_handle, len, "%s,%s", session_id, record_handles);
                xen_task_add_to_other_config(session->xen, job->task_handle, "ExportConnectionHandle", export_conn_handle);
                free(export_conn_handle);

                state = JobState_Completed;
                job_error_code = Xen_VirtualSystemSnapshotService_StartSnapshotForestExport_Completed_with_No_Error;
                free(description);
                description = NULL;
            }
            xen_string_set_free(record_handle_set);
            free(record_handles);
        }
    }

    if(args)
        xen_string_string_map_free(args);
    if(session_id)
        free(session_id);
    if(vm)
        xen_vm_free(vm);
    if(metadata_server_cert)
        free(metadata_server_cert);
    if(metadata_url_path)
        free(metadata_url_path);
    free(exp_ctx);

    if (job_error_code != Xen_VirtualSystemSnapshotService_StartSnapshotForestExport_Completed_with_No_Error) {
        description = xen_utils_get_xen_error(session->xen);
        state = JobState_Exception;
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Changing job state to %d", state));
    job_change_state(job, session, state, 100, job_error_code, description);

    if(description)
        free(description);
}

/* 
 * EndSnapshotForestExport asynchronous method 
 *
 * Calls the host plugin to spin down the transfer vms that were used to expose the snapshot tree 
 */
void _end_snapshot_forest_export_task(
    void* async_job
    )
{
    int state = JobState_Exception;
    int job_error_code = Xen_VirtualSystemSnapshotService_EndSnapshotForestExport_Failed;
    char *description = strdup("Error: Unknown error ending snapshot forest export");
    Xen_job *job = (Xen_job *)async_job;
    char* transfer_handles = (char *)job->job_context;
    xen_utils_session *session = job->session;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("EndSnapshotForestExport job started"));
    state = JobState_Running;
    job_change_state(job, session, state, 0, 0, NULL);

    /* unexpose each transfer handle in the set */
    xen_string_set *transfer_handle_set =
        xen_utils_copy_to_string_set(transfer_handles, ",");
    free(transfer_handles);

    if(transfer_handle_set == NULL || transfer_handle_set->size == 0) {
        description = strdup("Error: Not enough information in the ExportConnectionHandle parameter");
        goto Exit;
    }

    /* session id is the first in the list, pull it out */
    char *session_id = transfer_handle_set->contents[0];
    if(session_id) {
        /* logout the session from the start call */
        xen_session *xsession = calloc(1, sizeof(xen_session));
        xsession->ok = true;
        xsession->call_func = session->xen->call_func;
        xsession->api_version = session->xen->api_version;
        xsession->handle = session->xen->handle;
        xsession->session_id = strdup(session_id);
        xen_session_logout(xsession);
    }

    /* the rest of the strings in the transfer_handle set are actual transfer vm record 
       handles for the VDIs that have been exposed, unexpose them */
    int i;
    for(i=1; i<transfer_handle_set->size; i++) {
        xen_string_string_map *args = NULL;
        char *result = NULL;
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Unexposing Record Handle %s", transfer_handle_set->contents[i]));
        xen_utils_add_to_string_string_map("record_handle", transfer_handle_set->contents[i], &args);
        if(xen_host_call_plugin(session->xen, &result, session->host, "transfer", "unexpose", args) &&
           strcmp(result, "OK") == 0) {
            state = JobState_Completed;
            job_error_code = Xen_VirtualSystemSnapshotService_EndSnapshotForestExport_Completed_with_No_Error;
            free(description);
            description = NULL;
        }
        xen_string_string_map_free(args);
        free(result);
    }
    xen_string_set_free(transfer_handle_set);

Exit:

    if (job_error_code != Xen_VirtualSystemSnapshotService_EndSnapshotForestExport_Completed_with_No_Error) {
        description = xen_utils_get_xen_error(session->xen);
        state = JobState_Exception;
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Changing job state to %d", state));
    job_change_state(job, session, state, 100, job_error_code, description);

    if(description)
        free(description);
}
/******************************************************************************
 * Internal Methods 
 *****************************************************************************/
int create_snapshot(
    const CMPIBroker *broker,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    const CMPIArgs * argsout,
    CMPIStatus *status
    )
{
    xen_vm_record *vm_rec = NULL;;
    xen_vm vm = NULL;
    xen_vm_record *snapshot_rec = NULL;
    char *error_msg = "ERROR: Unknown error";
    int rc = Xen_VirtualSystemSnapshotService_CreateSnapshot_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;

    if (!xen_utils_get_affectedsystem(broker, session, argsin, status, &vm_rec, &vm)) {
        error_msg = "ERROR: Couldn't find the 'AffectedSystem' parameter";
        goto Exit;
    }

    CMPIInstance *vssd_inst = NULL;
    if (!xen_utils_get_vssd_param(broker, session, argsin, "SnapshotSettings", status, &vssd_inst)) {
        error_msg = "ERROR: Couldn't find the 'SnapshotSettings' parameter";
        goto Exit;
    }
    rc = Xen_VirtualSystemSnapshotService_CreateSnapshot_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    /* Convert the VSSD CIM settings to Xen specific settings. */
    if (!vssd_to_vm_rec(broker, vssd_inst, session, false, &snapshot_rec, status)) {
        error_msg = "ERROR: Couldn't parse the 'SnapshotSettings' parameter";
        goto Exit;
    }

    xen_vm snapshot = NULL;
    if (!xen_vm_snapshot(session->xen, &snapshot, vm, snapshot_rec->name_label)) {
        goto Exit;
    }
    xen_vm_record *new_snapshot_rec = NULL;
    if (!xen_vm_get_record(session->xen, &new_snapshot_rec, snapshot)) {
        goto Exit;
    }

    //CMPIObjectPath* job_instance_op = NULL;
    //CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
    CMPIObjectPath *op = snapshot_create_ref(broker, DEFAULT_NS, session, new_snapshot_rec);
    CMAddArg(argsout, "ResultingSnapshot", (CMPIValue *)&op, CMPI_ref);
    xen_vm_record_free(new_snapshot_rec);
    xen_vm_free(snapshot);
    rc = Xen_VirtualSystemSnapshotService_CreateSnapshot_Completed_with_No_Error;
    statusrc = CMPI_RC_OK;

    Exit:
    if (vm_rec)
        xen_vm_record_free(vm_rec);
    if (snapshot_rec)
        xen_vm_record_free(snapshot_rec);
    if (vm)
        xen_vm_free(vm);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

int destroy_snapshot(
    const CMPIBroker *broker,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    const CMPIArgs * argsout,
    CMPIStatus *status
    )
{
    xen_vm vm = NULL;
    xen_vm_record *vm_rec = NULL;
    CMPIData argdata;
    int statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    int rc = Xen_VirtualSystemSnapshotService_DestroySnapshot_Invalid_Parameter;
    char *err_msg = "ERROR: Unknown Error";
    xen_vbd_set *vbd_set = NULL;

    if (!_GetArgument(broker, argsin, "AffectedSnapshot", CMPI_ref, &argdata, status)) {
        err_msg = "ERROR: Couldn't find the 'AffectedSnapshot' parameter";
        goto Exit;
    }
    if (!vssd_find_vm(argdata.value.ref, session, &vm, &vm_rec, status)) {
        err_msg = "ERROR: Couldn't find the system specified in the 'AffectedSnapshot' parameter";
        goto Exit;
    }

    statusrc = CMPI_RC_ERR_FAILED;
    rc = Xen_VirtualSystemSnapshotService_DestroySnapshot_Failed;
    /* cleanup all resources consumed by the snapshot */
    if (xen_vm_get_vbds(session->xen, &vbd_set, vm)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("got %d VBDs for VM", vbd_set->size));
        /* Get the VBD list */
        int i=0;
        for (i=0; i<vbd_set->size; i++) {
            xen_vdi vdi = NULL;
            RESET_XEN_ERROR(session->xen);
            /* get the vdi for each vbd */
            if (xen_vbd_get_vdi(session->xen, &vdi, vbd_set->contents[i])) {
                xen_vbd_set *vbds_using_this_vdi = NULL;
                /* check how many other vbds are using it */
                if (xen_vdi_get_vbds(session->xen, &vbds_using_this_vdi, vdi)) {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("VDI contains %d VBDs", vbds_using_this_vdi->size));
                    /* Destroy if this is the only vbd using it */
                    if (vbds_using_this_vdi->size == 1) {
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("destroying VBD/VDI"));
                        xen_vdi_destroy(session->xen, vdi);
                    }
                    RESET_XEN_ERROR(session->xen); /* reset this for the next iteration */
                    xen_vbd_set_free(vbds_using_this_vdi);
                }
                xen_vdi_free(vdi);
            }
            else {
                RESET_XEN_ERROR(session->xen);  /* reset this for the next iteration */
            }
        }
        xen_vbd_set_free(vbd_set);
    }

    RESET_XEN_ERROR(session->xen);
    if (!xen_vm_destroy(session->xen, vm))
        goto Exit;

    //CMPIObjectPath* job_instance_op = NULL;
    //CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
    rc = Xen_VirtualSystemSnapshotService_DestroySnapshot_Completed_with_No_Error;
    statusrc = CMPI_RC_OK;
Exit:
    if (vm_rec)
        xen_vm_record_free(vm_rec);
    if (vm)
        xen_vm_free(vm);
    xen_utils_set_status(broker, status, statusrc, err_msg, session->xen);
    return rc;
}

int revert_to_snapshot(
    const CMPIBroker *broker,
    xen_utils_session *session,
    const CMPIArgs * argsin,
    const CMPIArgs * argsout,
    CMPIStatus *status
    )
{
    CMPIData argdata;
    int statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    int rc = Xen_VirtualSystemSnapshotService_ApplySnapshot_Invalid_Parameter;
    char *err_msg = "ERROR: Unknown Error";
    xen_vm snapshot = NULL;
    xen_vm_record *vm_rec = NULL;

    if (!_GetArgument(broker, argsin, "Snapshot", CMPI_ref, &argdata, status)) {
        /* return an error */
        err_msg = "ERROR: Couldn't get 'Snapshot' parameter";
        goto Exit;
    }
    if (!vssd_find_vm(argdata.value.ref, session, &snapshot, &vm_rec, status)) {
        err_msg = "ERROR: Couldn't find the snapshot specified in the 'Snapshot' parameter";
        goto Exit;
    }

    //CMPIObjectPath* job_instance_op = NULL;
    //CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
    statusrc = CMPI_RC_ERR_FAILED;
    rc = Xen_VirtualSystemSnapshotService_ApplySnapshot_Failed;

    if(xen_vm_revert(session->xen, snapshot)) {
        rc = Xen_VirtualSystemSnapshotService_ApplySnapshot_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
        err_msg = NULL;
    }

Exit:
    if (vm_rec)
        xen_vm_record_free(vm_rec);
    if (snapshot)
        xen_vm_free(snapshot);
    xen_utils_set_status(broker, status, statusrc, err_msg, session->xen);
    return rc;
}

/**
 * @brief start_snapshot_forest_export - Starts a VM 
 *        snapshot forest for export, which starts the
 *        transfervm for all the VDIs involved, thus making them
 *        available for download
 * @param IN const CMPIBroker * - the broker object handed to 
 *        the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 *  "System" - reference to Xen_ComputerSystem that
 *      reprsents the VM whose snapshot forest is being exported
 * @param OUT CMPIArgs * - output arguments 
 *     "DiskImageURLs" - string representing the URLs to disk
 *     images that needs to be downloaded.
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
int start_snapshot_forest_export(
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIContext *context,
    const CMPIArgs* argsin, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    char *error_msg = "ERROR: Unknown Error";
    int rc = Xen_VirtualSystemSnapshotService_StartSnapshotForestExport_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    CMPIData argdata;
    char *vm_uuid = NULL, *net_uuid = "management";
    char *ip_address_start = NULL, *ip_address_end = NULL, *subnet_mask = NULL, *ip_gateway = NULL;
    bool use_ssl = false;
    unsigned long timeout_in_minutes = 24*60;
    char *export_ctx = NULL;
    bool error_occured = false;
    snapshot_forest_export_context *args = NULL;
    xen_string_string_map *pluginargs = NULL;
    xen_vm vm = NULL;

    /* get the VM UUID */
    if (!_GetArgument(broker, argsin, "System", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "System", CMPI_string, &argdata, status)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("System was not specified"));
            error_msg = "ERROR: 'System' parameter was not specified";
            goto Exit;
        }
        else
            vm_uuid = CMGetCharPtr(argdata.value.string);
    }
    else {
        CMPIData key = CMGetKey(argdata.value.ref, "Name", status);
        vm_uuid = CMGetCharPtr(key.value.string);
    }

    if(_GetArgument(broker, argsin, "ExportContext", CMPI_chars, &argdata, status))
        export_ctx = CMGetCharPtr(argdata.value.string);

    /* get the usessk argument */
    if(_GetArgument(broker, argsin, "UseSSL", CMPI_boolean, &argdata, status))
       use_ssl = argdata.value.boolean;

    /* get the virtual switch (network) UUID */
    if (!_GetArgument(broker, argsin, "VirtualSwitch", CMPI_ref, &argdata, status)) {
        if (_GetArgument(broker, argsin, "VirtualSwitch", CMPI_string, &argdata, status))
            net_uuid = CMGetCharPtr(argdata.value.string);
    }
    else {
        CMPIData key = CMGetKey(argdata.value.ref, "Name", status);
        net_uuid = CMGetCharPtr(key.value.string);
    }

    if (_GetArgument(broker, argsin, "NetworkConfiguration", CMPI_stringA, &argdata, status)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Manual NetworkConfiguration has been specified"));
        /* IP configuration could have been specified */
        CMPIArray *arr = argdata.value.array;
        if(arr) {
            int elems = CMGetArrayCount(arr, NULL);
            if(elems == 4) {
                CMPIData arrelem = CMGetArrayElementAt(arr, 0, NULL);
                ip_address_start = CMGetCharPtr(arrelem.value.string);
                arrelem = CMGetArrayElementAt(arr, 1, NULL);
                ip_address_end = CMGetCharPtr(arrelem.value.string);
                arrelem = CMGetArrayElementAt(arr, 2, NULL);
                subnet_mask = CMGetCharPtr(arrelem.value.string);
                arrelem = CMGetArrayElementAt(arr, 3, NULL);
                ip_gateway = CMGetCharPtr(arrelem.value.string);
            }
        }
    }


    if (_GetArgument(broker, argsin, "TimeoutInMinutes", CMPI_uint32, &argdata, status))
        /* Timeout for connect, after which disconnect automatically kicks in */
        timeout_in_minutes = argdata.value.uint32;


    /* Get the VM, its snapshots and prepare the disks of all associated snapshots for transfer */
    if(xen_vm_get_by_uuid(session->xen, &vm, vm_uuid)) {
        xen_string_set *vm_uuid_set = NULL;
        xen_utils_add_to_string_set(vm_uuid, &vm_uuid_set);
        xen_vm_set *snapshots = NULL;
        if(xen_vm_get_snapshots(session->xen, &snapshots, vm)) {
            int i=0;
            for (;i<snapshots->size; i++) {
                char *snapshot_uuid = NULL;
                xen_vm_get_uuid(session->xen, &snapshot_uuid, snapshots->contents[i]);
                xen_utils_add_to_string_set(snapshot_uuid, &vm_uuid_set);
                free(snapshot_uuid);
            }
            xen_vm_set_free(snapshots);
        }
        RESET_XEN_ERROR(session->xen);
        char *vm_uuids = xen_utils_flatten_string_set(vm_uuid_set, ",");
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("exposing snapshot tree for: %s", vm_uuids));

        xen_utils_add_to_string_string_map("network_uuid", net_uuid, &pluginargs); 
        xen_utils_add_to_string_string_map("read_only", "true", &pluginargs); 
        xen_utils_add_to_string_string_map("vm_uuids", vm_uuids, &pluginargs); 
        if(use_ssl)
            xen_utils_add_to_string_string_map("use_ssl", "true", &pluginargs);

        if(ip_address_start && ip_address_end && subnet_mask && ip_gateway) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Exposing TVMs using %s/%s/%s/%s", 
                                                   ip_address_start, ip_address_end, subnet_mask, ip_gateway)); 
            xen_utils_add_to_string_string_map("network_mode", "manual_range", &pluginargs);
            xen_utils_add_to_string_string_map("network_ip_start",   ip_address_start, &pluginargs);
            xen_utils_add_to_string_string_map("network_ip_end",   ip_address_end, &pluginargs);
            xen_utils_add_to_string_string_map("network_mask", subnet_mask, &pluginargs);
            xen_utils_add_to_string_string_map("network_gateway", ip_gateway, &pluginargs);
        }

        /* use a 24 hour 'timeout' for unexpose, if unexpose is not called withing that time */
        if(timeout_in_minutes != 0) {
            char timeout_in_minutes_str[100];
            snprintf(timeout_in_minutes_str, sizeof(timeout_in_minutes_str)/sizeof(timeout_in_minutes_str[0])-1, 
                     "%lu", timeout_in_minutes);
            xen_utils_add_to_string_string_map("timeout_minutes", timeout_in_minutes_str, &pluginargs);
        }

        xen_string_set_free(vm_uuid_set);
        free(vm_uuids);

        args = calloc(1, sizeof(snapshot_forest_export_context));
        args->vm = vm;
        args->plugin_args = pluginargs;
        args->use_ssl = use_ssl;

        /* Start an asynchronous task to connect to the disk image since the connect 
          could take a while (becuase it has to spin up the transfer vm) */
        CMPIObjectPath *job_instance_op = NULL;
        if (!job_create(broker, context, session, "Xen_StartSnapshotForestExportJob", vm_uuid, 
                        _start_snapshot_forest_export_task, (void *)args, &job_instance_op, status)){
	    error_occured = true;
            goto Exit;
	}

        /* add the job reference to the output */
        CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
        rc = Xen_VirtualSystemSnapshotService_StartSnapshotForestExport_Method_Parameters_Checked___Job_Started;
        statusrc = CMPI_RC_OK;
    }

Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    if(error_occured){
      /* Cleanup not handled by Job, due to failure. Cleanup now instead. */
      if(pluginargs)
	xen_string_string_map_free(pluginargs);
      if(vm)
	xen_vm_free(vm);
      if(args)
	free(args);	
    }
    return rc;
}

/**
 * @brief end_snapshot_forest_export - CLeans up any 
 *        resources consumed during the DIskImageForest Export
 *        process.
 * @param IN const CMPIBroker * - the broker object handed to 
 *        the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 *  "System" - reference to Xen_ComputerSystem that
 *      reprsents the VM whose snapshot forest is being exported
 * @param OUT CMPIArgs * - output arguments 
 *     "DiskImageURLs" - string representing the URLs to disk
 *     images that needs to be downloaded.
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
int end_snapshot_forest_export(
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIContext *context,
    const CMPIArgs* argsin, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    char *error_msg = "ERROR: Unknown Error";
    int rc = Xen_VirtualSystemSnapshotService_EndSnapshotForestExport_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    CMPIData argdata;
    char *transfer_handles=NULL;

    /* Get the export connection handle that we need to unexpose */
    if (!_GetArgument(broker, argsin, "ExportConnectionHandle", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: 'ExportConnectionHandle' parameter was not specified";
        goto Exit;
    }
    else 
        transfer_handles = strdup(CMGetCharPtr(argdata.value.string));

    /* Start an asynchronous task to spin down the transfer vms */
    CMPIObjectPath *job_instance_op = NULL;
    if (!job_create(broker, context, session, "Xen_EndSnapshotForestExportJob", transfer_handles, 
                    _end_snapshot_forest_export_task, (void *)transfer_handles, &job_instance_op, status))
        goto Exit;

    /* add the job reference to the output */
    CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
    rc = Xen_VirtualSystemSnapshotService_EndSnapshotForestExport_Method_Parameters_Checked___Job_Started;
    statusrc = CMPI_RC_OK;

Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;

}
/**
 * @brief prepare_snapshot_forest_import - Prepares the snapshot
 *        forest import process. The caller is expected to have
 *        created a virtual disk and copied the contents of the
 *        snapshot metadata blob on ot it.
 * @param IN const CMPIBroker * - the broker object handed to 
 *        the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 * @param OUT CMPIArgs * - output arguments 
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
int prepare_snapshot_forest_import(
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIContext *context,
    const CMPIArgs* argsin, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    char *error_msg = "ERROR: Unknown Error";
    int rc = Xen_VirtualSystemSnapshotService_PrepareSnapshotForestImport_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    CMPIData argdata;
    char *metadata_vdi_uuid = NULL;

    /* Get the VDI that contains the contents of the snapshot forest metadata blob*/
    if (!_GetArgument(broker, argsin, "MetadataDiskImage", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "MetadataDiskImage", CMPI_string, &argdata, status)) {
            error_msg = "ERROR: 'MetadataDiskImage' parameter was not specified";
            goto Exit;
        }
        else
            metadata_vdi_uuid = strdup(CMGetCharPtr(argdata.value.string));
    }
    else {
        char buf[MAX_INSTANCEID_LEN];
        CMPIData key = CMGetKey(argdata.value.ref, "DeviceID", status);
        _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(key.value.string), sizeof(buf));
        metadata_vdi_uuid = strdup(buf);
    }

    /* get the instruction sequence for re-creating the snapshot forest sequence from the metadata vdi */
    xen_string_string_map *args = NULL;
    xen_utils_add_to_string_string_map("vm_metadata_vdi_uuid", metadata_vdi_uuid, &args);

    rc = Xen_VirtualSystemSnapshotService_PrepareSnapshotForestImport_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    char *result = NULL;
    if(xen_host_call_plugin(session->xen, &result, session->host, "transfer", "get_import_instructions", args)
       && result && *result != '\0')
    {
        CMAddArg(argsout, "ImportContext", result, CMPI_chars);
        free(result);
        rc = Xen_VirtualSystemSnapshotService_PrepareSnapshotForestImport_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
    }
    xen_string_string_map_free(args);

Exit:
    if(metadata_vdi_uuid)
        free(metadata_vdi_uuid);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

/*
 * Following are the instructions that are executed as part of the 
 * import instruction sequence. The metadata blob contains these instructions
 */
static int _exec_create_instruction(
    xen_string_set *instruction_args,
    xen_utils_session *session,
    char *sr_uuid,
    xen_string_string_map **vdi_map,
    char **dest_vdi_uuid,
    char **old_vdi_uuid
    )
{
    xen_sr sr_ref = NULL;
    xen_vdi newvdi = NULL;

    /* instruction is of the form 'create <old-vdi-uuid> <old-vdi-size>' */
    /* Create a new VDI whose size matches the exported one */
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Executing create instruction"));
    if(instruction_args->size != 3)
        return 0;
    if(!xen_sr_get_by_uuid(session->xen, &sr_ref, sr_uuid))
        return 0;

    *old_vdi_uuid = strdup(instruction_args->contents[1]);
    xen_vdi_record *vdi_rec = xen_vdi_record_alloc();
    vdi_rec->virtual_size = atoll(instruction_args->contents[2]);
    vdi_rec->name_label = strdup(*old_vdi_uuid);
    xen_sr_record_opt* sr_opt = xen_sr_record_opt_alloc();
    if(sr_opt) {
        sr_opt->is_record = false;
        sr_opt->u.handle = sr_ref;
        vdi_rec->sr = sr_opt;
    }
    vdi_rec->name_description = strdup("Created during snapshot tree import");
    vdi_rec->type = XEN_VDI_TYPE_USER;
    vdi_rec->sharable = false;
    vdi_rec->sm_config = xen_string_string_map_alloc(0);
    vdi_rec->other_config = xen_string_string_map_alloc(0);
    vdi_rec->xenstore_data = xen_string_string_map_alloc(0);
    if(xen_vdi_create(session->xen, &newvdi, vdi_rec)) {
        xen_vdi_get_uuid(session->xen, dest_vdi_uuid, newvdi);
        if(*dest_vdi_uuid) {
            xen_utils_add_to_string_string_map(
                *old_vdi_uuid, *dest_vdi_uuid, vdi_map);
        }
        xen_vdi_free(newvdi);
    }
    xen_vdi_record_free(vdi_rec);
    return 1;
}

static int _exec_clone_instruction(
    xen_string_set *instruction_args,
    xen_utils_session *session,
    xen_string_string_map **vdi_map,
    char **dest_vdi_uuid,
    char **old_vdi_uuid
    )
{
    xen_vdi parent_vdi = NULL;

    /* instruction is of the form 'clone <child-vdi-uuid> <parent-vdi-uuid>' */
    /* Clone an existing VDI */
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Executing clone instruction"));
    if(instruction_args->size != 3)
        return 0;
    char *child_vdi_uuid = strdup(instruction_args->contents[1]);
    char *parent_vdi_uuid = xen_utils_get_from_string_string_map(
                            *vdi_map, instruction_args->contents[2]);
    if(xen_vdi_get_by_uuid(session->xen, &parent_vdi, parent_vdi_uuid)){
        xen_string_string_map* driver_params = xen_string_string_map_alloc(0);
        xen_vdi newvdi = NULL;
        if(xen_vdi_clone(session->xen, &newvdi, parent_vdi, driver_params) && newvdi) {
            xen_vdi_get_uuid(session->xen, dest_vdi_uuid, newvdi);
            if(*dest_vdi_uuid) {
                xen_utils_add_to_string_string_map(
                    child_vdi_uuid, *dest_vdi_uuid, vdi_map);
            }
            xen_vdi_free(newvdi);
        }
        xen_string_string_map_free(driver_params);
        xen_vdi_free(parent_vdi);
    }
    *old_vdi_uuid = child_vdi_uuid;
    return 1;
}

static int _exec_reuse_instruction(
    xen_string_set *instruction_args,
    xen_string_string_map **vdi_map,
    char **dest_vdi_uuid,
    char **old_vdi_uuid
    )
{
    /* instruction is of the form 'reuse <child-vdi-uuid> <parent-vdi-uuid>' */
    /* reuse an existing vdi */
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Executing reuse instruction"));
    if(instruction_args->size != 3)
        return 0;
    *old_vdi_uuid = strdup(instruction_args->contents[1]);
    *dest_vdi_uuid = strdup(xen_utils_get_from_string_string_map(
                            *vdi_map, instruction_args->contents[2]));
    /* remove the parent uuid from the map */
    xen_utils_remove_from_string_string_map(
        instruction_args->contents[2], vdi_map);

    /* now update the child_uuid's contents with the dest_uuid */
    xen_utils_add_to_string_string_map(
        *old_vdi_uuid, *dest_vdi_uuid, vdi_map);
    return 1;
}

static int _exec_snap_instruction(
    xen_string_set *instruction_args,
    xen_utils_session *session,
    xen_string_string_map **vdi_map
    )
{
    /* instruction is of the form 'snap <vdi-uuid>' */
    /* create a new snapshot of an existing VDI */
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Executing snap instruction"));
    if(instruction_args->size != 2)
        return 0;
    char *dest_uuid = xen_utils_get_from_string_string_map(
                            *vdi_map, instruction_args->contents[1]);
    xen_vdi dest_vdi = NULL;
    if (xen_vdi_get_by_uuid(session->xen, &dest_vdi, dest_uuid)) {
        xen_sr sr = NULL;
        xen_vdi newvdi = NULL;
        xen_vdi_get_sr(session->xen, &sr, dest_vdi);

        xen_string_string_map* driver_params = xen_string_string_map_alloc(0);
        xen_vdi_snapshot(session->xen, &newvdi, dest_vdi, driver_params);
        xen_string_string_map_free(driver_params);
        xen_vdi_destroy(session->xen, dest_vdi);
        xen_vdi_set_name_label(session->xen, newvdi, instruction_args->contents[1]);

        xen_sr_scan(session->xen, sr);
        xen_sr_free(sr);
        xen_vdi_free(dest_vdi);
        char *dest_vdi_uuid = NULL;
        xen_vdi_get_uuid(session->xen, &dest_vdi_uuid, newvdi);
        if(dest_vdi_uuid) {
            xen_utils_add_to_string_string_map(
                instruction_args->contents[1], dest_vdi_uuid, vdi_map);
            free(dest_vdi_uuid);
        }
        xen_vdi_free(newvdi);
    }
    /* no disk to be returned to caller */
    return 1;
}

static int _exec_leaf_instruction(
    xen_string_set *instruction_args,
    xen_utils_session *session,
    xen_string_string_map **vdi_map
    )
{
    /* instruction is of the form 'leaf <vdi-uuid>' */
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Executing leaf instruction"));
    if(instruction_args->size != 2)
        return 0;
    char *dest_uuid = xen_utils_get_from_string_string_map(
                            *vdi_map, instruction_args->contents[1]);
    xen_vdi dest_vdi = NULL;
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("dest_vdi: %s, old_vdi: %s", 
                                           dest_uuid,instruction_args->contents[1]));
    if(xen_vdi_get_by_uuid(session->xen, &dest_vdi, dest_uuid)) {
        xen_vdi_set_name_label(session->xen, dest_vdi, instruction_args->contents[1]);
        //xen_utils_add_to_string_string_map(
        //    instruction_args->contents[1], dest_uuid, &vdi_map);
        xen_vdi_free(dest_vdi);
    }
    /* no disk to be returned to caller */
    return 1;
}



/**
 * @brief cleanup_snapshot_forest_import - removes state
 *        created by a snapshot tree import by removing the
 *        VDIs that have been created.
 * @param IN const CMPIBroker * - the broker object handed to 
 *        the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 * @param OUT CMPIArgs * - output arguments 
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
int cleanup_snapshot_forest_import(
   const CMPIBroker* broker,
   xen_utils_session* session,
   const CMPIContext *context,
   const CMPIArgs* argsin,
   CMPIArgs* argsout,
   CMPIStatus *status)
{
  char *error_msg = "ERROR: Unkown Error";
  int rc = Xen_VirtualSystemSnapshotService_CleanupSnapshotForestImport_Invalid_Parameter;
  CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
  CMPIData argdata;
  xen_string_string_map *vdi_map = NULL;
  xen_string_string_map *args = NULL;
  char *metadata_vdi_uuid = NULL;

    /* Get the VDI that contains the contents of the snapshot forest metadata blob*/
    if (!_GetArgument(broker, argsin, "MetadataDiskImage", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "MetadataDiskImage", CMPI_string, &argdata, status)) {
            error_msg = "ERROR: 'MetadataDiskImage' parameter was not specified";
            goto Exit;
        }
        else
            metadata_vdi_uuid = strdup(CMGetCharPtr(argdata.value.string));
    }
    else {
        char buf[MAX_INSTANCEID_LEN];
        CMPIData key = CMGetKey(argdata.value.ref, "DeviceID", status);
        _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(key.value.string), sizeof(buf));
        metadata_vdi_uuid = strdup(buf);
    }


  if (!_GetArgument(broker, argsin, "DiskImageMap", CMPI_string, &argdata, status)){
    error_msg = "ERROR: 'DiskImageMap' parameter was not specified";
    goto Exit;
  } else {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Disk_Map%s", CMGetCharPtr(argdata.value.string)));
    vdi_map = xen_utils_convert_string_to_string_map(CMGetCharPtr(argdata.value.string), ",");
  }

  //Flatten the map into a coma seperated list of vdi_uuids
  
  /* Calculate size of buffer needed to hold the flattened map. */
  unsigned int i = 0;
  unsigned int size = 0;
  char *flat_map = NULL;

  if (vdi_map == NULL || vdi_map->size == 0){
    error_msg = "ERROR: The DiskImageMap parameter is invalid.";
    goto Exit;
  }
  
  //Initialise the size to include the metadata vdi_uuid
  size = strlen(metadata_vdi_uuid) + 1;

  do {
    size += strlen(vdi_map->contents[i].val);
    size += 1; //for the ',' seperating the values.
    i++;
  } while(i < vdi_map->size);

  flat_map = (char *) calloc(1, size + 1);

  if (flat_map == NULL) {
    error_msg = "ERROR: Memory allocation failure";
    goto Exit;
  }

  /*Copy the metadata vdi_uuid into the flattened string */
  strcat(flat_map, metadata_vdi_uuid);
  if (vdi_map->size != 0)
    strcat(flat_map, ",");
  /*Copy the values from the disk maps into the string */
  for( i = 0; i < vdi_map->size; i++) {
    strcat(flat_map, vdi_map->contents[i].val);
    if ((i + 1) == vdi_map->size)
      break;
    strcat(flat_map, ",");
  }

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Flattend Disk List: %s", flat_map));

  char *response = NULL;
  xen_utils_add_to_string_string_map("vdi_uuids", flat_map, &args);
  xen_host_call_plugin(session->xen, &response, session->host, "transfer", "cleanup_import", args);
  free(flat_map);
  if(response == NULL)
    goto Exit;
 
  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("CancelSnapshotForestImport method completed successfully"));
  statusrc = CMPI_RC_OK;
  
Exit:
  if (args)
    xen_string_string_map_free(args);
  if (metadata_vdi_uuid)
    free(metadata_vdi_uuid);
  xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
  return rc;
}

/**
 * @brief create_next_disk_in_import_sequence - Begins 
 *        the process of instantiating the next disk in the
 *        snapshot forest import sequence. The client is
 *        expected to upload contents of the disk using BITS.
 * @param IN const CMPIBroker * - the broker object handed to 
 *        the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 * @param OUT CMPIArgs * - output arguments 
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
int create_next_disk_in_import_sequence(
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIContext *context,
    const CMPIArgs* argsin, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    char *error_msg = "ERROR: Unknown Error";
    int rc = Xen_VirtualSystemSnapshotService_CreateNextDiskInImportSequence_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    CMPIData argdata;
    char *import_sequence = NULL, *next_instruction = NULL;
    xen_string_string_map *vdi_map = NULL;
    char* dest_vdi_uuid = NULL;
    char* old_vdi_uuid = NULL;
    char *sr_uuid = NULL;
    xen_string_set *instruction_args = NULL;

    /* Get the instruction sequence that tells us what to do with the next disk */
    if (!_GetArgument(broker, argsin, "ImportContext", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: 'ImportContext' parameter was not specified";
        goto Exit;
    }
    else
        import_sequence = strdup(CMGetCharPtr(argdata.value.string));
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Import Sequence: %s", import_sequence));

    /* Get the VDI map that maps the old disk UUIDs to the new ones */
    if (!_GetArgument(broker, argsin, "DiskImageMap", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: 'DiskImageMap' parameter was not specified";
        goto Exit;
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Disk_Map:%s", CMGetCharPtr(argdata.value.string)));
        vdi_map = xen_utils_convert_string_to_string_map(CMGetCharPtr(argdata.value.string), ",");

	if (vdi_map->size > 0) {
	  int i;
	  for(i=0;i< vdi_map->size; i++){
	      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Key:%s,Val:%s", vdi_map->contents[i].key, vdi_map->contents[i].val));   
	  }
	}
    }

    /* Get the VDI map that maps the old disk UUIDs to the new ones */
    if (!_GetArgument(broker, argsin, "StoragePool", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "StoragePool", CMPI_string, &argdata, status)) {
            error_msg = "ERROR: 'StoragePool' parameter was not specified";
            goto Exit;
        }
        else
            sr_uuid = strdup(CMGetCharPtr(argdata.value.string));
    }
    else {
        char buf[MAX_INSTANCEID_LEN];
        CMPIData key = CMGetKey(argdata.value.ref, "InstanceID", status);
        _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(key.value.string), sizeof(buf));
        sr_uuid = strdup(buf);
    }

    /* each instruction is of the form '<command> <cmdparam1> <cmdparam2> ...' */
    if(import_sequence && *import_sequence != '\0' && *import_sequence != ' ') {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Import instruction sequence:\n%s\n", import_sequence));
        rc = Xen_VirtualSystemSnapshotService_CreateNextDiskInImportSequence_Failed;
        statusrc = CMPI_RC_ERR_FAILED;
        char *tmp = import_sequence;
	char *p;
        /*Cycle through the given instructions and remove carriage returns \r*/
        while((p = strchr(tmp,'\r')) != NULL){
              _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("A Carriage Return symbol has been found! '\r' This may stop us from parsing the input correctly."));
        }
        while((dest_vdi_uuid == NULL) && tmp) {
            /* Keep executing instructions till the next disk in the sequence has been created */
            /* the newly created disk will then have to be returned to caller so they can upload disk contents */
            next_instruction = strchr(tmp, '\n'); /* instructions are newline delimited */
            if(next_instruction)
                *next_instruction++ = '\0';
            instruction_args = xen_utils_copy_to_string_set(tmp, " "); /* instruction args are space delimited */
            if(instruction_args && instruction_args->size > 0) {
                if(strcmp(instruction_args->contents[0], "create") == 0) {
                    if(!_exec_create_instruction(instruction_args, session, sr_uuid, 
                                                 &vdi_map, &dest_vdi_uuid, &old_vdi_uuid))
                        goto Exit;
                }
                else if(strcmp(instruction_args->contents[0], "clone") == 0) {
                    if(!_exec_clone_instruction(instruction_args, session, 
                                                &vdi_map, &dest_vdi_uuid, &old_vdi_uuid))
                        goto Exit;
                }
                else if(strcmp(instruction_args->contents[0], "reuse") == 0) {
                    if(!_exec_reuse_instruction(instruction_args, 
                                                &vdi_map, &dest_vdi_uuid, &old_vdi_uuid))
                        goto Exit;
                }
                else if(strcmp(instruction_args->contents[0], "snap") == 0) {
                    if(!_exec_snap_instruction(instruction_args, session, &vdi_map))
                        goto Exit;
                }
                else if(strcmp(instruction_args->contents[0], "leaf") == 0) {
                    if(!_exec_leaf_instruction(instruction_args, session, &vdi_map))
                        goto Exit;
                }
                else if(strcmp(instruction_args->contents[0], "pass") == 0) {
                    /* pass */
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Executing pass instruction"));
                }
                else {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Unknown instruction"));
                    goto Exit;
                }
                xen_string_set_free(instruction_args);
            }
            tmp = next_instruction; /* iterate over next instruction if no new disk that needs to be returned to caller is created */
        }

        /* update the output arguments for the next instruction in the sequence */
        if(session->xen->ok) {
            if(dest_vdi_uuid) {
                /* this need to  be returnred to caller only if the old disk contents need to be copied over */
                CMPIObjectPath *disk_image_op = disk_image_create_ref(broker, DEFAULT_NS, session, sr_uuid, dest_vdi_uuid);
                CMAddArg(argsout, "NewDiskImage", &disk_image_op, CMPI_ref);
                CMAddArg(argsout, "OldDiskID", old_vdi_uuid, CMPI_chars); /* caller needs to identify disk to copy contents from */
            }
            char *vdi_map_str = xen_utils_flatten_string_string_map(vdi_map);
            CMAddArg(argsout, "DiskImageMap", vdi_map_str, CMPI_chars);
            free(vdi_map_str);
            statusrc = CMPI_RC_OK;
            if(next_instruction) {
                CMAddArg(argsout, "ImportContext", next_instruction, CMPI_chars);
                rc = Xen_VirtualSystemSnapshotService_CreateNextDiskInImportSequence_Completed_with_No_Error;
            }
            else {
                /* there are no more instructions to execute for the next iteration */
                rc = Xen_VirtualSystemSnapshotService_CreateNextDiskInImportSequence_No_more_Disks_to_be_Imported;
            }
        }
    }
    else {
        /* no import instructions in sequence */
        rc = Xen_VirtualSystemSnapshotService_CreateNextDiskInImportSequence_No_more_Disks_to_be_Imported;
        statusrc = CMPI_RC_ERR_NOT_FOUND;
    }

Exit:
    if(import_sequence)
        free(import_sequence);
    if(vdi_map)
        xen_string_string_map_free(vdi_map);
    if(sr_uuid)
        free(sr_uuid);
    if(old_vdi_uuid)
        free(old_vdi_uuid);
    if(dest_vdi_uuid)
        free(dest_vdi_uuid);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

/**
 * @brief finalize_snapshot_forest_import - Finalizes the 
 *        snapshot forest import process by remapping the VM and
 *        all its disks
 * @param IN const CMPIBroker * - the broker object handed to 
 *        the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 * @param OUT CMPIArgs * - output arguments 
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
int finalize_snapshot_forest_import (
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIContext *context,
    const CMPIArgs* argsin, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    char *error_msg = "ERROR: Unknown Error";
    int rc = Xen_VirtualSystemSnapshotService_FinalizeSnapshotForestImport_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    CMPIData argdata;
    char *metadata_vdi_uuid = NULL;
    char *vdi_map = NULL;
    char *sr_uuid = NULL;
    xen_string_string_map *args = NULL;

    /* Get the VDI that contains the contents of the snapshot forest metadata blob*/
    if (!_GetArgument(broker, argsin, "MetadataDiskImage", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "MetadataDiskImage", CMPI_string, &argdata, status)) {
            error_msg = "ERROR: 'MetadataDiskImage' parameter was not specified";
            goto Exit;
        }
        else
            metadata_vdi_uuid = strdup(CMGetCharPtr(argdata.value.string));
    }
    else {
        char buf[MAX_INSTANCEID_LEN];
        CMPIData key = CMGetKey(argdata.value.ref, "DeviceID", status);
        _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(key.value.string), sizeof(buf));
        metadata_vdi_uuid = strdup(buf);
    }

    /* Get the VDI map that maps the old disk UUIDs to the new ones */
    if (!_GetArgument(broker, argsin, "DiskImageMap", CMPI_string, &argdata, status)) {
        error_msg = "ERROR: 'DiskImageMap' parameter was not specified";
        goto Exit;
    }
    else
        vdi_map = CMGetCharPtr(argdata.value.string);

    /* Get the Storage pool */
    if (!_GetArgument(broker, argsin, "StoragePool", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "StoragePool", CMPI_string, &argdata, status)) {
            error_msg = "ERROR: 'StoragePool' parameter was not specified";
            goto Exit;
        }
        else
            sr_uuid = strdup(CMGetCharPtr(argdata.value.string));
    }
    else {
        char buf[MAX_INSTANCEID_LEN];
        CMPIData key = CMGetKey(argdata.value.ref, "InstanceID", status);
        _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(key.value.string), sizeof(buf));
        sr_uuid = strdup(buf);
    }

    /* call the transfer plugin to remap the vm and all its disks */
    rc = Xen_VirtualSystemSnapshotService_FinalizeSnapshotForestImport_Failed;
    statusrc  = CMPI_RC_ERR_FAILED;
    xen_utils_add_to_string_string_map("vm_metadata_vdi_uuid", metadata_vdi_uuid, &args);
    xen_utils_add_to_string_string_map("vdi_map", vdi_map, &args);
    xen_utils_add_to_string_string_map("sr_uuid", sr_uuid, &args);
    char *result = NULL;
    if(xen_host_call_plugin(session->xen, &result, session->host, "transfer", "remap_vm", args) 
       && result) {
        xen_vm_record *vm_rec = NULL;
        xen_vm vm = NULL;
        if(xen_vm_get_by_uuid(session->xen, &vm, result) &&
           xen_vm_get_record(session->xen, &vm_rec, vm)) {
            CMPIObjectPath *op = vm_create_ref(broker, DEFAULT_NS, session, vm_rec);
            CMAddArg(argsout, "VirtualSystem", &op, CMPI_ref);
            rc = Xen_VirtualSystemSnapshotService_FinalizeSnapshotForestImport_Completed_with_No_Error;
            statusrc  = CMPI_RC_OK;
            xen_vm_record_free(vm_rec);
            xen_vm_free(vm);
        }
        free(result);
    }

Exit:
    if(args)
        xen_string_string_map_free(args);
    if(metadata_vdi_uuid)
        free(metadata_vdi_uuid);
    if(sr_uuid)
        free(sr_uuid);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}
/******************************************************************************
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 *****************************************************************************/
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
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
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

    int argcount = CMGetArgCount(argsin, NULL);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- argsin=%d", argcount));

    argdata = CMGetKey(reference, "Name", &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        CMSetStatusWithChars(broker, &status, CMPI_RC_ERR_NOT_FOUND, 
            "Couldnt find the Xen_VirtualSystemSnapshotService object to invoke the method on. Have you specified the 'Name' key ?");
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt find the Xen_VirtualSystemSnapshotService object to invoke the method on"));
        goto Exit;
    }

    if (strcmp(methodname, "ApplySnapshot") == 0) {
        rc = revert_to_snapshot(broker, session, argsin, argsout, &status);
    }
    else 
    if (strcmp(methodname, "DestroySnapshot") == 0) {
        rc = destroy_snapshot(broker, session, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "CreateSnapshot") == 0) {
        rc = create_snapshot(broker, session, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "StartSnapshotForestExport") == 0) {
        rc = start_snapshot_forest_export(broker, session, context, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "EndSnapshotForestExport") == 0) {
        rc = end_snapshot_forest_export(broker, session, context, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "PrepareSnapshotForestImport") == 0) {
        rc = prepare_snapshot_forest_import(broker, session, context, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "CreateNextDiskInImportSequence") == 0) {
        rc = create_next_disk_in_import_sequence(broker, session, context, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "CleanupSnapshotForestImport") == 0) {
        rc = cleanup_snapshot_forest_import(broker, session, context, argsin, argsout, &status);
    }
    else if (strcmp(methodname, "FinalizeSnapshotForestImport") == 0) {
        rc = finalize_snapshot_forest_import(broker, session, context, argsin, argsout, &status);
    }
    else
        status.rc = CMPI_RC_ERR_METHOD_NOT_AVAILABLE;

Exit:
    if(session)
        xen_utils_cleanup_session(session);
    if (ctx)
        xen_utils_free_call_context(ctx);

    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);
    _SBLIM_RETURNSTATUS(status);
}

/* CMPI Method provider function table setup */
XenMethodMIStub(Xen_VirtualSystemSnapshotService)


