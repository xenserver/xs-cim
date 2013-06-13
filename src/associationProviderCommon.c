// Copyright (C) 2006 Novell, Inc.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
// ============================================================================
// Authors:       Jim Fehlig, <jfehlig@novell.com>
// Description:
// ============================================================================

#include <string.h>

/* Include the required CMPI data types, function headers, and macros */
#include "cmpidt.h"
#include "cmpift.h"
#include "cmpimacs.h"
#include "RASDs.h"

/* Include utility functions */
#include "cmpiutil.h"
#include "provider_common.h"

/* Include _SBLIM_TRACE() logging support */
#include "cmpitrace.h"


// ----------------------------------------------------------------------------
// COMMON GLOBAL VARIABLES
// ----------------------------------------------------------------------------
/* Handle to the CIM broker. Initialized when the provider lib is loaded. */
static const CMPIBroker *_BROKER;

// ============================================================================
// CMPI ASSOCIATION PROVIDER FUNCTION TABLE
// ============================================================================
// ----------------------------------------------------------------------------
// Info for the class supported by the association provider
// ----------------------------------------------------------------------------

typedef CMPIrc (*_set_association_properties_func)(const CMPIInstance *assoc_inst);

/* Name of the left and right hand side classes of this association. */
typedef struct _association_class_elements{
    char *assocclass;                             /* Name of the association class */
    char *baseclass;                              /* Name of the association base class */
    char *lhsclass;                               /* Name of the LHS class in the association */
    char *lhsnamespace;                           /* Name of the namespace the LHS class instance  */
    char *rhsclass;                               /* Name of the RHS class in the association */
    char *rhsnamespace;                           /* Name of the namespace the RHS class instance  */
    char *lhspropertyname;                        /* Name of the LHS association property  */
    char *rhspropertyname;                        /* Name of the RHS association property  */
    char *lhskeyname;                             /* the key property from the LHS class */
    _CMPIKeyValExtractFunc_t lhskey_extract_func; /* function to extract the key property from the LHS class */
    char *rhskeyname;                             /* the key property from the RHS class */
    _CMPIKeyValExtractFunc_t rhskey_extract_func; /* function to extract the key property from the RHS class */
    _set_association_properties_func set_properties;   /* funcion to set properties of the association class, if any */
}association_class_info;

typedef struct _association_class_info_set{
    association_class_info *contents;
    int size;
}association_class_info_set;


CMPIrc Set_ElementCapabilitiesProperties(
    const CMPIInstance *assoc_inst
    )
{
    int value = 2;
    CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&value, CMPI_uint16);
    CMSetProperty(assoc_inst, "Characteristics", (CMPIValue *)&arr, CMPI_uint16A);
    return CMPI_RC_OK;
}

/* Static table of all associations we are aware of */
association_class_info g_assoc_table[] = {
    /* Elements conforming to profile  */
    {"Xen_ElementConformsToSystemVirtualizationProfile", "CIM_ElementConformsToProfile", 
        "Xen_RegisteredSystemVirtualizationProfile", INTEROP_NS, "Xen_HostComputerSystem", DEFAULT_NS,
        "ConformantStandard", "ManagedElement", NULL, NULL, NULL, NULL, NULL},
    {"Xen_ElementConformsToVirtualSystemProfile", "CIM_ElementConformsToProfile", 
        "Xen_RegisteredVirtualSystemProfile", INTEROP_NS, "Xen_ComputerSystem", DEFAULT_NS,
        "ConformantStandard", "ManagedElement", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ElementConformsToStorageVirtualizationProfile", "CIM_ElementConformsToProfile",
        "Xen_RegisteredStorageVirtualizationProfile", INTEROP_NS, "Xen_HostComputerSystem", DEFAULT_NS,
        "ConformantStandard", "ManagedElement", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ElementConformsToEthernetPortVirtualizationProfile", "CIM_ElementConformsToProfile",
        "Xen_RegisteredEthernetPortVirtualizationProfile", INTEROP_NS, "Xen_VirtualSwitch", DEFAULT_NS,
        "ConformantStandard", "ManagedElement", NULL, NULL, NULL, NULL, NULL},

     /* Profile to pool associations */
     {"Xen_ResourcePoolConformsToProcessorAllocationProfile", "CIM_ElementConformsToProfile",
        "Xen_RegisteredProcessorAllocationProfile", INTEROP_NS, "Xen_ProcessorPool", DEFAULT_NS,
        "ConformantStandard", "ManagedElement", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ResourcePoolConformsToMemoryAllocationProfile", "CIM_ElementConformsToProfile",
        "Xen_RegisteredMemoryAllocationProfile", INTEROP_NS, "Xen_MemoryPool", DEFAULT_NS,
        "ConformantStandard", "ManagedElement", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ResourcePoolConformsToStorageVirtualizationProfile", "CIM_ElementConformsToProfile",
        "Xen_RegisteredStorageVirtualizationProfile", INTEROP_NS, "Xen_StoragePool", DEFAULT_NS,
        "ConformantStandard", "ManagedElement", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ResourcePoolConformsToEthernetPortVirtualizationProfile", "CIM_ElementConformsToProfile",
        "Xen_RegisteredEthernetPortVirtualizationProfile", INTEROP_NS, "Xen_NetworkConnectionPool", DEFAULT_NS,
        "ConformantStandard", "ManagedElement", NULL, NULL, NULL, NULL, NULL},

     /* Referenced Profiles */
     {"Xen_ReferencedProcessorAllocationProfile", "CIM_ReferencedProfile",
        "Xen_RegisteredSystemVirtualizationProfile", INTEROP_NS, "Xen_RegisteredProcessorAllocationProfile", INTEROP_NS,
        "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ReferencedMemoryAllocationProfile", "CIM_ReferencedProfile",
        "Xen_RegisteredSystemVirtualizationProfile", INTEROP_NS, "Xen_RegisteredMemoryAllocationProfile", INTEROP_NS,
        "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ReferencedGenericDeviceAllocationProfile", "CIM_ReferencedProfile",
        "Xen_RegisteredSystemVirtualizationProfile", INTEROP_NS, "Xen_RegisteredGenericDeviceAllocationProfile", INTEROP_NS,
        "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ReferencedStorageVirtualizationProfile", "CIM_ReferencedProfile",
        "Xen_RegisteredSystemVirtualizationProfile", INTEROP_NS, "Xen_RegisteredStorageVirtualizationProfile", INTEROP_NS,
        "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
     {"Xen_ReferencedEthernetPortVirtualizationProfile", "CIM_ReferencedProfile",
        "Xen_RegisteredSystemVirtualizationProfile", INTEROP_NS, "Xen_RegisteredEthernetPortVirtualizationProfile", INTEROP_NS,
        "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},

    /* Host to VM */
    {"Xen_HostedComputerSystem", "CIM_HostedDependency", 
        "Xen_ComputerSystem", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS,
        "Dependent", "Antecedent", "Host", strncpy, "Name", strncpy, NULL},

    /* Hosted Services */
    {"Xen_HostedVirtualSystemManagementService", "CIM_HostedService", 
        "Xen_VirtualSystemManagementService", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
        "Dependent", "Antecedent", NULL, NULL, NULL, NULL, NULL},
    {"Xen_HostedVirtualSystemMigrationService", "CIM_HostedService", 
        "Xen_VirtualSystemMigrationService", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
        "Dependent", "Antecedent", NULL, NULL, NULL, NULL, NULL},
    {"Xen_HostedVirtualSystemSnapshotService", "CIM_HostedService", 
        "Xen_VirtualSystemSnapshotService", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
        "Dependent", "Antecedent", NULL, NULL, NULL, NULL, NULL},

    /* Services that affect VMs */
    {"Xen_VirtualSystemManagementServiceAffectsComputerSystem", "CIM_ServiceAffectsElement", 
        "Xen_VirtualSystemManagementService", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
        "AffectingElement", "AffectedElement", NULL, NULL, NULL, NULL, NULL},
    {"Xen_VirtualSystemMigrationServiceAffectsComputerSystem", "CIM_ServiceAffectsElement", 
        "Xen_VirtualSystemMigrationService", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
        "AffectingElement", "AffectedElement", NULL, NULL, NULL, NULL, NULL},

    /* VM to VM device associations */
   {"Xen_ComputerSystemMemory", "CIM_SystemDevice", 
       "Xen_Memory", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},
   {"Xen_ComputerSystemDisk", "CIM_SystemDevice", 
       "Xen_Disk", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},
   {"Xen_ComputerSystemDiskDrive", "CIM_SystemDevice", 
       "Xen_DiskDrive", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},
   {"Xen_ComputerSystemNetworkPort", "CIM_SystemDevice", 
       "Xen_NetworkPort", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},
   {"Xen_ComputerSystemProcessor", "CIM_SystemDevice", 
       "Xen_Processor", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},
   {"Xen_ComputerSystemConsole", "CIM_SystemDevice", 
       "Xen_Console", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},

   /* Host to host device associations */
   {"Xen_HostComputerSystemMemory", "CIM_SystemDevice", 
       "Xen_HostMemory", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},
   /*{"Xen_HostComputerSystemDiskImage", "CIM_SystemDevice", 
       "Xen_DiskImage", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", strncpy, "Name", strncpy, NULL},
   {"Xen_HostComputerSystemNetworkPort", "CIM_SystemDevice", 
       "Xen_HostNetworkPort", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", strncpy, "Name", strncpy, NULL},*/
   {"Xen_HostComputerSystemProcessor", "CIM_SystemDevice", 
       "Xen_HostProcessor", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},

   /* Virtual switch to network port */
   {"Xen_VirtualSwitchVirtualSwitchPort", "CIM_SystemDevice", 
       "Xen_VirtualSwitchPort", DEFAULT_NS, "Xen_VirtualSwitch", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},
   /* Connection between two LAN endpoints */
   {"Xen_ActiveConnection", "CIM_ActiveConnection", 
       "Xen_ComputerSystemLANEndpoint", DEFAULT_NS, "Xen_VirtualSwitchLANEndpoint", DEFAULT_NS, 
       "Antecedent", "Dependent", "Name", strncpy, "Name", strncpy, NULL},
   {"Xen_VirtualSwitchPortSAPImplementation", "CIM_DeviceSAPImplementation", 
       "Xen_VirtualSwitchPort", DEFAULT_NS, "Xen_VirtualSwitchLANEndpoint", DEFAULT_NS, 
       "Antecedent", "Dependent", "DeviceID", _CMPIStrncpyDeviceNameFromID, "Name", strncpy, NULL},
   {"Xen_NetworkPortSAPImplementation", "CIM_DeviceSAPImplementation", 
       "Xen_NetworkPort", DEFAULT_NS, "Xen_ComputerSystemLANEndpoint", DEFAULT_NS, 
       "Antecedent", "Dependent", "DeviceID", _CMPIStrncpyDeviceNameFromID, "Name", strncpy, NULL},

   /* Extended VM Settings */
   {"Xen_ComputerSystemSettingsDefineState", "CIM_SettingsDefineState", 
       "Xen_ComputerSystemSettingData", DEFAULT_NS, "Xen_ComputerSystem", DEFAULT_NS, 
       "SettingData", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, NULL},
   /* VSSD from VSSD */
   {"Xen_ComputerSystemElementSettingData", "CIM_ElementSettingData", 
       "Xen_ComputerSystemSettingData", DEFAULT_NS, "Xen_ComputerSystemSettingData", DEFAULT_NS, 
        "SettingData", "ManagedElement", "InstanceID", strncpy, "InstanceID", strncpy, NULL},

   /* RASD from VSSD */
   {"Xen_MemorySettingDataComponent", "CIM_VirtualSystemSettingDataComponent", 
       "Xen_MemorySettingData", DEFAULT_NS, "Xen_VirtualSystemSettingData", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_ProcessorSettingDataComponent", "CIM_VirtualSystemSettingDataComponent", 
       "Xen_ProcessorSettingData", DEFAULT_NS, "Xen_VirtualSystemSettingData", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_DiskSettingDataComponent", "CIM_VirtualSystemSettingDataComponent", 
       "Xen_DiskSettingData", DEFAULT_NS, "Xen_VirtualSystemSettingData", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_NetworkPortSettingDataComponent", "CIM_VirtualSystemSettingDataComponent", 
       "Xen_NetworkPortSettingData", DEFAULT_NS, "Xen_VirtualSystemSettingData", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_ConsoleSettingDataComponent", "CIM_VirtualSystemSettingDataComponent", 
       "Xen_ConsoleSettingData", DEFAULT_NS, "Xen_VirtualSystemSettingData", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},

   /* Host Device to VM Device association */
  {"Xen_HostedDisk", "CIM_HostedDependency", 
       "Xen_Disk", DEFAULT_NS, "Xen_DiskImage", DEFAULT_NS, 
       "Dependent", "Antecedent", NULL, NULL, NULL, NULL, NULL},
  {"Xen_HostedDiskDrive", "CIM_HostedDependency", 
       "Xen_DiskDrive", DEFAULT_NS, "Xen_DiskImage", DEFAULT_NS, 
       "Dependent", "Antecedent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_HostedMemory", "CIM_HostedDependency", 
       "Xen_Memory", DEFAULT_NS, "Xen_HostMemory", DEFAULT_NS, 
       "Dependent", "Antecedent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_HostedNetworkPort", "CIM_HostedDependency", 
       "Xen_NetworkPort", DEFAULT_NS, "Xen_VirtualSwitchPort", DEFAULT_NS, 
       "Dependent", "Antecedent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_HostedProcessor", "CIM_HostedDependency", 
       "Xen_Processor", DEFAULT_NS, "Xen_HostProcessor", DEFAULT_NS, 
       "Dependent", "Antecedent", NULL, NULL, NULL, NULL, NULL},

   /* Host to Resource Pool */
   {"Xen_HostedMemoryPool", "CIM_HostedResourcePool", 
       "Xen_HostComputerSystem", DEFAULT_NS, "Xen_MemoryPool", DEFAULT_NS, 
       "GroupComponent", "PartComponent", "Name", strncpy, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_HostedProcessorPool", "CIM_HostedResourcePool", 
       "Xen_HostComputerSystem", DEFAULT_NS, "Xen_ProcessorPool", DEFAULT_NS, 
       "GroupComponent", "PartComponent", "Name", strncpy, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_HostedStoragePool", "CIM_HostedResourcePool", 
       "Xen_HostComputerSystem", DEFAULT_NS, "Xen_StoragePool", DEFAULT_NS, 
       "GroupComponent", "PartComponent", "Name", strncpy, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_HostedNetworkConnectionPool", "CIM_HostedResourcePool", 
       "Xen_VirtualSwitch", DEFAULT_NS, "Xen_NetworkConnectionPool", DEFAULT_NS, 
       "GroupComponent", "PartComponent", "Name", strncpy, "InstanceID", _CMPIStrncpyDeviceNameFromID, NULL},

   /* Resource pool capabilities */
   {"Xen_MemoryPoolAllocationCapabilities", "CIM_ElementCapabilities", 
       "Xen_MemoryAllocationCapabilities", DEFAULT_NS, "Xen_MemoryPool", DEFAULT_NS, 
       "Capabilities", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, Set_ElementCapabilitiesProperties},
   {"Xen_ProcessorPoolAllocationCapabilities", "CIM_ElementCapabilities", 
       "Xen_ProcessorAllocationCapabilities", DEFAULT_NS, "Xen_ProcessorPool", DEFAULT_NS, 
       "Capabilities", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, Set_ElementCapabilitiesProperties},
   {"Xen_StoragePoolAllocationCapabilities", "CIM_ElementCapabilities",
       "Xen_StorageAllocationCapabilities", DEFAULT_NS, "Xen_StoragePool", DEFAULT_NS,
       "Capabilities", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpyDeviceNameFromID, Set_ElementCapabilitiesProperties},
   {"Xen_NetworkConnectionPoolAllocationCapabilities", "CIM_ElementCapabilities",
       "Xen_NetworkConnectionAllocationCapabilities", DEFAULT_NS, "Xen_NetworkConnectionPool", DEFAULT_NS,
       "Capabilities", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpyDeviceNameFromID, Set_ElementCapabilitiesProperties},
   /* Host allocation capabilities */
   {"Xen_HostMemoryAllocationCapabilities", "CIM_ElementCapabilities", 
       "Xen_MemoryAllocationCapabilities", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
       "Capabilities", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, Set_ElementCapabilitiesProperties},
   {"Xen_HostProcessorAllocationCapabilities", "CIM_ElementCapabilities", 
       "Xen_ProcessorAllocationCapabilities", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS, 
       "Capabilities", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, Set_ElementCapabilitiesProperties},
   {"Xen_HostStorageAllocationCapabilities", "CIM_ElementCapabilities",
       "Xen_StorageAllocationCapabilities", DEFAULT_NS, "Xen_HostComputerSystem", DEFAULT_NS,
       "Capabilities", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, Set_ElementCapabilitiesProperties},
   {"Xen_VirtualSwitchPortAllocationCapabilities", "CIM_ElementCapabilities",
       "Xen_NetworkConnectionAllocationCapabilities", DEFAULT_NS, "Xen_VirtualSwitch", DEFAULT_NS,
       "Capabilities", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "Name", strncpy, Set_ElementCapabilitiesProperties},

   /* Resource pool to resource-pool-allocation associations */
   {"Xen_MemoryAllocatedFromPool", "CIM_ElementAllocatedFromPool", 
       "Xen_MemoryPool", DEFAULT_NS, "Xen_Memory", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MemorySettingAllocationFromPool", "CIM_ResourceAllocationFromPool", 
       "Xen_MemoryPool", DEFAULT_NS, "Xen_MemorySettingData", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MemorySettingsDefineState", "CIM_SettingsDefineState", 
       "Xen_MemorySettingData", DEFAULT_NS, "Xen_Memory", DEFAULT_NS, 
       "SettingData", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "SystemName", _CMPIStrncpyDeviceNameFromID, NULL},
  {"Xen_ProcessorAllocatedFromPool", "CIM_ElementAllocatedFromPool", 
       "Xen_ProcessorPool", DEFAULT_NS, "Xen_Processor", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_ProcessorSettingAllocationFromPool", "CIM_ResourceAllocationFromPool", 
       "Xen_ProcessorPool", DEFAULT_NS, "Xen_ProcessorSettingData", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_ProcessorSettingsDefineState", "CIM_SettingsDefineState", 
       "Xen_ProcessorSettingData", DEFAULT_NS, "Xen_Processor",DEFAULT_NS, 
       "SettingData", "ManagedElement", "InstanceID", _CMPIStrncpySystemNameFromID, "SystemName", _CMPIStrncpyDeviceNameFromID, NULL},
   {"Xen_NetworkPortAllocatedFromPool", "CIM_ElementAllocatedFromPool", 
       "Xen_NetworkConnectionPool", DEFAULT_NS, "Xen_NetworkPort", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_NetworkPortSettingAllocationFromPool", "CIM_ResourceAllocationFromPool", 
       "Xen_NetworkConnectionPool", DEFAULT_NS, "Xen_NetworkPortSettingData", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_NetworkPortSettingsDefineState", "CIM_SettingsDefineState", 
       "Xen_NetworkPortSettingData", DEFAULT_NS, "Xen_NetworkPort", DEFAULT_NS, 
       "SettingData", "ManagedElement", "InstanceID", _CMPIStrncpyDeviceNameFromID, "DeviceID", _CMPIStrncpyDeviceNameFromID, NULL},
   {"Xen_DiskAllocatedFromPool", "CIM_ElementAllocatedFromPool", 
       "Xen_StoragePool", DEFAULT_NS, "Xen_Disk", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_DiskDriveAllocatedFromPool", "CIM_ElementAllocatedFromPool",
       "Xen_StoragePool", DEFAULT_NS, "Xen_DiskDrive", DEFAULT_NS,
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_DiskSettingAllocationFromPool", "CIM_ResourceAllocationFromPool", 
       "Xen_StoragePool", DEFAULT_NS, "Xen_DiskSettingData", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_DiskSettingsDefineState", "CIM_SettingsDefineState", 
       "Xen_DiskSettingData", DEFAULT_NS, "Xen_Disk", DEFAULT_NS, 
       "SettingData", "ManagedElement", "InstanceID", _CMPIStrncpyDeviceNameFromID, "DeviceID", _CMPIStrncpyDeviceNameFromID, NULL},
   {"Xen_DiskDriveSettingsDefineState", "CIM_SettingsDefineState",
       "Xen_DiskSettingData", DEFAULT_NS, "Xen_DiskDrive", DEFAULT_NS,
       "SettingData", "ManagedElement", "InstanceID", _CMPIStrncpyDeviceNameFromID, "DeviceID", _CMPIStrncpyDeviceNameFromID, NULL},

   /* Resource pool to host resource association */
   {"Xen_MemoryPoolComponent", "CIM_ConcreteComponent", 
       "Xen_HostMemory", DEFAULT_NS, "Xen_MemoryPool", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_ProcessorPoolComponent", "CIM_ConcreteComponent", 
       "Xen_HostProcessor", DEFAULT_NS, "Xen_ProcessorPool", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_StoragePoolComponent", "CIM_ConcreteComponent", 
       "Xen_DiskImage", DEFAULT_NS, "Xen_StoragePool", DEFAULT_NS, 
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpyDeviceNameFromID, NULL},
   {"Xen_StoragePoolDiskDriveComponent", "CIM_ConcreteComponent",
       "Xen_HostDiskDrive", DEFAULT_NS, "Xen_StoragePool", DEFAULT_NS,
       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},

   /* Resource Capabilities */
   {"Xen_StorageSettingsDefineCapabilities", "CIM_SettingsDefineCapabilities",
       "Xen_StorageCapabilitiesSettingData", DEFAULT_NS, "Xen_StorageAllocationCapabilities", DEFAULT_NS,
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_MemorySettingsDefineCapabilities", "CIM_SettingsDefineCapabilities",
       "Xen_MemoryCapabilitiesSettingData", DEFAULT_NS, "Xen_MemoryAllocationCapabilities", DEFAULT_NS,
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_ProcessorSettingsDefineCapabilities", "CIM_SettingsDefineCapabilities",
       "Xen_ProcessorCapabilitiesSettingData", DEFAULT_NS, "Xen_ProcessorAllocationCapabilities", DEFAULT_NS,
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},
   {"Xen_NetworkConnectionSettingsDefineCapabilities", "CIM_SettingsDefineCapabilities",
       "Xen_NetworkConnectionCapabilitiesSettingData", DEFAULT_NS, "Xen_NetworkConnectionAllocationCapabilities", DEFAULT_NS,
       "PartComponent", "GroupComponent", "InstanceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},

// There is no relationship between host network port and network connection pool  
// {"Xen_NetworkConnectionPoolComponent", "CIM_ConcreteComponent", 
//      "Xen_HostNetworkPort", DEFAULT_NS, "Xen_NetworkConnectionPool", DEFAULT_NS, 
//       "PartComponent", "GroupComponent", "DeviceID", _CMPIStrncpySystemNameFromID, "InstanceID", _CMPIStrncpySystemNameFromID, NULL},

   /* Capabilities for various classes */
   /* VM capabilities (what VM state transitions are supported) info from VM object */
   {"Xen_ComputerSystemElementCapabilities", "CIM_ElementCapabilities", 
       "Xen_ComputerSystem", DEFAULT_NS, "Xen_ComputerSystemCapabilities", DEFAULT_NS, 
       "ManagedElement", "Capabilities","Name", strncpy, "InstanceID", _CMPIStrncpySystemNameFromID, Set_ElementCapabilitiesProperties},
   /* Virtualization capabilities of The Xen Host (if it is virtualizable or not) system */
   {"Xen_HasVirtualizationCapabilities", "CIM_ElementCapabilities", 
       "Xen_VirtualizationCapabilities", DEFAULT_NS, "Xen_HostComputerSystem",  DEFAULT_NS, 
        "Capabilities","ManagedElement", NULL, NULL, NULL, NULL, Set_ElementCapabilitiesProperties},
   {"Xen_VirtualSystemMigrationServiceCapabilities", "CIM_ElementCapabilities", 
       "Xen_VirtualSystemMigrationService", DEFAULT_NS, "Xen_VirtualSystemMigrationCapabilities", DEFAULT_NS, 
       "ManagedElement", "Capabilities", NULL, NULL, NULL, NULL, Set_ElementCapabilitiesProperties},
   /* VSMS capabilities (what methods/indications are supported) of a host's VSMS */
   {"Xen_VirtualSystemManagementServiceCapabilities", "CIM_ElementCapabilities", 
       "Xen_VirtualSystemManagementService", DEFAULT_NS, "Xen_VirtualSystemManagementCapabilities", DEFAULT_NS, 
       "ManagedElement", "Capabilities", NULL, NULL, NULL, NULL, Set_ElementCapabilitiesProperties},
   {"Xen_HostVSMSElementCapabilities", "CIM_ElementCapabilities", 
       "Xen_HostComputerSystem", DEFAULT_NS, "Xen_VirtualSystemManagementCapabilities", DEFAULT_NS, 
       "ManagedElement", "Capabilities", NULL, NULL, NULL, NULL, Set_ElementCapabilitiesProperties},

   /* Performance Metrics */
   {"Xen_MetricDefForHostProcessor", "CIM_MetricDefForME", 
       "Xen_HostProcessor", DEFAULT_NS, "Xen_HostProcessorMetricDefinition", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MetricForHostProcessor", "CIM_MetricForME", 
       "Xen_HostProcessor", DEFAULT_NS, "Xen_HostProcessorMetricValue", DEFAULT_NS, 
       "Antecedent", "Dependent", "DeviceID", _CMPIStrncpyDeviceNameFromID, "InstanceID", _CMPIStrncpyDeviceNameFromID, NULL},
   {"Xen_MetricInstanceHostProcessor", "CIM_MetricInstance", 
       "Xen_HostProcessorMetricDefinition", DEFAULT_NS, "Xen_HostProcessorMetricValue", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MetricDefForVirtualProcessor", "CIM_MetricDefForME", 
       "Xen_Processor", DEFAULT_NS, "Xen_ProcessorMetricDefinition", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MetricForVirtualProcessor", "CIM_MetricForME", 
       "Xen_Processor", DEFAULT_NS, "Xen_ProcessorMetricValue", DEFAULT_NS, 
       "Antecedent", "Dependent", "DeviceID", _CMPIStrncpyDeviceNameFromID, "InstanceID", _CMPIStrncpyDeviceNameFromID, NULL},
   {"Xen_MetricInstanceVirtualProcessor", "CIM_MetricInstance", 
       "Xen_ProcessorMetricDefinition", DEFAULT_NS, "Xen_ProcessorMetricValue", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MetricDefForDisk", "CIM_MetricDefForME", 
       "Xen_Disk", DEFAULT_NS, "Xen_DiskMetricDefinition", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MetricForDisk", "CIM_MetricForME", 
       "Xen_Disk", DEFAULT_NS, "Xen_DiskMetricValue", DEFAULT_NS, 
       "Antecedent", "Dependent", "DeviceID", _CMPIStrncpyDeviceNameFromID, "InstanceID", _CMPIStrncpyDeviceNameFromID, NULL},
   {"Xen_MetricInstanceDisk", "CIM_MetricInstance", 
       "Xen_DiskMetricDefinition", DEFAULT_NS, "Xen_DiskMetricValue", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MetricDefForNetworkPort", "CIM_MetricDefForME", 
       "Xen_NetworkPort", DEFAULT_NS, "Xen_NetworkPortMetricDefinition", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL},
   {"Xen_MetricForNetworkPort", "CIM_MetricForME", 
       "Xen_NetworkPort", DEFAULT_NS, "Xen_NetworkPortMetricValue", DEFAULT_NS, 
       "Antecedent", "Dependent", "DeviceID", _CMPIStrncpyDeviceNameFromID, "InstanceID", _CMPIStrncpyDeviceNameFromID, NULL},
   {"Xen_MetricInstanceNetworkPort", "CIM_MetricInstance", 
       "Xen_NetworkPortMetricDefinition", DEFAULT_NS, "Xen_NetworkPortMetricValue", DEFAULT_NS, 
       "Antecedent", "Dependent", NULL, NULL, NULL, NULL, NULL}
   };


association_class_info_set FindAssociationClasses(
    const char * assoc_class_name, 
    const char *ns
    )
{
    int i;
    association_class_info_set assoc_set;
    int size = 0;

    /* find the association class elements based on the name passed in  
     * the name passed in could be a base class */
    assoc_set.contents = NULL;
    assoc_set.size = 0;
    for(i=0; i<(sizeof(g_assoc_table)/sizeof(g_assoc_table[0])); i++) 
    {
        CMPIObjectPath * assocOP = CMNewObjectPath(_BROKER, ns, g_assoc_table[i].assocclass, NULL);
        if (CMClassPathIsA(_BROKER, assocOP, assoc_class_name, NULL)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- found=\"%s\" for %s", g_assoc_table[i].assocclass, assoc_class_name));
            assoc_set.contents = realloc(assoc_set.contents, (size+1) * sizeof(association_class_info));
            assoc_set.size = ++size;
            assoc_set.contents[size-1] = g_assoc_table[i];
        } 
    }
    return assoc_set;
}

static CMPIStatus _AssociationRoutine(
        CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,/* [in] Additional context info, if any. */
		const CMPIResult * results,	/* [out] Results of this operation. */
		const CMPIObjectPath * reference, /* [in] Contains source namespace, classname and object path. */
		const char * assocClass,
		const char * resultClass,
		const char * role,
		const char * resultRole,
        int refsOnly)
{
    CMPIStatus status = { CMPI_RC_OK, NULL };    /* Return status of CIM operations. */
    char *nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    char *sourceclass = CMGetCharPtr(CMGetClassName(reference, &status)); /* Class of the source reference object */
    char *targetclass = NULL; 				/* Class of the target object(s). */
    char *targetnamespace = NULL; 			/* Class Namespace of the target object(s). */
    char sourcename[MAX_SYSTEM_NAME_LEN];
    char resultname[MAX_SYSTEM_NAME_LEN];
    char *sourcekeyname = NULL;
    char *targetkeyname = NULL;
    _CMPIKeyValExtractFunc_t sourcekeyfunc;
    _CMPIKeyValExtractFunc_t targetkeyfunc;
    CMPIData namedata;

    association_class_info_set associations;
    /* Initialise contents as may be check on exit */
    associations.contents = NULL;

    _SBLIM_ENTER("AssociatorNames");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- assocClass=\"%s\"", assocClass));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- resultClass=\"%s\"", resultClass));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- role=\"%s\"", role));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- resultRole=\"%s\"", resultRole));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- sourceclass=\"%s\"", sourceclass));

    /* source class object path */
    CMPIObjectPath * srcclassop = CMNewObjectPath(_BROKER, nameSpace, sourceclass, NULL);
    CMPIInstance *referencedInstance = CBGetInstance(_BROKER,  context, reference, NULL, &status);
    if(referencedInstance == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Could not find referencedInstance"));
        goto exit;
    }

    /* Find association class to work with */
    associations = FindAssociationClasses(assocClass, nameSpace);
    if(associations.size == 0) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                     ("--- Unrecognized association %s. Ignoring request.", assocClass));
        goto exit;
    }

    /* Check that the reference matches the required role, if any. */
    if ((role != NULL) && strcmp(role, sourceclass) != 0) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                     ("--- Reference does not match required role. Ignoring request."));
        goto exit;
    }

    int i=0;
    for (i=0; i<associations.size; i++) {
        association_class_info *association = &associations.contents[i];
        CMPIObjectPath * lhsclassop = CMNewObjectPath(_BROKER, association->lhsnamespace, association->lhsclass, NULL);
        CMPIObjectPath * rhsclassop = CMNewObjectPath(_BROKER, association->rhsnamespace, association->rhsclass, NULL);

        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                         ("--- Checking class %s, with %d or %d. Ignoring request.",
                          sourceclass, association->lhsclass, association->rhsclass));

        /* Determine the target class from the source class. 
           Source class could be a base class or a derived class of the association LHS or RHS class*/
        if (CMClassPathIsA(_BROKER, lhsclassop, sourceclass, NULL) ||
            CMClassPathIsA(_BROKER, srcclassop, association->lhsclass, NULL)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                         ("--- source class %s matches %s", sourceclass, association->lhsclass));
            sourcekeyname = association->lhskeyname;
            targetclass = association->rhsclass;
            targetkeyname = association->rhskeyname;
            sourcekeyfunc = association->lhskey_extract_func;
            targetkeyfunc = association->rhskey_extract_func;
            targetnamespace = association->rhsnamespace;
        } else if (CMClassPathIsA(_BROKER, rhsclassop, sourceclass, NULL) || 
                   CMClassPathIsA(_BROKER, srcclassop, association->rhsclass, NULL)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                         ("--- source class %s matches %s", sourceclass, association->rhsclass));
            sourcekeyname = association->rhskeyname;
            targetclass = association->lhsclass;
            targetnamespace = association->lhsnamespace;
            targetkeyname = association->lhskeyname;
            sourcekeyfunc = association->rhskey_extract_func;
            targetkeyfunc = association->lhskey_extract_func;
        }
        else {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                         ("--- Unrecognized source class %s, didnt match %s or %s. Ignoring request.",
                          sourceclass, association->lhsclass, association->rhsclass));
            continue;
        }
    
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- targetclass=\"%s\" in namespace \"%s\"", targetclass, targetnamespace));
    
        if(sourcekeyname) {
            namedata = CMGetProperty(referencedInstance, sourcekeyname, NULL);
            if(namedata.value.string != NULL)
                sourcekeyfunc(sourcename, CMGetCharPtr(namedata.value.string), MAX_SYSTEM_NAME_LEN);
            else
                continue;
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- sourcekey=%s, sourcename=\"%s\"", CMGetCharPtr(namedata.value.string), sourcename));
        }
    
        /* Create an object path for the result class. */
        CMPIObjectPath * objectpath = CMNewObjectPath(_BROKER, targetnamespace, targetclass, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- CMNewObjectPath() failed - %s", CMGetCharPtr(status.msg)));
            CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIObjectPath");
            continue;
        }
    
        /* Get the list of all target class object instances from the providers. */
        CMPIEnumeration * enumeration = NULL;
        enumeration = CBEnumInstances(_BROKER, context, objectpath, NULL, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(enumeration)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                         ("--- CBEnumInstanceNames() failed - %s", CMGetCharPtr(status.msg)));
            CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot enumerate target class");
            continue;
        }

        CMPIArray *arr = CMToArray(enumeration, &status);
        CMPICount cnt = CMGetArrayCount(arr, &status);
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Enumeration returned %d count", cnt));

        /* Return all object paths/objects (depending on what was requested)  
         * that exactly match the target class and resultClass, if specified. */
        while (CMHasNext(enumeration, NULL)) {
            CMPIData data = CMGetNext(enumeration, NULL);
            CMPIObjectPath *objectPath = NULL;
            char *classname = NULL;
    
            objectPath = CMGetObjectPath(data.value.inst,NULL);
            classname = CMGetCharPtr(CMGetClassName(objectPath, NULL));

            /* The class name should match the targetclass or should be a subclass*/
            if (CMClassPathIsA(_BROKER, objectPath, targetclass, NULL) && 
                ((resultClass == NULL) ||  CMClassPathIsA(_BROKER, objectPath, resultClass, NULL))) {
                /* Only return entries whose key property matches the reference's key. */
                if(targetkeyname) {
                    CMPIData keydata;
                    keydata = CMGetProperty(data.value.inst, targetkeyname, NULL);
                    if(!CMIsNullValue(namedata)) {
                        char *target_key = CMGetCharPtr(keydata.value.string);
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- target_key = %s ", target_key));
                        if(target_key && targetkeyfunc(resultname, 
                                                       target_key,
                                                       MAX_SYSTEM_NAME_LEN)){
                            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Comparing %s with %s", sourcename, resultname));
                            if (strcmp(sourcename, resultname) == 0) {
                                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Match!! adding to result"));
                                if (refsOnly)
                                    CMReturnObjectPath(results, objectPath);
                                else
                                    CMReturnInstance(results, data.value.inst);
                            }
                        }
                    }
                }
                else 
                   if (refsOnly)
                       CMReturnObjectPath(results, objectPath);
                   else
                       CMReturnInstance(results, data.value.inst);
            }
        }
    }
    CMReturnDone(results);

 exit:
     if(associations.contents)
         free(associations.contents);
    _SBLIM_RETURNSTATUS(status);

}

static CMPIStatus _ReferencesRoutine(
    CMPIAssociationMI * self,	        /* [in] Handle to this provider (i.e. 'self'). */
	const CMPIContext * context,		/* [in] Additional context info, if any. */
	const CMPIResult * results,		    /* [out] Results of this operation. */
	const CMPIObjectPath * reference,	/* [in] Contains the namespace, classname and desired object path. */
	const char *assocClass,
	const char *role,
	const char **properties,            /* [in] List of desired properties (NULL=all). */
    int keysOnly)		
{
    CMPIStatus status = { CMPI_RC_OK, NULL };    /* Return status of CIM operations. */
    char *nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    char *sourceclass = CMGetCharPtr(CMGetClassName(reference, &status)); /* Class of the source reference object */
    char *targetclass = NULL;                           /* Class of the target object(s). */
    char *targetnamespace = NULL;
    char sourcename[MAX_SYSTEM_NAME_LEN];
    char resultname[MAX_SYSTEM_NAME_LEN];
    char *sourcekeyname = NULL;
    char *targetkeyname = NULL;
    _CMPIKeyValExtractFunc_t sourcekeyfunc;
    _CMPIKeyValExtractFunc_t targetkeyfunc;
    CMPIData namedata;

    _SBLIM_ENTER("References");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- assocClass=\"%s\"", assocClass));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- role=\"%s\"", role));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- sourceclass=\"%s\"", sourceclass));

    CMPIObjectPath * srcclassop = CMNewObjectPath(_BROKER, nameSpace, sourceclass, NULL);
    CMPIInstance *referencedInstance = CBGetInstance(_BROKER,  context, reference, properties, &status);
    if(referencedInstance == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Could not find referencedInstance"));
        goto exit;
    }

    /* Get more information about the assoc class (its key, function that extracts that key etc) */
    association_class_info_set associations;
    associations = FindAssociationClasses(assocClass, nameSpace);
    if(associations.size == 0)  {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Unrecognized association %s. Ignoring request.", assocClass));
        goto exit;
    }

    /* Check that the reference matches the required role, if any. */
    if ((role != NULL) && strcmp(role, sourceclass) != 0) {
       _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Reference does not match required role. Ignoring request."));
       goto exit;
    }

    int i=0;
    for (i=0; i<associations.size; i++) {
        association_class_info *association = &associations.contents[i];
        CMPIObjectPath * lhsclassop = CMNewObjectPath(_BROKER, association->lhsnamespace, association->lhsclass, NULL);
        CMPIObjectPath * rhsclassop = CMNewObjectPath(_BROKER, association->rhsnamespace, association->rhsclass, NULL);

        /* Determine the target class from the source class. */
        if (CMClassPathIsA(_BROKER, lhsclassop, sourceclass, NULL) ||
            CMClassPathIsA(_BROKER, srcclassop, association->lhsclass, NULL)) {
            sourcekeyname = association->lhskeyname;
            targetclass = association->rhsclass;
            targetnamespace = association->rhsnamespace;
            targetkeyname = association->rhskeyname;
            sourcekeyfunc = association->lhskey_extract_func;
            targetkeyfunc = association->rhskey_extract_func;
        } if (CMClassPathIsA(_BROKER, rhsclassop, sourceclass, NULL) ||
              CMClassPathIsA(_BROKER, srcclassop, association->rhsclass, NULL)) {
            sourcekeyname = association->rhskeyname;
            targetclass = association->lhsclass;
            targetnamespace = association->lhsnamespace;
            targetkeyname = association->lhskeyname;
            sourcekeyfunc = association->rhskey_extract_func;
            targetkeyfunc = association->lhskey_extract_func;
        } else {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                         ("--- Unrecognized source class %s. Didnt match %s or %s, Ignoring request.", 
                         sourceclass, association->lhsclass, association->rhsclass));
            continue;
        }
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- targetclass=\"%s\"", targetclass));

        if(sourcekeyname) {
            // namedata = CMGetKey(reference, sourcekeyname, NULL);
            namedata = CMGetProperty(referencedInstance, sourcekeyname, NULL);
            if(namedata.value.string != NULL)
                sourcekeyfunc(sourcename, CMGetCharPtr(namedata.value.string), MAX_SYSTEM_NAME_LEN);
            else continue;
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- sourcename=\"%s\"", sourcename));
        }
    
        /* Create an object path for the result class. */
        CMPIObjectPath * objectpath = CMNewObjectPath(_BROKER, targetnamespace, targetclass, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath)) {
           _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                        ("--- CMNewObjectPath() failed - %s", CMGetCharPtr(status.msg)));
           CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIObjectPath");
           continue;
        }
    
        /* Get the list of all target class object paths from the CIMOM. */
        CMPIEnumeration * objectpaths = CBEnumInstanceNames(_BROKER, context, objectpath, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpaths)) {
           _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                        ("--- CBEnumInstanceNames() failed - %s", CMGetCharPtr(status.msg)));
           CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot enumerate target class");
           continue;
        }
    
        /* Return all object paths that exactly match the target class and resultClass, if specified. */
        while (CMHasNext(objectpaths, NULL)) 
        {
            CMPIData data = CMGetNext(objectpaths, NULL);
            /* Create an object path for the association. */
            void *obj = NULL;
            if (keysOnly) 
                obj = (void *)CMNewObjectPath(_BROKER, nameSpace, association->assocclass, &status);
            else
                obj = (void *) _CMNewInstance(_BROKER, nameSpace, association->assocclass, &status);

            if ((status.rc != CMPI_RC_OK) || CMIsNullObject(obj)) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                             ("--- CMNewObjectPath() failed - %s", CMGetCharPtr(status.msg)));
                CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIObjectPath");
                continue;
            }

            /* Assign the references in the association appropriately. */
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("comparing %s with %s", sourceclass, association->rhsclass))
            if (strcmp(sourceclass, association->rhsclass) == 0) 
            {
                if (keysOnly) {
                    CMAddKey((CMPIObjectPath *)obj, association->rhspropertyname, &reference, CMPI_ref);
                    CMAddKey((CMPIObjectPath *)obj, association->lhspropertyname, &data.value.ref, CMPI_ref);
                } 
                else {
                     CMSetProperty((CMPIInstance *)obj, association->rhspropertyname, &reference, CMPI_ref);
                     CMSetProperty((CMPIInstance *)obj, association->lhspropertyname, &data.value.ref, CMPI_ref);
                     if(association->set_properties)
                         association->set_properties((CMPIInstance *) obj);
                }
            } 
            else {
                if (keysOnly) {
                    CMAddKey((CMPIObjectPath *)obj, association->rhspropertyname, &data.value.ref, CMPI_ref);
                    CMAddKey((CMPIObjectPath *)obj, association->lhspropertyname, &reference, CMPI_ref);
                } 
                else {
                     CMSetProperty((CMPIInstance *)obj, association->rhspropertyname, &data.value.ref, CMPI_ref);
                     CMSetProperty((CMPIInstance *)obj, association->lhspropertyname, &reference, CMPI_ref);
                     if(association->set_properties)
                         association->set_properties((CMPIInstance *) obj);
                }
            }

            /* Only return entries whose name matches the reference. */
            if(targetkeyname) {
                namedata = CMGetKey(data.value.ref, targetkeyname, &status);
                if(!CMIsNullValue(namedata) && (namedata.value.string != NULL)) {
                    char *target_key = CMGetCharPtr(namedata.value.string);
                    if (target_key && targetkeyfunc(resultname, 
                                                    target_key,
                                                    MAX_SYSTEM_NAME_LEN)) {
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                                     ("--- Comparing %s with %s", sourcename, resultname));
                        if (strcmp(sourcename, resultname) == 0) {
                            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Match!! adding to result"));
                            if(keysOnly)
                                CMReturnObjectPath(results, (CMPIObjectPath*)obj);
                            else
                                CMReturnInstance(results, (CMPIInstance*)obj);
                        }
                    }
                }
            }
            else 
                if (keysOnly) 
                 CMReturnObjectPath(results, (CMPIObjectPath*)obj);
                else
                 CMReturnInstance(results, (CMPIInstance*)obj);
        }
    }

exit:
   _SBLIM_RETURNSTATUS(status);

}
// ----------------------------------------------------------------------------
// AssociationCleanup()
// Perform any necessary cleanup immediately before this provider is unloaded.
// ----------------------------------------------------------------------------
static CMPIStatus AssociationCleanup(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,/* [in] Additional context info, if any. */
        CMPIBoolean terminating)   /* [in] True if MB is terminating */
{
   CMPIStatus status = { CMPI_RC_OK, NULL };	/* Return status of CIM operations. */

   _SBLIM_ENTER("AssociationCleanup");
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));

   /* Nothing needs to be done for cleanup. */
   _SBLIM_RETURNSTATUS(status);
}


// ----------------------------------------------------------------------------
// AssociatorNames()
// ----------------------------------------------------------------------------
static CMPIStatus AssociatorNames(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
		const CMPIResult * results,		/* [out] Results of this operation. */
		const CMPIObjectPath * reference,	/* [in] Contains source namespace, classname and object path. */
		const char * assocClass,
		const char * resultClass,
		const char * role,
		const char * resultRole)
{
    return _AssociationRoutine(self, context, results, reference, assocClass, resultClass, role, resultRole, 1);
}


// ----------------------------------------------------------------------------
// Associators()
// ----------------------------------------------------------------------------
static CMPIStatus Associators(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
		const CMPIResult * results,		/* [out] Results of this operation. */
		const CMPIObjectPath * reference,	/* [in] Contains the source namespace, classname and object path. */
		const char *assocClass,
		const char *resultClass,
		const char *role,
		const char *resultRole,
		const char ** properties)		/* [in] List of desired properties (NULL=all). */
{
    return _AssociationRoutine(self, context, results, reference, assocClass, resultClass, role, resultRole, 0);
}


// ----------------------------------------------------------------------------
// ReferenceNames()
// ----------------------------------------------------------------------------
static CMPIStatus ReferenceNames(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
		const CMPIResult * results,		/* [out] Results of this operation. */
		const CMPIObjectPath * reference,	/* [in] Contains the source namespace, classname and object path. */
		const char *assocClass, 
		const char *role)
{
    return _ReferencesRoutine(self,	context, results, reference, assocClass, role, NULL, 1);
}


// ----------------------------------------------------------------------------
// References()
// ----------------------------------------------------------------------------
static CMPIStatus References(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
		const CMPIResult * results,		/* [out] Results of this operation. */
		const CMPIObjectPath * reference,	/* [in] Contains the namespace, classname and desired object path. */
		const char *assocClass,
		const char *role,
		const char **properties)		/* [in] List of desired properties (NULL=all). */
{
    return _ReferencesRoutine(self,	context, results, reference, assocClass, role, properties, 0);

}


// ----------------------------------------------------------------------------
// AssociationInitialize()
// Perform any necessary initialization immediately after this provider is
// first loaded.
// ----------------------------------------------------------------------------
static void AssociationInitialize(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context)		/* [in] Additional context info, if any. */
{
   _SBLIM_ENTER("AssociationInitialize");
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
   //   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));

   /* Nothing needs to be done to initialize this provider */
   _SBLIM_RETURN();
}


// ============================================================================
// CMPI ASSOCIATION PROVIDER FUNCTION TABLE SETUP
// ============================================================================
CMAssociationMIStub( , Xen_associationProviderCommon, _BROKER, AssociationInitialize(&mi, ctx));

