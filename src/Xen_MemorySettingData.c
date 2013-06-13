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
#include <math.h>

#include <cmpidt.h>
#include <cmpimacs.h>
#include "cmpilify.h"
#include "cmpitrace.h"

#include "xen_utils.h"
#include <stdlib.h>
#include "provider_common.h"
#include "RASDs.h"
/******************************************************************************
* mem_rasd_to_vm_rec
*
* This function attempts to parse a Memory CIM instance and populate the
* memory information in the VM record.
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/
int memory_rasd_to_vm_rec(
    const CMPIBroker* broker,
    CMPIInstance *mem_rasd,
    xen_vm_record *vm_rec,
    vm_resource_operation op,
    CMPIStatus *status)
{
    CMPIData propertyvalue;
    int64_t multiplier = 1; /* bytes by default */
    int64_t reserve_mem_size = 0, mem_size = 0, limit_mem_size = 0;

    propertyvalue = CMGetProperty(mem_rasd, "AllocationUnits", status);
    if(status->rc != CMPI_RC_OK)
        propertyvalue = CMGetProperty(mem_rasd, "VirtualQuantityUnits", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        char *units = CMGetCharPtr(propertyvalue.value.string);
        multiplier = xen_utils_get_alloc_units(units);
        if(multiplier == 0) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Memory RASD has no AllocationUnits"));
            CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM,
                "Memory RASD AllocationUnits can be 'bytes*2^10', 'bytes*2^20', 'bytes*2^30', 'MB' or 'GB'");
            return 0;
        }
    }

    propertyvalue = CMGetProperty(mem_rasd, "VirtualQuantity", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
         mem_size = ((int64_t)propertyvalue.value.uint64) * multiplier;
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Memory RASD has no VirtualQuantity"));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM, "Memory RASD has no VirtualQuantity");
        return 0;
    }

    propertyvalue = CMGetProperty(mem_rasd, "Reservation", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.value.uint64 != 0))
        reserve_mem_size = (int64_t) (propertyvalue.value.uint64 * multiplier);
    else
        reserve_mem_size = mem_size;

    propertyvalue = CMGetProperty(mem_rasd, "Limit", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.value.uint64 != 0))
        limit_mem_size = (int64_t) (propertyvalue.value.uint64 * multiplier);
    else
        limit_mem_size = mem_size;

    /*
    * 1. Handle defaults if memory is not specified.
    */
    switch(op)
    {
    case resource_add:
        vm_rec->memory_static_max += mem_size;
        vm_rec->memory_static_min += reserve_mem_size;
        vm_rec->memory_dynamic_max += limit_mem_size;
        vm_rec->memory_dynamic_min += reserve_mem_size;
        break;
    case resource_delete:
        vm_rec->memory_static_max -= mem_size;
        vm_rec->memory_static_min -= reserve_mem_size;
        vm_rec->memory_dynamic_max -= limit_mem_size;
        vm_rec->memory_dynamic_min -= reserve_mem_size;
        break;
    case resource_modify:
        vm_rec->memory_static_max = mem_size;
        vm_rec->memory_static_min = reserve_mem_size;
        vm_rec->memory_dynamic_max = limit_mem_size;
        vm_rec->memory_dynamic_min = reserve_mem_size;
        break;
    }

    return 1;
}

#define ONE_MB (1 << 20)
int64_t round_memory(int64_t memory, int64_t round_to)
{
    /* round it to the nearest 1 MB */
    int64_t rounded_mem = memory;
    if(rounded_mem > round_to)
        /* round up */
        return(int64_t)ceilf((double)memory/round_to)*round_to;
    else
        return round_to;
}
/*
* memory_rasd_from_vm_rec
*
* This function attempts to construct a Memory CIM instance using the
* VM record as input. This is used by the Memory RASD provider to
* set the properties of the CIM instance
*
* Returns 1 on Success and 0 on failure.
*/
int memory_rasd_from_vm_rec(
    const CMPIBroker* broker,
    xen_utils_session *session,
    CMPIInstance* inst,
    xen_vm_record *vm_rec
    )
{
    char buf[MAX_INSTANCEID_LEN];
    int64_t memory = 0, actual_mem = 0;
    int resourceType = DMTF_ResourceType_Memory;
    int consumerVisibility = DMTF_ConsumerVisibility_Passed_Through;

    /* Set the CMPIInstance properties from the resource data. */
    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid, "Memory");
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    if(vm_rec->is_a_template)
        CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Template Memory Setting Data", CMPI_chars);
    else if(vm_rec->is_a_snapshot)
        CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Snapshot Memory Setting Data", CMPI_chars);
    else
        CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Domain Memory Setting Data", CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)vm_rec->name_label, CMPI_chars);
    CMSetProperty(inst, "ResourceType", (CMPIValue *)&resourceType, CMPI_uint16);
    CMSetProperty(inst, "ConsumerVisibility", (CMPIValue *)&consumerVisibility, CMPI_uint16);

    /* Get the memory_actual for actual VMs */
    if(vm_rec->metrics->is_record)
        actual_mem = vm_rec->metrics->u.record->memory_actual;
    else
        xen_vm_metrics_get_memory_actual(session->xen, &actual_mem, vm_rec->metrics->u.handle);
    if(actual_mem == 0)
        actual_mem = vm_rec->memory_static_max;

    memory = round_memory(vm_rec->memory_dynamic_min, ONE_MB)/ONE_MB;
    CMSetProperty(inst, "Reservation", (CMPIValue *)&memory, CMPI_uint64);
    memory = round_memory(actual_mem, ONE_MB)/ONE_MB;
    CMSetProperty(inst, "VirtualQuantity", (CMPIValue *)&memory, CMPI_uint64);
    memory = round_memory(vm_rec->memory_dynamic_max, ONE_MB)/ONE_MB;
    CMSetProperty(inst, "Limit", (CMPIValue *)&memory, CMPI_uint64);
    CMSetProperty(inst, "AllocationUnits", (CMPIValue *)"byte*2^20", CMPI_chars);
    CMSetProperty(inst, "VirtualQuantityUnits", (CMPIValue *)"byte*2^20", CMPI_chars);

    int alloctype = 1;
    CMSetProperty(inst, "AutomaticAllocation", (CMPIValue *)&alloctype, CMPI_boolean);
    CMSetProperty(inst, "AutomaticDeallocation", (CMPIValue *)&alloctype, CMPI_boolean);

    return CMPI_RC_OK;
}

void set_memory_defaults(xen_vm_record *vm_rec)
{
    vm_rec->memory_dynamic_max = 256 << 20; /* 256 MB default */
    vm_rec->memory_static_max = vm_rec->memory_dynamic_max;

    vm_rec->memory_dynamic_min = 256 << 20;
    vm_rec->memory_static_min = 128 << 20;
}
/*
* Modify the VM's memory settings based on the RASD
*/
int memory_rasd_modify(
    xen_utils_session *session,
    xen_vm vm,
    xen_vm_record *vm_template)
{
    int rc = 0;
    if(!vm_template || !vm) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- vm record or handle is NULL "));
        return 0;
    }
    xen_vm_record *vm_rec = NULL;
    if(xen_vm_get_record(session->xen, &vm_rec, vm) && vm_rec)
    {
        /* default to the VM's existing settings */
        int64_t static_max = vm_rec->memory_static_max, 
                static_min = vm_rec->memory_static_min,
                dynamic_max = vm_rec->memory_dynamic_max, 
                dynamic_min = vm_rec->memory_dynamic_min;

        bool need_update = false;

        /* update with with what's been passed in */
        if(vm_template->memory_static_max) {
            if (vm_template->memory_static_max != static_max) {
                static_max = vm_template->memory_static_max;
                need_update = true;
            }
        }
        if(vm_template->memory_static_min) {
            if (vm_template->memory_static_min != static_min) {
                static_min = vm_template->memory_static_min;
                need_update = true;
            }
        }
        if(vm_template->memory_dynamic_max) {
            if (vm_template->memory_dynamic_max != dynamic_max) {
                dynamic_max = vm_template->memory_dynamic_max;
                need_update = true;
            }
        }
        if(vm_template->memory_dynamic_min) {
            if (vm_template->memory_dynamic_min != dynamic_min) {
                dynamic_min = vm_template->memory_dynamic_min;
                need_update = true;
            }
        }

        if (need_update) {
            /* Change it in one go, otherwise the memory checks in Xen go crazy */
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, 
                         ("--- Changing VM memory settings for %s (smin:%lld, smax:%lld, dmin:%lld, dmax:%lld)", 
                         vm_rec->uuid, static_min, static_max, dynamic_min, dynamic_max));
            rc = xen_vm_set_memory_limits(session->xen, vm, static_min, static_max, dynamic_min, dynamic_max);

            /* This could fail depending on whether we are talking to Xen 5.6. or below */
            if(!session->xen->ok) {
                /* fall back to the old way of doing things */
                RESET_XEN_ERROR(session->xen);
                rc = (xen_vm_set_memory_static_max(session->xen, vm, static_max) &&
                      xen_vm_set_memory_static_min(session->xen, vm, static_min) &&
                      xen_vm_set_memory_dynamic_max(session->xen, vm, dynamic_max) &&
                      xen_vm_set_memory_dynamic_min(session->xen, vm, dynamic_min));
            }
        } else {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- No need to update memory configuration."));
        }
        xen_vm_record_free(vm_rec);
    }

    if(rc == 0)
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    return rc;
}

CMPIObjectPath *memory_rasd_create_ref(
    const CMPIBroker *broker, 
    const char *name_space,
    xen_utils_session *session,
    xen_vm_record *vm_rec
    )
{
    char inst_id[MAX_INSTANCEID_LEN];

    CMPIObjectPath *result_setting = CMNewObjectPath(broker, name_space, "Xen_MemorySettingData", NULL);
    _CMPICreateNewDeviceInstanceID(inst_id, MAX_INSTANCEID_LEN, vm_rec->uuid, "Memory");
    CMAddKey(result_setting, "InstanceID", (CMPIValue *)inst_id, CMPI_chars);
    return result_setting;
}

