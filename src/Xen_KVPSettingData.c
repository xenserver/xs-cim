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
#include "Xen_KVPSettingData.h"
#include "Xen_KVP.h"
#include "provider_common.h"
#include "xen_utils.h"
#include "RASDs.h"


int kvp_rasd_to_kvp_rec(
    const CMPIBroker *broker,
    CMPIInstance *kvp_rasd,
    kvp *kvp_rec,
    CMPIStatus *status)
{

  CMPIStatus local_status = {CMPI_RC_OK, NULL};
  CMPIData propertyvalue;
  
  kvp_rec = malloc(sizeof(kvp));

  if(kvp_rec == NULL)
    {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
		   ("--- Cannot malloc memory for kvp record"));
      CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM, "Unable to malloc memory");
      goto Error;
    }

  propertyvalue = CMGetProperty(kvp_rasd, "key", &local_status);
  if((local_status.rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue))
    {
      kvp_rec->key = strdup(CMGetCharPtr(propertyvalue.value.string));
    }
  propertyvalue = CMGetProperty(kvp_rasd, "value", &local_status);
  if((local_status.rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue))
    {
      kvp_rec->value = strdup(CMGetCharPtr(propertyvalue.value.string));
    }
  propertyvalue = CMGetProperty(kvp_rasd, "VM_ID", &local_status);
  if((local_status.rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue))
    {
      kvp_rec->vm_uuid = strdup(CMGetCharPtr(propertyvalue.value.string));
    }

  return 1;
 Error:
  return 0;

}

int xen_kvp_rec_to_kvp_rasd(
    const CMPIBroker *broker,
    xen_utils_session *session,
    CMPIInstance *inst,
    kvp *kvp_rec
    )
{
    char buf[MAX_INSTANCEID_LEN];
    DMTF_ResourceType type = DMTF_ResourceType_Graphics_controller;
    DMTF_ConsumerVisibility consumerVisibility = DMTF_ConsumerVisibility_Virtualized;
    uint64_t consoles = 1;
    int alloctype = 1;
    char *vm_name = "No VM information available";
    char *vm_desc = "No VM description available";

    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, kvp_rec->vm_uuid, kvp_rec->key);
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);

    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen KVP Settings", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)vm_desc, CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)vm_name, CMPI_chars);
    CMSetProperty(inst, "ResourceType", (CMPIValue *)&type, CMPI_uint16);
    CMSetProperty(inst, "ConsumerVisibility", (CMPIValue *)&consumerVisibility, CMPI_uint16);
    CMSetProperty(inst, "Reservation", (CMPIValue *)&consoles, CMPI_uint64);
    CMSetProperty(inst, "Limit", (CMPIValue *)&consoles, CMPI_uint64);
    CMSetProperty(inst, "VirtualQuantity", (CMPIValue *)&consoles, CMPI_uint64);
    CMSetProperty(inst, "AllocationUnits", (CMPIValue *)"count", CMPI_chars);
    CMSetProperty(inst, "VirtualQuantityUnits",(CMPIValue *)"count", CMPI_chars);
    CMSetProperty(inst, "AutomaticAllocation", (CMPIValue *)&alloctype, CMPI_boolean);
    CMSetProperty(inst, "AutomaticDeallocation", (CMPIValue *)&alloctype, CMPI_boolean);
    //CMSetProperty(inst, "Protocol", (CMPIValue *)&(con_rec->protocol), CMPI_uint16);

    // Unused properties
    // 
    //CMSetProperty(inst, "Address",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "AddressOnParent",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ChangeableType",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ConfigurationName",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Connection",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "HostResource",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "MappingBehavior",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Parent",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PoolID",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Weight",(CMPIValue *)&<value>, CMPI_uint32);


  return 1;
}
			    

/******************************************************************************
* kvp_rasd_create_ref
*
* This function creates a CIMObjectPath to represent a reference to a
* KVP RASD object
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/

CMPIObjectPath *kvp_rasd_create_ref(
    const CMPIBroker *broker,
    const char *name_space,
    xen_utils_session *session,
    xen_vm_record *vm_rec,
    kvp *kvp_rec
    )
{
  char inst_id[MAX_INSTANCEID_LEN];
  CMPIObjectPath *result_setting = NULL;

  result_setting = CMNewObjectPath(broker, name_space, "Xen_KVPSettingData", NULL);
  if (result_setting) {
    _CMPICreateNewDeviceInstanceID(inst_id, MAX_INSTANCEID_LEN, kvp_rec->vm_uuid, kvp_rec->key);
    CMAddKey(result_setting, "InstanceID", (CMPIValue *)inst_id, CMPI_chars);
  }
  return result_setting;

}
