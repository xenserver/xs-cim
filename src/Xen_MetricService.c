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

#include <curl/curl.h>
#include <curl/easy.h>
#include <unistd.h>
#include "Xen_MetricService.h"
#include "providerinterface.h"
#include "xen_utils.h"

static const char * classname = "Xen_MetricService";    
static const char *keys[] = {"SystemName","SystemCreationClassName","CreationClassName","Name"}; 
static const char *key_property = "Name";

/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
static int get_performance_metrics_for_system(
    const CMPIBroker *broker, 
    const CMPIContext *context,
    xen_utils_session *session, 
    CMPIObjectPath *system_ref, 
    CMPIDateTime *starttime, 
    CMPIDateTime *endtime, 
    unsigned int duration, 
    unsigned int resolution, 
    char **metrics_xml_out, 
    CMPIStatus *status);

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
    resources->current_resource  = 0;
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
    /* only 1 fake Service object has to be returned */
    if (resources_list->current_resource >= 1)
        return CMPI_RC_ERR_NOT_FOUND;
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
#ifdef HOST_NAME_MAX
    char systemname[HOST_NAME_MAX];
    gethostname(systemname,HOST_NAME_MAX);
#else
    char systemname[255];
    gethostname(systemname,255);
#endif

    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AvailableRequestedStates",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Performance Metrics Service", CMPI_chars);
    DMTF_CommunicationStatus commStatus =  DMTF_CommunicationStatus_Communication_OK;
    CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&commStatus, CMPI_uint16);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_MetricService", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)"Xen Performance Metrics Service", CMPI_chars);
    //CMSetProperty(inst, "DetailedStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ElementName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "EnabledState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    DMTF_HealthState health_state = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&health_state, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "InstanceID",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)"Xen Metrics Service", CMPI_chars);
    //CMSetProperty(inst, "OperatingStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PrimaryOwnerContact",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PrimaryOwnerName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PrimaryStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "RequestedState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Started",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "StartMode",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Status",(CMPIValue *)DMTF_Status_OK, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)systemname, CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TransitioningToState",(CMPIValue *)&<value>, CMPI_uint16);

    return CMPI_RC_OK;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_MetricService)

/*******************************************************************************
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 ******************************************************************************/
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,       /* [in] CMPI Broker service */
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
        CMSetStatusWithChars(broker, &status, CMPI_RC_ERR_NOT_FOUND, 
            "Couldnt find the Xen_MetricService object to invoke the method on. Have you specified the 'Name' key ?");
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt find the Xen_MetricService object to invoke the method on"));
        goto Exit;
    }

    if (strcmp(methodname, "GetPerformanceMetricsForSystem") == 0) {
        CMPIDateTime *starttime = NULL, *endtime = NULL;
        char *metrics = NULL;
        unsigned int resolution = 0, duration = 0;
        CMPIObjectPath *system_ref = NULL;

        if (!_GetArgument(broker, argsin, "System", CMPI_ref, &argdata, &status) && 
            !CMIsNullValue(argdata))
            goto Exit;
        system_ref = argdata.value.ref;
        if (_GetArgument(broker, argsin, "StartTime", CMPI_dateTime, &argdata, &status))
            starttime = argdata.value.dateTime;
        if (_GetArgument(broker, argsin, "EndTime", CMPI_dateTime, &argdata, &status))
            endtime = argdata.value.dateTime;
        if(endtime == NULL && starttime == NULL)
            if (_GetArgument(broker, argsin, "TimeDuration", CMPI_uint32, &argdata, &status))
                duration = argdata.value.uint32;
        if (_GetArgument(broker, argsin, "ResolutionInterval", CMPI_uint32, &argdata, &status))
            resolution = argdata.value.uint32;

        rc = get_performance_metrics_for_system(broker, context, session, system_ref, 
                                                starttime, endtime, duration, resolution, 
                                                &metrics, &status);
        if (rc == 0 && metrics) {
            CMAddArg(argsout, "Metrics", (CMPIValue *)metrics, CMPI_chars);
            free(metrics);
        }
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
XenMethodMIStub(Xen_MetricService)

/******************************************************************************
 * Helper functions to fetch the Historical XML Performance data from xapi 
 *****************************************************************************/
typedef struct _curl_resp {
    char *data;
    int len;
} curl_resp;

/* CURL callback function */
static size_t _write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    curl_resp *resp = userp;
    resp->data = (char *) realloc(resp->data, resp->len+ realsize +1);

    memcpy(&(resp->data[resp->len]), buffer, realsize);
    resp->len += realsize;
    resp->data [resp->len] = '\0';

    return realsize;
}

#define GUID_STRLEN 36      // size of a GUID
#define UINT64_STRLEN 20    // size of the largest 64-bit integer
static char* _create_curl_url(
    char *server, bool host, char *uuid, xen_session *session,
    time_t starttime, time_t endtime, int resolution)
{
    /* calculate max possible size of URL */
    int buf_len = strlen("http://") + strlen(server) + strlen("/rrd_updates?") + 
                  strlen("&session_id=") + GUID_STRLEN + strlen("&vm_uuid=") + GUID_STRLEN + strlen("&host=true") + 
                  strlen("&start=") + UINT64_STRLEN + strlen("&end=") + UINT64_STRLEN + 1;
    char *metrics_url = calloc(1, buf_len);
    if (host)
        snprintf(metrics_url, buf_len-1,
            "http://%s/rrd_updates?session_id=%s&vm_uuid=none&host=true", server, session->session_id);
    else
        snprintf(metrics_url, buf_len-1,
            "http://%s/rrd_updates?session_id=%s&vm_uuid=%s", server, session->session_id, uuid);

    if (starttime == 0) {
        starttime = time(NULL); /* get the current metrics, if the start time was not specified */
    }
    char tmp[UINT64_STRLEN+1];
    memset(tmp, 0, sizeof(tmp));
    snprintf(tmp, UINT64_STRLEN, "%lu", starttime);
    strncat(metrics_url, "&start=", buf_len - strlen(metrics_url) -1);
    strncat(metrics_url, tmp, buf_len - strlen(metrics_url) -1);

    return metrics_url;
}

/******************************************************************************
 * get_performance_metrics_for_system
 *
 *     Get the requestsed performance metrics for the host or vm by making 
 *     queries to the RRD daemon. The returned data is in XPort XML format
 *     (http://oss.oetiker.ch/rrdtool/doc/rrdxport.en.html)
 *
 *     Metrics are collected for running VMs on the server that its running on.
 *     All VM requests need to be directed to its host. Also, host requests
 *     need to be directred to the host itself.
 * @param in broker - CMPI Broker services
 * @param in context - caller's context (including creds)
 * @param in session - validated xen session handle
 * @param in system_ref - the CIM reference to the System (VM or host) whose metrics
                        need to be gathered
 * @param in cmpistarttime - start time for gathering the metrics
 * @param ni cmpiendtime - end time for gathering the metrics
 * @param in resolution - metric gathering interval
 * @param out metrics_xml_out - histroic metrics XML in XPORT format 
 * @param in/out status - CMPI status
 *
 * @returns DMTF method return codes (see Xen_MetricsService.h)
 *****************************************************************************/
static int get_performance_metrics_for_system(
    const CMPIBroker *broker, 
    const CMPIContext *context,
    xen_utils_session *session, 
    CMPIObjectPath *system_ref, 
    CMPIDateTime *cmpistarttime, 
    CMPIDateTime *cmpiendtime, 
    unsigned int duration, 
    unsigned int resolution, 
    char **metrics_xml_out, 
    CMPIStatus *status)
{
    char *class_name = NULL, *uuid = NULL;
    bool host_metrics = false;
    int http_code = 0;
    xen_host host = NULL;
    char *host_ip = NULL;
    time_t starttime, endtime;
    char *status_msg = "ERROR: Unknown error";
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    int rc = Xen_MetricService_GetPerformanceMetricsForSystem_Invalid_Parameter;
    char msg[MAX_TRACE_LEN];

    /* Get the keys to identify which host/vm we need to get metrics for */
    CMPIData keydata = CMGetKey(system_ref, "Name", status);
    if (!CMIsNullValue(keydata))
        uuid = CMGetCharPtr(keydata.value.string);

    keydata = CMGetKey(system_ref, "CreationClassName", status);
    if (!CMIsNullValue(keydata))
        class_name = CMGetCharPtr(keydata.value.string);

    if (!uuid || !class_name) {
        status_msg = "ERROR: The System reference passed in doesnt contain the Name or the CreationClassName keys";
        goto Exit;
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Get metrics for %s(%s)", class_name, uuid));
    if (strcmp(class_name, "Xen_HostComputerSystem") == 0) {
        /* host metrics need to be collected from the host themselves */
        host_metrics = true;
        if (!xen_host_get_by_uuid(session->xen, &host, uuid)) {
            goto Exit;
        }
    }
    else {
        /* VM metrics are to be collected from the host its running on, if started */ 
        xen_vm vm = NULL;
        enum xen_vm_power_state state;
        if (!xen_vm_get_by_uuid(session->xen, &vm, uuid))
            goto Exit;

        /* check if the vm is running or paused, only then get the host its resident on */
        /* for some reason, getting the resident_on property always succeeds even if its Null */
        if (!xen_vm_get_power_state(session->xen, &state, vm))
            goto Exit;

        if (((state != XEN_VM_POWER_STATE_PAUSED) && (state != XEN_VM_POWER_STATE_RUNNING)) || 
            !xen_vm_get_resident_on(session->xen, &host, vm) || 
            (host == NULL)) {
            RESET_XEN_ERROR(session->xen); /* reset the error */
            /* Get the metrics from the pool master */
            xen_pool_set *pool_set = NULL;
            if (!xen_pool_get_all(session->xen, &pool_set) || 
                (pool_set == NULL) || (pool_set->size == 0))
                goto Exit;
            if (!xen_pool_get_master(session->xen, &host, pool_set->contents[0]) 
                || (host == NULL))
                goto Exit;
            xen_pool_set_free(pool_set);
        }
        xen_vm_free(vm);
    }

    /* Get the host's IP address to use in the URL */
    if (!xen_host_get_address(session->xen, &host_ip, host))
        goto Exit;

    if(duration == 0) {
        starttime = xen_utils_CMPIDateTime_to_time_t(broker, cmpistarttime);
        endtime = xen_utils_CMPIDateTime_to_time_t(broker, cmpiendtime);
    } else {
        endtime = time(NULL); /* 'now' in seconds since epoch */
        starttime = endtime - (duration*60);
    }

    /* reinitialize the error code */
    rc = Xen_MetricService_GetPerformanceMetricsForSystem_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    CURL *curl = NULL;
    CURLcode res = CURLE_OK;
    curl = curl_easy_init();
    if (curl) {

        /* setup the response buffer */
        curl_resp* resp = malloc (sizeof(curl_resp));
        resp->data = NULL;
        resp->len = 0;

        /* create unique urls for specific metrics */
        char *metrics_url = _create_curl_url(host_ip, host_metrics, uuid, session->xen, starttime, endtime, resolution);
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Getting metrics (from %ld to %ld) for URL %s", starttime, endtime, metrics_url));

        /* perform curl transaction and get HTTP response */
        curl_easy_setopt(curl, CURLOPT_URL, metrics_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_data);  /* use callback above */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);            /* use our datastructure for callback handling */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, resp);       /* handle 3XX redirects */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);      /* Dont verify server's SSL cert */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);      /* Dont check the host's cert */

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &http_code);
        if (http_code == 200 && res != CURLE_ABORTED_BY_CALLBACK) {
            *metrics_xml_out = resp->data; /* caller will free this */
        }
        else {
            /* nobody's using the buffer, free it */
            if (resp->data)
                free(resp->data);
        }
        free(resp);
        curl_easy_cleanup(curl);
        free(metrics_url);
    }

    if (*metrics_xml_out != NULL) {
        statusrc = CMPI_RC_OK;
        rc = Xen_MetricService_GetPerformanceMetricsForSystem_Completed_with_No_Error;
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Got Metrics successfully"));
    } else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("HTTP Error %d, Curl error %d", http_code, res));
        snprintf(msg, sizeof(msg)/sizeof(msg[0]), 
                 "ERROR: HTTP error %d accessing metrics. Metrics may not be available for the time duration specified.",
                 http_code);
        status_msg = msg;
    }

    Exit:
    if (host)
        xen_host_free(host);
    if (host_ip != NULL)
        free(host_ip);
    xen_utils_set_status(broker, status, statusrc, status_msg, session->xen);

    return rc;
}
