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
#include "provider_common.h"
#include "RASDs.h"
#include "Xen_Disk.h"
/******************************************************************************
* get_sr
*
* Get the SR pointed to by the sr-Info lable
* (could be a label or could be the uuid).
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/
static int _get_sr(
    const CMPIBroker *broker,
    xen_utils_session *session,
    char* sr_info,        /* in - GUID or name-label */
    bool sr_uuid,         /* info string is GUID and not name-label */
    xen_sr* sr,           /* out - handle to SR */
    CMPIStatus* status)   /* out - status of call */
{
    bool success = true;
    xen_sr_set* srs ;

    if (sr_uuid) {
        if (!xen_sr_get_by_uuid(session->xen, sr, sr_info))
            success = false;
    }
    else {
        if (!xen_sr_get_by_name_label(session->xen, &srs, sr_info))
            success = false;
        if (srs->size != 1) {
            char error_msg[XEN_UTILS_ERROR_BUF_LEN];
            sprintf(error_msg, "SR set for %s returned %d, expecting just 1\n", sr_info, srs->size);
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, (error_msg));
            xen_sr_set_free(srs);
            success = false;
        }
        else {
            *sr = srs->contents[0];
            srs->contents[0] = NULL;
            xen_sr_set_free(srs);
        }
    }
    return success;
}
/******************************************************************************
* disk_rasd_to_vbd
*
* This function attempts to parse a Xen_Disk CIM instance and populate a new
* VBD record. It also gets the record to the VDI that the VBD attaches to (could
* be a newly created VDI, if requested) and a handle to an existing SR as
* pointed to by the PoolID field in the CIM instance
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/
int disk_rasd_to_vbd(
    const CMPIBroker *broker,
    xen_utils_session* session,
    CMPIInstance *disk_rasd,
    xen_vbd_record **vbd_rec,
    xen_vdi_record **vdi_rec,
    xen_sr  *sr,
    CMPIStatus *status)
{
    CMPIData propertyvalue;
    char *error_msg = "ERROR: Unknown error";
    char *sr_label = NULL, *vdi_name_label = NULL, *vdi_name_desc = NULL;
    char *vbd_device = NULL, *vbd_uuid = NULL, *vdi_uuid = NULL;
    enum xen_vbd_type vbd_type = XEN_VBD_TYPE_DISK; /* default with Disk (as opposed to CDRom) */
    enum xen_vdi_type vdi_type = XEN_VDI_TYPE_USER;
    bool vbd_readonly = false, vbd_bootable = false;/* defaults for Disk type */
    int vbd_mode= XEN_VBD_MODE_RW;                  /* defaults for Disk type */
    int64_t disk_size = -1;                         /* for CDRoms - this is the size expected */
    char buf[MAX_INSTANCEID_LEN];

    *vbd_rec  = NULL;
    *vdi_rec  = NULL;

    /* only resource types of 15,16 and 19 are currently supported */
    int rc = CMPI_RC_ERR_INVALID_PARAMETER;
    propertyvalue = CMGetProperty(disk_rasd, "ResourceType", status);
    if ((status->rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue)) {
        error_msg = "ERROR: Xen_DiskSettingData has no ResourceType";
        goto Error;
    }
    int res_type = propertyvalue.value.uint16;
    if ((res_type == DMTF_ResourceType_CD_Drive) || (res_type == DMTF_ResourceType_DVD_drive)) {
        vbd_mode = XEN_VBD_MODE_RO;
        vbd_type = XEN_VBD_TYPE_CD;
    }
    else if ((res_type == DMTF_ResourceType_Storage_Extent) || (res_type == DMTF_ResourceType_Disk_Drive))
        vbd_type = XEN_VBD_TYPE_DISK;
    else {
        error_msg = "ERROR: Xen_DiskSettingData has unsupported ResourceType";
        goto Error;
    }

    /* Get the instance ID which has the device's UUID in it, if its available
      This will be usd during deletes and not used during create*/
    propertyvalue = CMGetProperty(disk_rasd, "InstanceID", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(propertyvalue.value.string),
            sizeof(buf)/sizeof(buf[0]));
        vbd_uuid = strdup(buf);
    }

    /* The HostResource property is used to identify the VDI, if available */
    propertyvalue = CMGetProperty(disk_rasd, "HostResource", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        /* HostResource is always in WBEM URI format and it points to a 
          Xen_DiskImage object reference. Convert it to CMPIObjectPath */
        CMPIData data = CMGetArrayElementAt(propertyvalue.value.array, 0, NULL);
        CMPIObjectPath *obj_path = xen_utils_WBEM_URI_to_CMPIObjectPath(
                                       broker, 
                                       CMGetCharPtr(data.value.string));
        if (obj_path) {
            /* this should be a Xen_DiskImage object reference */
            /* Get the DeviceID key */
            propertyvalue = CMGetKey(obj_path, "DeviceID", status);
            if ((status->rc == CMPI_RC_OK) && 
                !CMIsNullValue(propertyvalue) && 
                propertyvalue.type == CMPI_string) {
                memset(buf, 0, sizeof(buf));
                _CMPIStrncpyDeviceNameFromID(buf, CMGetCharPtr(propertyvalue.value.string),
                                             sizeof(buf)/sizeof(buf[0]));
                vdi_uuid = strdup(buf);
            }
        }
    }
    if (!vdi_uuid) {
        /* second chance, Try the HostExtentName */
        propertyvalue = CMGetProperty(disk_rasd, "HostExtentName", status);
        if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
            vdi_uuid = strdup(CMGetCharPtr(propertyvalue.value.string));
    }
    /* Device is specified as the address on the host bus */
    propertyvalue = CMGetProperty(disk_rasd, "AddressOnParent", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
        vbd_device = strdup(CMGetCharPtr(propertyvalue.value.string));

    propertyvalue = CMGetProperty(disk_rasd, "Bootable", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
        vbd_bootable = propertyvalue.value.boolean;

    propertyvalue = CMGetProperty(disk_rasd, "Access", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        if (propertyvalue.value.uint16 == Access_Readable)
            vbd_mode = XEN_VBD_MODE_RO;
    }

    /*
    * The Pool ID property is used to identify the Xen SR
    * This could be NULL if VDIs are reused or if
    * the VBD has to be created on the default SR.
    */
    propertyvalue = CMGetProperty(disk_rasd, "PoolID", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        char *poolid = CMGetCharPtr(propertyvalue.value.string);
        if (poolid && (*poolid != '\0'))
            sr_label = strdup(poolid);
    }

    propertyvalue = CMGetProperty(disk_rasd, "ElementName", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
        vdi_name_label = strdup(CMGetCharPtr(propertyvalue.value.string));

    propertyvalue = CMGetProperty(disk_rasd, "Description", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
        vdi_name_desc = strdup(CMGetCharPtr(propertyvalue.value.string));

    /* Get the disk size from the CIM instance -
    * If this is a new disk, this property is required
    * If we are instantiating a CDRom VDI, this is not required */
    int64_t multiplier = 1; /* default to bytes */
    propertyvalue = CMGetProperty(disk_rasd, "AllocationUnits", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        char *units = CMGetCharPtr(propertyvalue.value.string);
        if ((multiplier = xen_utils_get_alloc_units(units)) == 0) {
            error_msg = "ERROR: Xen_DiskSettingData has unsupported AllocationUnits";
            goto Error;
        }
    }

    /* Limit and VirtualQuantity properties mean the same when we create the VBD */
    propertyvalue = CMGetProperty(disk_rasd, "VirtualQuantity", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
        disk_size = (propertyvalue.value.uint64) * multiplier;

    if (vbd_type == XEN_VBD_TYPE_CD)
        vdi_type = XEN_VDI_TYPE_USER;

    /* We have enough information... create the Xen device records so we can */
    /* create new devices or identify existing ones */
    /* Create the VDI device record */
    rc = CMPI_RC_ERR_FAILED;
    *vdi_rec = xen_vdi_record_alloc();
    if (*vdi_rec == NULL) {
        error_msg = "ERROR: Unable to malloc memory";
        goto Error;
    }

    (*vdi_rec)->virtual_size  = disk_size;
#if OSS_XENAPI
    (*vdi_rec)->other_config  = xen_string_string_map_alloc(0);
    vdi_location = "";
    (*vdi_rec)->other_config->contents[0].key= strdup("location");
    (*vdi_rec)->other_config->contents[0].val = strdup(vdi_location);
    (*vdi_rec)->other_config->size = 1;
#else
    (*vdi_rec)->other_config  = xen_string_string_map_alloc(0);
    (*vdi_rec)->other_config->size = 0;
#endif
    (*vdi_rec)->type          = vdi_type;
    (*vdi_rec)->read_only     = vbd_readonly;
    (*vdi_rec)->name_label    = vdi_name_label;
    (*vdi_rec)->name_description = vdi_name_desc;

#if XENAPI_VERSION > 400
    (*vdi_rec)->managed = true;
#endif

    /* If VDI has already been created, use it */
    if (vdi_uuid) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VDI specified in the RASD: -%s-", vdi_uuid));
        (*vdi_rec)->uuid = vdi_uuid;
    }

    /* create the VBD record */
    *vbd_rec = xen_vbd_record_alloc();
    if (*vbd_rec == NULL) {
        error_msg = "ERROR: Unable to malloc memory";
        goto Error;
    }

    if (vbd_uuid)
        (*vbd_rec)->uuid = vbd_uuid;

#if XENAPI_VERSION > 400
    if (vbd_device)
        (*vbd_rec)->userdevice = vbd_device;
    (*vbd_rec)->other_config = xen_string_string_map_alloc(0);
#endif
    (*vbd_rec)->bootable = vbd_bootable;
    (*vbd_rec)->mode = vbd_mode;
    (*vbd_rec)->type = vbd_type;
    (*vbd_rec)->qos_algorithm_params = xen_string_string_map_alloc(0);

    if (vbd_type == XEN_VBD_TYPE_CD)
        (*vdi_rec)->sharable = true;
    else
        (*vdi_rec)->sharable = false;

    /* Identify the Storage Repository where this VDI will be created (if its being created) */
    if (sr_label) {
        if (!_get_sr(broker, session, sr_label, true, sr, status)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                ("--- getting SR for %s failed", sr_label));
            goto Error;
        }
        free(sr_label);
    }
    return 1;

    Error:
    if (sr_label)
        free(sr_label);
    if (vdi_name_label) {
        free(vdi_name_label);
        if (*vdi_rec)
            (*vdi_rec)->name_label = NULL;
    }
    if(vdi_name_desc) {
        free(vdi_name_desc);
    }
    if(vbd_device) {
        free(vbd_device);
    }
    if(vbd_uuid) {
        free(vbd_uuid);
    }

    /* frees fields as well */
    if (*vbd_rec) {
        xen_vbd_record_free(*vbd_rec);
        *vbd_rec = NULL;
    }
    if (*vdi_rec) {
        xen_vdi_record_free(*vdi_rec);
        *vdi_rec = NULL;
    }
    xen_utils_set_status(broker, status, rc, error_msg, session->xen);
    return 0;
}
/****************************************************************************
* disk_rasd_from_vbd
*
* This function builds a new Xen_Disk CIM instance based on the information
* in the VBD record
*
* Returns 1 on Success and 0 on failure.
*****************************************************************************/
int disk_rasd_from_vbd(
    const CMPIBroker* broker,
    xen_utils_session *session,
    CMPIInstance *inst,
    xen_vm_record *vm_rec,
    xen_vbd_record *vbd_rec,
    xen_vdi_record *vdi_rec
    )
{
    char buf[MAX_INSTANCEID_LEN];
    xen_sr_record *sr_rec = NULL;
    unsigned long long physical_utilization = 1, limit = 1, virtual_quantity = 1;
    DMTF_ResourceType rasd_type = DMTF_ResourceType_Storage_Extent;
    uint64_t block_size = 1;
    bool autoalloc = false;
    DMTF_MappingBehavior behaviour = DMTF_MappingBehavior_Dedicated; 
    char *alloc_unit = "byte";
    Access access = Access_Unknown;

    RESET_XEN_ERROR(session->xen);
    if (vm_rec == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Orphaned VBD"));
        return 0;
    }

    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid, vbd_rec->uuid);
    if (vdi_rec) {
        if (vdi_rec->sharable)
            behaviour = DMTF_MappingBehavior_Hard_Affinity;
        physical_utilization = vdi_rec->physical_utilisation;
        limit = vdi_rec->virtual_size;
        virtual_quantity = vdi_rec->virtual_size;
        if (vdi_rec->sr->is_record)
            sr_rec = vdi_rec->sr->u.handle;
        else
            xen_sr_get_record(session->xen, &sr_rec, vdi_rec->sr->u.handle);
    }

    if (vbd_rec->mode == XEN_VBD_MODE_RW)
        access = Access_Read_Write_Supported;
    else
        access = Access_Readable;
    DMTF_ConsumerVisibility virtualized = DMTF_ConsumerVisibility_Virtualized;
    if (vbd_rec->type == XEN_VBD_TYPE_CD) {
        rasd_type = DMTF_ResourceType_DVD_drive;
        alloc_unit = "count";
        virtual_quantity = 1;
    }

    /* Set the CMPIInstance properties from the resource data. */
    /* Key Proerpty first */
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    /* Rest of the keys follow */
    CMSetProperty(inst, "Caption",(CMPIValue *)"Extended Settings for a Xen Virtual Disk", CMPI_chars);
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&rasd_type, CMPI_uint16);
    CMSetProperty(inst, "ResourceSubType",(CMPIValue *)"DMTF:xen:vbd", CMPI_chars); /* required by DMTF */
    CMSetProperty(inst, "Access",(CMPIValue *)&access, CMPI_uint16);
    CMSetProperty(inst, "AddressOnParent",(CMPIValue *)vbd_rec->userdevice, CMPI_chars);
    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)alloc_unit, CMPI_chars);
    CMSetProperty(inst, "AutomaticAllocation",(CMPIValue *)&autoalloc, CMPI_boolean);
    CMSetProperty(inst, "AutomaticDeallocation", (CMPIValue *)&autoalloc, CMPI_boolean);
    CMSetProperty(inst, "ConsumerVisibility", (CMPIValue *)&virtualized, CMPI_uint16);
    CMSetProperty(inst, "VirtualQuantity",(CMPIValue *)&virtual_quantity, CMPI_uint64);
    CMSetProperty(inst, "VirtualQuantityUnits",(CMPIValue *)alloc_unit, CMPI_chars);
    CMSetProperty(inst, "Reservation",(CMPIValue *)&limit, CMPI_uint64);
    CMSetProperty(inst, "Limit",(CMPIValue *)&limit, CMPI_uint64);
    CMSetProperty(inst, "VirtualResourceBlockSize",(CMPIValue *)&block_size, CMPI_uint64);
    CMSetProperty(inst, "HostResourceBlockSize",(CMPIValue *)&block_size, CMPI_uint64);

    /* Setup the Configuration properties */
    if (vdi_rec) {
        CMSetProperty(inst, "ElementName",(CMPIValue *)vdi_rec->name_label, CMPI_chars);
        CMSetProperty(inst, "Description",(CMPIValue *)vdi_rec->name_description, CMPI_chars);
        /* HostResource requires string in WBEM URI Format */
        CMPIObjectPath *disk_ref = disk_image_create_ref(broker, DEFAULT_NS, session, sr_rec->uuid, vdi_rec->uuid);
        if (disk_ref) {
            /* conver objectpath to string form */
            char *disks_uri = xen_utils_CMPIObjectPath_to_WBEM_URI(broker, disk_ref);
            if (disks_uri) {
                CMPIArray *arr = CMNewArray(broker, 1, CMPI_chars, NULL);
                CMSetArrayElementAt(arr, 0, (CMPIValue *) disks_uri, CMPI_chars);
                CMSetProperty(inst, "HostResource", &arr, CMPI_charsA);
            }
        }
        CMSetProperty(inst, "HostExtentName",(CMPIValue *)vdi_rec->uuid, CMPI_chars);
        NameFormat name_format = NameFormat_Other;
        CMSetProperty(inst, "HostExtentNameFormat",(CMPIValue *)&name_format, CMPI_uint16);
        CMSetProperty(inst, "OtherHostExtentNameFormat",(CMPIValue *)"Xen VDI UUID", CMPI_chars);
        NameNamespace name_namespace = NameNamespace_Other;
        CMSetProperty(inst, "HostExtentNameNamespace",(CMPIValue *)&name_namespace, CMPI_uint16);
        CMSetProperty(inst, "OtherHostExtentNameNamespace",(CMPIValue *)"Xen UUID", CMPI_chars);
    }
    CMSetProperty(inst, "MappingBehavior",(CMPIValue *)&behaviour, CMPI_uint16);
    if (sr_rec)
        CMSetProperty(inst, "PoolID", sr_rec->uuid, CMPI_chars);

    // Derived class properties
    CMSetProperty(inst, "Bootable",(CMPIValue *)&vbd_rec->bootable, CMPI_boolean);

    // Others not used
    //CMSetProperty(inst, "Address",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *) buf, CMPI_chars);
    //CMSetProperty(inst, "Connection", &arr, CMPI_charsA);
    //CMSetProperty(inst, "ChangeableType",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ConfigurationName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "OtherResourceType", (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "HostExtentStartingAddress",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "MaxConsumableResource",(CMPIValue *)<value>, CMPI_uint64);
    //CMSetProperty(inst, "CurrentlyConsumedResource",(CMPIValue *)<value>, CMPI_uint64);
    //CMSetProperty(inst, "ConsumedResourceUnit",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Parent",(CMPIValue *)<value>, CMPI_chars); /* This is going to be useful for diff disks WBEM URI */ 
    //CMSetProperty(inst, "Weight",(CMPIValue *)&<value>, CMPI_uint32);

    /* Free time */
    if (sr_rec && vdi_rec && !vdi_rec->sr->is_record)
        xen_sr_record_free(sr_rec);

    return CMPI_RC_OK;
}

/******************************************************************************
* disk_rasd_modify
*
* This function modifyies an existing disk identified by the RASD InstanceID
* with the settings passed in as part of the RASD
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/
int disk_rasd_modify(
    xen_utils_session *session,
    xen_vbd_record *vbd_rec_template,
    xen_vdi_record *vdi_rec_template
    )
{
    xen_vdi current_vdi = NULL;
    xen_vbd vbd = NULL;
    xen_vdi vdi = NULL;
    xen_vbd_record *current_vbd_rec = NULL;
    xen_vdi_record *current_vdi_rec = NULL;
    bool is_empty = false;

    if (!vbd_rec_template || !vdi_rec_template) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- No VBD or VDI template has been specified"));
        return 0;
    }
    if (xen_vbd_get_by_uuid(session->xen, &vbd, vbd_rec_template->uuid)) {
        if (vdi_rec_template->uuid && 
            !xen_vdi_get_by_uuid(session->xen, &vdi, vdi_rec_template->uuid)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- VDI specified -%s-", vdi_rec_template->uuid));
            if (*(vdi_rec_template->uuid) == '\0') {
                //This is okay, assume we are being asked to eject existing VDI
                RESET_XEN_ERROR(session->xen)
            }
        }
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Couldnt find VBD %s", vbd_rec_template->uuid));
        return 0;
    }
    xen_vbd_get_record(session->xen, &current_vbd_rec, vbd);

    /* Find if the current vdi is empty */
    xen_vbd_get_empty(session->xen, &is_empty, vbd);
    if (is_empty || xen_vbd_get_vdi(session->xen, &current_vdi, vbd)) {
        /* Get the current vdi record */
        if (!is_empty)
            xen_vdi_get_record(session->xen, &current_vdi_rec, current_vdi);
        /* change VDI to what's been specified if its different from what's currently set*/
        if (is_empty || (current_vdi_rec != NULL)) {
            /* We may be asked to change the underlying VDI */
            if (vdi_rec_template->uuid) {
                /* An empty string for the VDI UUID implies we are being asked to unplug the existing vdi */
                if (is_empty || strcmp(current_vdi_rec->uuid, vdi_rec_template->uuid) != 0) {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Changing VDI from %s to %s", 
                        (is_empty ? "empty" : current_vdi_rec->uuid), vdi_rec_template->uuid));
                    /* Current VDI doesnt match what's passed in */
                    /* Eject the current one */
                    if (!is_empty) {
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Ejecting existing VDI"));
                        xen_vbd_eject(session->xen, vbd);
                    }

                    /* empty vdi string implies just an eject of existing VDI */
                    if (*(vdi_rec_template->uuid) != '\0') {
                        /* insert the new one */
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Inserting new VDI %s", vdi_rec_template->uuid));
                        xen_vbd_insert(session->xen, vbd, vdi);
                    }
                }
            }
        }
    }

    /* Other changes mentioned here */
#if XENAPI_VERSION > 400
    if (current_vbd_rec) {
        if (vbd_rec_template->userdevice && 
            (strcmp(current_vbd_rec->userdevice, vbd_rec_template->userdevice) != 0))
            xen_vbd_set_userdevice(session->xen, vbd, vbd_rec_template->userdevice);

        //if(vbd_rec_template->bootable)
        //    xen_vbd_set_bootable(xen_session *session, xen_vbd vbd, bool bootable);
        //if(vbd_rec_template->mode)
        //    xen_vbd_set_mode(xen_session *session, xen_vbd vbd, enum xen_vbd_mode mode);
        //if(vbd_rec_template->type)
        //    xen_vbd_set_type(session, xen_vbd vbd, enum xen_vbd_type type);
        //if(vbd_rec_template->unpluggable)
        //    xen_vbd_set_unpluggable(xen_session *session, xen_vbd vbd, bool unpluggable);
        //if(vbd_rec_template->other_config)
        //    xen_vbd_add_to_other_config(xen_session *session, xen_vbd vbd, char *key, char *value);
        //f(vbd_rec_template->qos_algorithm_type)
        //  xen_vbd_set_qos_algorithm_type(session, vbd, vbd_rec_template->qos_algorithm_type);
        //if(vbd_rec_template->qos_algorithm_params)
        //    xen_vbd_set_qos_algorithm_params(xen_session *session, xen_vbd vbd, xen_string_string_map *algorithm_params);
        //if(vbd_rec_template->qos_algorithm_params)
        //    xen_vbd_add_to_qos_algorithm_params(session, vbd, key, value);
    }
#endif

    xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    if (current_vdi_rec && vdi_rec_template && 
        current_vbd_rec->type != XEN_VBD_TYPE_CD) {
        if (vdi_rec_template->name_label && 
            (strcmp(current_vdi_rec->name_label, vdi_rec_template->name_label) != 0))
            xen_vdi_set_name_label(session->xen, vdi, vdi_rec_template->name_label);
        if (vdi_rec_template->name_description && 
            strcmp(current_vdi_rec->name_description, vdi_rec_template->name_description) != 0)
            xen_vdi_set_name_description(session->xen, vdi, vdi_rec_template->name_description);
        //if(vdi_rec_template->sharable)
        //    xen_vdi_set_sharable(session, vdi, vdi_rec_template->sharable);
        //if(vdi_rec_template->other_config)
        //    xen_vdi_set_other_config(session, vdi, vdi_rec_template->other_config);
        //if(vdi_rec_template->other_config)
        //    xen_vdi_add_to_other_config(session, vdi, key, value);
        //if(vdi_rec_template->other_config)
        //    xen_vdi_remove_from_other_config(session, vdi, key);
        //if(vdi_rec_template->xenstore_data)
        //    xen_vdi_set_xenstore_data(session, vdi, xenstore_data);
        //if(vdi_rec_template->xenstore_data)
        //    xen_vdi_add_to_xenstore_data(session, vdi, key, value);
        //if(vdi_rec_template->xenstore_data)
        //    xen_vdi_remove_from_xenstore_data(session, vdi, key);
        //if(vdi_rec_template->sm_config)
        //    xen_vdi_set_sm_config(session, vdi, sm_config);
        //if(vdi_rec_template->sm_config)
        //    xen_vdi_add_to_sm_config(session, vdi, key, value);
        //if(vdi_rec_template->sm_config)
        //    xen_vdi_remove_from_sm_config(session, vdi, key);
        //if(vdi_rec_template->managed)
        //    xen_vdi_set_managed(session, vdi, value);
        //if(vdi_rec_template->read_only)
        //    xen_vdi_set_read_only(session, vdi, value);
        //if(vdi_rec_template->read_only)
        //    xen_vdi_set_missing(session, vdi, value);
        // 
        if ((vdi_rec_template->virtual_size > 0) && 
            (current_vdi_rec->virtual_size != vdi_rec_template->virtual_size)) {
          xen_vdi_resize(session->xen, vdi, vdi_rec_template->virtual_size);
	}
    }
    if (vdi)
        xen_vdi_free(vdi);
    if (vbd)
        xen_vbd_free(vbd);
    if (current_vdi)
        xen_vdi_free(current_vdi);
    if(current_vdi_rec)
        xen_vdi_record_free(current_vdi_rec);
    if(current_vbd_rec)
        xen_vbd_record_free(current_vbd_rec);

    return session->xen->ok;
}
/******************************************************************************
* disk_rasd_create_ref
*
* This function creates a CIMObjectPath to represent a reference to a
* Disk RASD object
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/
CMPIObjectPath *disk_rasd_create_ref(
    const CMPIBroker *broker,
    const char *name_space,
    xen_utils_session *session,
    xen_vm_record *vm_rec,
    xen_vbd vbd
    )
{
    char inst_id[MAX_INSTANCEID_LEN];
    char *vbd_uuid = NULL;
    CMPIObjectPath *result_setting = NULL;

    /* find the VBD's uuid */
    if (xen_vbd_get_uuid(session->xen, &vbd_uuid, vbd)) {
        /* create a CIM reference object and set the key properties */
        result_setting = CMNewObjectPath(broker, name_space, "Xen_DiskSettingData", NULL);
        if (result_setting) {
            _CMPICreateNewDeviceInstanceID(inst_id, MAX_INSTANCEID_LEN, vm_rec->uuid, vbd_uuid);
            CMAddKey(result_setting, "InstanceID", (CMPIValue *)inst_id, CMPI_chars);
        }
        free(vbd_uuid);
    }
    return result_setting;
}
