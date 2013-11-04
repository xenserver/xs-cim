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
#include "xen_utils.h"
#include "Xen_VirtualSystemSettingData.h"
#include "RASDs.h"

#include "cmpilify.h"
#include "cmpitrace.h"

#include "xen_utils.h"
#include "cmpitrace.h"
#include <cmpidt.h>
#include <cmpimacs.h>
#include <string.h>
#include <memory.h>
#include "provider_common.h"

#define XAPI_NULL_REF "OpaqueRef:NULL"

/*********************************************************
 * These are the Xen specific property names - 
 * keep in sync with the MOF
*********************************************************/
#define ACTIONS_AFTER_REBOOT    "AutomaticStartupAction"
#define ACTIONS_AFTER_SHUTDOWN  "AutomaticShutdownAction"
#define ACTIONS_AFTER_CRASH     "AutomaticRecoveryAction"
#define CREATION_TIME           "CreationTime"
#define PLATFORM                "Platform"
#define HVM_BOOT_POLICY         "HVM_Boot_Policy"
#define HVM_BOOT_PARAMS         "HVM_Boot_Params"
#define HVM_SHADOW_MULTIPLIER   "HVM_ShadowMultiplier"
#define PV_KERNEL               "PV_Kernel"
#define OS_VERSION              "OS_Version"
#define XEN_TOOLS_VERSION       "PV_Drivers_Version"
#define XEN_TOOLS_UPTODATE      "PV_Drivers_UpToDate"
#define XEN_TOOLS_RUNNING       "Xen_Tools_AreRunning"
#define PV_RAMDISK              "PV_RAMDisk"
#define PV_ARGS                 "PV_Args"
#define PV_LEGACY_ARGS          "PV_Legacy_Args"
#define PV_BOOTLOADER           "PV_Bootloader"
#define PV_BOOTLOADER_ARGS      "PV_Bootloader_Args"
#define OTHER_CONFIG            "Other_Config"
#define XENSTORE_DATA           "Xenstore_Data"
#define STARTTIME               "StartTime"
#define HOST                    "Host"
#define HOST_AFFINITY           "HostAffinity"
#define AVAILABLE_VBD_DEVICES   "AvailableBlockDeviceSlots"
#define AVAILABLE_VIF_DEVICES   "AvailableNetworkInterfaceSlots"
#define PARENT                  "Parent"
#define CHILDREN                "Children"
#define SNAPSHOT_OF             "SnapshotOf"
#define SNAPSHOT_TIME           "SnapshotTime"

/* forward decls */
CMPIObjectPath* vm_create_ref(
    const CMPIBroker *broker, 
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec);
CMPIObjectPath* snapshot_create_ref(
    const CMPIBroker *broker, 
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec);
CMPIObjectPath* template_create_ref(
    const CMPIBroker *broker, 
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec);

/* map DMTF actions to xen actions */
int actions_after_reboot_map[3][2] = {
    { Xen_VirtualSystemSettingData_AutomaticStartupAction_None,             XEN_ON_NORMAL_EXIT_UNDEFINED},
    { Xen_VirtualSystemSettingData_AutomaticStartupAction_Restart_if_previously_active, XEN_ON_NORMAL_EXIT_RESTART},
    { Xen_VirtualSystemSettingData_AutomaticStartupAction_Always_startup,   XEN_ON_NORMAL_EXIT_RESTART}
};

int actions_after_shutdown_map[3][2] = {
    { Xen_VirtualSystemSettingData_AutomaticShutdownAction_Turn_Off,    XEN_ON_NORMAL_EXIT_DESTROY},
    { Xen_VirtualSystemSettingData_AutomaticShutdownAction_Save_state,  XEN_ON_NORMAL_EXIT_UNDEFINED},
    { Xen_VirtualSystemSettingData_AutomaticShutdownAction_Shutdown,    XEN_ON_NORMAL_EXIT_DESTROY}
};

int actions_after_crash_map[3][2] = {
    { Xen_VirtualSystemSettingData_AutomaticRecoveryAction_None,        XEN_ON_CRASH_BEHAVIOUR_UNDEFINED},
    { Xen_VirtualSystemSettingData_AutomaticRecoveryAction_Restart,     XEN_ON_CRASH_BEHAVIOUR_RESTART},
    { Xen_VirtualSystemSettingData_AutomaticRecoveryAction_Revert_to_snapshot,  XEN_ON_CRASH_BEHAVIOUR_RESTART}
};

int map_action_xen (int map[3][2], int dmtf_value)
{
    int i=0;
    while (i<3) {
        if (map[i][0] == dmtf_value) {
            return map[i][1];
        }
        i++;
    }
    return map[0][1]; // default
}

int map_action_dmtf(int map[3][2], int xen_value)
{
    int i=0;
    while (i<3) {
        if (map[i][1] == xen_value) {
            return map[i][0];
        }
        i++;
    }
    return map[0][0]; // default
}

/* Support modifying the following settings ...
 * PV Specific: Kernel, RAMDisk, KernelOptions, LegacyKernelOptions, BootLoader, PV_BOOTLOADER_ARGS
 * HVM Specific: BootPolicy, PV_BOOTLOADER_ARGS
 * Common: localtime, OnPoweroff, ACTIONS_AFTER_REBOOT, OnCrash, ShadowMultiplier,
 *         Platform (includes apic, acpi, pae, usb, usbdevice, stdvga, viridian, nx)
 */
static int _modify_vm_setting(
    xen_utils_session* pSession,
    xen_vm vm_handle,
    xen_vm_record *vm_rec,
    const char *name,
    CMPIData* data
    )
{
    char *prop_val = NULL;
    if(data->type == CMPI_string)
        prop_val = CMGetCharPtr(data->value.string);

#if XENAPI_VERSION > 400
    if (strcmp(name, "Description") == 0) {
        xen_vm_set_name_description(pSession->xen, vm_handle, prop_val);
    }
    else
#endif
    if (strcmp(name, "ElementName") == 0) {
        xen_vm_set_name_label(pSession->xen, vm_handle, prop_val);
    }
    else if (strcmp(name, PV_KERNEL) == 0) {
        xen_vm_set_pv_kernel(pSession->xen, vm_handle, prop_val);
    }
    else if (strcmp(name, PV_RAMDISK) == 0) {
        xen_vm_set_pv_ramdisk(pSession->xen, vm_handle, prop_val);
    }
    else if (strcmp(name, PV_ARGS) == 0) {
        xen_vm_set_pv_args(pSession->xen, vm_handle, prop_val);
    }
    else if (strcmp(name, PV_LEGACY_ARGS) == 0) {
        xen_vm_set_pv_legacy_args(pSession->xen, vm_handle, prop_val);
    }
    else if (strcmp(name, PV_BOOTLOADER) == 0) {
        xen_vm_set_pv_bootloader(pSession->xen, vm_handle, prop_val);
    }
    else if (strcmp(name, PV_BOOTLOADER_ARGS) == 0) {
        xen_vm_set_pv_bootloader_args(pSession->xen, vm_handle, prop_val);
    }
    else if (strcmp(name, HVM_BOOT_PARAMS) == 0) {
        xen_string_string_map *map = 
            xen_utils_convert_CMPIArray_to_string_string_map(data->value.array);
        if (map) {
            xen_vm_set_hvm_boot_params(pSession->xen, vm_handle, map);
            xen_string_string_map_free(map);
        }
    }
    else if (strcmp(name, HVM_BOOT_POLICY) == 0) {
        xen_vm_set_hvm_boot_policy(pSession->xen, vm_handle, prop_val);
    }
    else if (strcmp(name, PLATFORM) == 0) {
        xen_string_string_map *map = 
            xen_utils_convert_CMPIArray_to_string_string_map(data->value.array);
        if (map) {
            xen_vm_set_platform(pSession->xen, vm_handle, map);
            xen_string_string_map_free(map);
        }
    }
    else if (strcmp(name, ACTIONS_AFTER_SHUTDOWN) == 0) {
        xen_vm_set_actions_after_shutdown(pSession->xen,vm_handle, 
            map_action_xen(actions_after_shutdown_map, data->value.uint16));
    }
    else if (strcmp(name, ACTIONS_AFTER_REBOOT) == 0) {
        xen_vm_set_actions_after_reboot(pSession->xen, vm_handle, 
            map_action_xen(actions_after_reboot_map, data->value.uint16));
    }
    else if (strcmp(name, ACTIONS_AFTER_CRASH) == 0) {
        xen_vm_set_actions_after_crash(pSession->xen,
            vm_handle, 
            map_action_xen(actions_after_crash_map, data->value.uint16));
    }
    else if (strcmp(name, HVM_SHADOW_MULTIPLIER) == 0) {
#if XENAPI_VERSION > 400
        xen_vm_set_hvm_shadow_multiplier(pSession->xen,
            vm_handle, data->value.real64);
#endif
    }
    else if (strcmp(name, XENSTORE_DATA) == 0) {
        xen_string_string_map *map = 
            xen_utils_convert_CMPIArray_to_string_string_map(data->value.array);
        if (map) {
            xen_vm_set_xenstore_data(pSession->xen, vm_handle, map);
            xen_string_string_map_free(map);
        }
    }
    else if (strcmp(name, OTHER_CONFIG) == 0) {
        xen_string_string_map *map = 
            xen_utils_convert_CMPIArray_to_string_string_map(data->value.array);
        if (map) {
            int i=0; 
            xen_string_string_map *other_config = NULL;
            xen_vm_get_other_config(pSession->xen, &other_config, vm_handle);
            for (i=0; i<map->size; i++)
                xen_utils_add_to_string_string_map(map->contents[i].key, map->contents[i].val, &other_config);
            if (other_config) {
                xen_vm_set_other_config(pSession->xen, vm_handle, other_config);
                xen_string_string_map_free(other_config);
            }
        }
            xen_string_string_map_free(map);
    }
    else if (strcmp(name, HOST_AFFINITY) == 0) {
        char *affinity = CMGetCharPtr(data->value.string);
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Host affinity set as %s", affinity));
        xen_host host;
        if(xen_host_get_by_uuid(pSession->xen, &host, affinity)) {
            xen_vm_set_affinity(pSession->xen, vm_handle, host);
            xen_host_free(host);
        }
    }

    if (!pSession->xen->ok) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("Failed to modify domain %s setting", name));
        xen_utils_trace_error(pSession->xen, __FILE__, __LINE__);
        return 0;
    }
    return 1;
}

int vssd_modify(
    xen_utils_session* pSession,
    xen_vm vm_handle,
    xen_vm_record *vm_rec,
    CMPIInstance *modified_inst,
    const char **properties)
{
    int i;
    CMPIStatus status = {CMPI_RC_OK, NULL};
    CMPIData data;

    _SBLIM_ENTER("vssd_modify");

    if (properties) {
        for (i = 0; properties[i] != NULL; i++) {
            data = CMGetProperty(modified_inst, properties[i], &status);
            if ((status.rc != CMPI_RC_OK) || CMIsNullValue(data)) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                    ("Unable to retrieve property %s from VSSD",
                    properties[i]));
                _SBLIM_RETURN(0);
            }

            if (!_modify_vm_setting(pSession, vm_handle, vm_rec, properties[i], &data)) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("_modify_vm_setting failed"));
                _SBLIM_RETURN(0);
            }
        }
        _SBLIM_RETURN(1);
    }
    /* BUGBUG: Pegasus doesnt allow us to distinguish between properties that have been explicity
       set to NULL (for the purposes of unsettng a previous value) and properties that have
       not been set at all.  Both show up as NULL values.... For now unsetting is not supported !
       */
    unsigned int prop_count = CMGetPropertyCount(modified_inst, NULL);
    unsigned int j;
    for(j=0; j<prop_count; j++) {
        CMPIString *prop_name = NULL;
        CMPIData data = CMGetPropertyAt(modified_inst, j, &prop_name, &status);
        if (status.rc == CMPI_RC_OK && !CMIsNullValue(data) 
            && prop_name && CMGetCharPtr(prop_name) != NULL) {
            /* bug in peagsus - array properties dont show up properly if retrieved with the GetPropertyAt API */
            CMPIData realdata = CMGetProperty(modified_inst, CMGetCharPtr(prop_name), NULL);
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Modifying property %s", CMGetCharPtr(prop_name)));
            _modify_vm_setting(pSession, vm_handle, vm_rec, CMGetCharPtr(prop_name), &realdata);
        }
    }
    _SBLIM_RETURN(1);
}

/******************************************************************************
* _get_vm_platform_config
*
* Parses a Xen_ComputerSystemSettingData CIM instance and populates a
* xen VM record.
*
* Returns 1 on Success and 0 on failure.
******************************************************************************/
static int _get_vm_platform_config(
    const CMPIBroker* broker,
    CMPIInstance *vssd,
    xen_vm_record *vm_rec,
    CMPIStatus *status)
{
    _SBLIM_ENTER("_get_vm_platform_config");
    char *plat_val, *current_val;
    CMPIData propertyvalue;
    propertyvalue = CMGetProperty(vssd, PLATFORM, status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        vm_rec->platform = 
            xen_utils_convert_CMPIArray_to_string_string_map(
                propertyvalue.value.array);
    }
    else {
        /* Default stdvga to 0, default platform string setup
         * Platform string seems to need at least 1 setting, use "stdvga=false"
         * to make sure we have something set
         * Note: this is only used if no platform setting is received. */
        plat_val = "false";
        current_val = xen_utils_get_from_string_string_map(vm_rec->platform, "stdvga");
        if (!current_val) {
            if (!xen_utils_add_to_string_string_map("stdvga", plat_val, &(vm_rec->platform))) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                    ("Cannot malloc memory for xend platform settings list"));
                CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM,
                    "Unable to malloc memory");
                _SBLIM_RETURN(0);
            }
        }
    }
    _SBLIM_RETURN(1);
}

/******************************************************************************
* vssd_to_vm_rec
*
* Parses a Xen_ComputerSystemSettingData CIM instance and populates a
* xen VM record.
*
* Returns 1 on Success and 0 on failure.
******************************************************************************/
int vssd_to_vm_rec(
    const CMPIBroker* broker,
    CMPIInstance *vssd,
    xen_utils_session *session,
    bool strict_checks,
    xen_vm_record** vm_rec_out,
    CMPIStatus *status
    )
{
    CMPIData propertyvalue;
    CMPIObjectPath* objectpath = NULL;
    char *vsType = NULL;
    xen_string_string_map *map = NULL;
    xen_vm_record *vm_rec = NULL;

    status->rc = CMPI_RC_OK;

    /* Get the class type of the setting data instance */
    objectpath = CMGetObjectPath(vssd, NULL);
    char *settingclassname = CMGetCharPtr(CMGetClassName(objectpath, NULL));

    /* Ensure we have either a Xen_ComputerSystemSettingData or Xen_ComputerSystemTemplate */
    if (xen_utils_class_is_subclass_of(broker, settingclassname, "CIM_VirtualSystemSettingData") == false) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Unrecognized setting data class - %s", settingclassname));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER, "Unrecognized setting data class");
        goto Error;
    }

    vm_rec = xen_vm_record_alloc();
    if (vm_rec == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Cannot malloc memory for vm record"));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM, "Unable to malloc memory");
        goto Error;
    }
    /*
     * Get domain name.
     * WARNING!
     */
    propertyvalue = CMGetProperty(vssd, "ElementName", status);
    if ((status->rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue) || (propertyvalue.type != CMPI_string)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("failed to retrieve ElementName property value"));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER,
            "Unable to retrieve ElementName (name)  property from virtual system setting data");
        goto Error;
    }
    vm_rec->name_label = strdup(CMGetCharPtr(propertyvalue.value.string));

    propertyvalue = CMGetProperty(vssd, "Description", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string))
        vm_rec->name_description = strdup(CMGetCharPtr(propertyvalue.value.string));

    propertyvalue = CMGetProperty(vssd, "VirtualSystemIdentifier", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
        vm_rec->uuid = strdup(CMGetCharPtr(propertyvalue.value.string));
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_WARNING, ("no VirtualSystemIndentifier property value in VSSD"));
    }

    propertyvalue = CMGetProperty(vssd, ACTIONS_AFTER_SHUTDOWN, status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_INTEGER))
        vm_rec->actions_after_shutdown = map_action_xen(actions_after_shutdown_map, propertyvalue.value.uint16);

    propertyvalue = CMGetProperty(vssd, ACTIONS_AFTER_REBOOT, status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_INTEGER)){
        vm_rec->actions_after_reboot = map_action_xen(actions_after_reboot_map, propertyvalue.value.uint16);
    } else {
       //If no value is set for actions_after_reboot, we should set the default value to be restart
      vm_rec->actions_after_reboot = XEN_ON_NORMAL_EXIT_RESTART;
    }

    propertyvalue = CMGetProperty(vssd, ACTIONS_AFTER_CRASH, status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_INTEGER))
        vm_rec->actions_after_crash = map_action_xen(actions_after_crash_map, propertyvalue.value.uint16);

    propertyvalue = CMGetProperty(vssd, HOST_AFFINITY, status);
    if (propertyvalue.type != CMPI_string)
           _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Host affinity is of type string"));
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string)){
        xen_host host = NULL;
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Host affinity should be set to %s",CMGetCharPtr(propertyvalue.value.string) ));
        if(xen_host_get_by_uuid(session->xen, &host, CMGetCharPtr(propertyvalue.value.string))) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Host reference has been returned"));
            xen_host_record_opt *host_opt = xen_host_record_opt_alloc();
            host_opt->u.handle = host;
            host_opt->is_record = false;
            vm_rec->affinity = host_opt;
        }
        else
            goto Error;
    }

    propertyvalue = CMGetProperty(vssd, OTHER_CONFIG, status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_stringA))
        vm_rec->other_config = xen_utils_convert_CMPIArray_to_string_string_map(propertyvalue.value.array);
    else 
        vm_rec->other_config = xen_string_string_map_alloc(0); /* create an empty other config */

    /* PV Driver version information - common to both HVM and PV domains */

    /* Paravirtual or HVM domain? */
    propertyvalue = CMGetProperty(vssd, "VirtualSystemType", status);
    if ((status->rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue) || (propertyvalue.type != CMPI_string)) {
        if (strict_checks) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("VirtualSystemType not specified"));
            CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER, "VirtualSystemType not specified");
            goto Error;
        }
    }
    else
        vsType = CMGetCharPtr(propertyvalue.value.string);

    if (!_get_vm_platform_config(broker, vssd, vm_rec, status)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("_get_vm_platform_config failed"));
        goto Error;
    }

#if XENAPI_VERSION > 400
    vm_rec->hvm_shadow_multiplier = 1.0; // required for both PV and HVM VMs
#endif

    if (vsType == NULL || vsType[0] == '\0') {
        if (strict_checks) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("VirtualSystemType not specified"));
            CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER, "VirtualSystemType not specified");
            goto Error;
        }
    }
    else {
        if (strstr(vsType, "PV")) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("VirtualSystemType : Xen paravirtualized"));

            /* add a null HVM boot params since this is a PV guest, 
              Oddly, vm_create requires it*/
            map = xen_string_string_map_alloc(0);
            if (map == NULL)
                goto Error;
            map->size = 0;
            vm_rec->hvm_boot_params = map;
            vm_rec->hvm_boot_policy = strdup("");

            propertyvalue = CMGetProperty(vssd, PV_BOOTLOADER, status);
            if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string)) {
                vm_rec->pv_bootloader = strdup(CMGetCharPtr(propertyvalue.value.string));
                propertyvalue = CMGetProperty(vssd, PV_BOOTLOADER_ARGS, status);
                if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string)) {
                    vm_rec->pv_bootloader_args = strdup(CMGetCharPtr(propertyvalue.value.string));
                }
            }

            /* Only honor Kernel if Bootloader not specified. */
            propertyvalue = CMGetProperty(vssd, PV_KERNEL, status);
            if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string) && 
                vm_rec->pv_bootloader == NULL) {
                vm_rec->pv_kernel = strdup(CMGetCharPtr(propertyvalue.value.string));
                propertyvalue = CMGetProperty(vssd, PV_RAMDISK, status);
                if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string)) {
                    vm_rec->pv_ramdisk = strdup(CMGetCharPtr(propertyvalue.value.string));
                }
            }
            else {
	      if (vm_rec->pv_bootloader)
		free(vm_rec->pv_bootloader);
                vm_rec->pv_bootloader = strdup("pygrub");
	    }
            propertyvalue = CMGetProperty(vssd, PV_ARGS, status);
            if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string))
                vm_rec->pv_args = strdup(CMGetCharPtr(propertyvalue.value.string));
            else
                vm_rec->pv_args = strdup("Term=xterm");

            propertyvalue = CMGetProperty(vssd, PV_LEGACY_ARGS, status);
            if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string))
                vm_rec->pv_legacy_args = strdup(CMGetCharPtr(propertyvalue.value.string));
        }
        else if (strstr(vsType, "HVM")) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("VirtualSystemType: HVM"));
            propertyvalue = CMGetProperty(vssd, HVM_BOOT_PARAMS, status);
            if (((status->rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue) || (propertyvalue.type != CMPI_string)) 
                && strict_checks) {
                CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER,
                    "No BootParams specified for HVM guest - Using defaults");
                vm_rec->hvm_boot_params = xen_utils_convert_string_to_string_map("order=dc", ",");
            }
            else
                vm_rec->hvm_boot_params =
                    xen_utils_convert_CMPIArray_to_string_string_map(propertyvalue.value.array);

            propertyvalue = CMGetProperty(vssd, HVM_BOOT_POLICY, status);
            if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string))
                vm_rec->hvm_boot_policy = strdup(CMGetCharPtr(propertyvalue.value.string));
            else
                vm_rec->hvm_boot_policy = strdup("BIOS order");

            propertyvalue = CMGetProperty(vssd, HVM_SHADOW_MULTIPLIER, status);
            if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_REAL)) {
    #if XENAPI_VERSION > 400
                vm_rec->hvm_shadow_multiplier = propertyvalue.value.real64;
    #endif
            }
            else
                vm_rec->hvm_shadow_multiplier = 1.0;
        }
        else {
            CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER, "Invalid VirtualSystemType specified");
            goto Error;
        }
    }

    /* Common to both HVM and PV */
    propertyvalue = CMGetProperty(vssd, XENSTORE_DATA, status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_stringA))
        vm_rec->xenstore_data = 
            xen_utils_convert_CMPIArray_to_string_string_map(propertyvalue.value.array);

    status->rc = CMPI_RC_OK;
    *vm_rec_out = vm_rec;
    return 1;

    Error:
    if (vm_rec)
        xen_vm_record_free(vm_rec);
    return 0;
}

void _update_vm_from_vm_rec(
    xen_utils_session *session,
    xen_vm_record* vm_rec,
    xen_vm vm)
{

    /* update all the VM's fields */
    if (vm_rec->name_label)
        xen_vm_set_name_label(session->xen, vm, vm_rec->name_label);
    if (vm_rec->name_description)
        xen_vm_set_name_description(session->xen, vm, vm_rec->name_description);
    if (vm_rec->actions_after_shutdown)
        xen_vm_set_actions_after_shutdown(session->xen, vm, vm_rec->actions_after_shutdown);
    if (vm_rec->actions_after_reboot)
        xen_vm_set_actions_after_reboot(session->xen, vm, vm_rec->actions_after_reboot);
    if (vm_rec->actions_after_crash)
        xen_vm_set_actions_after_crash(session->xen, vm, vm_rec->actions_after_crash);

    return;
}
/*
* vssd_from_vm_rec
*
* Parses a Xen VM record (also metrics and guest_metrics records) and populates the
* Xen_ComputerSystemSettingData CIM instance with all the necessary information.
*
* Returns 1 on Success and 0 on failure.
*/
int vssd_from_vm_rec(
    const CMPIBroker* broker,
    xen_utils_session *session,
    CMPIInstance *inst,
    xen_vm vm,
    xen_vm_record *vm_rec,
    bool ref_only
    )
{
    char buf[MAX_INSTANCEID_LEN];
    memset(buf, 0, sizeof(buf));
    char *map_val=NULL;
    CMPIArray *arr = NULL;
    bool is_cssd = false, is_snapshot = false;
    xen_host_record_opt *host_affinity_opt = vm_rec->affinity;
    xen_host_record_opt *host_opt = vm_rec->resident_on;
    xen_host_record *host_affinity_rec = NULL, *host_rec = NULL;
    char *host_uuid = NULL, *host_affinity_uuid = NULL;
    xen_vm_metrics vm_metrics = NULL;

    vssd_create_instance_id(session, vm_rec, buf, sizeof(buf));
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    if(ref_only)
        return CMPI_RC_OK;
    
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("vm reference: %s", vm));
    if (xen_vm_get_metrics(session->xen, &vm_metrics, vm) && 
        (vm_metrics != NULL)) {
        xen_vm_metrics_record *vm_metrics_rec = NULL;
        if (xen_vm_metrics_get_record(session->xen, &vm_metrics_rec, vm_metrics) 
            && (vm_metrics_rec != NULL)) {
#if XENAPI_VERSION > 400
            CMPIDateTime *install_time = xen_utils_time_t_to_CMPIDateTime(broker, vm_metrics_rec->install_time);
            CMSetProperty(inst, CREATION_TIME, &install_time, CMPI_dateTime);
#endif
            if (vm_metrics_rec->start_time) {
                CMPIDateTime *start_time = xen_utils_time_t_to_CMPIDateTime(broker, vm_metrics_rec->start_time);
                CMSetProperty(inst, STARTTIME,(CMPIValue *)&start_time, CMPI_dateTime);
            }
            xen_vm_metrics_record_free(vm_metrics_rec);
        }
        xen_vm_metrics_free(vm_metrics);
    }
    RESET_XEN_ERROR(session->xen); /* reset any session errors */

    if (host_affinity_opt && (strcmp(host_affinity_opt->u.handle, XAPI_NULL_REF) != 0)) {
        if (host_affinity_opt->is_record)
            host_affinity_rec = host_affinity_opt->u.record;
        else
            xen_host_get_uuid(session->xen, &host_affinity_uuid, host_affinity_opt->u.handle);
        RESET_XEN_ERROR(session->xen);
    }
    else
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Caught a 'OpaqueRef:NULL (host_affinity_opt)"));

    if (host_opt && (strcmp(host_opt->u.handle, XAPI_NULL_REF) != 0)) {
        if (host_opt->is_record)
            host_rec = host_opt->u.record;
        else 
	    xen_host_get_uuid(session->xen, &host_uuid, host_opt->u.handle);

	
        RESET_XEN_ERROR(session->xen);
    }
    else
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Caught a 'OpaqueRef:NULL (host_opt)"));

    // Either active settings, template or snapshot settings
    CMPIStatus status = {CMPI_RC_OK, NULL};
    CMPIObjectPath *op = CMGetObjectPath(inst, &status);
    CMPIString *cn = CMGetClassName(op, &status);
    if (strcmp(CMGetCharPtr(cn), "Xen_ComputerSystemTemplate") == 0)
        CMSetProperty(inst, "Caption", (CMPIValue *)"Template for a Xen virtual machine", CMPI_chars);
    else if (strcmp(CMGetCharPtr(cn), "Xen_ComputerSystemSnapshot") == 0) {
        CMSetProperty(inst, "Caption", (CMPIValue *)"Settings for a Snapshot of a Xen virtual machine", CMPI_chars);
        is_snapshot = true;
    }
    else {
        is_cssd = true;
        CMSetProperty(inst, "Caption", (CMPIValue *)"Active Settings of a Xen virtual machine", CMPI_chars);
    }

    CMSetProperty(inst, "Description",(CMPIValue *)vm_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)vm_rec->name_label, CMPI_chars);
    CMSetProperty(inst, "VirtualSystemIdentifier", (CMPIValue *)vm_rec->uuid, CMPI_chars);

    if ((map_val = xen_utils_get_from_string_string_map(vm_rec->hvm_boot_params, "order"))) {
        /* HVM settings */
        CMSetProperty(inst, "VirtualSystemType", (CMPIValue *)HVM_VM_TYPE, CMPI_chars);
        arr = xen_utils_convert_string_string_map_to_CMPIArray(broker, vm_rec->hvm_boot_params);
        if(arr)
            CMSetProperty(inst, HVM_BOOT_PARAMS, (CMPIValue *)&arr, CMPI_charsA);

        if (vm_rec->hvm_boot_policy && vm_rec->hvm_boot_policy[0] != '\0')
            CMSetProperty(inst, HVM_BOOT_POLICY, (CMPIValue *)vm_rec->hvm_boot_policy, CMPI_chars);
        if (vm_rec->hvm_shadow_multiplier) {
#if XENAPI_VERSION > 400
            CMSetProperty(inst, HVM_SHADOW_MULTIPLIER, (CMPIValue *)&(vm_rec->hvm_shadow_multiplier), CMPI_real64);
#endif
        }

    }
    else {
        /* PV settings */
        CMSetProperty(inst, "VirtualSystemType", (CMPIValue *)PV_VM_TYPE, CMPI_chars);
        if (vm_rec->pv_kernel && vm_rec->pv_kernel[0] != '\0')
            CMSetProperty(inst, PV_KERNEL,(CMPIValue *)vm_rec->pv_kernel, CMPI_chars);
        if (vm_rec->pv_ramdisk && vm_rec->pv_ramdisk[0] != '\0')
            CMSetProperty(inst, PV_RAMDISK,(CMPIValue *)vm_rec->pv_ramdisk, CMPI_chars);
        if (vm_rec->pv_args && vm_rec->pv_args[0] != '\0')
            CMSetProperty(inst, PV_ARGS, (CMPIValue *)vm_rec->pv_args, CMPI_chars);
        if (vm_rec->pv_legacy_args && vm_rec->pv_legacy_args[0] != '\0')
            CMSetProperty(inst, PV_LEGACY_ARGS, (CMPIValue *)vm_rec->pv_legacy_args, CMPI_chars);
        if (vm_rec->pv_bootloader && vm_rec->pv_bootloader[0] != '\0')
            CMSetProperty(inst, PV_BOOTLOADER,(CMPIValue *)vm_rec->pv_bootloader, CMPI_chars);
        if (vm_rec->pv_bootloader_args && vm_rec->pv_bootloader_args[0] != '\0')
            CMSetProperty(inst, PV_BOOTLOADER_ARGS, (CMPIValue *)vm_rec->pv_bootloader_args, CMPI_chars);
    }
    /* Set the object's parent */
    if(vm_rec->parent) {
        xen_vm_record *parent_rec = NULL;
        if(vm_rec->parent->is_record)
            parent_rec = vm_rec->parent->u.record;
        else {
	  if( strcmp(vm_rec->parent->u.handle, XAPI_NULL_REF) != 0) {
               xen_vm_get_record(session->xen, &parent_rec, vm_rec->parent->u.handle);
	       //_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VM Ref %s", vm_rec->parent->u.handle));
	  }
	  else
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VM has no parent reference"));
	    }    
        if(parent_rec) {
            CMPIObjectPath *parent_op = NULL;
            if(parent_rec->is_a_snapshot)
                parent_op = snapshot_create_ref(broker, DEFAULT_NS, session, parent_rec);
            else if(parent_rec->is_a_template)
                parent_op = template_create_ref(broker, DEFAULT_NS, session, parent_rec);
            else
                parent_op = vm_create_ref(broker, DEFAULT_NS, session, parent_rec);
            if(parent_op) {
                char *parent_path = xen_utils_CMPIObjectPath_to_WBEM_URI(broker, parent_op);
                CMSetProperty(inst, PARENT, parent_path, CMPI_chars);
            }
            if(!vm_rec->parent->is_record) {
                xen_vm_record_free(parent_rec);
            }
        }
    }
    ///*
    if(vm_rec->children) {
        xen_string_set *children_wbem_uris = NULL;
        int i=0;
        for(i=0; i<vm_rec->children->size; i++) {
            xen_vm_record *child_rec = NULL;
            xen_vm_record_opt *child_vm_opt = vm_rec->children->contents[i];
            if(child_vm_opt->is_record)
                child_rec = child_vm_opt->u.record;
            else {
	      if( strcmp(child_vm_opt->u.handle,XAPI_NULL_REF) != 0) {
                xen_vm_get_record(session->xen, &child_rec, child_vm_opt->u.handle);
		_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VM ref %s", child_vm_opt->u.handle));
	      }
	      else
		_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VM has no children. Null ref caught"));
	    }
            if(child_rec) {
                CMPIObjectPath *child_op = NULL;
                if(child_rec->is_a_snapshot)
                    child_op = snapshot_create_ref(broker, DEFAULT_NS, session, child_rec);
                else if(child_rec->is_a_template)
                    child_op = template_create_ref(broker, DEFAULT_NS, session, child_rec);
                else
                    child_op = vm_create_ref(broker, DEFAULT_NS, session, child_rec);
                if(child_op) {
                    char *child_wbem_uri = xen_utils_CMPIObjectPath_to_WBEM_URI(broker, child_op);
                    xen_utils_add_to_string_set(child_wbem_uri, &children_wbem_uris);
                }
                if(!child_vm_opt->is_record) {
                    xen_vm_record_free(child_rec);
                }
            }
        }
        if(children_wbem_uris) {
            CMPIArray *children_arr = xen_utils_convert_string_set_to_CMPIArray(broker, children_wbem_uris);
            CMSetProperty(inst, CHILDREN, &children_arr, CMPI_charsA);
	    xen_string_set_free(children_wbem_uris);
        }
    }
    //*/
    RESET_XEN_ERROR(session->xen); /* reset errors */

    if (strcmp(CMGetCharPtr(cn), "Xen_ComputerSystemSnapshot") == 0) {
        if(vm_rec->snapshot_of) {
            xen_vm_record *snap_of_rec = NULL;
            if(vm_rec->snapshot_of->is_record)
                snap_of_rec = vm_rec->snapshot_of->u.record;
            else {
	      if(strcmp(vm_rec->snapshot_of->u.handle, XAPI_NULL_REF) != 0) {
                xen_vm_get_record(session->xen, &snap_of_rec, vm_rec->snapshot_of->u.handle);
		//_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VM Ref %s", vm_rec->snapshot_of->u.handle));
	      }
	    }
            if(snap_of_rec) {
                CMPIObjectPath *snap_of_op = vm_create_ref(broker, DEFAULT_NS, session, snap_of_rec);
                if(snap_of_op) {
                    char *snap_of_path = xen_utils_CMPIObjectPath_to_WBEM_URI(broker, snap_of_op);
                    CMSetProperty(inst, SNAPSHOT_OF, snap_of_path, CMPI_chars);
                }
                if(!vm_rec->snapshot_of->is_record) {
                    xen_vm_record_free(snap_of_rec);
                }
            }
        }
        RESET_XEN_ERROR(session->xen); /* reset errors */

        if(vm_rec->snapshot_time) {
            CMPIDateTime *snap_time = xen_utils_time_t_to_CMPIDateTime(broker, vm_rec->snapshot_time);
            CMSetProperty(inst, SNAPSHOT_TIME, &snap_time, CMPI_dateTime);
        }
    }
    /* These settings can also be specified via Xen_MemorySettingData RASD */
    if (is_cssd) {
        /* which host VM is running on, if started */
        if (host_uuid)
            CMSetProperty(inst, HOST, (CMPIValue *)host_uuid, CMPI_chars);
        /* which host VM has storage on (or general affinity towards) */
        if (host_affinity_uuid)
            CMSetProperty(inst, HOST_AFFINITY, (CMPIValue *)host_affinity_uuid, CMPI_chars);

        if (vm_rec->xenstore_data && vm_rec->xenstore_data->size != 0) {
            arr = xen_utils_convert_string_string_map_to_CMPIArray(broker, vm_rec->xenstore_data);
            if (arr)
                CMSetProperty(inst, XENSTORE_DATA, (CMPIValue *)&arr, CMPI_charsA);
        }
        xen_string_set *string_set = NULL;
        if (xen_vm_get_allowed_vbd_devices(session->xen, &string_set, vm) && string_set) {
            char *val = xen_utils_flatten_string_set(string_set, ",");
            CMSetProperty(inst, AVAILABLE_VBD_DEVICES, (CMPIValue *)val, CMPI_chars);
            xen_string_set_free(string_set);
            string_set = NULL;
            free(val);
        }
        if (xen_vm_get_allowed_vif_devices(session->xen, &string_set, vm) && string_set) {
            char *val = xen_utils_flatten_string_set(string_set, ",");
            CMSetProperty(inst, AVAILABLE_VIF_DEVICES, (CMPIValue *)val, CMPI_chars);
            xen_string_set_free(string_set);
            string_set = NULL;
            free(val);
        }
        /* check to see if the tools are running. */ 
        /* One way to do this is to check if the clean_shutdown operation is avialbale on the VM */
        bool tools_are_running = false;
        int i;
        if(vm_rec->allowed_operations) {
            for (i=0; i<vm_rec->allowed_operations->size; i++) {
                if(vm_rec->allowed_operations->contents[i] == 
                   XEN_VM_OPERATIONS_CLEAN_SHUTDOWN) {
                    tools_are_running = true;
                    break;
                }
            }
        }
        CMSetProperty(inst, XEN_TOOLS_RUNNING, (CMPIValue *)&tools_are_running, CMPI_boolean);
    }
    RESET_XEN_ERROR(session->xen); /* reset errors */
    if(is_cssd || is_snapshot) {
        /* Some guest metrics information such as xen tools version, os version etc */
        xen_vm_guest_metrics guest_metrics = NULL;
        if(xen_vm_get_guest_metrics(session->xen, &guest_metrics, vm) && (strcmp(guest_metrics,XAPI_NULL_REF) != 0)) {
            xen_vm_guest_metrics_record *guest_metrics_rec = NULL;
            if(xen_vm_guest_metrics_get_record(session->xen, &guest_metrics_rec, guest_metrics) && 
               (guest_metrics_rec != NULL)) {
                arr = xen_utils_convert_string_string_map_to_CMPIArray(broker, guest_metrics_rec->pv_drivers_version);
                if(arr)
                    CMSetProperty(inst, XEN_TOOLS_VERSION, (CMPIValue *)&arr, CMPI_charsA);
                arr = xen_utils_convert_string_string_map_to_CMPIArray(broker, guest_metrics_rec->os_version);
                if(arr)
                    CMSetProperty(inst, OS_VERSION, (CMPIValue *)&arr, CMPI_charsA);
                CMSetProperty(inst, XEN_TOOLS_UPTODATE, (CMPIValue *)&guest_metrics_rec->pv_drivers_up_to_date, CMPI_boolean);
                xen_vm_guest_metrics_record_free(guest_metrics_rec);
            }

        }
        xen_vm_guest_metrics_free(guest_metrics);
    }
    RESET_XEN_ERROR(session->xen); /* reset errors */

    arr = xen_utils_convert_string_string_map_to_CMPIArray(broker, vm_rec->other_config);
    if (arr)
        CMSetProperty(inst, OTHER_CONFIG, (CMPIValue *)&arr, CMPI_charsA);

    /* Common settings */
    arr = xen_utils_convert_string_string_map_to_CMPIArray(broker, vm_rec->platform);
    if (arr)
        CMSetProperty(inst, PLATFORM, (CMPIValue *)&arr, CMPI_charsA);

    int action;
    action = map_action_dmtf(actions_after_shutdown_map, vm_rec->actions_after_shutdown);
    CMSetProperty(inst, ACTIONS_AFTER_SHUTDOWN, (CMPIValue *)&action, CMPI_uint16);
    action = map_action_dmtf(actions_after_reboot_map, vm_rec->actions_after_reboot);
    CMSetProperty(inst, ACTIONS_AFTER_REBOOT, (CMPIValue *)&action, CMPI_uint16);
    action = map_action_dmtf(actions_after_crash_map, vm_rec->actions_after_crash);
    CMSetProperty(inst, ACTIONS_AFTER_CRASH, (CMPIValue *)&action, CMPI_uint16);
    CMSetProperty(inst, "SystemName", (CMPIValue *)vm_rec->uuid, CMPI_chars);

    if (host_rec && !host_opt->is_record)
        xen_host_record_free(host_rec);
    if (host_affinity_rec && !host_affinity_opt->is_record)
        xen_host_record_free(host_affinity_rec);
    if (host_affinity_uuid)
      free(host_affinity_uuid);
    if (host_uuid)
      free(host_uuid);

    return 1;
}

int vssd_find_vm(
    CMPIObjectPath *ref, 
    xen_utils_session *session,
    xen_vm *vm,
    xen_vm_record **vm_rec,
    CMPIStatus *status
    )
{
    CMPIData vm_uuid_data = CMGetKey(ref, "InstanceID", status);
    status->rc = CMPI_RC_OK;
    if (status->rc != CMPI_RC_OK || CMIsNullValue(vm_uuid_data)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- InstanceID not found in SystemSettings"));
        status->rc = CMPI_RC_ERR_INVALID_PARAMETER;
        return 0;
    }
    ;
    char vm_uuid[MAX_INSTANCEID_LEN];
    _CMPIStrncpySystemNameFromID(vm_uuid, CMGetCharPtr(vm_uuid_data.value.string), MAX_INSTANCEID_LEN);
    if (!xen_vm_get_by_uuid(session->xen, vm, vm_uuid)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Couldnt find VM %s", vm_uuid));
        status->rc = CMPI_RC_ERR_NOT_FOUND;
        return 0;
    }
    xen_vm_get_record(session->xen, vm_rec, *vm);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Found VSSD %s", (*vm_rec)->uuid));
    xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    return 1;
}

void vssd_create_instance_id(
    xen_utils_session *session,
    xen_vm_record *vm_rec, 
    char* buf, 
    int buf_len)
{
    _CMPICreateNewSystemInstanceID(buf, buf_len, vm_rec->uuid);
}

CMPIObjectPath* vm_create_ref(
    const CMPIBroker *broker, 
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec)
{
    //char buf[MAX_INSTANCEID_LEN];
    CMPIObjectPath * op = CMNewObjectPath(broker, nameSpace, "Xen_ComputerSystem", NULL);
    CMAddKey(op, "CreationClassName", (CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
    //vssd_create_instance_id(session, vm_rec, buf, sizeof(buf));
    CMAddKey(op, "Name", (CMPIValue *)vm_rec->uuid, CMPI_chars);
    return op;
}

CMPIObjectPath *snapshot_create_ref(
    const CMPIBroker *broker,
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec
    )
{
    CMPIObjectPath * op = CMNewObjectPath(broker, nameSpace, "Xen_ComputerSystemSnapshot", NULL);
    char buf[MAX_INSTANCEID_LEN+1];
    vssd_create_instance_id(session, vm_rec, buf, sizeof(buf));
    CMAddKey(op, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    return op;
}

CMPIObjectPath *vssd_create_ref(
    const CMPIBroker *broker,
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec
    )
{
    CMPIObjectPath * op = CMNewObjectPath(broker, nameSpace, "Xen_ComputerSystemSettingData", NULL);
    char buf[MAX_INSTANCEID_LEN+1];
    vssd_create_instance_id(session, vm_rec, buf, sizeof(buf));
    CMAddKey(op, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    return op;
}

CMPIObjectPath *template_create_ref(
    const CMPIBroker *broker,
    const char *nameSpace,
    xen_utils_session *session,
    xen_vm_record *vm_rec
    )
{
    CMPIObjectPath * op = CMNewObjectPath(broker, nameSpace, "Xen_ComputerSystemTemplate", NULL);
    char buf[MAX_INSTANCEID_LEN+1];
    vssd_create_instance_id(session, vm_rec, buf, sizeof(buf));
    CMAddKey(op, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    return op;
}

