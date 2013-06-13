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

/*#include other header files required by the provider */
#include <assert.h>
#include "providerinterface.h"
#include "RASDs.h"
#include "Xen_KVP.h"
#include "xen_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "provider_common.h"
#include <cmpidt.h>
#include <cmpimacs.h>
#include "cmpilify.h"
#include "cmpitrace.h"


#define MAX_KVP_KEY_LEN 256

/* TODO: Define any local resources here */
/*
typedef struct _kvp_resource {
  kvp *kvp;
}local_kvp_resource;
*/

typedef struct _kvp_resource_set {
  int size;
  kvp **contents;
}local_kvp_resource_set;


//static const char *classname = "Xen_KVP";    
static const char *keys[] = {"SystemName","SystemCreationClassName","CreationClassName","DeviceID"}; 
static const char *key_property = "DeviceID";

/******************************************************************************
 ************ Provider Export functions ************************************* 
 *****************************************************************************/
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classnamestr
    )
{
    //if(xen_utils_class_is_subclass_of(_BROKER, classname, classnamestr))
    return key_property;
}

static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classnamestr
    )
{
    //if(xen_utils_class_is_subclass_of(_BROKER, classname, classnamestr))
    return keys;
}
/******************************************************************************
 * Function to enumerate a xen resource
 *
 * @param session - handle to a xen_utils_session object
 * @param resources - pointer to the provider_resource_list
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_list_enum(
       xen_utils_session *session, 
       provider_resource_list *resources
)
{
  enum domain_choice choice = vms_only;
  xen_domain_resources *domain_set = NULL;

  if(!xen_utils_get_domain_resources(session, &domain_set, choice))
    return CMPI_RC_ERR_FAILED;

  xen_vm resource_handle;
  xen_vm_record *resource_rec = NULL;

  kvp_set *complete_set;
  
  initialise_kvp_set(&complete_set);

  for(domain_set->currentdomain=0; domain_set->currentdomain < domain_set->numdomains; domain_set->currentdomain++){

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Current Domain: %d", domain_set->currentdomain));

    resource_handle = domain_set->domains->contents[domain_set->currentdomain];
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Current resource handle = %s", resource_handle));
    
    if(!xen_vm_get_record(session->xen, &resource_rec, resource_handle)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
		   ("--- xen_vm_get_record failed: \"%s\" \"%s\"",
		    session->xen->error_description[0], resource_handle));
      char *error = xen_utils_get_xen_error(session->xen);
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("%s", error));
      RESET_XEN_ERROR(session->xen);
      if (error)
	free(error);
      continue;
    }

    char *res = xen_utils_get_from_string_string_map(resource_rec->other_config, "kvp_enabled");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("result = %s", res));
    if(res) {
      /* Result is not NULL - therefore the VM is counted as being 'enabled' for KVP */
      
      xen_host_record_opt *host = resource_rec->resident_on;

      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VM is resident on host '%s'",host->u.handle));

      if (host->u.handle){

	char *address = NULL;
	
	if(!xen_host_get_address(session->xen, &address, host->u.handle))
	{
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Host address not found"));
	} else {
	  /* Make remote call for store */
	  char *plugin = "services/plugin/xscim";
	  char *xenref = (char *)((session->xen)->session_id);
	  /* Add overhead of 15 characters */

	  int len = sizeof(char) * (25 + strlen(plugin) + strlen(address) + strlen(resource_rec->uuid) + strlen(xenref));

	  char *url = (char *)malloc(len);

	  sprintf(url, "http://%s/%s/vm/%s?session_id=%s", address, plugin, resource_rec->uuid, xenref);
	  
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Fetch store from URL '%s'", url));
	  
	  kvp_set *set;
	  Xen_KVP_RC rc = xen_utils_get_kvp_store(url, (char *)resource_rec->uuid,&set);
	  if (rc != Xen_KVP_RC_OK) {
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Unable to retrieve KVP store for VM %s",
						   resource_rec->uuid));
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Continuing on to the next domain"));
	  } else {
          _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("sets so far %d", set->size));

	  /* Append the contents of the returned set */
	  xen_utils_append_kvp_set(complete_set, set);

	  
	  if (set)
	    xen_utils_free_kvpset(set);	  
	  	  
	  if(url)
	    free(url);

	  /* Free the host address */
	  if (address)
	    free(address);

	  }
	
	}
      } else {
	/* VM is not started, and so may not be resident on any host. */
      }
      
    }
    
    xen_vm_record_free(resource_rec);
  }
  xen_utils_free_domain_resources(domain_set);

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("all in %d keys", complete_set->size));
    /* Return the set of KVPs */
    resources->ctx = complete_set;

    return CMPI_RC_OK;
}
/******************************************************************************
 * Function to cleanup provider specific resource, this function is
 * called at various places in Xen_ProviderGeneric.c
 *
 * @param resources - handle to the provider_resource_list to be
 *    be cleaned up. Clean up the provider specific part of the
 *    resource.
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_list_cleanup(
       provider_resource_list *resources
)
{
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Entering xen_resource_list_cleanup"));
      kvp_set *set = resources->ctx;
      xen_utils_free_kvpset(set);
 
      return CMPI_RC_OK;
}
/*****************************************************************************
 * Function to get the next xen resource in the resource list
 *
 * @param resources_list - handle to the provide_resource_list object
 * @param session - handle to the xen_utils_session object
 * @param prov_res - handle to the next provider_resource to be filled in.
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_record_getnext(
       provider_resource_list *resources_list,  /* in */
       xen_utils_session *session,              /* in */
       provider_resource *prov_res              /* in , out */
)
{

    kvp *kvp_cpy;
  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("getnext"));
    kvp_set *kvp_set = resources_list->ctx;
    if (kvp_set == NULL || resources_list->current_resource == kvp_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    kvp *kvp = &kvp_set->contents[resources_list->current_resource];

    /* Copying KVP in order to allow the ProxyProvider to free this
       individual record, and later the complete list */
    xen_utils_kvp_copy(kvp, &kvp_cpy);
    prov_res->ctx = kvp_cpy;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("contents %s", kvp_cpy->key));

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

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("xen_resource_record_cleanup"));
    kvp *kvp_obj = (kvp *)prov_res->ctx;
    if(kvp_obj) {
      xen_utils_free_kvp(kvp_obj);
     }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("xen_resource_record_cleanup complete"));

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
    char vm_uuid[MAX_INSTANCEID_LEN];
    char key[MAX_KVP_KEY_LEN];
    char *value=NULL;
    CMPIrc cim_rc = CMPI_RC_ERR_FAILED;
    kvp *kvp=NULL;

    /* Parse the DeviceID */
    _CMPIStrncpyDeviceNameFromID(key, res_uuid, sizeof(key));
    _CMPIStrncpySystemNameFromID(vm_uuid, res_uuid, sizeof(vm_uuid));

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("xen_resource_get_from_id (%s=%s)", key, vm_uuid));

    Xen_KVP_RC rc = xen_utils_get_from_kvp_store(session, vm_uuid, key, &value);

    if (rc != Xen_KVP_RC_OK) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Failure to retrieve key %s for vm %s", key, vm_uuid));
      goto exit;
    }

    if(!xen_utils_create_kvp(key, value, vm_uuid, &kvp)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Failed to create KVP key"));
        goto exit;
    }

    prov_res->ctx = kvp;

    cim_rc = CMPI_RC_OK;

exit:
    /* Free the KVP value string */
    if (value)
        free(value);

    return cim_rc;
}

#ifdef FULL_INSTANCE_PROVIDER 
// Use this only  if the CIM object CreateInstance, ModifyINstance
// and DEleteInstance
/*****************************************************************************
 * This function is called from add in the Xen_ProviderGeneric.c
 * The code specific to a provider may be implemented below.
 *
 * @param broker - CMPI Factory broker
 * @param session - xen session handle
 * @param res - resource to be added
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_add(
    const CMPIBroker *broker,
    xen_utils_session *session,
    void *res,
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
static CMPIrc xen_delete_resource(
    const CMPIBroker *broker,
    xen_utils_session *session, 
    char *inst_id
)
{
    return CMPI_RC_ERR_NOT_SUPPORTED;
    //If the following part is not required you may delete it. This is just a template
    //xen_KVP KVP_handle;
    //if(!xen_KVP_get_by_uuid(session->xen, &KVP_handle,inst_id)) {
    //     xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    //     return CMPI_RC_ERR_FAILED;
    //}
     
    //if(!xen_KVP_destroy(session->xen, KVP_handle)) {
    //    xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    //    return CMPI_RC_ERR_FAILED;
    //}
    //xen_KVP_free(KVP_handle);
    //
    //return CMPI_RC_OK;
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
static CMPIrc xen_modify_resource(
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

    //If the following part is not required you may delete it. This is just a template

    //provider_resource *prov_resource = (provider_resource *)modified_res;
    //CMPIInstance *modified_inst;
    //CMPIData data;
    //char uuid[MAX_SYSTEM_NAME_LEN];
    //xen_KVP_record *target_KVP_rec;
    //char *tmp_str = NULL;
    //
    //if (prov_resource == NULL ||
    //    prov_resource->is_KVP_record)
    //return CMPI_RC_ERR_FAILED;
    //
    //modified_inst = prov_resource->u.cmpi_inst;
    /* Extract the device uuid from InstanceID property. */
    //if (!_CMPIStrncpyDeviceNameFromID(uuid, inst_id, MAX_SYSTEM_NAME_LEN))
    //    return CMPI_RC_ERR_FAILED;
    //if (!xen_KVP_get_record(session->xen, &target_KVP_rec, (xen_KVP)uuid)) {
    //    xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    //    goto Error;
    //}
    //Error:
    //if(tmp_str)
    //    free(tmp_str);
    //if(target_KVP_rec)
    //    xen_KVP_record_free(target_KVP_rec);
    //return CMPI_RC_OK;
       
}
/************************************************************************
 * Function to extract resources
 *
 * @param res - provider specific resource to get values from
 * @param inst - CIM object whose properties are being set
 * @param properties - list of properties to be used while modifying
 * @return CMPIrc return values
*************************************************************************/
static CMPIrc xen_extract_resource(
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

    /* Template for implementation when required */
    //  provider_resource *prov_resource;
    //
    //  (void)properties;
    /* Get memory for resource. */
    //  prov_resource = (provider_resource *)calloc(1, sizeof(provider_resource));
    //  if (prov_resource == NULL)
    //     return CMPI_RC_ERR_FAILED;
    //
    //  prov_resource->u.cmpi_inst = (CMPIInstance *)inst;
    //  *res = (void *)prov_resource;
    //  return CMPI_RC_OK;
}
#endif //FULL_INSTANCE_PROVIDER
/************************************************************************
 * Function that sets the properties of a CIM object with values from the
 * provider specific resource.
 *
 * @param resource - provider specific resource to get values from
 * @param inst - CIM object whose properties are being set
 * @return CMPIrc return values
*************************************************************************/
static CMPIrc xen_resource_set_properties(provider_resource *resource, CMPIInstance *inst)
{
  kvp *ctx = (kvp *)resource->ctx;
  char buf[MAX_INSTANCEID_LEN];

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("xen_resource_set_properties"));

  if(!ctx || !ctx->vm_uuid || !ctx->key) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("xen_resource_set_properties failed because of invalid context"));
      return CMPI_RC_ERROR; 
  }

  _CMPICreateNewDeviceInstanceID(buf, sizeof(buf),ctx->vm_uuid, ctx->key);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AdditionalAvailability",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "Availability",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);


    //CMSetProperty(inst, "AvailableRequestedStates",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "Caption",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_KVP", CMPI_chars);
    //CMSetProperty(inst, "Description",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "DetailedStatus",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "Key", (CMPIValue *)ctx->key, CMPI_chars);
    CMSetProperty(inst, "Value", (CMPIValue *)ctx->value, CMPI_chars);
    CMSetProperty(inst, "Vm_uuid", (CMPIValue *)ctx->vm_uuid, CMPI_chars);
    //CMSetProperty(inst, "ElementName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "EnabledState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ErrorCleared",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "ErrorDescription",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "HealthState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, <time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "LastErrorCode",(CMPIValue *)&<value>, CMPI_uint32);
    //CMSetProperty(inst, "LocationIndicator",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "MaxQuiesceTime",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "Name",(CMPIValue *)ctx->key, CMPI_chars);
    //CMSetProperty(inst, "OperatingStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "PowerManagementSupported",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "PowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "PrimaryStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "RequestedState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Status",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "StatusInfo",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)ctx->vm_uuid, CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, <time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "TransitioningToState",(CMPIValue *)&<value>, CMPI_uint16);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Complete"));
    return CMPI_RC_OK;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_KVP)



/* CMPI Method provider function table setup */
//XenMethodMIStub(Xen_KVP)

