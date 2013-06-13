// Copyright (C) 2006 IBM Corporation
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
////    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
// ============================================================================
// Authors:       Dr. Gareth S. Bestor, <bestor@us.ibm.com>
// Contributors:  Jim Fehlig, <jfehlig@novell.com>
//		  Raj Subrahmanian <raj.subrahmanian@unisys.com>
// Description:
// ============================================================================

/* Include the required CMPI data types, function headers, and macros */
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

/* Xen API headers */
#include <limits.h>
#include "cmpiutil.h"
#include "providerinterface.h"
#include "RASDs.h"
#include "Xen_HostComputerSystem.h"
#include "Xen_VirtualSystemManagementService.h"
#include "Xen_Job.h"
#include "Xen_VirtualSystemSettingData.h"

/* external functions used */
CMPIObjectPath *template_create_ref(
    const CMPIBroker *broker,
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec
    );

/* Async task support */
typedef struct _copy_vm_job_context {
    CMPIInstance* vsSettingDataInst;
    xen_vm_record *vm_rec;
    xen_vm vm_to_copy_from;
    xen_sr sr_to_use;
} copy_vm_job_context;

typedef struct _add_resources_job_context {
    CMPIInstance* vsSettingDataInst;
    xen_vm_record *vm_rec;
    bool memory_and_proc_need_updating;
    xen_vbd_record_set *vbd_recs;
    xen_vdi_record_set *vdi_recs;
    xen_sr_set *srs;                       /* SR to create new disks in */
    xen_vif_record_set *vifs;
    xen_console_record *con_rec;
    bool remove_vm_on_error; /* if there was an error, delete the vm*/
    bool provision_disks;    /* provision the disks from the template */
}add_resources_job_context;

#define JOB_NAME_VSMS "Xen_VirtualSystemManagementServiceJob"
#define JOB_NAME_ADD "Xen_VirtualSystemModifyResourcesJob"
#define JOB_NAME_CREATE "Xen_VirtualSystemCreateJob"
#define JOBSTATE_ADD_STARTED "Started adding resources to VM"

void add_resources_job(void* job);
void copy_vm_job(void* async_job);

/* Internal functions */
static xen_vm _get_reference_configuration(
    const CMPIBroker *broker, 
    const CMPIArgs* argsin,     
    xen_utils_session *session,
    CMPIStatus *status
    );
static int _get_affected_configuration(
    const CMPIBroker *broker, 
    xen_utils_session *session,
    const CMPIArgs* argsin,
    CMPIStatus *status,
    xen_vm_record **vm_rec_out,
    xen_vm *vm_out
    );
static int _vm_add_resources(
    Xen_job *job,
    xen_utils_session* session,
    add_resources_job_context* job_ctx,
    int *kensho_error_code,         /* out */
    CMPIStatus *status);           /* out - status of the call */
static int _vm_add_resource(
    const CMPIBroker *broker,
    xen_utils_session *session,
    xen_vm vm, 
    xen_vm_record *vmRec, 
    CMPIInstance *sourceSettingInst, 
    CMPIObjectPath **resultSetting,
    char *nameSpace,
    CMPIStatus *status);
static int _vm_destroy(
    xen_utils_session *session, 
    xen_vm vm,
    bool remove_associated_disks);
static int _vm_vbds_destroy(
    xen_utils_session *session,
    xen_vm vm
    );
static int _vbd_destroy(
    xen_utils_session *session,
    xen_vbd vbd,
    bool destroy_associated_vdi
    );
static int _vm_provision_disks(
    const CMPIBroker *broker,
    xen_utils_session *session,
    xen_vm vm,
    xen_vm_record *new_vm_rec,
    xen_sr sr_to_use,
    CMPIStatus *status
    );
static xen_sr _sr_find_default(
    xen_utils_session *session
    );
static int _rasd_parse(
    const CMPIBroker *broker,
    xen_utils_session *session,
    CMPIData *setting_data,      /* in - rasd setting data */
    vm_resource_operation add_remove_or_replace, /* for proc and mem rasds */
    xen_vm *vm,                    /* in/out - vm handle */
    xen_vm_record **vm_rec,        /* in/out - vm record */
    bool strict_checks,
    xen_vbd_record_set **vbd_recs, /* out */
    xen_vdi_record_set **vdi_recs, /* out */
    xen_sr_set         **srs,      /* out */
    xen_vif_record_set **vifs,     /* out */
    xen_console_record **con_rec,  /* out */
    kvp **kvp_rec,                 /* out */
    CMPIStatus* status);
static void _parsed_devices_free(
    xen_vm_record *vm_rec,
    xen_sr_set* srs,
    xen_vbd_record_set *vbd_recs,
    xen_vdi_record_set *vdi_recs,
    xen_vif_record_set *vif_recs,
    xen_console_record *con_rec,
    kvp *kvp_rec
    );
static int _kvp_rasd_to_kvp(
    const CMPIBroker *broker,
    CMPIInstance *kvp_rasd,
    kvp **kvp_obj,
    CMPIStatus *status
    );
/*******************************************************************************
 * Following are the CIM methods defined/exported by the 
 * Xen_VirtualSystemManagementService class.
 *******************************************************************************/
/**
* @brief StartService - Enable the host for virtualization
* @param None
* @return int - 0 on success, non-zero on error
*/
static int StartService(xen_utils_session* session)
{
    int rc;
    xen_host host;
    if (!xen_session_get_this_host(session->xen, &host, session->xen)) {
        return 1;
    }
    rc = xen_host_enable(session->xen, host);
    xen_host_free(host);
    return 0;
}
/** 
 * @brief StopService - Disables the host for Virtualization
 * @param None
 * @return int - 0 on success, non-zero on error
 */
static int StopService(xen_utils_session* session)
{
    int rc;
    xen_host host;
    if (!xen_session_get_this_host(session->xen, &host, session->xen)) {
        return 1;
    }

    rc = xen_host_disable(session->xen, host);
    xen_host_free(host);
    return 0;
}
/** 
 * @brief DestroySystem - Destroys a VM
 * @param IN const CMPIBroker * - the broker object handed to 
 *              the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Arguments 
 *          "AffectedSystem" - reference to Xen_ComputerSystem
 * @param IN const CMPIContext *context - A CMPI provided 
 *              context object that's used while spawning off
 *              worker threads
 * @param IN char * - nameSpace for the CIM call 
 * @param OUT CMPIArgs * - output arguments 
 *          "Job" - reference to the Xen_Job object to be
 *              checked for completion
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
static int DestroySystem(
    const CMPIBroker *broker,
    xen_utils_session* session,
    const CMPIArgs * argsin,
    int argsInCount,
    const CMPIContext* context,
    char *nameSpace,
    CMPIArgs* argsout,
    CMPIStatus *status
    )
{
    int rc = VSMS_DestroySystem_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    xen_vm_record *vm_rec = NULL;
    char *status_msg = "ERROR: Unknown Error";
    xen_vm vm = NULL;

    /* Check that the method has the correct number of arguments. */
    if (argsInCount != 1) {
        status_msg = "ERROR: Incorrect number of method arguments";
        goto Exit;
    }

    /* Get affected domain */
    if (!xen_utils_get_affectedsystem(broker, session, argsin, status, &vm_rec, &vm)) {
        status_msg = "ERROR: Missing 'AffectedSystem' parameter";
        goto Exit;
    }

    rc = VSMS_DestroySystem_Invalid_State;
    statusrc = CMPI_RC_ERR_FAILED;
    enum xen_vm_power_state power_state;
    if (!xen_vm_get_power_state(session->xen, &power_state, vm)) {
        status_msg = "ERROR: Unable to determine power state of Xen_ComputerSystem instance.";
        goto Exit;
    }

    if (power_state != XEN_VM_POWER_STATE_HALTED) {
        if (!xen_vm_clean_shutdown(session->xen, vm))
            status_msg = "ERROR: Unable to cleanly shutdown the Xen_ComputerSystem instance.";
        goto Exit;
    }

    /* Before destroying VM, make sure that the VDIs used by this VM are cleaned up */
    if (!_vm_destroy(session, vm, true)) {
        rc = VSMS_DestroySystem_Failed;
        status_msg = "ERROR: Unable to destroy the Xen_ComputerSystem instance.";
    }
    else {
        rc = VSMS_DestroySystem_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
    }

    Exit:
    if (vm_rec)
        xen_vm_record_free(vm_rec);
    if (vm)
        xen_vm_free(vm);

    xen_utils_set_status(broker, status, statusrc, status_msg, session->xen);
    return rc;
}

/** 
 * @brief ModifySystemSettings - Modifies the VM settings
 * @param IN const CMPIBroker * - the broker object handed to 
 *              the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Arguments 
 *          "SystemSettings" - reference to new Xen_ComputerSystemSettingData 
 *               that contain properties to use to set the VM
 * @param IN const CMPIContext *context - A CMPI provided 
 *              context object that's used while spawning off
 *              worker threads
 * @param IN char * - nameSpace for the CIM call 
 * @param OUT CMPIArgs * - output arguments 
                None
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
static int ModifySystemSettings(
    const CMPIBroker *broker,
    xen_utils_session* session,
    const CMPIArgs* argsin,
    int argsInCount,
    const CMPIContext* context,
    char *nameSpace,
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    CMPIData argdata;
    char *status_msg = "ERROR: Unknown Error";
    int rc = VSMS_ModifySystemSettings_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;

    /* Check that the method has the correct number of arguments. */
    if (argsInCount != 1) {
        status_msg = "ERROR: Incorrect number of method arguments, Expecting 1.";
        goto Exit;
    }

    /* Get object path of domain */
    if (!_GetArgument(broker, argsin, "SystemSettings", CMPI_instance, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "SystemSettings", CMPI_string, &argdata, status)) {
            status_msg = "ERROR: Missing SystemSettings parameter."; 
            goto Exit;
        }
    }

    /* if the argument was a string, parse it */
    CMPIInstance* modified_inst = NULL; 
    if (argdata.type == CMPI_string) {
        char* newSetting = CMGetCharPtr(argdata.value.string);
        modified_inst = xen_utils_parse_embedded_instance(broker, newSetting);
        if (modified_inst == NULL) {  /* parser returns zero for success, non-zero for error */
            status_msg = "ERROR: Error parsing the 'SystemSettings' parameter";
            goto Exit;
        }
    }
    else if (argdata.type == CMPI_instance) {
        modified_inst = argdata.value.inst; 
    }

    xen_vm vm = NULL; xen_vm_record *vm_rec = NULL;
#ifdef SFCB
    CMPIObjectPath *op = CMGetObjectPath(modified_inst, status); 
#else
    /* The object path from instance created by pegasus doesnt contain the proper object path, create one now */
    CMPIData inst_id_prop = CMGetProperty(modified_inst, "InstanceID", NULL);
    CMPIObjectPath *op = CMNewObjectPath(broker, nameSpace, "Xen_ComputerSystemSettingData", NULL);
    CMAddKey(op, "InstanceID", CMGetCharPtr(inst_id_prop.value.string), CMPI_chars);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("INstanceID: %s", CMGetCharPtr(inst_id_prop.value.string)));
#endif
    if (!vssd_find_vm(op, session, &vm, &vm_rec, status)) {
        status_msg = "ERROR: Couldnt find the Xen_ComputerSystem associated with the VSSD.";
        statusrc = CMPI_RC_ERR_NOT_FOUND;
        goto Exit;
    }

    vssd_modify(session, vm, vm_rec, modified_inst, NULL);
    xen_vm_record_free(vm_rec);
    xen_vm_free(vm);
    rc = VSMS_ModifySystemSettings_Completed_with_No_Error;
    statusrc = CMPI_RC_OK;

    Exit:
    xen_utils_set_status(broker, status, statusrc, status_msg, session->xen);
    return rc;
}
/** 
 * @brief AddResourceSetting - Adds one resource to the VM
 * @param IN const CMPIBroker * - the broker object handed to 
 *              the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 *    "AffectedSystem" - reference to Xen_ComputerSystem that
 *          the resource will be added to
 *    "ResourceSetting"- instance of
 *    CIM_ResourceAllocationSettingData (memory, processor, disk
 *    or network) to be added to the VM
 * @param IN const CMPIContext *context - A CMPI provided 
 *              context object that's used while spawning off
 *              worker threads
 * @param IN char * - nameSpace for the CIM call 
 * @param OUT CMPIArgs * - output arguments 
 *     "Job" - reference to Xen_Job to be checked for completion
 *     "ResultingResourceSetting" - reference to
 *     CIM_ResourceAllocationSettingData for the the newly
 *     created resource
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
static int AddResourceSetting(
    const CMPIBroker *broker,
    xen_utils_session* session,
    const CMPIArgs* argsin,
    int argsInCount,
    const CMPIContext* context,
    char *nameSpace,
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    int rc = VSMS_AddResourceSetting_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *status_msg = "ERROR: Unknown Error";
    xen_vm_record* vm_rec = NULL;
    xen_vm vm = NULL;
    CMPIData argdata;
    memset(&argdata, 0, sizeof(argdata));
    CMPIStatus localstatus = {CMPI_RC_OK, NULL};

    if (argsInCount != 2) {
        status_msg = "ERROR: Wrong number of arguments passed into method. Expecting 2.";
        goto Exit;
    }

    /* Get vm record based on VirtualSystemSettingData object path. */
    if (!xen_utils_get_affectedsystem(broker, session, argsin, &localstatus, &vm_rec, &vm)) {
        status_msg = "ERROR: Couldn't get the 'AffectedSystem' parameter";
        goto Exit;
    }

    /*
     * Get input ResourceAllocationSettingData instance
     * that will be added to domain.
     */
    if (!_GetArgument(broker, argsin, "ResourceSetting", CMPI_instance, &argdata, &localstatus)) {
        if (!_GetArgument(broker, argsin, "ResourceSetting", CMPI_string, &argdata, &localstatus)) {
            status_msg = "ERROR: Couldn't get the 'ResourceSetting' parameter";
            goto Exit;
        }
    }
    /* if the argument was a string, parse it */
    CMPIInstance* newSettingInst; 
    if (argdata.type == CMPI_string) {
        char* newSetting = CMGetCharPtr(argdata.value.string);
        newSettingInst = xen_utils_parse_embedded_instance(broker, newSetting);
        if (newSettingInst == NULL) { /* parser returns zero for success, non-zero for error */
            status_msg = "ERROR: Failed to parse the 'ResourceSetting' parameter.";
            goto Exit;
        }
    }
    else if (argdata.type == CMPI_instance) {
        newSettingInst = argdata.value.inst; 
    }
    else
    {
        assert(false);
    }

    CMPIObjectPath *resultSetting = NULL;
    rc = VSMS_AddResourceSetting_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    if (!_vm_add_resource(broker, session, vm, vm_rec, newSettingInst, &resultSetting, nameSpace, &localstatus)) {
        status_msg = "ERROR: Failed to add the resource to the Xen_ComputerSystem instance";
        goto Exit;
    }

    rc = VSMS_AddResourceSetting_Completed_with_No_Error;
    statusrc = CMPI_RC_OK;
    CMAddArg(argsout, "ResultingResourceSetting", (CMPIValue*)&resultSetting, CMPI_ref);

    Exit:
    if (vm)
        xen_vm_free(vm);
    if (vm_rec)
        xen_vm_record_free(vm_rec);

    xen_utils_set_status(broker, status, statusrc, status_msg, session->xen);
    return rc;
}

/** 
 * @brief AddDeleteOrResourceSetting - Adds one resource to the 
 *        VM
 * @param IN const CMPIBroker * - the broker object handed to 
 *              the provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 *    "AffectedSystem" - reference to Xen_ComputerSystem that
 *          the resource will be added to
 *    "ResourceSetting"- instance of
 *    CIM_ResourceAllocationSettingData (memory, processor, disk
 *    or network) to be added to the VM
 * @param IN const CMPIContext *context - A CMPI provided 
 *              context object that's used while spawning off
 *              worker threads
 * @param IN char * - nameSpace for the CIM call 
 * @param OUT CMPIArgs * - output arguments 
 *     "Job" - reference to Xen_Job to be checked for completion
 *     "ResultingResourceSetting" - reference to
 *     CIM_ResourceAllocationSettingData for the the newly
 *     created resource
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
static int AddDeleteOrModifyResourceSettings(
    const CMPIBroker *broker,
    xen_utils_session* session,
    const CMPIArgs* argsin,
    int argsInCount,
    const CMPIContext* context,
    char *nameSpace,
    CMPIArgs* argsout, 
    CMPIStatus *status,
    vm_resource_operation op)
{
    int rc = VSMS_AddResourceSettings_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown Error";
    xen_vm_record* vm_rec = NULL;
    xen_vm vm = NULL;
    CMPIData argdata;
    CMPIInstance* vsSettingDataInst = NULL; 
    bool strict_checks = true;

    if (((op == resource_add) && argsInCount != 2) || 
        ((op == resource_modify || op == resource_delete) && argsInCount != 1)) {
        error_msg = "ERROR: Wrong number of arguments passed in. Expecting 2.";
        goto Exit;
    }

    /* Get vm record based on VirtualSystemSettingData object path. */
    if (op == resource_add) {
        if (!_get_affected_configuration(broker, session, argsin, status, &vm_rec, &vm)) {
            error_msg = "ERROR: Couldn't get the 'AffectedConfiguration' parameter";
            goto Exit;
        }
    }
    else {
        strict_checks = false;
    }

    /*
     * Get input ResourceAllocationSettingData instance
     * that will be added to domain.
     */
    if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_instanceA, &argdata, status)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Not CMPI_instanceA"));
        if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_stringA, &argdata, status)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Not CMPI_stringA"));
            if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_refA, &argdata, status)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Not CMPI_refA"));
                if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_instance, &argdata, status)) { /* not an array, single resource case */
                    if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_string, &argdata, status)) { /* not an array, single resource case */
                        error_msg = "ERROR: Couldn't find the 'ResourceSettings' parameter.";
                        goto Exit;
                    }
                }
            }
        }
    }

    if ((CMPI_ARRAY & argdata.type) || (CMPI_instance & argdata.type) || 
        (CMPI_chars & argdata.type) || (CMPI_string & argdata.type)) {
        /* Go through each resource specfied in the RASd array and add it to the list to be added/deleted/modified */
        xen_vbd_record_set *vbd_recs = NULL;
        xen_vdi_record_set *vdi_recs = NULL;
        xen_sr_set *srs = NULL;
        xen_vif_record_set *vif_recs = NULL;
        xen_console_record *con_rec = NULL;
	kvp *kvp_rec = NULL;
        int i= 0;

	_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Resources Count %d",CMGetArrayCount(argdata.value.array, NULL)));
        error_msg = "ERROR: Couldn't parse the 'ResourceSettings' parameter array";
        if (CMPI_ARRAY & argdata.type) {
            for (i = 0; i < CMGetArrayCount(argdata.value.array, NULL); i++) {
                CMPIData setting_data = CMGetArrayElementAt(argdata.value.array, i, status);
                if ((status->rc != CMPI_RC_OK) || CMIsNullValue(setting_data))
                    goto Exit;
                if (!_rasd_parse(broker, session, &setting_data, op, &vm, &vm_rec, strict_checks, &vbd_recs, &vdi_recs, &srs, &vif_recs, &con_rec, &kvp_rec, status)) {
		  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("flag"));
                    goto Exit;
		}
            }
        }
        else {
	  if (!_rasd_parse(broker, session, &argdata, op, &vm, &vm_rec, strict_checks, &vbd_recs, &vdi_recs, &srs, &vif_recs, &con_rec, &kvp_rec, status)) {
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("flag"));
                goto Exit;
	  }
        }


	if (op == resource_add && kvp_rec) {
	   /* Create KVP object - no async */	  
	  kvp_rec->vm_uuid = strdup(vm_rec->uuid);
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Push KVP %s (%s)", kvp_rec->key, kvp_rec->vm_uuid));
    
	  if (xen_utils_push_kvp(session, kvp_rec) != Xen_KVP_RC_OK) {
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error occured pushing KVP"));
	    xen_utils_free_kvp(kvp_rec);
	    goto Exit;
	  }
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Returned OK"));
	  xen_utils_free_kvp(kvp_rec);
	  rc = VSMS_AddResourceSetting_Completed_with_No_Error;
	  statusrc = CMPI_RC_OK;
	  goto Exit;
	  
	}
        else if (op == resource_add) {
	    /* Create the async job */
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Resource Add"));
            add_resources_job_context* job_context = calloc(1, sizeof(add_resources_job_context));
            job_context->vsSettingDataInst = vsSettingDataInst;
            job_context->vm_rec = vm_rec;
            job_context->vbd_recs = vbd_recs;
            job_context->vdi_recs = vdi_recs;
            job_context->srs = srs;
            job_context->vifs = vif_recs;
            job_context->con_rec = con_rec;
            job_context->remove_vm_on_error = false;

            /* do the processor and memory update inline */
            proc_rasd_modify(session, vm, vm_rec);
            memory_rasd_modify(session, vm, vm_rec);

            /* add devices */
            CMPIObjectPath* job_instance_op = NULL;
            if(!job_create(broker, context, session, JOB_NAME_ADD, vm_rec->name_label, 
                           add_resources_job, job_context, &job_instance_op, status)) {
                error_msg = "ERROR: Couldnt' prepare the AddResource job";
                goto Exit;
            }

            CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
            rc = VSMS_AddResourceSettings_Method_Parameters_Checked___Job_Started;
            statusrc = CMPI_RC_OK;
            vm_rec = NULL; /* Do not delete this since its been passed along to the job thread */
        }
        else {
            /* resource delete or modify */
	     if (op == resource_delete && kvp_rec) {
		  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Delete KVP record"));
		  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Key: %s Value: %s", kvp_rec->key, kvp_rec->value));

		  if(xen_utils_delete_kvp(session, kvp_rec) != Xen_KVP_RC_OK) {
            error_msg = "ERROR: Failed to delete KVP. Please check the guest service is running.";
		    rc = VSMS_RemoveResourceSettings_Failed;
                    statusrc = CMPI_RC_ERR_FAILED;
		    xen_utils_free_kvp(kvp_rec);
		    goto Exit;
		  }
		  
		  xen_utils_free_kvp(kvp_rec);
		  rc = VSMS_RemoveResourceSettings_Completed_with_No_Error;
		  statusrc = CMPI_RC_OK;
		
		  goto Exit;
	     }

            if (vm_rec) {
                rc = VSMS_ModifyResourceSettings_Completed_with_No_Error;
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- Modifying VM Processor and Memory"));

                /* Common to modify and delete */
                proc_rasd_modify(session, vm, vm_rec);
                memory_rasd_modify(session, vm, vm_rec);
                if(!session->xen->ok)
                    goto Exit;
            }

            statusrc = CMPI_RC_OK;
            if (op == resource_modify) {
                rc = VSMS_ModifyResourceSettings_Completed_with_No_Error;
                if (vbd_recs) {
                    for (i=0; i<vbd_recs->size; i++) {
                        RESET_XEN_ERROR(session->xen);
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- Modifying VBD %s and VDI %s", vbd_recs->contents[i]->uuid,
                            vdi_recs->contents[i]->uuid));
                        if (!disk_rasd_modify(session, vbd_recs->contents[i], vdi_recs->contents[i])) {
                            rc = VSMS_ModifyResourceSettings_Failed;
                            statusrc = CMPI_RC_ERR_FAILED;
                            error_msg = "ERROR: Failed to modify at least one disk resource";
                        }
                    }
                }
                if (vif_recs) {
                    for (i=0; i<vif_recs->size; i++) {
                        RESET_XEN_ERROR(session->xen);
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- Modifying VIF %s", vif_recs->contents[i]->uuid));
                        if (!network_rasd_modify(session, vif_recs->contents[i])) {
                            rc = VSMS_ModifyResourceSettings_Failed;
                            statusrc = CMPI_RC_ERR_FAILED;
                            error_msg = "ERROR: Failed to modify at least one network interface resource";
                        }
                    }
                }
            }
            else if (op == resource_delete) {
	      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Action: resource_delete"));
                rc = VSMS_RemoveResourceSettings_Completed_with_No_Error;
                int i=0;
                if (vbd_recs) {
                    for (i=0; i<vbd_recs->size; i++) {
                        RESET_XEN_ERROR(session->xen);
                        xen_vbd vbd = NULL;
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- Deleting VBD %s", vbd_recs->contents[i]->uuid));
                        if (xen_vbd_get_by_uuid(session->xen, &vbd, vbd_recs->contents[i]->uuid)) {
                            if (!_vbd_destroy(session, vbd, false)) {
                                rc = VSMS_RemoveResourceSettings_Failed;
                                statusrc = CMPI_RC_ERR_FAILED;
                                error_msg = "ERROR: Failed to delete at least one disk resource";
                            }
                            xen_vbd_free(vbd);
                        }
                        else {
                            rc = VSMS_RemoveResourceSettings_Failed;
                            statusrc = CMPI_RC_ERR_FAILED;
                            error_msg = "ERROR: Failed to delete at least one disk resource";
                        }
                    }
                }
                if (vif_recs) {
                    for (i=0; i<vif_recs->size; i++) {
                        RESET_XEN_ERROR(session->xen);
                        xen_vif vif = NULL;
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- Deleting VIF %s", vif_recs->contents[i]->uuid));
                        if (xen_vif_get_by_uuid(session->xen, &vif, vif_recs->contents[i]->uuid)) {
                            if (!xen_vif_destroy(session->xen, vif)) {
                                rc = VSMS_RemoveResourceSettings_Failed;
                                statusrc = CMPI_RC_ERR_FAILED;
                                error_msg = "ERROR: Failed to delete at least one NetworkPort resource";
                            }
                            xen_vif_free(vif);
                        }
                        else {
                            rc = VSMS_RemoveResourceSettings_Failed;
                            statusrc = CMPI_RC_ERR_FAILED;
                            error_msg = "Failed to delete at least one NetworkPort resource";
                        }
                    }
                }

            }
            /* free all the devices that we got out of parsing */
            _parsed_devices_free(NULL, srs, vbd_recs, vdi_recs, vif_recs, con_rec, kvp_rec);
        }
    }

    Exit:
    if (vm)
        xen_vm_free(vm);
    if (vm_rec)
        xen_vm_record_free(vm_rec);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}
/** 
 * @brief DefineSystem - Creates a new VM 
 * @param IN const 
 *              CMPIBroker * - the broker object handed to the
 *              provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 *  "SystemSetting" - instance of
 *      Xen_ComputerSystemSettingData that defines the VM
 *      settings
 *  "ResourceSetting"- instance of
 *      CIM_ResourceAllocationSettingData (memory, processor,
 *      disk or network) to be added to the VM
 * @param IN const CMPIContext *context - A CMPI provided 
 *      context object that's used while spawning off worker
 *      threads
 * @param IN char * - nameSpace for the CIM call 
 * @param OUT CMPIArgs * - output arguments 
 *     "Job" - reference to Xen_Job to be checked for completion
 *     "ResultingSystem" - reference to CIM_ComputerSystem for
 *     the the newly created VM
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
static int DefineSystem(
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIArgs* argsin, 
    int argsInCount,
    const CMPIContext* context,
    char *nameSpace, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    CMPIData argdata;
    CMPIInstance* vsSettingDataInst = NULL; 
    bool error_occured = false;
    bool spawn_async_thread = false;
    xen_vm vm = NULL;
    xen_vm_record* vm_rec = NULL;
    xen_vbd_record_set *vbd_recs = NULL;
    xen_vdi_record_set *vdi_recs = NULL;
    xen_sr_set *srs = NULL;
    xen_vif_record_set *vif_recs = NULL;
    xen_console_record *con_rec = NULL;
    char* error_msg = "ERROR: Unknown Error";
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    int rc = VSMS_DefineSystem_Invalid_Parameter;
    CMPIObjectPath* job_instance_op = NULL;
    bool provision_disks = false;
    bool mem_proc_update = false;
    kvp *kvp_rec = NULL;

    /* Get the template to create the VM From. This could be empty if the 
     * caller has passed in all the information in the SystemSettings field */
    xen_vm template_vm = NULL;
    bool strict_checks = true; /* validate VSSD for completeness of required properties, if a template is not specified */

    template_vm = _get_reference_configuration(broker, argsin, session, status);
    if (template_vm == NULL && (status->rc != CMPI_RC_ERR_NOT_FOUND)) {
        error_msg = "ERROR: Couldnt get the 'ReferenceConfiguration' parameter";
	error_occured = true;
        goto Exit;
    }

    /* If template VM is specifed, ALL properties are not required in the VSSD */
    if (template_vm)
        strict_checks = false;

    /* Get embedded instance of VirtualSystemSettingData. */
    if (!xen_utils_get_vssd_param(broker, session, argsin, "SystemSettings", status, &vsSettingDataInst)) {
        error_msg = "ERROR: Couldn't find or parse the 'SystemSettings' parameter";
	error_occured = true;
        goto Exit;
    }
    /* Convert the VSSD CIM settings to Xen specific settings. */
    if (!vssd_to_vm_rec(broker, vsSettingDataInst, session, strict_checks, &vm_rec, status)) {
        error_msg = "ERROR: 'SystemSettings' parameter doesnt contain enough properties to create a VM";
	error_occured = true;
        goto Exit;
    }

    /* 
     * Get input ResourceAllocationSettingData instance
     * that will be added to domain.
     */
    if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_instanceA, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_stringA, &argdata, status)) {
            if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_refA, &argdata, status)) {
                if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_instance, &argdata, status)) { /* not an array, single resource case */
                    if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_string, &argdata, status)) { /* not an array, single resource case */
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- No ResourceSettingData passed in"));
                        //goto Exit; No ResourceSetting is acceptable. We'll create an empty VM
                    }
                }
            }
        }
    }

    /* Get the RASD (in array form, or string form)*/
    if (status->rc == CMPI_RC_OK && ((CMPI_ARRAY & argdata.type) || (CMPI_instance & argdata.type) || 
        (CMPI_chars & argdata.type) || (CMPI_string & argdata.type))) {
        if (CMPI_ARRAY & argdata.type) {
            int i= 0;
            for (i = 0; i < CMGetArrayCount(argdata.value.array, NULL); i++) {
                CMPIData setting_data = CMGetArrayElementAt(argdata.value.array, i, status);
                if ((status->rc != CMPI_RC_OK) || CMIsNullValue(setting_data)) {
                    error_msg = "ERROR: Couldn't parse the ResourceSettings array";
		    error_occured = true;
                    goto Exit;
                }

                if (!_rasd_parse(broker, session, &setting_data, resource_modify, 
                    &vm, &vm_rec, true, &vbd_recs, &vdi_recs,  
                    &srs, &vif_recs, &con_rec, &kvp_rec,status)) {
                    error_msg = "ERROR: Couldn't parse the 'ResourceSettings' array";
		    error_occured = true;
                    goto Exit;
                }
                spawn_async_thread = true; /* since there are resources to be added, we better do this async */
            }
        }
        else {
            if (!_rasd_parse(broker, session, &argdata, resource_modify, 
                &vm, &vm_rec, true, &vbd_recs, &vdi_recs, &srs, &vif_recs, &con_rec, &kvp_rec, status)) {
                if (status->rc == CMPI_RC_ERR_NOT_FOUND) {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- ResourceSettingData is empty, skipping addresource"));
                    status->rc = CMPI_RC_OK;
                }
            }
        }
    }
    RESET_XEN_ERROR(session->xen);

    /* We have the VM settings ready.  First create the vm. */
    rc = VSMS_DefineSystem_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    if (template_vm != NULL) {
        xen_vm_record *new_vm_rec = NULL;
        /* Clone from template, and set additional VM settings */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Cloning template VM and calling it %s", vm_rec->name_label));

        if (xen_vm_clone(session->xen, &vm, template_vm, vm_rec->name_label)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Modifying the new VM"));

            /* spawn a thread to provision disks, since it could take time (debian etch template installs the OS too) */
            spawn_async_thread = true;
            provision_disks = true;
            mem_proc_update = true;

            /* get the newly created VM's record */
            xen_vm_get_record(session->xen, &new_vm_rec, vm);

            /* template uses a different set of memory and processor settings, 
             * than what's specified in ResourceSettings, do the processor & memory update inline */
            new_vm_rec->vcpus_at_startup = vm_rec->vcpus_at_startup;
            new_vm_rec->vcpus_max = vm_rec->vcpus_max;

            new_vm_rec->memory_dynamic_max = vm_rec->memory_dynamic_max;
            new_vm_rec->memory_static_max = vm_rec->memory_static_max;
            new_vm_rec->memory_dynamic_min = vm_rec->memory_dynamic_min;
            new_vm_rec->memory_static_min = vm_rec->memory_static_min;
            vssd_modify(session, vm, new_vm_rec, vsSettingDataInst, NULL);

            xen_vm_record_free(vm_rec);
            vm_rec = new_vm_rec;
        }
        xen_vm_free(template_vm);
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Creating New VM '%s'", vm_rec->name_label));

        /* If we dont have any CPUs and memory specified in the VM 
         * (in the case of a basic VM create with no RASDs specified) then specify defaults */
        if (vm_rec->vcpus_max == 0)
            set_processor_defaults(vm_rec);
        if (vm_rec->memory_static_max == 0)
            set_memory_defaults(vm_rec);

        if (xen_vm_create(session->xen, &vm, vm_rec)) {
            xen_vm_record_free(vm_rec); /* Free this record, we'll get a fresh one below */
            xen_vm_get_record(session->xen, &vm_rec, vm);

            /* do the processor & memory update inline */
            proc_rasd_modify(session, vm, vm_rec);
            memory_rasd_modify(session, vm, vm_rec);
        }
    }
    if (!session->xen->ok) {
	error_occured = true;
        goto Exit;
    }
   

    /* Return the objectpath for the resulting DomU in the output args */
    CMPIObjectPath* op = vm_create_ref(broker, nameSpace, session, vm_rec);
    CMAddArg(argsout, "ResultingSystem", (CMPIValue*)&op, CMPI_ref);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- VirtualSystem=%s", CMGetCharPtr(CDToString(broker, op, NULL))));

    /* Now launch the job to add the actual RASD specified devices, including imported disks */
    if (spawn_async_thread) {
        /* Create the async job, import, especially could take a while */
        add_resources_job_context* job_context = calloc(1, sizeof(add_resources_job_context));
        job_context->vsSettingDataInst = vsSettingDataInst;
        job_context->provision_disks = provision_disks;
        job_context->vm_rec = vm_rec;
        job_context->vbd_recs = vbd_recs;
        job_context->vdi_recs = vdi_recs;
        job_context->srs = srs;
        job_context->vifs = vif_recs;
        job_context->con_rec = con_rec;
        job_context->remove_vm_on_error = true;
        job_context->memory_and_proc_need_updating = mem_proc_update;

        if (!job_create(broker, context, session, JOB_NAME_ADD, vm_rec->name_label, 
            add_resources_job, job_context, &job_instance_op, status)) {
            error_msg = "ERROR: Couldnt create the CIM_VirtualSystemManagementServiceJob to schedule an async operation";
	    error_occured = true;
            goto Exit;
        }

        vm_rec = NULL; /* do not release this */
        CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
        rc = VSMS_DefineSystem_Method_Parameters_Checked___Job_Started;
        statusrc =  CMPI_RC_OK;
        error_msg = NULL;
    }
    else {
        /* since we are not spawning a thread, we are done */
        rc = VSMS_DefineSystem_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
        error_msg = NULL;
    }
 Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    if(!session->xen->ok) {
        /* Unwind this mess. */
        _vm_destroy(session, vm, true);
        vm = NULL;
    }
    if(!spawn_async_thread || error_occured)
      _parsed_devices_free(NULL, srs, vbd_recs, vdi_recs, vif_recs, con_rec, NULL);
    if (vm)
        xen_vm_free(vm);
    if (vm_rec)
        xen_vm_record_free(vm_rec);
    return rc;
}

/** 
 * @brief CopySystem - Copies a new VM from a template or a VM 
 * @param IN const 
 *              CMPIBroker * - the broker object handed to the
 *              provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 *  "SystemSetting" - instance of
 *      Xen_ComputerSystemSettingData that defines the VM
 *      settings
 * @param IN const CMPIContext *context - A CMPI provided 
 *      context object that's used while spawning off worker
 *      threads
 * @param IN char * - nameSpace for the CIM call 
 * @param OUT CMPIArgs * - output arguments 
 *     "Job" - reference to Xen_Job to be checked for completion
 *     "ResultingSystem" - reference to CIM_ComputerSystem for
 *     the the newly created VM
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
static int CopySystem(
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIArgs* argsin, 
    int argsInCount,
    const CMPIContext* context,
    char *nameSpace, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    int rc = VSMS_DefineSystem_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    CMPIInstance* vsSettingDataInst = NULL; 
    char buf[MAX_INSTANCEID_LEN];
    CMPIObjectPath* job_instance_op = NULL;
    char *error_msg = "ERROR: Unknown Error";
    xen_vm template_vm = NULL;
    xen_vm_record *vm_rec = NULL;
    xen_sr sr_to_use= NULL;
    CMPIData argdata;

    memset(buf, 0, sizeof(buf));
    if (!_GetArgument(broker, argsin, "StoragePool", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "StoragePool", CMPI_string, &argdata, status)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("StoragePool was not specified"));
        }
        else
            strncpy(buf, CMGetCharPtr(argdata.value.string), sizeof(buf)-1);
    }
    else {
        argdata = CMGetKey(argdata.value.ref, "InstanceID", status);
        _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(argdata.value.string), sizeof(buf)-1);
    }

    if (buf[0] != '\0')
        xen_sr_get_by_uuid(session->xen, &sr_to_use, buf);

    template_vm = _get_reference_configuration(broker, argsin, session, status);
    if (template_vm == NULL) {
        error_msg = "ERROR: Couldnt get the 'ReferenceConfiguration' parameter";
        goto Exit;
    }

    /* Get embedded instance of VirtualSystemSettingData. */
    if (!xen_utils_get_vssd_param(broker, session, argsin, "SystemSettings", status, &vsSettingDataInst)) {
        error_msg = "ERROR: Couldn't find or parse the 'SystemSettings' parameter";
        goto Exit;
    }
    /* Convert the VSSD CIM settings to Xen specific settings. */
    if (!vssd_to_vm_rec(broker, vsSettingDataInst, session, false, &vm_rec, status)) {
        error_msg = "ERROR: 'SystemSettings' parameter doesnt contain enough properties to create a VM";
        goto Exit;
    }

    rc = VSMS_DefineSystem_Failed;
    statusrc = CMPI_RC_ERR_FAILED;

    /* Now launch the job to add the actual RASD specified devices, including 
       imported disks. Create the async job, import, especially could take a while */
    copy_vm_job_context* job_context = calloc(1, sizeof(copy_vm_job_context));
    job_context->vsSettingDataInst = CMClone(vsSettingDataInst, NULL);
    job_context->vm_rec = vm_rec;
    job_context->vm_to_copy_from = template_vm;
    job_context->sr_to_use = sr_to_use;
    if (!job_create(broker, context, session, JOB_NAME_CREATE, vm_rec->name_label, 
        copy_vm_job, job_context, &job_instance_op, status))
        goto Exit;

    /* The ResultingSystem is not available until the async thread's done 
      Set it as part of the job property
      CMPIObjectPath* op = vm_create_ref(broker, nameSpace, session, vm_rec);
      CMAddArg(argsout, "ResultingSystem", (CMPIValue*)&op, CMPI_ref);
     */

    CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
    rc = VSMS_DefineSystem_Method_Parameters_Checked___Job_Started;
    statusrc =  CMPI_RC_OK;

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;

    Exit:
    if (template_vm)
        xen_vm_free(template_vm);
    if (vm_rec)
        xen_vm_record_free(vm_rec);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

/** 
 * @brief ConvertToXenTemplate - Convert an existing VM into a 
 *        Xen template. This is a destructive operation.
 * @param IN const 
 *              CMPIBroker * - the broker object handed to the
 *              provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 *  "System" - reference to Xen_ComputerSystem object
 * @param IN const CMPIContext *context - A CMPI provided 
 *      context object that's used while spawning off worker
 *      threads
 * @param IN char * - nameSpace for the CIM call 
 * @param OUT CMPIArgs * - output arguments 
 *     "Resultingtemplate" - reference to
 *     Xen_ComputerSystemTemplate for the the newly created
 *     template
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */
static int ConvertToXenTemplate(
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIArgs* argsin, 
    int argsInCount,
    const CMPIContext* context,
    char *nameSpace, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    int rc = VSMS_DefineSystem_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown Error";
    char *vm_uuid = NULL;
    CMPIData argdata;
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

    /* Get the VM and convert it to a template */
    if (xen_vm_get_by_uuid(session->xen, &vm, vm_uuid)) {
        if (xen_vm_set_is_a_template(session->xen, vm, true)) {
            xen_vm_record *vm_rec = NULL;
            if (xen_vm_get_record(session->xen, &vm_rec, vm)) {
                CMPIObjectPath *op = template_create_ref(broker, DEFAULT_NS, session, vm_rec);
                CMAddArg(argsout, "ResultingTemplate", (CMPIValue *)&op, CMPI_ref);
                statusrc = CMPI_RC_OK;
                rc = VSMS_DefineSystem_Completed_with_No_Error;
                xen_vm_record_free(vm_rec);
            }
        }
    }

    Exit:
    if (vm)
        xen_vm_free(vm);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

/** 
 * @brief FindPossibleHostsToBootOn - Find the possible hosts that a VM 
 *        can boot on.
 * @param IN const 
 *              CMPIBroker * - the broker object handed to the
 *              provider from the CMPI interface
 * @param IN const CMPIArgs * - input Argument 
 *          "System" - reference to Xen_ComputerSystem object
 * @param IN const CMPIContext *context - A CMPI provided 
 *      context object that's used while spawning off worker
 *      threads
 * @param IN char * - nameSpace for the CIM call 
 * @param OUT CMPIArgs * - output arguments 
 *     "PossibleHosts" - UUIDs of all hosts that the VM can boot
 *     on
 * @param OUT CMPIStatus * - CIM Status of the call
 * @return int - 0 on success, non-zero on error
 */

int FindPossibleHostsToBootOn(
    const CMPIBroker* broker,
    xen_utils_session* session,
    const CMPIArgs* argsin, 
    int argsInCount,
    const CMPIContext* context,
    char *nameSpace, 
    CMPIArgs* argsout, 
    CMPIStatus *status)
{
    xen_vm vm = NULL;
    xen_vm_record *vm_rec = NULL;
    char *status_msg = NULL;
    int rc = VSMS_DefineSystem_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;

    if (!xen_utils_get_affectedsystem(broker, session, argsin, status, &vm_rec, &vm)) {
        status_msg = "ERROR: Missing 'AffectedSystem' parameter";
        goto Exit;
    }

    rc = VSMS_DefineSystem_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    xen_host_set* host_set = NULL;
    if(xen_host_get_all(session->xen, &host_set) && host_set) {
        int i, bootable_hosts = 0;
        for (i=0; i<host_set->size; i++) {
            if(xen_vm_assert_can_boot_here(session->xen, vm, host_set->contents[i]))
                bootable_hosts++;
            RESET_XEN_ERROR(session->xen);
        }
        CMPIArray *arr = CMNewArray(broker, bootable_hosts, CMPI_ref, NULL);
        bootable_hosts = 0;
        for(i=0; i<host_set->size; i++) {
            if(xen_vm_assert_can_boot_here(session->xen, vm, host_set->contents[i])) {
                CMPIObjectPath *op = host_create_ref(broker, session, host_set->contents[i]);
                //char *host_wbem_uri = xen_utils_CMPIObjectPath_to_WBEM_URI(broker, op);
                CMSetArrayElementAt(arr, bootable_hosts, (CMPIValue *)&op, CMPI_ref);
                //free(host_wbem_uri);
                bootable_hosts++;
            }
            RESET_XEN_ERROR(session->xen);
        }
        CMAddArg(argsout, "PossibleHosts", (CMPIValue *)&arr, CMPI_refA);
        statusrc = CMPI_RC_OK;
        rc = VSMS_DefineSystem_Completed_with_No_Error;
        xen_host_set_free(host_set);
    }

Exit:
    if(vm_rec)
        xen_vm_record_free(vm_rec);
    if(vm)
        xen_vm_free(vm);
    xen_utils_set_status(broker, status, statusrc, status_msg, session->xen);
    return rc;
}

/*=============================================================================
 * Job threads for Adding resources to VM
 *
 * Uses the CMPIBroker provided threading facility that also includes
 * synchronization mechanisms.
 *
 * When an async jobs are running, the VirtualSystemManagementService should
 * be prevented from being unloaded.
 *============================================================================*/
/* Async job thread callback function */
void add_resources_job(void* async_job)
{
    CMPIStatus status;
    int state = JobState_Exception;
    int error_code = 0;
    int job_error_code = 0;
    char *description = JOBSTATE_ADD_STARTED, *xen_error=NULL;
    Xen_job *job = (Xen_job *)async_job;
    xen_utils_session *session = job->session;

    add_resources_job_context *job_context = (add_resources_job_context *)job->job_context;
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("AddResources job started"));

    state = JobState_Running;
    job_change_state(job, session, state, 0, error_code, NULL);

    /* Add resources to the VM, this could be a long operation depending on how many resources we have */
    if (!_vm_add_resources(job, session, job_context, &error_code, &status)) {
        /* status set in create_vm */
        state = JobState_Exception;
        if (error_code != 0) {
            job_error_code = error_code;
            if(!session->xen->ok)
                description = xen_error = xen_utils_get_xen_error(session->xen);
            else
                description = "ERROR: Adding VM resources failed";
        }
        else {
            job_error_code = status.rc;
            description = CMGetCharPtr(status.msg);
        }
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- _vm_add_resource() failed"));
    }
    else {
        state = JobState_Completed;
        description = "Successfull";
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- add_resources job completed successfully"));
    }

    /* Update the CIM job object's status */
    job_change_state(job, session, state, 100, job_error_code, description);

    /* cleanup */
    if (job_context) {
        _parsed_devices_free(job_context->vm_rec,
            job_context->srs, 
            job_context->vbd_recs, 
            job_context->vdi_recs, 
            job_context->vifs, 
            job_context->con_rec,
            NULL);
        free(job_context);
    }
    if(xen_error)
        free(xen_error);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Add_resources job completed"));
}

void copy_vm_job(void* async_job)
{
    CMPIStatus status;
    int state = JobState_Exception;
    int job_error_code = VSMS_DefineSystem_Failed;
    char *description = JOBSTATE_ADD_STARTED;
    Xen_job *job = (Xen_job *)async_job;
    xen_utils_session *session = job->session;

    copy_vm_job_context *job_context = (copy_vm_job_context *)job->job_context;
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("AddResources job started"));

    state = JobState_Running;
    job_change_state(job, session, state, 0, 0, NULL);

    /* If the original vm is a template, then we need to call vm_provision, 
       if its a vm we need to call vm_copy */
    bool is_a_template = false;
    xen_vm_get_is_a_template(session->xen, &is_a_template, job_context->vm_to_copy_from);

    xen_vm result_vm = NULL;

    /* If no SR is specified, assume default */
    if (job_context->sr_to_use == NULL)
        job_context->sr_to_use = _sr_find_default(session);

    if (is_a_template) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Provisioning template to %s", job_context->vm_rec->name_label));
        /* Clone the template, and provision its disks */
        if (xen_vm_clone(session->xen, &result_vm, job_context->vm_to_copy_from, job_context->vm_rec->name_label)) {
            /* get the newly created VM's record */
            xen_vm_record *new_vm_rec = NULL;
            xen_vm_get_record(session->xen, &new_vm_rec, result_vm);
            xen_vm_record_free(job_context->vm_rec);
            job_context->vm_rec = new_vm_rec;

            if (!_vm_provision_disks(job->broker, session, result_vm, job_context->vm_rec, job_context->sr_to_use, &status))
                goto Exit;
        }
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Copying VM to %s", job_context->vm_rec->name_label));
        /* Make a full copy of the VM */
        if (!xen_vm_copy(session->xen, &result_vm, job_context->vm_to_copy_from,
            job_context->vm_rec->name_label, job_context->sr_to_use))
            goto Exit;
    }
    /* modify the vm's settings with the VSSD passed in */
    vssd_modify(session,result_vm, job_context->vm_rec, job_context->vsSettingDataInst, NULL);

    /* Return the objectpath for the resulting DomU in the output args */
    xen_vm_record *result_vm_rec = NULL;
    if(xen_vm_get_record(session->xen, &result_vm_rec, result_vm)) {
        CMPIObjectPath* op = vm_create_ref(job->broker, DEFAULT_NS, session, result_vm_rec);
        char *wbem_uri = xen_utils_CMPIObjectPath_to_WBEM_URI(job->broker, op);
        if (wbem_uri) {
            /* Add the 'ResultingSystem' property to the job, this will get picked up by the Job class */
            xen_task_add_to_other_config(session->xen, job->task_handle, "ResultingSystem", wbem_uri);
        }
        xen_vm_record_free(result_vm_rec);
    }
    state = JobState_Completed;
    job_error_code = VSMS_DefineSystem_Completed_with_No_Error;
    description = "";

    Exit:
    /* Update the CIM job object's status */
    if (job_error_code != VSMS_DefineSystem_Completed_with_No_Error) {
        description = session->xen->error_description[0];
        state = JobState_Exception;
    }
    job_change_state(job, session, state, 100, job_error_code, description);

    /* cleanup */
    if (job_context) {
        CMRelease(job_context->vsSettingDataInst);
        if (job_context->sr_to_use)
            xen_sr_free(job_context->sr_to_use);
        if (job_context->vm_rec)
            xen_vm_record_free(job_context->vm_rec);
        if (job_context->vm_to_copy_from)
            xen_vm_free(job_context->vm_to_copy_from);
        free(job_context);
    }

    /* Remove VM on error */
    if (!session->xen->ok) {
        if (result_vm)
            xen_vm_destroy(session->xen, result_vm);
    }
    if (result_vm)
        xen_vm_free(result_vm);

    xen_utils_set_status(job->broker, &status, job_error_code, "Error: Copying VM", session->xen);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("CopyVM job completed"));
}

/*============================================================================
 * Helper routines to create devices, convert CIM information to Xen specific 
 * structures and so on.
 *===========================================================================*/
static void _parsed_devices_free(
    xen_vm_record *vm_rec,
    xen_sr_set* srs,
    xen_vbd_record_set *vbd_recs,
    xen_vdi_record_set *vdi_recs,
    xen_vif_record_set *vif_recs,
    xen_console_record *con_rec,
    kvp *kvp_rec
    )
{
    if (vm_rec)
        xen_vm_record_free(vm_rec);
    if (srs)
        xen_sr_set_free(srs);
    if (vbd_recs)
        xen_vbd_record_set_free(vbd_recs);
    if (vdi_recs)
        xen_vdi_record_set_free(vdi_recs);
    if (vif_recs)
        xen_vif_record_set_free(vif_recs);
    if (con_rec)
        xen_console_record_free(con_rec);
    if (kvp_rec)
        xen_utils_free_kvp(kvp_rec);
}

/******************************************************************************
 * _rasd_parse
 * 
 * Parses one RASD of any given type (processor, memory, disk or network) and 
 * creates a xen resource based on it. For the processor and memory RASDs
 * it populates the VM record passed in with the values from the rasd. 
 * For the disk RASD, it createsa VBD and VDI record and an SR handle for
 * where the VBD could be created/deleted from. The records are appended to the
 * vbd_record_set, vdi_record_set and the sr_sets passed in.
 * For the Network RASD, it creates a VIF record and appends it to the 
 * vif_record_set passed in.
 * Similarly for the console RASD.
 *   
******************************************************************************/
static int _rasd_parse(
    const CMPIBroker *broker,
    xen_utils_session *session,
    CMPIData *setting_data,      /* in - rasd setting data */
    vm_resource_operation add_remove_or_replace, /* for proc and memory rasds */
    xen_vm *vm,                   /* in/out - vm handle, will be filled in if its null */
    xen_vm_record **vm_rec,       /* in/out - vm record */
    bool strict_checks,           /* in - perform strict checking of the RASD to make sure all necessary properties have been passed in*/
    xen_vbd_record_set **vbd_recs, /* out */
    xen_vdi_record_set **vdi_recs, /* out */
    xen_sr_set         **srs,      /* out */
    xen_vif_record_set **vifs,     /* out */
    xen_console_record **con_rec,   /* out */
    kvp **kvp_rec,                  /* out */
    CMPIStatus* status
    )
{
    /* Convert RASD settings to their respecitve xen settings. */
    CMPIInstance *instance = NULL;
    CMPIObjectPath *objectpath = NULL;
    char *settingclassname = NULL;
    int resourceType = 0;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("_rasd_parse"));

    if (!xen_utils_get_cmpi_instance(broker, setting_data, &objectpath, &instance) || !instance) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- resource instance is NULL"));
        goto Exit;
    }

    /* Get the class type of the setting data instance */
    settingclassname = CMGetCharPtr(CMGetClassName(objectpath, NULL));
    CMPIData prop = CMGetProperty(instance, "ResourceType", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type & CMPI_INTEGER)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("ResourceType is %d", resourceType));
        resourceType = prop.value.uint16;
    }

    if (*vm_rec == NULL) {
        /* no VM information has been passed in to be filled up, infer from the InstanceID property of the RASD */
        prop = CMGetProperty(instance, "InstanceID", status);
        if ((status->rc != CMPI_RC_OK) || CMIsNullValue(prop)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- InstanceID property is missing"));
	    
	    prop = CMGetProperty(instance, "DeviceID", status);
	    if ((status->rc != CMPI_RC_OK) || CMIsNullValue(prop)){
	      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- DeviceID property is also missing"));
              goto Exit;
	    }

        }

        /* RASD InstanceID is of the form "Xen:VM_UUID\Device_UUID"*/
        char buf[MAX_SYSTEM_NAME_LEN];
        _CMPIStrncpySystemNameFromID(buf, CMGetCharPtr(prop.value.string), sizeof(buf));

        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Getting VM %s, %s", buf, CMGetCharPtr(prop.value.string)));
        if (!xen_vm_get_by_uuid(session->xen, vm, buf)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Couldnt get VM information "));
            goto Exit;
        }
        if (!xen_vm_get_record(session->xen, vm_rec, *vm)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Couldnt get VM record "));
            goto Exit;
        }
    }

    /* Populate config with instance data from the virtual device. */
    if ((strcmp(settingclassname,"Xen_ProcessorSettingData") == 0) ||
        (strcmp(settingclassname, "CIM_ResourceAllocationSettingData") == 0 &&
        resourceType == DMTF_ResourceType_Processor)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- adding Xen_ProcessorSettingData to configuration"));
        if (!proc_rasd_to_vm_rec(broker, instance, *vm_rec, add_remove_or_replace, status))
            goto Exit;
    }
    else if ((strcmp(settingclassname,"Xen_MemorySettingData") == 0) ||
        (strcmp(settingclassname, "CIM_ResourceAllocationSettingData") == 0 &&
        resourceType == DMTF_ResourceType_Memory)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- adding Xen_MemorySettingData to configuration"));
        if (!memory_rasd_to_vm_rec(broker, instance, *vm_rec, add_remove_or_replace, status))
            goto Exit;
    }
    else if ((strcmp(settingclassname,"Xen_DiskSettingData") == 0) ||
        (strcmp(settingclassname, "CIM_ResourceAllocationSettingData") == 0 &&
        ((resourceType == DMTF_ResourceType_Storage_Extent) || 
        (resourceType == DMTF_ResourceType_DVD_drive) || 
        (resourceType == DMTF_ResourceType_CD_Drive) ||
        (resourceType == DMTF_ResourceType_Disk_Drive)))) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- adding Xen_DiskSettingData to configuration"));
        xen_vbd_record *vbd_rec = NULL;
        xen_vdi_record *vdi_rec = NULL;
        xen_sr sr = NULL;
        if (!disk_rasd_to_vbd(broker, session, instance, &vbd_rec, &vdi_rec, &sr, status))
            goto Exit;
        if (vdi_recs && vbd_recs && srs) {
            /* These are the devices that we are responsible for creating */
            ADD_DEVICE_TO_LIST((*srs), sr, xen_sr);
            ADD_DEVICE_TO_LIST((*vdi_recs), vdi_rec, xen_vdi_record);
            ADD_DEVICE_TO_LIST((*vbd_recs), vbd_rec, xen_vbd_record);
        }
        else {
            if(vbd_rec) {
	        xen_vbd_record_free(vbd_rec);
            }
            if(vdi_rec) {
                xen_vdi_record_free(vdi_rec);
            }
            if(sr) {
                xen_sr_free(sr);
            }
        }
    }
    else if (vifs && ((strcmp(settingclassname,"Xen_NetworkPortSettingData") == 0) ||
        (strcmp(settingclassname, "CIM_ResourceAllocationSettingData") == 0 &&
        resourceType == DMTF_ResourceType_Ethernet_Connection))) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- adding Xen_NetworkPortSettingData to configuration"));
        xen_vif_record *vif_rec;
        if (!network_rasd_to_vif(broker, session, instance, strict_checks, &vif_rec, status))
            goto Exit;
        ADD_DEVICE_TO_LIST((*vifs), vif_rec, xen_vif_record);
    }
    else if (con_rec && (strcmp(settingclassname,"Xen_ConsoleSettingData") == 0 ||
        (strcmp(settingclassname,"CIM_ResourceAllocationSettingData") == 0 &&
        resourceType == DMTF_ResourceType_Graphics_controller))) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- adding Xen_ConsoleSettingData to configuration"));
        if (!console_rasd_to_xen_console_rec(broker, instance, con_rec, status))
            goto Exit;
    }
    else if (strcmp(settingclassname, "Xen_KVP") == 0) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- adding Xen_KVP to configuration"));
      if(!_kvp_rasd_to_kvp(broker, instance, kvp_rec, status)) {
	_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("failed to get kvp"));
	 goto Exit;
      }
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- invalid setting data - class %s, resource type %d", settingclassname, resourceType));
        goto Exit;
    }

    return 1;

    Exit:
    return 0;
}

/******************************************************************************
 * _sr_find_default
 * 
 * returns the default SR for the pool.
 * VBDs with no SR ID or VDI ID, specified will get their disks created here
 *   
******************************************************************************/
static xen_sr _sr_find_default(
    xen_utils_session *session
    )
{
    xen_sr def_sr = NULL;
    xen_pool_set* pool_set = NULL;
    _SBLIM_ENTER("_sr_find_default");

    if (!xen_pool_get_all(session->xen, &pool_set) || (pool_set == NULL) || (pool_set->size != 1)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("Couldnt get pool %s to find default SR: size %d", 
            (pool_set ? "<pool_name>":"NULL"),
            (pool_set ? pool_set->size:0)) );
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return NULL;
    }

    if (!xen_pool_get_default_sr(session->xen, &def_sr, pool_set->contents[0]))
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);

    xen_pool_set_free(pool_set);
    _SBLIM_RETURN(def_sr);
}

/*-----------------------------------------------------------------------------
* Create a VDI on the given storage reposiory
*----------------------------------------------------------------------------*/
static int _vdi_find_default(
    const CMPIBroker *broker,
    xen_utils_session* pSession,
    xen_vdi_record* vdi_rec,  /* in - VDI details  */
    xen_sr sr_handle,         /* in - SR, on which to create */
    xen_vdi* new_vdi,         /* out - handle to new VDI */
    CMPIStatus *status)       /* out - status of call */
{
    int ccode;
    char error_msg[XEN_UTILS_ERROR_BUF_LEN];
    _SBLIM_ENTER("_vdi_find_default");

    if (!vdi_rec)
        _SBLIM_RETURN(0);

    xen_sr_record_opt sr_record = {
        .u.handle = sr_handle
    };
    vdi_rec->sr = &sr_record;

    if (vdi_rec->uuid) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Finding VDI: %s", vdi_rec->uuid));
        ccode = xen_vdi_get_by_uuid(pSession->xen, new_vdi, vdi_rec->uuid);
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Creating new VDI %s", vdi_rec->name_label));
        ccode = xen_vdi_create(pSession->xen, new_vdi, vdi_rec);
    }

    /* Set sr field of vdi record to NULL so it is not freed */
    vdi_rec->sr = NULL;
    if (!ccode) {
        XEN_UTILS_GET_ERROR_STRING(error_msg, pSession->xen);
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- xen_vdi_create failed: %s", error_msg));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, error_msg);
        _SBLIM_RETURN(0);
    }
    _SBLIM_RETURN(1);
}

static int _kvp_rasd_to_kvp(
		    const CMPIBroker *broker,
		    CMPIInstance *kvp_rasd,
		    kvp **kvp_obj,
		    CMPIStatus *status)
{
  CMPIData propertyvalue;
  char *error_msg = "Error: an unkown error occured when parsing the KVP";

  /* Allocate KVP Object */
  if(!initialise_kvp(kvp_obj)) {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not allocate KVP object"));
    goto Error;
  }

  char key[MAX_INSTANCEID_LEN];
  char vm_uuid[MAX_INSTANCEID_LEN];

  /* Get the KVP Key */
  propertyvalue = CMGetProperty(kvp_rasd, "Key", status);
  if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {

    (*kvp_obj)->key = strdup(CMGetCharPtr(propertyvalue.value.string));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Got key: %s", (*kvp_obj)->key));

  } 

  /* Get the KVP Value */
  propertyvalue = CMGetProperty(kvp_rasd, "Value", status);
  if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
    (*kvp_obj)->value = strdup(CMGetCharPtr(propertyvalue.value.string));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Got value: %s", (*kvp_obj)->value));
  } 


  /* Parse the DeviceID */
  propertyvalue = CMGetProperty(kvp_rasd, "DeviceID", status);
  if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)){

    char *val = CMGetCharPtr(propertyvalue.value.string);
    _CMPIStrncpyDeviceNameFromID(key, val, sizeof(key));
    _CMPIStrncpySystemNameFromID(vm_uuid, val, sizeof(vm_uuid));

    /* If we have not already set the key from the 'Key' parameter
     * take the value obtained from the device ID
     */
    if ((*kvp_obj)->key == NULL) {
      (*kvp_obj)->key = strdup(key);
    }
    
    (*kvp_obj)->vm_uuid = strdup(vm_uuid);
  }


  /* Need to validate that we actually have a key before continuing */

  if ((*kvp_obj)->key == NULL) {
    error_msg = "Error parsing KVP: No Key has been provided";
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, (error_msg));
    goto Error;
  }

  /* Validate the length of the key and value to match
   * the specified requirements:
   * key_min = 1 key_max = 256
   * value_min = 1 value_max = 40000
   */

  int key_length = strlen(((*kvp_obj)->key));

  if (key_length < 1 || key_length > 256) {
    error_msg = "Error: Key length for the KVP does not fall within the range 1-256";
    goto Error;
  } 

  /* It is valid for a KVP object to not specify a value, e.g. in the case
   * where the caller is asking for a KVP to be deleted.
   * If no value has been provided, we should just ignore.
   */

  if ((*kvp_obj)->value != NULL) {
    int val_length = strlen(((*kvp_obj)->value));
    if (val_length < 1 || val_length > 40000) {
      error_msg = "Error: Value length for the KVP does not fall within the range 1-40000";
      goto Error;
    }
  } 

  /*No errors have been returned */
  return 1;

 Error:

  if (error_msg) {
    CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, error_msg);
  }

  /*Free */
  if(*kvp_obj)
    xen_utils_free_kvp(*kvp_obj);

  return 0;
  
}

/*-----------------------------------------------------------------------------
* Create a VBD on the give VM, given a handle to a VDI
*----------------------------------------------------------------------------*/
static int _vbd_create(
    const CMPIBroker *broker,
    xen_utils_session* pSession,
    xen_vm_record_opt* vm_record_opt,/* in - VM to attach to */
    xen_vdi vdi_handle,              /* in - VDI to attach to */
    xen_vbd_record *vbd_rec,         /* in - VBD details */
    xen_vbd* new_vbd,         /* out - handle to new VBD */
    CMPIStatus *status)       /* out - status of call */
{
    int ccode = 0;

    _SBLIM_ENTER("_vbd_create");

    vbd_rec->vm = vm_record_opt;
    xen_vdi_record_opt vdi_record_opt  = {
        .u.handle = vdi_handle
    };
    vbd_rec->vdi = &vdi_record_opt;

    /* If there wasnt a device name specified, find a suitable one now */
#if XENAPI_VERSION > 400
    if (vbd_rec->userdevice == NULL) {
        xen_string_set *allowed_vbd_devices = NULL;
        if (!xen_vm_get_allowed_vbd_devices(pSession->xen, &allowed_vbd_devices, vm_record_opt->u.handle) 
            || (allowed_vbd_devices->size == 0)) {
            xen_utils_trace_error(pSession->xen, __FILE__, __LINE__);
            CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, "No more devices available in VM");
            goto Exit;
        }
        else {
            /* Pick the first devicename off the list */
            vbd_rec->userdevice = strdup(allowed_vbd_devices->contents[0]);
            xen_string_set_free(allowed_vbd_devices);
        }
    }
#endif
    if (vdi_handle == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("no vdi found"));
        if (vbd_rec->type == XEN_VBD_TYPE_CD)
            /* ignore no vdi for CDs -- create virtual CD for .iso file */
            vbd_rec->empty = true;

    }
    ccode = xen_vbd_create(pSession->xen, new_vbd, vbd_rec);
    /* Set vm and vdi fields of vbd record to NULL so they are not freed */
    if (!ccode) {
        xen_utils_trace_error(pSession->xen, __FILE__, __LINE__);
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, "Error creating VBD");
    }
    Exit:
    vbd_rec->vm = NULL; /* This is freed outside */
    vbd_rec->vdi = NULL;/* This is freed outside */

    _SBLIM_RETURN(ccode);
}
/*-----------------------------------------------------------------------------
* Create a new VIF and attach it to the given VM
*----------------------------------------------------------------------------*/
static int _vif_create(
    const CMPIBroker *broker,
    xen_utils_session* pSession,
    xen_vm_record_opt* vm_record_opt,/* in -VM to atatach to */
    xen_vif_record *vif_rec,         /* in - VIF details */
    xen_vif* new_vif,         /* out - handle to new VIF */ 
    CMPIStatus *status)       /* out - status of call */
{
    int ccode;
    char error_msg[XEN_UTILS_ERROR_BUF_LEN];

    vif_rec->vm = vm_record_opt;

    /* If no device is specified, pick an available one */
    if (vif_rec->device == NULL) {
        xen_string_set *allowed_vif_devices = NULL;
        if (!xen_vm_get_allowed_vif_devices(pSession->xen, &allowed_vif_devices, vm_record_opt->u.handle) 
            || (allowed_vif_devices->size == 0)) {
            xen_utils_trace_error(pSession->xen, __FILE__, __LINE__);
            CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, "No more VIF devices available in VM");
            return 0;
        }
        else {
            /* Pick the first devicename off the list */
            vif_rec->device = strdup(allowed_vif_devices->contents[0]);
            xen_string_set_free(allowed_vif_devices);
        }
    }
    ccode = xen_vif_create(pSession->xen, new_vif, vif_rec);
    /* Set vm field of vif record to NULL so it is not freed */
    vif_rec->vm = NULL;
    if (!ccode) {
        XEN_UTILS_GET_ERROR_STRING(error_msg, pSession->xen);
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- xen_vif_create failed %s", error_msg));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, error_msg);
        return 0;
    }
    return 1;
}
/*-----------------------------------------------------------------------------
* Create a new console device and attach it to the given VM
*----------------------------------------------------------------------------*/
static int _console_create(
    const CMPIBroker *broker,
    xen_utils_session* pSession,
    xen_vm_record_opt* vm_record_opt, /* in - VM to attach to */
    xen_console_record* con_rec,      /* in - console details */
    xen_console* neo_con,      /* out - handle to new console */
    CMPIStatus *status)        /* out - status of the call */       
{
    int ccode;
    char error_msg[XEN_UTILS_ERROR_BUF_LEN];

    con_rec->vm = vm_record_opt;

    ccode = xen_console_create(pSession->xen, neo_con, con_rec);
    /* Set vm field of console record to NULL so it is not freed */
    con_rec->vm = NULL;
    if (!ccode) {
        XEN_UTILS_GET_ERROR_STRING(error_msg, pSession->xen);
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- xen_console_create failed %s", error_msg));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, error_msg);
        return 0;
    }
    return 1;
}

static int _vm_provision_disks(
    const CMPIBroker *broker,
    xen_utils_session *session,
    xen_vm vm,
    xen_vm_record *new_vm_rec,
    xen_sr sr_to_use,
    CMPIStatus *status
    )
{
    int rc = VSMS_AddResourceSettings_Completed_with_No_Error;

    /* If there are disks specified in 'other_config' field, provision them here */
    char *disk_config = xen_utils_get_from_string_string_map(new_vm_rec->other_config, "disks");
    if (disk_config) {
        char buf[512], buf2[512];
        char *sr_uuid = NULL;
        xen_sr default_sr = NULL;

        /* if an SR needs to be specified for disks being provisioned, use default SR */
        if (sr_to_use == NULL) {
            default_sr = _sr_find_default(session);
            sr_to_use = default_sr;
        }
        if (!xen_sr_get_uuid(session->xen, &sr_uuid, sr_to_use)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Invalid SR specified or no default SR"));
            return 0;
        }

        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Modifying the new VM disk config with SR %s", sr_uuid));

        memset(buf, 0, sizeof(buf));    /* source string buffer to check for sr="" substring */
        memset(buf2, 0, sizeof(buf2));  /* destination buffer with the sr specified correctly */
        strncpy(buf, disk_config, (sizeof(buf)/sizeof(buf[0])));
        strncpy(buf2, disk_config, (sizeof(buf2)/sizeof(buf2[0])));
        char *tmp = strstr(buf, "sr=\"\"");

        /* update the provision xml config string with correct sr's UUID */
        while (tmp) {
            *tmp = '\0';
            tmp = tmp + strlen("sr=\"\""); /* skip over the sr="" part */
            snprintf(buf2, (sizeof(buf2)/sizeof(buf2[0])), 
                "%ssr=\"%s\"%s", buf, sr_uuid, tmp);
            strcpy(buf, buf2);             /* string to check for next round*/
            tmp = strstr(buf, "sr=\"\"");  /* find next sr="" */
        }

        /* write the updated xml back to the other config field, so provision can pick it up */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("New VM disks spec: %s", buf2));
        xen_utils_add_to_string_string_map("disks", buf2, &new_vm_rec->other_config);
        xen_vm_set_other_config(session->xen, vm, new_vm_rec->other_config);

        if (default_sr)
            xen_sr_free(default_sr);
        if (sr_uuid)
            free(sr_uuid);

        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Disks are being provisioned for the VM"));
    }
    /* Now provision the disks (makes a CoW copy of the disks) */
    if (!xen_vm_provision(session->xen, vm))
        rc = VSMS_AddResourceSettings_Failed;

    xen_utils_set_status(broker, status, rc, "ERROR: Provisioning disks for the VM", session->xen);
    return(rc == VSMS_AddResourceSettings_Completed_with_No_Error);
}

#define _ADD_OP_TO_SET(x, y) \
          char *__tmp__ = xen_utils_CMPIObjectPath_to_WBEM_URI(broker, x); \
          if(__tmp__)                                                      \
              xen_utils_add_to_string_set(__tmp__, &y); 

/*-----------------------------------------------------------------------------
* Given all the resource allocation settings data (rasd) and 
* virtual system setting data (vssd) from CIM, add them to the VM
*----------------------------------------------------------------------------*/
static int _vm_add_resources(
    Xen_job *job,
    xen_utils_session *session,
    add_resources_job_context* job_context,
    int *kensho_error_code,
    CMPIStatus *status)          /* out - status of the call */
{
    xen_vm vm = NULL;
    const CMPIBroker *broker = job->broker;
    xen_vm_record *vm_rec = job_context->vm_rec;
    xen_vbd_record_set *vbd_recs = job_context->vbd_recs;
    xen_vdi_record_set *vdi_recs = job_context->vdi_recs;
    xen_sr_set *srs = job_context->srs;
    xen_vif_record_set *vifs = job_context->vifs;
    xen_console_record *con_rec = job_context->con_rec;
    bool remove_vm_on_error = job_context->remove_vm_on_error;
    xen_string_set *obj_path_set = NULL;
    bool failure = true;

    //xen_sr sr;
    int i;
    int rc = 0;
    int ccode;

    _SBLIM_ENTER("_vm_add_resource");

    /* Find the VM to add the devices to */
    if (!xen_vm_get_by_uuid(session->xen, &vm, vm_rec->uuid)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Failed to find VM name %s", vm_rec->name_label));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, "Failed to find virtual system");
        goto Exit;
    }

    *kensho_error_code = 0;
    xen_vm_record_opt vm_record_opt =
    {
        .u.handle = vm
    };

    /* Provision any disks that might be specified in the template */
    if (job_context->provision_disks) {
        /* When it gets here, vm better still be a template */
        if (!_vm_provision_disks(broker, session, vm, vm_rec, NULL, status))
            goto Exit;
    }

    /* do processor and memory update */
    if (job_context->memory_and_proc_need_updating) {
        memory_rasd_modify(session, vm, vm_rec);
        proc_rasd_modify(session, vm, vm_rec);
    }

    /* Add all of the virtual network devices. */
    if (vifs) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Creating Virtual network interface"));
        for (i = 0; i < vifs->size; i++) {
            xen_vif new_vif;
            if (!_vif_create(broker, session, &vm_record_opt, vifs->contents[i], &new_vif, status))
                goto Exit;
            CMPIObjectPath *new_op = network_rasd_create_ref(broker, XEN_CLASS_NAMESPACE, session, vm_rec, new_vif);
            _ADD_OP_TO_SET(new_op, obj_path_set)
            xen_vif_free(new_vif);
        }
    }

    /* Add console device if specified */
    if (con_rec) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Creating console device"));
        xen_console new_con;
        if (!_console_create(broker, session, &vm_record_opt, con_rec, &new_con, status))
            goto Exit;
        CMPIObjectPath *new_op = console_rasd_create_ref(broker, XEN_CLASS_NAMESPACE, session, vm_rec, new_con);
        _ADD_OP_TO_SET(new_op, obj_path_set);
        xen_console_free(new_con);
    }

    /* Add the VBDs at the end, since they create VBDs that could fail */
    if (vdi_recs) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Adding %d disks", vdi_recs->size));

        for (i = 0; i < vdi_recs->size; i++) {
            xen_vdi new_vdi = NULL;
            xen_vbd new_vbd = NULL;
            if (srs->contents[i] || vdi_recs->contents[i]->uuid) {
                if (!_vdi_find_default(broker, session, vdi_recs->contents[i], srs->contents[i], &new_vdi, status)) {
                    goto Exit;
                }
            }
            ccode = _vbd_create(broker, session, &vm_record_opt, new_vdi, vbd_recs->contents[i], &new_vbd, status);
            if(new_vdi) {
                xen_vdi_free(new_vdi);
            }
            if (!ccode) {
                goto Exit;
            }
            CMPIObjectPath *new_op = disk_rasd_create_ref(broker, XEN_CLASS_NAMESPACE, session, vm_rec, new_vbd);
            _ADD_OP_TO_SET(new_op, obj_path_set);
            if(new_vbd) {
                xen_vbd_free(new_vbd);
            }
        }
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Successfully added resources to VM")); 

    /* update the task object with the object paths */
    char *obj_paths = xen_utils_flatten_string_set(obj_path_set, ";");
    if (obj_paths) {
        xen_task_add_to_other_config(session->xen, job->task_handle, "AffectedResources", obj_paths);
        free(obj_paths);
    }
    failure = false;
    rc = 1;
    CMSetStatusWithChars(broker, status, CMPI_RC_OK, "");

Exit:
    /* Unwind if we failed.
     * remove_vm will nuke any vifs / vbds created and the vm.
     */
    if(obj_path_set)
        xen_string_set_free(obj_path_set);
    if(vm) {
        if (failure) {
            if (remove_vm_on_error) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Removing VM due to error %s", session->xen->error_description[0]));   
                _vm_destroy(session, vm, true);
            }
        }
        xen_vm_free(vm);
    }
    _SBLIM_RETURN(rc);
}

/*-----------------------------------------------------------------------------
* Add a new resource to an existing VM given the new resource allocation 
* settings data (rasd) from CIM
*----------------------------------------------------------------------------*/
static int _vm_add_resource(
    const CMPIBroker *broker, 
    xen_utils_session *session,
    xen_vm vm,        /* in - existing VM handle */
    xen_vm_record *vmRec, /* in - existing VM record */
    CMPIInstance *sourceSettingInst,/* in - rasd */
    CMPIObjectPath **resultSetting, /* out - resource CIM object */
    char *nameSpace,  
    CMPIStatus *status)  /* out - status of the call */
{
    CMPIObjectPath *op=NULL;
    char *settingclassname=NULL;
    int ccode=0;
    int resourceType = 0;
    xen_vm_record_opt vm_record_opt =   
    {
        .u.handle = vm
    };

    _SBLIM_ENTER("_vm_add_resource");

    op = CMGetObjectPath(sourceSettingInst, NULL);
    CMPIData resourceTypeData = CMGetProperty(sourceSettingInst, "ResourceType", status);
    resourceType = resourceTypeData.value.uint16;

    /* Get the class type of the setting data instance */
    settingclassname = CMGetCharPtr(CMGetClassName(op, NULL));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- settingclassname=%s", settingclassname));

    /* Add the resource to the domain. */
    if ((strcmp(settingclassname,"Xen_ProcessorSettingData") == 0 ||
        strcmp(settingclassname, "CIM_ResourceAllocationSettingData") == 0) &&
        resourceType == DMTF_ResourceType_Processor) {
        /* Add a processor */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- adding Xen_ProcessorSettingData to configuration"));
        if (!proc_rasd_to_vm_rec(broker, sourceSettingInst, vmRec, resource_add, status)) {
            _SBLIM_RETURN(0);
        }
        if (!xen_vm_set_vcpus_max(session->xen, vm, vmRec->vcpus_max) || 
            !xen_vm_set_vcpus_at_startup(session->xen, vm, vmRec->vcpus_at_startup) ||
            !xen_vm_set_vcpus_params(session->xen, vm, vmRec->vcpus_params)) {
            xen_utils_trace_error(session->xen, __FILE__, __LINE__);
            CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, "Coulndt update processor settings");
            _SBLIM_RETURN(0);
        }
        /* Create new object path for setting */
        *resultSetting = proc_rasd_create_ref(broker, nameSpace, session, vmRec);
    }
    else if ((strcmp(settingclassname,"Xen_MemorySettingData") == 0 ||
        strcmp(settingclassname, "CIM_ResourceAllocationSettingData") == 0) &&
        resourceType == DMTF_ResourceType_Memory) {
        /* Add more memory */
        if (!memory_rasd_to_vm_rec(broker, sourceSettingInst, vmRec, resource_add, status)) {
            _SBLIM_RETURN(0);
        }

        if (!xen_vm_set_memory_limits(session->xen, vm, 
            vmRec->memory_static_min, vmRec->memory_static_max, 
            vmRec->memory_dynamic_min, vmRec->memory_dynamic_max)) {
            xen_utils_trace_error(session->xen, __FILE__, __LINE__);
            CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, "Couldnt update memory settings");
            _SBLIM_RETURN(0);
        }
        /* Add object path to output array */
        *resultSetting = memory_rasd_create_ref(broker, nameSpace, session, vmRec);
    }
    else if ((strcmp(settingclassname,"Xen_DiskSettingData") == 0) ||
             ((strcmp(settingclassname, "CIM_ResourceAllocationSettingData") == 0) &&
              ((resourceType == DMTF_ResourceType_Storage_Extent) || 
               (resourceType == DMTF_ResourceType_DVD_drive) || 
               (resourceType == DMTF_ResourceType_CD_Drive) ||
               (resourceType == DMTF_ResourceType_Disk_Drive)))) {
        /* Add a new disk */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- adding Xen_DiskSettingData to configuration"));
        xen_vbd_record *vbd_rec = NULL;
        xen_vdi_record *vdi_rec = NULL;
        xen_sr  sr_handle = NULL;
        xen_vdi new_vdi = NULL;
        xen_vbd new_vbd = NULL;
        ccode=0;

        /* get configuration information from CIM */
        if (!disk_rasd_to_vbd(broker, session, sourceSettingInst, &vbd_rec, &vdi_rec, &sr_handle, status)) {
            /* status set in disk_rasd2vmconfig */
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Error parsing disk settings"));
            _SBLIM_RETURN(0);
        }

        if (!sr_handle)
            /* SR has not been specified */
            if (!vdi_rec->uuid)
                /* An existing VDI has not been specified either */
                sr_handle = _sr_find_default(session);

            /* Create/Find VDI */
        if (sr_handle || vdi_rec->uuid)
            _vdi_find_default(broker, session, vdi_rec, sr_handle, &new_vdi, status);

        if (session->xen->ok) {
            ccode = _vbd_create(broker, session, &vm_record_opt, new_vdi, vbd_rec, &new_vbd, status);
            if (ccode) {
                /* Add object path to output array */
                *resultSetting = disk_rasd_create_ref(broker, nameSpace, session, vmRec, new_vbd);
            }
        }

        /* Free allocated objects. */
        if (!ccode)
            xen_utils_trace_error(session->xen, __FILE__, __LINE__);

        if (new_vdi) xen_vdi_free(new_vdi);
        if (new_vbd) xen_vbd_free(new_vbd);
        if (sr_handle) xen_sr_free(sr_handle);
        if (vbd_rec) xen_vbd_record_free(vbd_rec);
        if (vdi_rec) xen_vdi_record_free(vdi_rec);

        _SBLIM_RETURN(ccode);
    }
    else if (((strcmp(settingclassname,"Xen_NetworkPortSettingData") == 0) || 
        (strcmp(settingclassname, "CIM_ResourceAllocationSettingData") == 0)) &&
        resourceType == DMTF_ResourceType_Ethernet_Connection) {
        /* Add a new network interface */
        xen_vif_record *vif_rec = NULL;
        xen_vif new_vif = NULL;
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- adding Xen_NetworkPortSettingData to configuration"));
        if (!network_rasd_to_vif(broker, session, sourceSettingInst, true, &vif_rec, status)) {
            /* status set in nic_rasd2vmconfig */
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Error parsing network port settings"));
            _SBLIM_RETURN(0);
        }
        ccode = _vif_create(broker, session, &vm_record_opt, vif_rec, &new_vif, status);
        /* Set vm field of vif record to NULL so it is not freed */
        if (!ccode) {
    	    vif_rec->vm = NULL;
            xen_utils_trace_error(session->xen, __FILE__, __LINE__);
            CMSetStatusWithChars(broker, status, CMPI_RC_ERR_FAILED, "Could not create VIF");
        }
        else {
            /* Add object path to output array */
            *resultSetting = network_rasd_create_ref(broker, nameSpace, session, vmRec, new_vif);
        }

        /* free allocated objects */
        if (vif_rec) xen_vif_record_free(vif_rec);
        if (new_vif) 
	  free(new_vif);
        _SBLIM_RETURN(ccode);
    } else if (strcmp(settingclassname, "Xen_KVP") == 0){
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Add resource Xen_KVP"));
      kvp *kvp_obj;
      ccode = _kvp_rasd_to_kvp(broker, sourceSettingInst, &kvp_obj, status);

      if (ccode) {
	_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Key=%s Value=%s", kvp_obj->key, kvp_obj->value));
	kvp_obj->vm_uuid = strdup(vmRec->uuid);
	if(xen_utils_push_kvp(session, kvp_obj) != Xen_KVP_RC_OK) {
	  ccode = 0;
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Failed to push KVP"));
	}
	/* Free the kvp_obj */
	xen_utils_free_kvp(kvp_obj);
      }

      _SBLIM_RETURN(ccode);
    
    } else {
        /* unrecognized resource - cannot add */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Unrecognized setting data class - %s", settingclassname));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER, "Unrecognized ResourceSetting");
        _SBLIM_RETURN(0);
    }
    _SBLIM_RETURN(1);
}

static int _vm_destroy(
    xen_utils_session *session, 
    xen_vm vm,
    bool remove_associated_disks
    )
{
    RESET_XEN_ERROR(session->xen);
    if (remove_associated_disks)
        _vm_vbds_destroy(session, vm);

    RESET_XEN_ERROR(session->xen);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Destroying VM"));
    return xen_vm_destroy(session->xen, vm);
}

static int _vm_vbds_destroy(
    xen_utils_session *session,
    xen_vm vm
    )
{
    xen_vbd_set *vbd_set = NULL;
    if (xen_vm_get_vbds(session->xen, &vbd_set, vm)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("got %d VBDs for VM", vbd_set->size));
        /* Get the VBD list */
        int i=0;
        for (i=0; i<vbd_set->size; i++) {
            RESET_XEN_ERROR(session->xen);
            _vbd_destroy(session, vbd_set->contents[i], true);
        }
        xen_vbd_set_free(vbd_set);
    }
    return 1;
}

static int _vbd_destroy(
    xen_utils_session *session,
    xen_vbd vbd,
    bool destroy_associated_vdi
    )
{
    xen_vdi vdi = NULL;
    if (destroy_associated_vdi && 
        xen_vbd_get_vdi(session->xen, &vdi, vbd)) {
        xen_vbd_set *vbds_using_this_vdi = NULL;
        /* check how many other vbds are using it */
        if (xen_vdi_get_vbds(session->xen, &vbds_using_this_vdi, vdi)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("VDI contains %d VBDs", vbds_using_this_vdi->size));

            /* Destroy if this is the only vbd using it, its not a CD-Rom ISO (read-only) and its not sharable */
            bool read_only = true; 
            bool sharable = true;
            xen_vdi_get_read_only(session->xen, &read_only, vdi);
            xen_vdi_get_sharable(session->xen, &sharable, vdi);
            if (vbds_using_this_vdi->size == 1 && !read_only && !sharable) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("destroying VBD/VDI"));
                xen_vdi_destroy(session->xen, vdi);
            }
            RESET_XEN_ERROR(session->xen); /* reset this for the next iteration */
            xen_vbd_set_free(vbds_using_this_vdi);
        }
        xen_vdi_free(vdi);
    }
    return xen_vbd_destroy(session->xen, vbd);
}

static xen_vm _get_reference_configuration(
    const CMPIBroker *broker,
    const CMPIArgs* argsin,
    xen_utils_session *session,
    CMPIStatus *status
    )
{
    CMPIData argdata;
    xen_vm template_vm = NULL;

    status->rc = CMPI_RC_OK;
    if (_GetArgument(broker, argsin, "ReferenceConfiguration", CMPI_ref, &argdata, status)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("ReferenceConfiguration found" ));
        xen_vm_record *template_rec = NULL;
        vssd_find_vm(argdata.value.ref, session, &template_vm, &template_rec, status);
        if (template_rec)
            xen_vm_record_free(template_rec);
    }
    else if (_GetArgument(broker, argsin, "ReferenceConfiguration", CMPI_string, &argdata, status)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("ReferenceConfiguration UUID found" ));
        char *template_uuid = CMGetCharPtr(argdata.value.string);
        if (template_uuid) {
            xen_vm_get_by_uuid(session->xen, &template_vm, template_uuid);
        }
    }
    else
        status->rc = CMPI_RC_ERR_NOT_FOUND;

    if (template_vm)
        status->rc = CMPI_RC_OK;

    return template_vm;
}

static int _get_affected_configuration(
    const CMPIBroker *broker, 
    xen_utils_session *session,
    const CMPIArgs* argsin,
    CMPIStatus *status,
    xen_vm_record **vm_rec_out,
    xen_vm *vm_out
    )
{
    CMPIData argdata;
    char buf[MAX_INSTANCEID_LEN];
    char *uuid = NULL;
    int rc = 0;

    if (_GetArgument(broker, argsin, "AffectedConfiguration", CMPI_ref, &argdata, status)) {
        CMPIData key = CMGetKey(argdata.value.ref, "InstanceID", status);
        if(key.type == CMPI_string) {
            _CMPIStrncpySystemNameFromID(buf, CMGetCharPtr(key.value.string), sizeof(buf)/sizeof(buf[0]));
            uuid = buf;
        }
    }
    else if (_GetArgument(broker, argsin, "AffectedConfiguration", CMPI_string, &argdata, status)) {
        /* UUID of the VM in string form */
        uuid = CMGetCharPtr(argdata.value.string);
    }
    else  {
        status->rc = CMPI_RC_ERR_INVALID_PARAMETER;
        goto Exit;
    }

    if (!xen_vm_get_by_uuid(session->xen, vm_out, uuid)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not find VM -%s", uuid));
        goto Exit;
    }
    xen_vm_get_record(session->xen, vm_rec_out, *vm_out);
    rc = 1;

    Exit:
    return rc;
}

/********************************************************************************
* InvokeMethod()
* Execute an extrinsic method on the specified instance.
*********************************************************************************/
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,        /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,   /* [in] CMPI Factory Broker */
    const CMPIContext * context,/* [in] Additional context info, if any */
    const CMPIResult * results, /* [out] Results of this operation */
    const CMPIObjectPath * reference,   /* [in] Contains the CIM namespace, classname and desired object path */
    const char * methodname,    /* [in] Name of the method to apply against the reference object */
    const CMPIArgs * argsin,    /* [in] Method input arguments */
    CMPIArgs * argsout)         /* [in] Method output arguments */
{
    CMPIStatus status = {CMPI_RC_OK, NULL};  /* Return status of CIM operations */
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    int rc = CMPI_RC_OK;
    int argsInCount;
    CMPIData argdata;
    xen_utils_session *pSession = NULL;

    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(broker, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\"%s\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));

    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(context, &ctx, &status)) {
        goto Exit;
    }
    if (!xen_utils_validate_session(&pSession, ctx)) {
        CMSetStatusWithChars(broker, &status, CMPI_RC_ERR_METHOD_NOT_AVAILABLE, "Unable to connect to xen daemon");
        goto Exit;
    }

    argsInCount = CMGetArgCount(argsin, NULL);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- # argsin=%d", argsInCount));

    argdata = CMGetKey(reference, "Name", &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        CMSetStatusWithChars(broker, &status, CMPI_RC_ERR_NOT_FOUND, 
            "Couldnt find the Xen_VirtualSystemManagementService object to invoke the method on. Have you specified the 'Name' key ?");
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt find the Xen_VirtualSystemManagementService object to invoke the method on"));
        goto Exit;
    }

    /* Methods supported. */
    if (strcmp(methodname, "StartService") == 0) {
        rc = StartService(pSession);
    }
    else if (strcmp(methodname, "StopService") == 0) {
        rc = StopService(pSession);
    }
    else if (strcmp(methodname, "DefineSystem") == 0) {
        rc = DefineSystem(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status);
    }
    else if (strcmp(methodname, "AddResourceSetting") == 0) {
        rc = AddResourceSetting(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status);
    }
    else if (strcmp(methodname, "DestroySystem") == 0) {
        rc = DestroySystem(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status);
    }
    else if (strcmp(methodname, "AddResourceSettings") == 0) {
        rc = AddDeleteOrModifyResourceSettings(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status, resource_add);
    }
    else if (strcmp(methodname, "ModifyResourceSettings") == 0) {
        rc = AddDeleteOrModifyResourceSettings(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status, resource_modify);
    }
    else if (strcmp(methodname, "RemoveResourceSettings") == 0) {
        rc = AddDeleteOrModifyResourceSettings(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status, resource_delete);
    }
    else if (strcmp(methodname, "ModifySystemSettings") == 0) {
        rc = ModifySystemSettings(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status);
    }
    else if (strcmp(methodname, "CopySystem") == 0) {
        rc = CopySystem(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status);
    }
    else if (strcmp(methodname, "ConvertToXenTemplate") == 0) {
        rc = ConvertToXenTemplate(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status);
    }
    else if (strcmp(methodname, "FindPossibleHostsToRunOn") == 0) {
        rc = FindPossibleHostsToBootOn(broker, pSession, argsin, argsInCount, context, nameSpace, argsout, &status);
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Method \"%s\" is not supported", methodname));
        CMSetStatusWithChars(broker, &status, CMPI_RC_ERR_METHOD_NOT_AVAILABLE, NULL);
        rc = CMPI_RC_ERR_METHOD_NOT_AVAILABLE;
        goto Exit;
    }

    Exit:
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- method \"%s\" returning %d", methodname, rc));
    if (ctx)
        xen_utils_free_call_context(ctx);
    if (pSession)
        xen_utils_cleanup_session(pSession);

    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);
    _SBLIM_RETURNSTATUS(status);
}

/* ============================================================================
 * CMPI METHOD PROVIDER FUNCTION TABLE SETUP
 * ========================================================================== */
XenMethodMIStub(Xen_VirtualSystemManagementService)

    
