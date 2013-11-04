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
#include <cmpidt.h>
#include <cmpimacs.h>
#include "cmpilify.h"
#include "cmpitrace.h"
#include <stdlib.h>
#include "xen_utils.h"
#include "provider_common.h"
#include "RASDs.h"
/*******************************************************************************
* proc_rasd_to_vm_rec
*
* This function attempts to parse the RASD CIM instance and build a VM record
* with all the processor related information filled in.
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/
#define VCPUS_MAX 8 /* Maximum allowed VCPUs per VM */
int proc_rasd_to_vm_rec(
    const CMPIBroker* broker,
    CMPIInstance *proc_rasd, /* in */
    xen_vm_record *vm_rec,   /* in, out */
    vm_resource_operation operation,
    CMPIStatus  *status)     /* out */
{
    CMPIData propertyvalue;

    propertyvalue = CMGetProperty(proc_rasd, "VirtualQuantity", status);
    if((status->rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Memory RASD has no VirtualQuantity"));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM, "Memory RASD has no VirtualQuantity");
        return 0;
    }

    /*
     * Default to 1 vcpu if VirtualQuantity not specified.
     * Allocation capabilities should describe this default behavior.
     */
    switch(operation)
    {
    case resource_add:
        vm_rec->vcpus_max += propertyvalue.value.uint64;
        break;
    case resource_delete:
        vm_rec->vcpus_max -= propertyvalue.value.uint64;
        break;
    case resource_modify:
        vm_rec->vcpus_max = propertyvalue.value.uint64;
        break;
    }

    propertyvalue = CMGetProperty(proc_rasd, "Reservation", status);
    if((status->rc == CMPI_RC_OK) && 
        !CMIsNullValue(propertyvalue) && 
        (propertyvalue.value.uint64 != 0))
    {
        /* Modify existing value */
        switch(operation)
        {
        case resource_add:
            vm_rec->vcpus_at_startup += propertyvalue.value.uint64;
            break;
        case resource_delete:
            vm_rec->vcpus_at_startup -= propertyvalue.value.uint64;
            break;
        case resource_modify:
            vm_rec->vcpus_at_startup = propertyvalue.value.uint64;
            break;
        }
    }
    else
        vm_rec->vcpus_at_startup = vm_rec->vcpus_max; /* Default behaviour */

    if(vm_rec->vcpus_at_startup > VCPUS_MAX || vm_rec->vcpus_max > VCPUS_MAX) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- VCPUs exceed limit of 8 per VM"));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM, "VCPUs exceed limit of 8 per VM");
        return 0;
    }

    /* Weight and Cap in XenServer is explained at http://support.citrix.com/article/CTX117960 */
    char buf[64];
    propertyvalue = CMGetProperty(proc_rasd, "Limit", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.value.uint64 != 0)) {
        snprintf(buf, 64, "%lld", propertyvalue.value.uint64);
        xen_utils_add_to_string_string_map("cap", buf, &(vm_rec->vcpus_params));
    }
    propertyvalue = CMGetProperty(proc_rasd, "Weight", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.value.uint64 != 0))  {
        snprintf(buf, 64, "%d", propertyvalue.value.uint32);
        xen_utils_add_to_string_string_map("weight", buf, &(vm_rec->vcpus_params));
    }

    if(vm_rec->vcpus_params == NULL)
        vm_rec->vcpus_params = xen_string_string_map_alloc(0); 

    return 1;
}
/*
* proc_rasd_from_vm_rec
*
* This function attempts to construct a Processor CIM instance using the
* VM record as input. This is used by the Processor RASD provider to
* set the properties of the CIM instance
*
* Returns 1 on Success and 0 on failure.
*/
int proc_rasd_from_vm_rec(
    const CMPIBroker* broker,
    xen_utils_session *session,
    CMPIInstance *inst,
    xen_vm_record * vm_rec
    )
{
    int int_prop_val=0;
    int64_t int64_prop_val=0;
    char buf[MAX_INSTANCEID_LEN];
    memset(&buf, 0, sizeof(buf));

    /* Set the CMPIInstance properties from the resource data. */
    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid, "Processor");
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)vm_rec->name_label, CMPI_chars);
    if(vm_rec->is_a_snapshot)
        CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Snapshot Processor Setting Data", CMPI_chars);
    else if(vm_rec->is_a_template)
        CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Template Processor Setting Data", CMPI_chars);
    else
        CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Domain Processor Setting Data", CMPI_chars);

    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)"count", CMPI_chars);

    int_prop_val = DMTF_ResourceType_Processor;
    CMSetProperty(inst, "ResourceType", (CMPIValue *)&int_prop_val, CMPI_uint16);
    int_prop_val = DMTF_ConsumerVisibility_Virtualized;
    CMSetProperty(inst, "ConsumerVisibility", (CMPIValue *)&int_prop_val, CMPI_uint16);

    int_prop_val = 1; /* true */
    CMSetProperty(inst, "AutomaticAllocation", (CMPIValue *)&int_prop_val, CMPI_boolean);
    CMSetProperty(inst, "AutomaticDeallocation", (CMPIValue *)&int_prop_val, CMPI_boolean);

    xen_host_record* host_rec = xen_utils_get_domain_host(session, vm_rec);
    if(host_rec) {
        CMSetProperty(inst, "PoolID", (CMPIValue *)host_rec->uuid, CMPI_chars);
        xen_utils_free_domain_host(vm_rec, host_rec);
    }

    int64_prop_val = vm_rec->vcpus_at_startup;
    CMSetProperty(inst, "Reservation", (CMPIValue *)&int64_prop_val, CMPI_uint64);
    int64_prop_val = vm_rec->vcpus_max;
    CMSetProperty(inst, "VirtualQuantity", (CMPIValue *)&int64_prop_val, CMPI_uint64);
    if(vm_rec->vcpus_params == NULL)
        return CMPI_RC_ERR_FAILED;

    /* Weight and Cap in XenServer is explained at http://support.citrix.com/article/CTX117960 */
    /* Weight is described in relative terms between VCPUs and each can be a value from 1-65536 */
    char *map_val = xen_utils_get_from_string_string_map(vm_rec->vcpus_params, "weight");
    if(map_val) {
        int_prop_val = atol(map_val);
        CMSetProperty(inst, "Weight",(CMPIValue *)&int_prop_val, CMPI_uint32);
    }
    /* Cap is the max host CPU % that a vCPU can use even if its idle (100 is maximum) */
    map_val = xen_utils_get_from_string_string_map(vm_rec->vcpus_params, "cap");
    if(map_val) {
        int64_prop_val = atoll(map_val);
        CMSetProperty(inst, "Limit",(CMPIValue *)&int64_prop_val, CMPI_uint64);
    }
    return CMPI_RC_OK;
}

/*
* set_processor_defaults
* Picks valid defaults for processor information for a VM
*/
void set_processor_defaults(xen_vm_record *vm_rec)
{
    xen_utils_add_to_string_string_map("cap", "100", &(vm_rec->vcpus_params));
    xen_utils_add_to_string_string_map("weight", "512", &(vm_rec->vcpus_params));
    vm_rec->vcpus_max = 1;
    vm_rec->vcpus_at_startup = 1;
}

/*
* Modify the VM's processor settings based on the RASD
*/
int proc_rasd_modify(
    xen_utils_session *session,
    xen_vm vm,
    xen_vm_record *vm_rec
    )
{
    int rc = 0;
    RESET_XEN_ERROR(session->xen);

    if(!vm_rec || !vm) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Modify VM processor - record is NULL "));
        return 0;
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- Modify VM processor for %s", vm_rec->uuid));

    int64_t max_vcpus;
    int64_t startup_vcpus;

    //Set the startup VCPUs as 1 initially
    rc = xen_vm_set_vcpus_at_startup(session->xen, vm, 1);
 
    if (vm_rec->vcpus_max) {
        rc = xen_vm_get_vcpus_max(session->xen, &max_vcpus, vm);

        // Make sure we don't do anything if an update is not required.
        if (vm_rec->vcpus_max != max_vcpus) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Updating vcpus_max to %lld", vm_rec->vcpus_max));
            rc = xen_vm_set_vcpus_max(session->xen, vm, vm_rec->vcpus_max);
        }

    }
    if(vm_rec->vcpus_at_startup ) {    
        rc = xen_vm_get_vcpus_at_startup(session->xen, &startup_vcpus, vm);

        // Make sure we don't do anything if an update is not required.
        if (vm_rec->vcpus_at_startup != startup_vcpus) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Updating vcpus_at_startup to %lld", vm_rec->vcpus_at_startup));
            rc = xen_vm_set_vcpus_at_startup(session->xen, vm, vm_rec->vcpus_at_startup);
        }

    } else {

      // Like above, check whether this actually requires an update call.
      rc = xen_vm_get_vcpus_at_startup(session->xen, &startup_vcpus, vm);

      if (vm_rec->vcpus_max != startup_vcpus) {
          _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Updating vcpus_at_startup to %lld", vm_rec->vcpus_max));
          rc = xen_vm_set_vcpus_at_startup(session->xen, vm, vm_rec->vcpus_max);
      }

    }
    if(rc == 0)
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    return rc;
}

CMPIObjectPath *proc_rasd_create_ref(
    const CMPIBroker *broker, 
    const char *name_space,
    xen_utils_session *session,
    xen_vm_record *vm_rec
    )
{
    char inst_id[MAX_INSTANCEID_LEN];

    CMPIObjectPath *result_setting = CMNewObjectPath(broker, name_space, "Xen_ProcessorSettingData", NULL);
    _CMPICreateNewDeviceInstanceID(inst_id, MAX_INSTANCEID_LEN, vm_rec->uuid, "Processor");
    CMAddKey(result_setting, "InstanceID", (CMPIValue *)inst_id, CMPI_chars);
    return result_setting;
}

