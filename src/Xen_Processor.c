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
#include <assert.h>
#include "providerinterface.h"

typedef struct _local_vcpu_resource{
    unsigned int vcpu_id;
    char domain_uuid[XENID_LEN+1];
} local_vcpu_resource;

typedef struct _local_vcpu_list {
    local_vcpu_resource *vcpus;
    int64_t total_vcpus;
}local_vcpu_list;

/* globals for this provider */
static const char *proc_cn = "Xen_Processor";
static const char *proc_keys[] = {"CreationClassName","SystemCreationClassName","DeviceID","SystemName"}; 
static const char *proc_key_property = "DeviceID";

static const char *metrics_keys[] = {"Id"}; 
static const char *metrics_key_property = "Id";

/* Internal functions */
static CMPIrc _processor_set_properties(
    provider_resource *resource,
    xen_vm_record *vm_rec,
    xen_vm_metrics_record *metrics_rec,
    xen_host_cpu_record* cpu_rec,
    CMPIInstance *inst);
static CMPIrc _processor_metric_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    xen_vm_record *vm_rec,
    xen_vm_metrics_record *metrics_rec,
    CMPIInstance *inst);

static void _free_vcpu_resource(
    local_vcpu_resource* vcpu)
{
    if(vcpu) {
        free(vcpu->domain_uuid);
        free(vcpu);
    }
}
/*****************************************************************************
 ************ Provider Export functions **************************************
 *****************************************************************************/
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, proc_cn, classname))
        return proc_key_property;
    else
        return metrics_key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, proc_cn, classname))
        return proc_keys;
    else
        return metrics_keys;
}
/******************************************************************************
 * Function to enumerate provider specific resource
 *
 * @param session - handle to a xen_utils_session object
 * @param resources - pointer to the provider_resource_list
 *   object, the provider specific resource defined above
 *   is a member of this struct
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_list_enum(
    xen_utils_session *session, 
    provider_resource_list *resources)
{
    xen_vm_set *vms = NULL;
    xen_vm_metrics vm_metrics = NULL;
    xen_vm_record *vm_rec = NULL;
    xen_vm vm = NULL;
    int64_t vcpus_total = 0;
    int64_t vcpus_number = 0;
    int64_t vcpu_ndx = 0;
    local_vcpu_resource *vcpu_list = NULL;

    xen_domain_resources *dom_resources = NULL;
    if(!xen_utils_get_domain_resources(session, &dom_resources, vms_only) ||
       dom_resources == NULL ||
       dom_resources->domains == NULL) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        free(resources);
        return CMPI_RC_ERR_FAILED;
    }

    /*
    * Iterate through domains and get their processor resources
    */
    while(xen_utils_get_next_domain_resource(session, dom_resources, &vm, &vm_rec) && vm_rec != NULL) 
    {
        vcpus_number = vm_rec->vcpus_max;
        vcpus_total += vcpus_number;
        vcpu_list = (local_vcpu_resource *)realloc(vcpu_list, sizeof(local_vcpu_resource)*vcpus_total);
        if(vcpu_list == NULL) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Not enough memory"));
            return CMPI_RC_ERR_FAILED;
        }
        int i;
        for (i = 0; i < vcpus_number; i++,vcpu_ndx++) {
            local_vcpu_resource *vcpu = &vcpu_list[vcpu_ndx];
            vcpu->vcpu_id = i;
            strncpy(vcpu->domain_uuid, vm_rec->uuid, XENID_LEN);
            vcpu->domain_uuid[XENID_LEN] = '\0';
        }
        xen_utils_free_domain_resource(vm, vm_rec);
        vm_rec = NULL;
    }
    xen_utils_free_domain_resources(dom_resources);

    local_vcpu_list* ctx = calloc(1, sizeof(local_vcpu_list ));
    if(ctx == NULL)
        goto Error;
    ctx->total_vcpus = vcpus_total;
    ctx->vcpus = vcpu_list;

    /* Set cur_vcpu to beginning of resource list. */
    resources->ctx = ctx;
    return CMPI_RC_OK;

Error:
    if(vm_rec)
        xen_vm_record_free(vm_rec);
    if(vm_metrics)
        xen_vm_metrics_free(vm_metrics);
    if(vms)
        xen_vm_set_free(vms);
    if(vcpu_list)
        free(vcpu_list);
    return CMPI_RC_ERR_FAILED;
}
/******************************************************************************
 * Function to cleanup provider specific resource, this function is
 * called at various places in Xen_ProviderGeneric.c
 *
 * @param resources - handle to the provider_resource_list to be
 *    be cleaned up. Clean up the provider specific part of the
 *    resource.
 * @return CMPIrc error codes
 ******************************************************************************/
static CMPIrc xen_resource_list_cleanup(
    provider_resource_list *resources
    )
{
    local_vcpu_list *ctx = (local_vcpu_list *)resources->ctx;
    if (ctx) {
        if(ctx->vcpus)
            free(ctx->vcpus);
        free(ctx);
    }
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
    local_vcpu_list *ctx = (local_vcpu_list *)resources_list->ctx;
    if (ctx == NULL || (resources_list->current_resource >= ctx->total_vcpus))
        return CMPI_RC_ERR_NOT_FOUND;
    local_vcpu_resource *vcpu_list = ctx->vcpus;
    prov_res->ctx = malloc(sizeof(local_vcpu_resource));
    if(prov_res->ctx) { 
        memcpy(prov_res->ctx,&vcpu_list[resources_list->current_resource],sizeof(local_vcpu_resource));
        return CMPI_RC_OK;
    }
    else {
        return CMPI_RC_ERROR;
    }
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
  /* We only need to free the local_vcpu_resource datastructure */
  local_vcpu_resource *vcpu = (local_vcpu_resource *) prov_res->ctx;
  if (vcpu)
    free(vcpu);

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
    char *p = NULL;
    if ((p = strstr(res_uuid, "VCPU")) == NULL) {
        _free_vcpu_resource(prov_res->ctx);
        return CMPI_RC_ERR_FAILED;
    }
    local_vcpu_resource *vcpu = calloc(1, sizeof(local_vcpu_resource));
    strncpy(vcpu->domain_uuid, buf, XENID_LEN);
    vcpu->vcpu_id = atoi(p + 4);
    prov_res->ctx = vcpu;
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
    provider_resource *prov_res, 
    CMPIInstance *inst)
{

    xen_vm_record *vm_rec = NULL;
    xen_vm vm = NULL;
    xen_vm_metrics vm_metrics = NULL;
    xen_host_record *host_rec = NULL;
    xen_host_cpu_record* cpu_rec = NULL;
    bool free_cpu_rec = false;
    xen_vm_metrics_record *metrics_rec = NULL;

    local_vcpu_resource *resource = prov_res->ctx;
    if (!xen_vm_get_by_uuid(prov_res->session->xen, &vm, resource->domain_uuid)) {
        xen_utils_trace_error(prov_res->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    if (!xen_vm_get_record(prov_res->session->xen, &vm_rec, vm)) {
        xen_utils_trace_error(prov_res->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    if (xen_vm_get_metrics(prov_res->session->xen, &vm_metrics, vm)) {
        if (!xen_vm_metrics_get_record(prov_res->session->xen, &metrics_rec, vm_metrics)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                ("xen_vm_metrics_get_record failed for %s with %s",
                vm_rec->name_label, prov_res->session->xen->error_description[0]));
            RESET_XEN_ERROR(prov_res->session->xen);
        }
        xen_vm_metrics_free(vm_metrics);
    }
    else
        RESET_XEN_ERROR(prov_res->session->xen);

    /* Depending on which class this provider is handling set properties differently */
    if (xen_utils_class_is_subclass_of(prov_res->broker, proc_cn, prov_res->classname)) {
        xen_host host = NULL;
        if (xen_vm_get_resident_on(prov_res->session->xen, &host, vm)) {
            if (!xen_host_get_record(prov_res->session->xen, &host_rec, host)) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                    ("xen_host_get_record failed with %s", 
                     prov_res->session->xen->error_description[0]));
                RESET_XEN_ERROR(prov_res->session->xen);
            }
            else {
                if (host_rec->host_cpus->size >= resource->vcpu_id) {
                    xen_host_cpu_record_opt *cpu_opt = host_rec->host_cpus->contents[resource->vcpu_id];
                    if (cpu_opt->is_record)
                        cpu_rec = host_rec->host_cpus->contents[resource->vcpu_id]->u.record;
                    else {
                        xen_host_cpu_get_record(prov_res->session->xen, &cpu_rec, cpu_opt->u.handle);
                        free_cpu_rec = true;
                    }
                }
            }
            xen_host_free(host);
        }
        else {
            RESET_XEN_ERROR(prov_res->session->xen);
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                ("xen_vm_get_resident_on failed with %s", 
                 prov_res->session->xen->error_description[0]));
        }
        _processor_set_properties(prov_res, vm_rec, metrics_rec, cpu_rec, inst);
    }
    else
        _processor_metric_set_properties(prov_res->broker, prov_res, vm_rec, metrics_rec, inst);

    if (vm)
        xen_vm_free(vm);
    if (free_cpu_rec)
        xen_host_cpu_record_free(cpu_rec);
    if (metrics_rec)
        xen_vm_metrics_record_free(metrics_rec);
    if (host_rec)
        xen_host_record_free(host_rec);
    if (vm_rec)
        xen_vm_record_free(vm_rec);

    return CMPI_RC_OK;
}

static CMPIrc _processor_set_properties(
    provider_resource *resource,
    xen_vm_record *vm_rec,
    xen_vm_metrics_record *metrics_rec,
    xen_host_cpu_record* cpu_rec,
    CMPIInstance *inst)
{
    int prop_val_32;
    char deviceid[MAX_INSTANCEID_LEN];
    char vcpu_id[20];

    local_vcpu_resource *vcpu = resource->ctx;
    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "Caption",(CMPIValue *)"Processor", CMPI_chars);
    prop_val_32 = 1; /* CPU Enabled */
    CMSetProperty(inst, "CPUStatus",(CMPIValue *) &prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "CreationClassName", (CMPIValue *)"Xen_Processor", CMPI_chars);
    CMSetProperty(inst, "Description", (CMPIValue *)"Xen Virtual Processor", CMPI_chars);
    snprintf(vcpu_id, 20, "VCPU%d", vcpu->vcpu_id);
    _CMPICreateNewDeviceInstanceID(deviceid, MAX_INSTANCEID_LEN, vcpu->domain_uuid, vcpu_id);
    CMSetProperty(inst, "DeviceID",(CMPIValue *)deviceid, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)vm_rec->name_label, CMPI_chars);
    prop_val_32 = 5; /* Enabled */
    CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "EnabledState",(CMPIValue *)&prop_val_32, CMPI_uint16);
    if (cpu_rec) {
        CMSetProperty(inst, "CurrentClockSpeed", (CMPIValue *) &(cpu_rec->speed), CMPI_uint32);
        CMSetProperty(inst, "ExternalBusClockSpeed", (CMPIValue *) &(cpu_rec->speed), CMPI_uint32);

#if XENAPI_VERSION > 400
        CMSetProperty(inst, "Family", (CMPIValue *) &cpu_rec->family, CMPI_uint16);
#endif
        CMSetProperty(inst, "MaxClockSpeed", (CMPIValue *) &(cpu_rec->speed), CMPI_uint32);
        CMSetProperty(inst, "Stepping", (CMPIValue *) cpu_rec->stepping, CMPI_chars);
    }
    prop_val_32 = 5; /* OK */
    CMSetProperty(inst, "HealthState", (CMPIValue *) &prop_val_32, CMPI_uint16);
    int load_percentage = 0;
    if (metrics_rec && metrics_rec->vcpus_utilisation && metrics_rec->vcpus_utilisation->size > vcpu->vcpu_id) {
        load_percentage = metrics_rec->vcpus_utilisation->contents[vcpu->vcpu_id].val * 100;
    }
    CMSetProperty(inst, "LoadPercentage", (CMPIValue *) &load_percentage, CMPI_uint16);

    CMSetProperty(inst, "Purpose",(CMPIValue *)"Processor", CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)vm_rec->uuid, CMPI_chars);
    prop_val_32 = 2; /* OK */
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, &prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus", &arr, CMPI_uint16A);
    prop_val_32 = 12; /* Not applicable */
    CMSetProperty(inst, "RequestedState",(CMPIValue *) &prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "Role",(CMPIValue *) "Virtual Processor", CMPI_chars);
    CMSetProperty(inst, "Status", (CMPIValue *)"OK", CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName", (CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName", (CMPIValue *)vcpu->domain_uuid, CMPI_chars);
    prop_val_32 = 6; /* None */
    CMSetProperty(inst, "UpgradeMethod", (CMPIValue *)&prop_val_32, CMPI_uint16);

    // Fields not used
    // CMSetProperty(inst, "AdditionalAvailability",(CMPIValue *) &prop_val_32A, CMPI_uint16A);
    // CMSetProperty(inst, "AddressWidth",(CMPIValue *) &prop_val_32A, CMPI_uint16A);
    // CMSetProperty(inst, "Availability",(CMPIValue *) &prop_val_32, CMPI_uint16);
    // CMSetProperty(inst, "Characterisitics",(CMPIValue *) &prop_val_32A, CMPI_uint16A);
    //
    // CMSetProperty(inst, "DataWidth", (CMPIValue *) &prop_val_32, CMPI_uint16); /* 32 or 64 */
    // CMSetProperty(inst, "ErrorCleared", (CMPIValue *) &prop_val_32, CMPI_boolean);
    // CMSetProperty(inst, "ErrorDescription", (CMPIValue *) &str, CMPI_chars); /* 32 or 64 */
    // CMSetProperty(inst, "IdentifyingDescriptions", (CMPIValue *) &strA, CMPI_stringA);
    // CMSetProperty(inst, "InstallDate", (CMPIValue *) &time, CMPI_dateTime);
    // CMSetProperty(inst, "LastErrorCode", (CMPIValue *)&prop_val_32, CMPI_uint32);
    // CMSetProperty(inst, "LoadPercentageHistory", (CMPIValue *) &prop_val_32, CMPI_uint16);
    // CMSetProperty(inst, "MaxQuiesceTime", (CMPIValue *) &prop_val_64, CMPI_uint64);
    // CMSetProperty(inst, "OtherEnabledState",(CMPIValue *) &str, CMPI_chars);
    // CMSetProperty(inst, "OtherFamilyDescriptions",(CMPIValue *) &str, CMPI_string);
    // CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *) &strA, CMPI_stringA);
    // CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *) &prop_val_32_a, CMPI_uint16A);
    // CMSetProperty(inst, "PowerManagementSupported",(CMPIValue *) &prop_val_32, CMPI_boolean);
    // CMSetProperty(inst, "PowerOnHours",(CMPIValue *) &prop_val_64, CMPI_uint64);
    // CMSetProperty(inst, "StatusDescriptions",(CMPIValue *) &strA, CMPI_stringA);
    // CMSetProperty(inst, "StatusInfo", (CMPIValue *)&prop_val_32, CMPI_uint16);
    // CMSetProperty(inst, "TimeOfLastStateChange", (CMPIValue *)&dateTime, CMPI_dateTime);
    // CMSetProperty(inst, "TotalPowerObHours", (CMPIValue *)&uint64, CMPI_uint64);
    // CMSetProperty(inst, "UinqueID", (CMPIValue *)&str, CMPI_chars);
    return CMPI_RC_OK;
}

static CMPIrc _processor_metric_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    xen_vm_record *vm_rec,
    xen_vm_metrics_record *metrics_rec,
    CMPIInstance *inst)
{
    char buf[MAX_INSTANCEID_LEN];
    char vcpu_id[20];
    local_vcpu_resource *vcpu = resource->ctx;
    snprintf(vcpu_id, 20, "VCPU%d", vcpu->vcpu_id);
    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid, vcpu_id);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    // Is this the MetricsDefinitionID or the classname ?
    snprintf(buf, MAX_INSTANCEID_LEN, "%sDef", resource->classname);
    CMSetProperty(inst, "MetricDefinitionId",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "BreakdownDimension",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "BreakdownValue",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Virtual Processor metrics", CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "Duration",(CMPIValue *)&date_time, CMPI_dateTime);
    CMSetProperty(inst, "ElementName",(CMPIValue *)vm_rec->uuid, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "MeasuredElementName",(CMPIValue *)vm_rec->name_label, CMPI_chars);

    double load_percentage = 0.0;
    xen_vm vm = NULL;
    xen_vm_get_by_uuid(resource->session->xen, &vm, vm_rec->uuid);
    if (vm) {
        snprintf(buf, MAX_INSTANCEID_LEN, "cpu%d", vcpu->vcpu_id);
        xen_vm_query_data_source(resource->session->xen, &load_percentage, vm, buf);
        CMSetProperty(inst, "Description",(CMPIValue *)buf, CMPI_chars);
        xen_vm_free(vm);
    }
    snprintf(buf, MAX_INSTANCEID_LEN, "%f", (load_percentage*100));
    CMSetProperty(inst, "MetricValue",(CMPIValue *)buf, CMPI_chars);
    CMPIDateTime *date_time = xen_utils_CMPIDateTime_now(broker);
    CMSetProperty(inst, "TimeStamp",(CMPIValue *)&date_time, CMPI_dateTime);
    bool vol=true;
    CMSetProperty(inst, "Volatile",(CMPIValue *)&vol, CMPI_boolean);

    return CMPI_RC_OK;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_Processor)
    
