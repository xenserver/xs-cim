// Copyright (C) 2008 Citrix Systems Inc
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

#include "Xen_Processor.h"
#include "providerinterface.h"

static const char *hp_cn = "Xen_HostProcessor";
static const char *hp_keys[] = {"CreationClassName","SystemCreationClassName","DeviceID","SystemName"};
static const char *hp_key_property = "DeviceID";

//static const char *metrics_cn = "Xen_HostProcessorUtilization";
static const char *metrics_keys[] = {"InstanceID"}; 
static const char *metrics_key_property = "InstanceID";

static Family get_processor_family(int family_id, char *vendor_id, char* model_name);

static CMPIrc hostprocessor_set_properties(provider_resource *resource, 
                                    xen_host_record *host_rec, 
                                    CMPIInstance *inst);
static CMPIrc processor_metric_set_properties(const CMPIBroker *broker,
                                       provider_resource *resource, 
                                       xen_host_record *host_rec,
                                       CMPIInstance *inst);

/*********************************************************
 ************ Provider Specific functions **************** 
 ********************************************************/
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if(xen_utils_class_is_subclass_of(broker, hp_cn, classname))
        return hp_key_property;
    else
        return metrics_key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if(xen_utils_class_is_subclass_of(broker, hp_cn, classname))
        return hp_keys;
    else
        return metrics_keys;
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
    xen_host_cpu_set *cpu_set = NULL;
    if(!xen_host_cpu_get_all(session->xen, &cpu_set))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = cpu_set;
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
        xen_host_cpu_set_free((xen_host_cpu_set*)resources->ctx);
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
    xen_host_cpu_set *cpu_set = (xen_host_cpu_set *)resources_list->ctx;
    if(cpu_set == NULL || resources_list->current_resource == cpu_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_host_cpu_record *cpu_rec = NULL;
    if(!xen_host_cpu_get_record(
        session->xen,
        &cpu_rec,
        cpu_set->contents[resources_list->current_resource]
        ))
    {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    prov_res->ctx = cpu_rec;
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
    if(prov_res->ctx)
        xen_host_cpu_record_free((xen_host_cpu_record *)prov_res->ctx);
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
    xen_host_cpu host_cpu;
    xen_host_cpu_record *host_cpu_rec = NULL;

    _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));
    if(!xen_host_cpu_get_by_uuid(session->xen, &host_cpu, buf) || 
        !xen_host_cpu_get_record(session->xen, &host_cpu_rec, host_cpu))
    {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    xen_host_cpu_free(host_cpu);
    prov_res->ctx = host_cpu_rec;
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
    CMPIInstance *inst)
{

    xen_host_record* host_rec = NULL;
    xen_host_cpu_record *cpu_rec = (xen_host_cpu_record *)resource->ctx;
    if(cpu_rec->host->is_record)
        host_rec = cpu_rec->host->u.record;
    else
        xen_host_get_record(resource->session->xen, &host_rec, cpu_rec->host->u.handle);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("setting properties on %s", resource->classname));

    if(xen_utils_class_is_subclass_of(resource->broker, hp_cn, resource->classname))
        return hostprocessor_set_properties(resource, host_rec, inst);
    else
        return processor_metric_set_properties(resource->broker, resource, host_rec, inst);
}

static CMPIrc hostprocessor_set_properties(provider_resource *resource, 
                                    xen_host_record *host_rec, 
                                    CMPIInstance *inst)
{
    char buf[MAX_INSTANCEID_LEN];
    xen_host_cpu_record *cpu_rec = (xen_host_cpu_record *)resource->ctx;
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf), host_rec->uuid, cpu_rec->uuid);
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)host_rec->uuid, CMPI_chars);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_HostProcessor", CMPI_chars);

    /* Populate the instance's properties with the backend data */
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AdditionalAvailability",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "AddressWidth",(CMPIValue *)&<value>, CMPI_uint16);
    DMTF_Availability avail = DMTF_Availability_Running_Full_Power;
    CPUStatus cpuStatus =  CPUStatus_CPU_Enabled;
    DMTF_EnabledState eState = DMTF_EnabledState_Enabled;
    if(!host_rec->enabled)
    {
        avail = DMTF_Availability_Off_Line;
        cpuStatus = CPUStatus_Other;
        eState = DMTF_EnabledState_Disabled;
    }

    CMSetProperty(inst, "Availability",(CMPIValue *)&avail, CMPI_uint16);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Host Processor", CMPI_chars);
    CMSetProperty(inst, "CPUStatus",(CMPIValue *)&cpuStatus, CMPI_uint16);
    CMSetProperty(inst, "CurrentClockSpeed",(CMPIValue *)&cpu_rec->speed, CMPI_uint32);
    //CMSetProperty(inst, "DataWidth",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "Description",(CMPIValue *)cpu_rec->modelname, CMPI_chars);
    char model[4];

#if XENAPI_VERSION > 400
    sprintf(model, "%3" PRId64, cpu_rec->model);
#endif
    CMSetProperty(inst, "ElementName",(CMPIValue *)cpu_rec->vendor, CMPI_chars);
    DMTF_EnabledDefault eDefault = DMTF_EnabledDefault_Enabled;
    CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&eDefault, CMPI_uint16);
    CMSetProperty(inst, "EnabledState",(CMPIValue *)&eState, CMPI_uint16);
    //CMSetProperty(inst, "ErrorCleared",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "ErrorDescription",(CMPIValue *)<value>, CMPI_chars);

#if XENAPI_VERSION > 400
    Family family = get_processor_family(cpu_rec->family, cpu_rec->vendor, cpu_rec->modelname);
    CMSetProperty(inst, "Family",(CMPIValue *)&family, CMPI_uint16);
#endif
    DMTF_HealthState hState = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&hState, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "LastErrorCode",(CMPIValue *)&<value>, CMPI_uint32);
    int load_percentage = cpu_rec->utilisation * 100;
    CMSetProperty(inst, "LoadPercentage", (CMPIValue *) &load_percentage, CMPI_uint16);
    CMSetProperty(inst, "MaxClockSpeed",(CMPIValue *)&cpu_rec->speed, CMPI_uint32);
    //CMSetProperty(inst, "MaxQuiesceTime",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "Name",(CMPIValue *)cpu_rec->vendor, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherFamilyDescription",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "PowerManagementSupported",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "PowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "RequestedState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Role",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Status",(CMPIValue *)DMTF_Status_OK, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "StatusInfo",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "Stepping",(CMPIValue *)cpu_rec->stepping, CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "UniqueID",(CMPIValue *)cpu_rec->uuid, CMPI_chars);
    //CMSetProperty(inst, "UpgradeMethod",(CMPIValue *)&<value>, CMPI_uint16);

    if(!cpu_rec->host->is_record)
        xen_host_record_free(host_rec);

    return CMPI_RC_OK;
}

static CMPIrc processor_metric_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    xen_host_record *host_rec,
    CMPIInstance *inst)
{
    char buf[MAX_INSTANCEID_LEN];
    xen_host_cpu_record *cpu_rec = (xen_host_cpu_record *)resource->ctx;
    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, host_rec->uuid, cpu_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    // Is this the MetricsDefinitionID or the classname ?
    snprintf(buf, MAX_INSTANCEID_LEN, "%sDef", resource->classname);
    CMSetProperty(inst, "MetricDefinitionId",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "BreakdownDimension",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "BreakdownValue",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Host Processor Metrics", CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "Duration",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "ElementName",(CMPIValue *)host_rec->uuid, CMPI_chars);
    CMSetProperty(inst, "MeasuredElementName",(CMPIValue *)host_rec->name_label, CMPI_chars);

    xen_host host = NULL;
    double load_percentage = 0.0;
    xen_host_get_by_uuid(resource->session->xen, &host, host_rec->uuid);
    if(host) {

        snprintf(buf, MAX_INSTANCEID_LEN, "cpu%" PRId64, cpu_rec->number);
        xen_host_query_data_source(resource->session->xen, &load_percentage, host, buf);
        CMSetProperty(inst, "Description",(CMPIValue *)buf, CMPI_chars);
        xen_host_free(host);
    }
    snprintf(buf, MAX_INSTANCEID_LEN, "%f", (load_percentage*100));
    CMSetProperty(inst, "MetricValue",(CMPIValue *)buf, CMPI_chars);

    CMPIDateTime *date_time = xen_utils_CMPIDateTime_now(broker);
    CMSetProperty(inst, "TimeStamp",(CMPIValue *)&date_time, CMPI_dateTime);
    bool vol=true;
    CMSetProperty(inst, "Volatile",(CMPIValue *)&vol, CMPI_boolean);
    return CMPI_RC_OK;
}

/* map model name of the processor 'id' to the respective CIM value */
Family get_processor_family(int family_id, char *vendor_id, char* model_name) {
    Family rv    = Family_Unknown;
    /* Intel Family */
    if( strstr( model_name, "Intel") != NULL )
    {
        /* Pentium */
        if( strstr(model_name, "Pentium") != NULL )
        {
            if( strstr( model_name, "Pro") != NULL ) rv = Family_Pentium_R__Pro;
            else if( strstr( model_name, "III") != NULL )
            {
                if( strstr( model_name, "Xeon") != NULL ) rv = Family_Pentium_R__III_Xeon_TM_;
                else if( strstr( model_name, "SpeedStep") != NULL )
                    rv = Family_Pentium_R__III_Processor_with_Intel_R__SpeedStep_TM__Technology;
                else rv = Family_Pentium_R__III;
            }
            /* II */
            else if( strstr( model_name, "II") != NULL )
            {
                if( strstr( model_name, "Xeon") != NULL ) rv = Family_Pentium_R__II_Xeon_TM_;
                else rv = Family_Pentium_R__II;
            }
            else if( strstr( model_name, "MMX") != NULL ) rv = Family_Pentium_R__processor_with_MMX_TM__technology;
            else if( strstr( model_name, "Celeron") != NULL ) rv = Family_Celeron_TM_;
            else if( strstr( model_name, "4") != NULL ) rv = Family_Celeron_TM_;
            else rv = Family_Pentium_R__brand;
        }
        /* Core family */
        else if(strstr(model_name, "Core"))
        {
            if(strstr(model_name, "Core(TM)2") != NULL) rv = Family_Intel_R__Core_TM_2_Duo_Processor;
            else rv = Family_Intel_R__Core_TM__Solo_Processor;
        }
        else rv = Family_Other; /* Other */
    }
    /* AMD Family */
    else if( strstr( model_name, "AMD") != NULL )
    {
        if( strstr( model_name, "K5") != NULL ) rv = Family_K5_Family;
        else if( strstr( model_name, "K6-2") != NULL ) rv = Family_K6_2;
        else if( strstr( model_name, "K6-3") != NULL ) rv = Family_K6_3;
        else if( strstr( model_name, "K6") != NULL ) rv = Family_K6_Family;
        else if( strstr( model_name, "Athlon") != NULL ) rv = Family_AMD_Athlon_TM__Processor_Family;
    }
    return rv;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_HostProcessor)

