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

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "providerinterface.h"
#include "Xen_Job.h"
#include "Xen_VirtualSystemMigrationService.h"

/*********************************************************
 * Implements the CIM_VirtualSystemMIgrationService as 
 * defined in the DMTF Virtual System Migration Profile
 *********************************************************/
#define MIGRATE_VM_TASK_NAME "Xen_VirtualSystemMigrationServiceJob"
#define STATE_MIGRATE_STARTED "Xen Virtual System Migration has begun"

static void migrate_task(void *job);

typedef struct _migrate_job_context {
     xen_vm vm;
     xen_host host;
} migrate_job_context;

int MigrateVirtualSystem(
    const CMPIBroker *broker,
    const CMPIContext *context,
    const CMPIArgs *argsin, 
    const CMPIArgs *argsout, 
    xen_utils_session *session,
    bool host_ip, 
    bool migrate_check_only, 
    CMPIStatus *status);

/******************************************************************************
 * Provider export function
 * Execute an extrinsic method on the specified CIM instance.
 *****************************************************************************/
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,       /* [in] CMPI Factory services */
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
    
    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\"%s\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));

    struct xen_call_context *ctx = NULL;
    if(!xen_utils_get_call_context(context, &ctx, &status)){
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
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("Couldnt find the Virtual System Migration Service to invoke method on"));
        goto Exit;
    }
    /* Check that the method has the correct number of arguments. */
    if(strcmp(methodname, "MigrateVirtualSystemToHost") == 0) {
        rc = MigrateVirtualSystem(broker, context, argsin, argsout, session, true, false, &status);
        //CMPIObjectPath* job_instance_op = NULL;
        //CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
    }
    else if(strcmp(methodname, "MigrateVirtualSystemToSystem") == 0) {
        rc = MigrateVirtualSystem(broker, context, argsin, argsout, session, false, false, &status);
        //CMPIObjectPath* job_instance_op = NULL;
        //CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
        //CMPIObjectPath* newcomputersystem_instance_op = NULL;
        //CMAddArg(argsout, "NewComputerSystem", (CMPIValue *)&newcomputersystem_instance_op, CMPI_ref);
    }
    else if(strcmp(methodname, "CheckVirtualSystemIsMigratableToHost") == 0) {
        rc = MigrateVirtualSystem(broker, context, argsin, argsout, session, true, true, &status);
    }
    else if(strcmp(methodname, "CheckVirtualSystemIsMigratableToSystem") == 0) {
        rc = MigrateVirtualSystem(broker, context, argsin, argsout, session, false, true, &status);
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

/* CMPI Method provider function table setup */
XenMethodMIStub(Xen_VirtualSystemMigrationService)

/******************************************************************************
* checks for migratability or actually does the migration
******************************************************************************/
int MigrateVirtualSystem(
    const CMPIBroker *broker,   /* in - CMPI Broker that does most of the work */
    const CMPIContext *context, /* in - CMPI context for the caller */
    const CMPIArgs *argsin,     /* in - All the arguments for the method */
    const CMPIArgs *argsout,    /* out - All the output arguments for the method */
    xen_utils_session *session, /* in - Session for making xen calls */
    bool host_ip,               /* in - The host parameter is an IP address */
    bool migrate_check_only,    /* in -Check if migration is possible only, dont actually migrate */
    CMPIStatus *status)         /* out - Report CMPI status of method */
{
    char *hostid = NULL, *vm_uuid = NULL;
    CMPIData argdata;
    xen_vm vm = NULL;
    xen_host_set *host_set = NULL;
    CMPIInstance *msd = NULL;
    xen_host host = NULL;
    int rc = Xen_VirtualSystemMigrationService_MigrateVirtualSystemToSystem_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    xen_string_string_map *other_config=NULL;

    /* For now only Live migrations are supported */
    if(_GetArgument(broker, argsin, "MigrationSettingData", CMPI_chars, &argdata, status)) {
        /* Argument passed in as a MOF string, parse it */
        msd = xen_utils_parse_embedded_instance(broker, CMGetCharPtr(argdata.value.string));
        if (msd == NULL) { // parser returns zero for success, non-zero for error
            error_msg = "ERROR: Couldnt parse the 'MigrationSettingData' parameter";
            goto Exit;
        }
    }
    else
        /* Argument could have been passed in as an intance */
        if(_GetArgument(broker, argsin, "MigrationSettingData", CMPI_instance, &argdata, status))
            msd = argdata.value.inst;

    if(msd != NULL) {
        CMPIData data = CMGetProperty(msd, "MigrationType", status);
        if(data.value.uint16 != Xen_VirtualSystemMigrationSettingData_MigrationType_Live) {
            error_msg = "ERROR: 'MigrationSettingData' contains an invalid MigrationType (Live expected)";
            goto Exit;
        }
    }

    /* Host to migrate to */
    if(!host_ip) {
        /* required parameters */
        if(!_GetArgument(broker, argsin, "DestinationSystem", CMPI_ref, &argdata, status)){
            error_msg = "ERROR: 'DestionationSystem' parameter is missing";
            goto Exit;
        }
        else {
            /* This is the CIM reference to an existing Host object */
            CMPIData key;
            key = CMGetKey(argdata.value.ref, "Name", status);
            if(status->rc != CMPI_RC_OK || CMIsNullValue(key)) {
                error_msg = "ERROR: 'DestinationSystem' is missing the required 'Name' key";
                goto Exit;
            }
            hostid = CMGetCharPtr(key.value.string);
            if(!xen_host_get_by_uuid(session->xen, &host, hostid)) 
                goto Exit;
        }
    }
    else {
        if(!_GetArgument(broker, argsin, "DestinationHost", CMPI_string, &argdata, status)) {
            error_msg = "ERROR: 'DestinationHost' parameter is missing";
            goto Exit;
        }
        else {
            /* Determing Xen host based on IP address,. Cannot use inet_pton() and so on since DNS may not have been configured properly */
            hostid = CMGetCharPtr(argdata.value.string);
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Trying to migrate to DestinationHost : %s", hostid));
            if(!xen_host_get_all(session->xen, &host_set))
                goto Exit;
            int i=0;
            for (i=0; i<host_set->size; i++) {
                xen_host_record *host_rec = NULL;
                if(!xen_host_get_record(session->xen, &host_rec, host_set->contents[i]))
                    goto Exit;
                /* DestinationHost could be an IP address or the hostname */
                if((host_rec->address &&  (strcmp(hostid, host_rec->address) == 0)) || 
                   (host_rec->hostname && (strcmp(hostid, host_rec->hostname) == 0)) ||
                   (host_rec->name_label && (strcmp(hostid, host_rec->name_label) == 0))) { 
                    xen_host_record_free(host_rec);
                    host = host_set->contents[i];
                    host_set->contents[i] = NULL; /* dont free this one */
                    break;
                }
                xen_host_record_free(host_rec);
            }
        }
    }

    /* VM to migrate - required parameter */
    if(!_GetArgument(broker, argsin, "ComputerSystem", CMPI_ref, &argdata, status)) {
        if(!_GetArgument(broker, argsin, "ComputerSystem", CMPI_string, &argdata, status)) {
            error_msg = "ERROR: Missing the required 'ComputerSystem' parameter";
            goto Exit;
        }
        else
            vm_uuid = CMGetCharPtr(argdata.value.string);
    } else {
        argdata = CMGetKey(argdata.value.ref, "Name", status);
        if(status->rc != CMPI_RC_OK || CMIsNullValue(argdata)) {
            error_msg = "ERROR: ComputerSystem is missing the required 'Name' key";
            goto Exit;
        }
        vm_uuid = CMGetCharPtr(argdata.value.string);
    }
    status->rc = CMPI_RC_ERR_FAILED;
    rc = Xen_VirtualSystemMigrationService_MigrateVirtualSystemToSystem_Failed;
    if(xen_vm_get_by_uuid(session->xen, &vm, vm_uuid)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Migrating %s to %s", vm_uuid, hostid));
        if(migrate_check_only) {
            /* Check to see if migration is possible */
            statusrc = CMPI_RC_OK;
            rc = Xen_VirtualSystemMigrationService_MigrateVirtualSystemToSystem_Completed_with_No_Error;
            bool migratable = xen_vm_assert_can_boot_here(session->xen, vm, host);
            if(migratable == false || !session->xen->ok) {
                migratable = false;

                // Workaround for kvp migration
                if(session->xen->error_description_count==1) {
                    if(xen_vm_get_other_config(session->xen, &other_config, vm)) {
                         if(xen_utils_get_from_string_string_map(other_config, "kvp_enabled")) {
                            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Overwriting migratable to mark kvp vm as migratable, although its network"));
                            migratable=true;
                            RESET_XEN_ERROR(session->xen);
                         }
                         free(other_config);
                    } else {
                       _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get other config."));
                    }
                }

                if(migratable == false && session->xen->error_description) {
                    /* This is not part of the MOF (and is not documented), but nice to have */
                    char *xen_error = xen_utils_get_xen_error(session->xen);
                    CMAddArg(argsout, "Reason", (CMPIValue *)xen_error, CMPI_chars);
                    free(xen_error);
                }
            }
            CMAddArg(argsout, "IsMigratable", (CMPIValue *)&migratable, CMPI_boolean);
        }
        else {
            /* Do the actual migration, this could take a few minutes */
            CMPIObjectPath* job_instance_op = NULL;
            migrate_job_context *job_context = calloc(1, sizeof(migrate_job_context));
            if(!job_context) {
                error_msg = "ERROR: Couldn't allocate memory for the migrate job.";
                goto Exit;
            }
            job_context->vm = vm;
            job_context->host = host;
            if(!job_create(broker, context, session, MIGRATE_VM_TASK_NAME, vm_uuid, 
                           migrate_task, job_context, &job_instance_op, status)) {
                error_msg = "ERROR: Couldn't prepare the Migrate job. Job wasnt started.";
                goto Exit;
            }
            statusrc = CMPI_RC_OK;
            rc = Xen_VirtualSystemMigrationService_MigrateVirtualSystemToHost_Method_Parameters_Checked___Job_Started;
            CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
            vm = NULL;      /* freed by async thread */
            host = NULL;    /* freed by async thread */
        }
    }

Exit:
    if(host)
        xen_host_free(host);
    if(vm)
        xen_vm_free(vm);
    if(host_set)
        xen_host_set_free(host_set);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

/*
 * Async job worker that gets called on a separate thread (see Xen_Job.c)
*/
static void migrate_task(void *async_job)
{
    /* Perform the migration */
    Xen_job *job = (Xen_job *)async_job;
    xen_utils_session *session = job->session;
    int state = JobState_Exception;
    CMPIrc statusrc = CMPI_RC_ERR_FAILED;
    char *job_status_description = "ERROR: Migrate VM failed with unknown error";
    char *xen_error = NULL;
    migrate_job_context *job_context = (migrate_job_context *)job->job_context;
    bool kvpNeedsReenabling=false;
    bool error=false;
    char *uuid=NULL;
    xen_string_string_map *other_config=NULL;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Migration job started"));

    state = JobState_Running;
    job_change_state(job, session, state, 0, 0, STATE_MIGRATE_STARTED);
    sleep(1);

    if(!error && !xen_vm_get_uuid(session->xen, &uuid, job_context->vm)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get the UUID of the vm."));
        error=true;
    }

    if(!error && !xen_vm_get_other_config(session->xen, &other_config, job_context->vm)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get other_config of the VM"));
        error=true;
    }

    if(!error && xen_utils_get_from_string_string_map(other_config, "kvp_enabled")) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("KVP is enabled, disabling it to allow migration"));
        kvpNeedsReenabling=true;
        if(xen_utils_preparemigration_kvp_channel(session, uuid) != Xen_KVP_RC_OK) {
            error=true;
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not prepare KVP for migration"));
        }
    }

    if(!error) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Doing the migration"));
        xen_string_string_map *options = xen_string_string_map_alloc(0);
        if(!xen_vm_pool_migrate(session->xen, job_context->vm, job_context->host, options)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not do the migration"));
            error=true;
        }
        xen_string_string_map_free(options);
    }

    if(!error) {
        state = JobState_Completed;
        statusrc = CMPI_RC_OK;
        job_status_description = "Completed Successfully";
    }
    else {
        state = JobState_Exception;
        statusrc = CMPI_RC_ERR_FAILED;
        xen_error = xen_utils_get_xen_error(session->xen);
        job_status_description = xen_error;
    }

    // The reenabling of KVM is always done,  it is not depending on whether the migration was succesfull / an error occured
    if(kvpNeedsReenabling) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Reenabling KVP"));
        if (xen_utils_finishmigration_kvp_channel(session, uuid) != Xen_KVP_RC_OK) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not reenable KVP"));
			state = JobState_Exception;
			statusrc = CMPI_RC_ERR_FAILED;
			job_status_description = "Error: Could not re-enable KVP channel";
        }
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Migration job status %d (%s)", statusrc, job_status_description));
    job_change_state(job, session, state, 100, statusrc, job_status_description);

    if(job_context->vm) 
        xen_vm_free(job_context->vm);
    if(job_context->host)
        xen_host_free(job_context->host);
    free(job_context);
    if(xen_error)
        free(xen_error);
    if(uuid)
        free(uuid);
    if(other_config)
        free(other_config);
}
