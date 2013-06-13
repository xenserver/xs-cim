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

#ifndef __DMTF_ENUMS_H__
#define __DMTF_ENUMS_H__

/* Values for the various properties of the class */

/* Capabilities related classes */
typedef enum _DMTF_RequestTypesSupported{
    DMTF_RequestTypesSupported_Unknown=0,
    DMTF_RequestTypesSupported_Specific=2,
    DMTF_RequestTypesSupported_General=3,
    DMTF_RequestTypesSupported_Both=4,
    /*DMTF_RequestTypesSupported_DMTF_reserved=..,*/
    /*DMTF_RequestTypesSupported_Vendor_Reserved=0x8000..0xFFFF,*/
}DMTF_RequestTypesSupported;

typedef enum _DMTF_SharingMode{
    DMTF_SharingMode_Unknown=0,
    DMTF_SharingMode_Other=1,
    DMTF_SharingMode_Dedicated=2,
    DMTF_SharingMode_Shared=3,
    /*DMTF_SharingMode_DMTF_reserved=..,*/
    /*DMTF_SharingMode_Vendor_Reserved=0x8000..0xFFFF,*/
}DMTF_SharingMode;

typedef enum _DMTF_SupportedAddStates{
    DMTF_SupportedAddStates_Unknown=0,
    DMTF_SupportedAddStates_Other=1,
    DMTF_SupportedAddStates_Enabled=2,
    DMTF_SupportedAddStates_Disabled=3,
    DMTF_SupportedAddStates_Shutting_Down=4,
    DMTF_SupportedAddStates_Not_Applicable=5,
    DMTF_SupportedAddStates_Enabled_but_Offline=6,
    DMTF_SupportedAddStates_In_Test=7,
    DMTF_SupportedAddStates_Deferred=8,
    DMTF_SupportedAddStates_Quiesce=9,
    DMTF_SupportedAddStates_Starting=10,
    DMTF_SupportedAddStates_Paused=11,
    DMTF_SupportedAddStates_Suspended=12,
    /*DMTF_SupportedAddStates_DMTF_Reserved=..,*/
    /*DMTF_SupportedAddStates_Vendor_Reserved=0x8000..0xFFFF,*/
}DMTF_SupportedAddStates;

typedef enum _DMTF_SupportedRemoveStates{
    DMTF_SupportedRemoveStates_Unknown=0,
    DMTF_SupportedRemoveStates_Other=1,
    DMTF_SupportedRemoveStates_Enabled=2,
    DMTF_SupportedRemoveStates_Disabled=3,
    DMTF_SupportedRemoveStates_Shutting_Down=4,
    DMTF_SupportedRemoveStates_Not_Applicable=5,
    DMTF_SupportedRemoveStates_Enabled_but_Offline=6,
    DMTF_SupportedRemoveStates_In_Test=7,
    DMTF_SupportedRemoveStates_Deferred=8,
    DMTF_SupportedRemoveStates_Quiesce=9,
    DMTF_SupportedRemoveStates_Starting=10,
    DMTF_SupportedRemoveStates_Paused=11,
    DMTF_SupportedRemoveStates_Suspended=12,
    /*DMTF_SupportedRemoveStates_DMTF_Reserved=..,*/
    /*DMTF_SupportedRemoveStates_Vendor_Reserved=0x8000..0xFFFF,*/
}DMTF_SupportedRemoveStates;

typedef enum _DMTF_CreateGoalSettings{
    DMTF_CreateGoalSettings_Success=0,
    DMTF_CreateGoalSettings_Not_Supported=1,
    DMTF_CreateGoalSettings_Unknown=2,
    DMTF_CreateGoalSettings_Timeout=3,
    DMTF_CreateGoalSettings_Failed=4,
    DMTF_CreateGoalSettings_Invalid_Parameter=5,
    DMTF_CreateGoalSettings_Alternative_Proposed=6,
    /*DMTF_CreateGoalSettings_DMTF_Reserved=..,*/
    /*DMTF_CreateGoalSettings_Vendor_Specific=32768..65535,*/
}DMTF_CreateGoalSettings;

/* Device specific classes */
typedef enum _DMTF_AdditionalAvailability{
    DMTF_AdditionalAvailability_Other=1,
    DMTF_AdditionalAvailability_Unknown=2,
    DMTF_AdditionalAvailability_Running_Full_Power=3,
    DMTF_AdditionalAvailability_Warning=4,
    DMTF_AdditionalAvailability_In_Test=5,
    DMTF_AdditionalAvailability_Not_Applicable=6,
    DMTF_AdditionalAvailability_Power_Off=7,
    DMTF_AdditionalAvailability_Off_Line=8,
    DMTF_AdditionalAvailability_Off_Duty=9,
    DMTF_AdditionalAvailability_Degraded=10,
    DMTF_AdditionalAvailability_Not_Installed=11,
    DMTF_AdditionalAvailability_Install_Error=12,
    DMTF_AdditionalAvailability_Power_Save___Unknown=13,
    DMTF_AdditionalAvailability_Power_Save___Low_Power_Mode=14,
    DMTF_AdditionalAvailability_Power_Save___Standby=15,
    DMTF_AdditionalAvailability_Power_Cycle=16,
    DMTF_AdditionalAvailability_Power_Save___Warning=17,
    DMTF_AdditionalAvailability_Paused=18,
    DMTF_AdditionalAvailability_Not_Ready=19,
    DMTF_AdditionalAvailability_Not_Configured=20,
    DMTF_AdditionalAvailability_Quiesced=21,
}DMTF_AdditionalAvailability;

typedef enum _DMTF_Availability{
    DMTF_Availability_Other=1,
    DMTF_Availability_Unknown=2,
    DMTF_Availability_Running_Full_Power=3,
    DMTF_Availability_Warning=4,
    DMTF_Availability_In_Test=5,
    DMTF_Availability_Not_Applicable=6,
    DMTF_Availability_Power_Off=7,
    DMTF_Availability_Off_Line=8,
    DMTF_Availability_Off_Duty=9,
    DMTF_Availability_Degraded=10,
    DMTF_Availability_Not_Installed=11,
    DMTF_Availability_Install_Error=12,
    DMTF_Availability_Power_Save___Unknown=13,
    DMTF_Availability_Power_Save___Low_Power_Mode=14,
    DMTF_Availability_Power_Save___Standby=15,
    DMTF_Availability_Power_Cycle=16,
    DMTF_Availability_Power_Save___Warning=17,
    DMTF_Availability_Paused=18,
    DMTF_Availability_Not_Ready=19,
    DMTF_Availability_Not_Configured=20,
    DMTF_Availability_Quiesced=21,
}DMTF_Availability;

typedef enum _DMTF_AvailableRequestedStates{
    DMTF_AvailableRequestedStates_Enabled=2,
    DMTF_AvailableRequestedStates_Disabled=3,
    DMTF_AvailableRequestedStates_Shut_Down=4,
    DMTF_AvailableRequestedStates_Offline=6,
    DMTF_AvailableRequestedStates_Test=7,
    DMTF_AvailableRequestedStates_Defer=8,
    DMTF_AvailableRequestedStates_Quiesce=9,
    DMTF_AvailableRequestedStates_Reboot=10,
    DMTF_AvailableRequestedStates_Reset=11,
    /*DMTF_AvailableRequestedStates_DMTF_Reserved=..,*/
}DMTF_AvailableRequestedStates;

typedef enum _DMTF_CommunicationStatus{
    DMTF_CommunicationStatus_Unknown=0,
    DMTF_CommunicationStatus_Not_Available=1,
    DMTF_CommunicationStatus_Communication_OK=2,
    DMTF_CommunicationStatus_Lost_Communication=3,
    DMTF_CommunicationStatus_No_Contact=4,
    /*DMTF_CommunicationStatus_DMTF_Reserved=..,*/
    /*DMTF_CommunicationStatus_Vendor_Reserved=0x8000..,*/
}DMTF_CommunicationStatus;

typedef enum _DMTF_Dedicated{
    DMTF_Dedicated_Not_Dedicated=0,
    DMTF_Dedicated_Unknown=1,
    DMTF_Dedicated_Other=2,
    DMTF_Dedicated_Storage=3,
    DMTF_Dedicated_Router=4,
    DMTF_Dedicated_Switch=5,
    DMTF_Dedicated_Layer_3_Switch=6,
    DMTF_Dedicated_Central_Office_Switch=7,
    DMTF_Dedicated_Hub=8,
    DMTF_Dedicated_Access_Server=9,
    DMTF_Dedicated_Firewall=10,
    DMTF_Dedicated_Print=11,
    DMTF_Dedicated_I_O=12,
    DMTF_Dedicated_Web_Caching=13,
    DMTF_Dedicated_Management=14,
    DMTF_Dedicated_Block_Server=15,
    DMTF_Dedicated_File_Server=16,
    DMTF_Dedicated_Mobile_User_Device=17,
    DMTF_Dedicated_Repeater=18,
    DMTF_Dedicated_Bridge_Extender=19,
    DMTF_Dedicated_Gateway=20,
    DMTF_Dedicated_Storage_Virtualizer=21,
    DMTF_Dedicated_Media_Library=22,
    DMTF_Dedicated_ExtenderNode=23,
    DMTF_Dedicated_NAS_Head=24,
    DMTF_Dedicated_Self_contained_NAS=25,
    DMTF_Dedicated_UPS=26,
    DMTF_Dedicated_IP_Phone=27,
    DMTF_Dedicated_Management_Controller=28,
    DMTF_Dedicated_Chassis_Manager=29,
    DMTF_Dedicated_Host_based_RAID_controller=30,
    DMTF_Dedicated_Storage_Device_Enclosure=31,
    DMTF_Dedicated_Desktop=32,
    DMTF_Dedicated_Laptop=33,
    DMTF_Dedicated_Virtual_Tape_Library=34,
    DMTF_Dedicated_Virtual_Library_System=35,
    /*DMTF_Dedicated_DMTF_Reserved=36..32567,*/
    /*DMTF_Dedicated_Vendor_Reserved=32568..65535,*/
}DMTF_Dedicated;

typedef enum _DMTF_DetailedStatus{
    DMTF_DetailedStatus_Not_Available=0,
    DMTF_DetailedStatus_No_Additional_Information=1,
    DMTF_DetailedStatus_Stressed=2,
    DMTF_DetailedStatus_Predictive_Failure=3,
    DMTF_DetailedStatus_Non_Recoverable_Error=4,
    DMTF_DetailedStatus_Supporting_Entity_in_Error=5,
    /*DMTF_DetailedStatus_DMTF_Reserved=..,*/
    /*DMTF_DetailedStatus_Vendor_Reserved=0x8000..,*/
}DMTF_DetailedStatus;

typedef enum _DMTF_EnabledDefault{
    DMTF_EnabledDefault_Enabled=2,
    DMTF_EnabledDefault_Disabled=3,
    DMTF_EnabledDefault_Not_Applicable=5,
    DMTF_EnabledDefault_Enabled_but_Offline=6,
    DMTF_EnabledDefault_No_Default=7,
    DMTF_EnabledDefault_Quiesce=9,
    /*DMTF_EnabledDefault_DMTF_Reserved=..,*/
    /*DMTF_EnabledDefault_Vendor_Reserved=32768..65535,*/
}DMTF_EnabledDefault;

typedef enum _DMTF_EnabledState{
    DMTF_EnabledState_Unknown=0,
    DMTF_EnabledState_Other=1,
    DMTF_EnabledState_Enabled=2,
    DMTF_EnabledState_Disabled=3,
    DMTF_EnabledState_Shutting_Down=4,
    DMTF_EnabledState_Not_Applicable=5,
    DMTF_EnabledState_Enabled_but_Offline=6,
    DMTF_EnabledState_In_Test=7,
    DMTF_EnabledState_Deferred=8,
    DMTF_EnabledState_Quiesce=9,
    DMTF_EnabledState_Starting=10,
    /*DMTF_EnabledState_DMTF_Reserved=11..32767,*/
    /*DMTF_EnabledState_Vendor_Reserved=32768..65535,*/
}DMTF_EnabledState;

typedef enum _DMTF_HealthState{
    DMTF_HealthState_Unknown=0,
    DMTF_HealthState_OK=5,
    DMTF_HealthState_Degraded_Warning=10,
    DMTF_HealthState_Minor_failure=15,
    DMTF_HealthState_Major_failure=20,
    DMTF_HealthState_Critical_failure=25,
    DMTF_HealthState_Non_recoverable_error=30,
    /*DMTF_HealthState_DMTF_Reserved=..,*/
}DMTF_HealthState;

typedef enum _DMTF_OperatingStatus{
    DMTF_OperatingStatus_Unknown=0,
    DMTF_OperatingStatus_Not_Available=1,
    DMTF_OperatingStatus_Servicing=2,
    DMTF_OperatingStatus_Starting=3,
    DMTF_OperatingStatus_Stopping=4,
    DMTF_OperatingStatus_Stopped=5,
    DMTF_OperatingStatus_Aborted=6,
    DMTF_OperatingStatus_Dormant=7,
    DMTF_OperatingStatus_Completed=8,
    DMTF_OperatingStatus_Migrating=9,
    DMTF_OperatingStatus_Emigrating=10,
    DMTF_OperatingStatus_Immigrating=11,
    DMTF_OperatingStatus_Snapshotting=12,
    DMTF_OperatingStatus_Shutting_Down=13,
    DMTF_OperatingStatus_In_Test=14,
    DMTF_OperatingStatus_Transitioning=15,
    DMTF_OperatingStatus_In_Service=16,
    /*DMTF_OperatingStatus_DMTF_Reserved=..,*/
    /*DMTF_OperatingStatus_Vendor_Reserved=0x8000..,*/
}DMTF_OperatingStatus;

typedef enum _DMTF_OperationalStatus{
    DMTF_OperationalStatus_Unknown=0,
    DMTF_OperationalStatus_Other=1,
    DMTF_OperationalStatus_OK=2,
    DMTF_OperationalStatus_Degraded=3,
    DMTF_OperationalStatus_Stressed=4,
    DMTF_OperationalStatus_Predictive_Failure=5,
    DMTF_OperationalStatus_Error=6,
    DMTF_OperationalStatus_Non_Recoverable_Error=7,
    DMTF_OperationalStatus_Starting=8,
    DMTF_OperationalStatus_Stopping=9,
    DMTF_OperationalStatus_Stopped=10,
    DMTF_OperationalStatus_In_Service=11,
    DMTF_OperationalStatus_No_Contact=12,
    DMTF_OperationalStatus_Lost_Communication=13,
    DMTF_OperationalStatus_Aborted=14,
    DMTF_OperationalStatus_Dormant=15,
    DMTF_OperationalStatus_Supporting_Entity_in_Error=16,
    DMTF_OperationalStatus_Completed=17,
    DMTF_OperationalStatus_Power_Mode=18,
    /*DMTF_OperationalStatus_DMTF_Reserved=..,*/
    /*DMTF_OperationalStatus_Vendor_Reserved=0x8000..,*/
}DMTF_OperationalStatus;

typedef enum _DMTF_PowerManagementCapabilities{
    DMTF_PowerManagementCapabilities_Unknown=0,
    DMTF_PowerManagementCapabilities_Not_Supported=1,
    DMTF_PowerManagementCapabilities_Disabled=2,
    DMTF_PowerManagementCapabilities_Enabled=3,
    DMTF_PowerManagementCapabilities_Power_Saving_Modes_Entered_Automatically=4,
    DMTF_PowerManagementCapabilities_Power_State_Settable=5,
    DMTF_PowerManagementCapabilities_Power_Cycling_Supported=6,
    DMTF_PowerManagementCapabilities_Timed_Power_On_Supported=7,
}DMTF_PowerManagementCapabilities;

typedef enum _DMTF_PrimaryStatus{
    DMTF_PrimaryStatus_Unknown=0,
    DMTF_PrimaryStatus_OK=1,
    DMTF_PrimaryStatus_Degraded=2,
    DMTF_PrimaryStatus_Error=3,
    /*DMTF_PrimaryStatus_DMTF_Reserved=..,*/
    /*DMTF_PrimaryStatus_Vendor_Reserved=0x8000..,*/
}DMTF_PrimaryStatus;

typedef enum _DMTF_RequestedState{
    DMTF_RequestedState_Unknown=0,
    DMTF_RequestedState_Enabled=2,
    DMTF_RequestedState_Disabled=3,
    DMTF_RequestedState_Shut_Down=4,
    DMTF_RequestedState_No_Change=5,
    DMTF_RequestedState_Offline=6,
    DMTF_RequestedState_Test=7,
    DMTF_RequestedState_Deferred=8,
    DMTF_RequestedState_Quiesce=9,
    DMTF_RequestedState_Reboot=10,
    DMTF_RequestedState_Reset=11,
    DMTF_RequestedState_Not_Applicable=12,
    /*DMTF_RequestedState_DMTF_Reserved=..,*/
    /*DMTF_RequestedState_Vendor_Reserved=32768..65535,*/
}DMTF_RequestedState;

typedef enum _DMTF_ResetCapability{
    DMTF_ResetCapability_Other=1,
    DMTF_ResetCapability_Unknown=2,
    DMTF_ResetCapability_Disabled=3,
    DMTF_ResetCapability_Enabled=4,
    DMTF_ResetCapability_Not_Implemented=5,
}DMTF_ResetCapability;

typedef enum _DMTF_StatusInfo{
    DMTF_StatusInfo_Other=1,
    DMTF_StatusInfo_Unknown=2,
    DMTF_StatusInfo_Enabled=3,
    DMTF_StatusInfo_Disabled=4,
    DMTF_StatusInfo_Not_Applicable=5,
}DMTF_StatusInfo;

typedef enum _DMTF_TransitioningToState{
    DMTF_TransitioningToState_Unknown=0,
    DMTF_TransitioningToState_Enabled=2,
    DMTF_TransitioningToState_Disabled=3,
    DMTF_TransitioningToState_Shut_Down=4,
    DMTF_TransitioningToState_No_Change=5,
    DMTF_TransitioningToState_Offline=6,
    DMTF_TransitioningToState_Test=7,
    DMTF_TransitioningToState_Defer=8,
    DMTF_TransitioningToState_Quiesce=9,
    DMTF_TransitioningToState_Reboot=10,
    DMTF_TransitioningToState_Reset=11,
    DMTF_TransitioningToState_Not_Applicable=12,
    /*DMTF_TransitioningToState_DMTF_Reserved=..,*/
}DMTF_TransitioningToState;

typedef enum _DMTF_RequestStateChange{
    DMTF_RequestStateChange_Completed_with_No_Error=0,
    DMTF_RequestStateChange_Not_Supported=1,
    DMTF_RequestStateChange_Unknown_or_Unspecified_Error=2,
    DMTF_RequestStateChange_Cannot_complete_within_Timeout_Period=3,
    DMTF_RequestStateChange_Failed=4,
    DMTF_RequestStateChange_Invalid_Parameter=5,
    DMTF_RequestStateChange_In_Use=6,
    /*DMTF_RequestStateChange_DMTF_Reserved=..,*/
    DMTF_RequestStateChange_Method_Parameters_Checked___Job_Started=4096,
    DMTF_RequestStateChange_Invalid_State_Transition=4097,
    DMTF_RequestStateChange_Use_of_Timeout_Parameter_Not_Supported=4098,
    DMTF_RequestStateChange_Busy=4099,
    /*DMTF_RequestStateChange_Method_Reserved=4100..32767,*/
    /*DMTF_RequestStateChange_Vendor_Specific=32768..65535,*/
}DMTF_RequestStateChange;

typedef enum _DMTF_Usage{
    DMTF_Usage_Other=1,
    DMTF_Usage_Unrestricted=2,
    DMTF_Usage_Reserved_for_ComputerSystem__the_block_server_=3,
    /*DMTF_Usage_DMTF_Reserved=..,*/
    /*DMTF_Usage_Vendor_Reserved=32768..65535,*/
}DMTF_Usage;

#define DMTF_StartMode_Automatic "Automatic"
#define DMTF_StartMode_Manual "Manual"

#define DMTF_Status_OK "OK"
#define DMTF_Status_Error "Error"
#define DMTF_Status_Degraded "Degraded"
#define DMTF_Status_Unknown "Unknown"
#define DMTF_Status_Pred_Fail "Pred Fail"
#define DMTF_Status_Starting "Starting"
#define DMTF_Status_Stopping "Stopping"
#define DMTF_Status_Service "Service"
#define DMTF_Status_Stressed "Stressed"
#define DMTF_Status_NonRecover "NonRecover"
#define DMTF_Status_No_Contact "No Contact"
#define DMTF_Status_Lost_Comm "Lost Comm"
#define DMTF_Status_Stopped "Stopped"

#define DMTF_NameFormat_Other "Other"
#define DMTF_NameFormat_IP "IP"
#define DMTF_NameFormat_Dial "Dial"
#define DMTF_NameFormat_HID "HID"
#define DMTF_NameFormat_NWA "NWA"
#define DMTF_NameFormat_HWA "HWA"
#define DMTF_NameFormat_X25 "X25"
#define DMTF_NameFormat_ISDN "ISDN"
#define DMTF_NameFormat_IPX "IPX"
#define DMTF_NameFormat_DCC "DCC"
#define DMTF_NameFormat_ICD "ICD"
#define DMTF_NameFormat_E_164 "E.164"
#define DMTF_NameFormat_SNA "SNA"
#define DMTF_NameFormat_OID_OSI "OID/OSI"
#define DMTF_NameFormat_WWN "WWN"
#define DMTF_NameFormat_NAA "NAA"

/* RASD specific enums */
typedef enum _DMTF_ChangeableType{
    DMTF_ChangeableType_Not_Changeable_Persistent=0,
    DMTF_ChangeableType_Changeable_Transient=1,
    DMTF_ChangeableType_Changeable_Persistent=2,
    DMTF_ChangeableType_Not_Changeable_Transient=3,
}DMTF_ChangeableType;

typedef enum _DMTF_ConsumerVisibility{
    DMTF_ConsumerVisibility_Unknown=0,
    DMTF_ConsumerVisibility_Passed_Through=2,
    DMTF_ConsumerVisibility_Virtualized=3,
    DMTF_ConsumerVisibility_Not_represented=4,
    /*DMTF_ConsumerVisibility_DMTF_reserved=..,*/
    /*DMTF_ConsumerVisibility_Vendor_Reserved=32767..65535,*/
}DMTF_ConsumerVisibility;

typedef enum _DMTF_MappingBehavior{
    DMTF_MappingBehavior_Unknown=0,
    DMTF_MappingBehavior_Not_Supported=2,
    DMTF_MappingBehavior_Dedicated=3,
    DMTF_MappingBehavior_Soft_Affinity=4,
    DMTF_MappingBehavior_Hard_Affinity=5,
    /*DMTF_MappingBehavior_DMTF_Reserved=..,*/
    /*DMTF_MappingBehavior_Vendor_Reserved=32767..65535,*/
}DMTF_MappingBehavior;

typedef enum _DMTF_ResourceType{
    DMTF_ResourceType_Other=1,
    DMTF_ResourceType_Computer_System=2,
    DMTF_ResourceType_Processor=3,
    DMTF_ResourceType_Memory=4,
    DMTF_ResourceType_IDE_Controller=5,
    DMTF_ResourceType_Parallel_SCSI_HBA=6,
    DMTF_ResourceType_FC_HBA=7,
    DMTF_ResourceType_iSCSI_HBA=8,
    DMTF_ResourceType_IB_HCA=9,
    DMTF_ResourceType_Ethernet_Adapter=10,
    DMTF_ResourceType_Other_Network_Adapter=11,
    DMTF_ResourceType_I_O_Slot=12,
    DMTF_ResourceType_I_O_Device=13,
    DMTF_ResourceType_Floppy_Drive=14,
    DMTF_ResourceType_CD_Drive=15,
    DMTF_ResourceType_DVD_drive=16,
    DMTF_ResourceType_Disk_Drive=17,
    DMTF_ResourceType_Tape_Drive=18,
    DMTF_ResourceType_Storage_Extent=19,
    DMTF_ResourceType_Other_storage_device=20,
    DMTF_ResourceType_Serial_port=21,
    DMTF_ResourceType_Parallel_port=22,
    DMTF_ResourceType_USB_Controller=23,
    DMTF_ResourceType_Graphics_controller=24,
    DMTF_ResourceType_IEEE_1394_Controller=25,
    DMTF_ResourceType_Partitionable_Unit=26,
    DMTF_ResourceType_Base_Partitionable_Unit=27,
    DMTF_ResourceType_Power=28,
    DMTF_ResourceType_Cooling_Capacity=29,
    DMTF_ResourceType_Ethernet_Switch_Port=30,
    DMTF_ResourceType_Logical_Disk=31,
    DMTF_ResourceType_Storage_Volume=32,
    DMTF_ResourceType_Ethernet_Connection=33,
    /*DMTF_ResourceType_DMTF_reserved=..,*/
    /*DMTF_ResourceType_Vendor_Reserved=0x8000..0xFFFF,*/
}DMTF_ResourceType;

typedef enum _DMTF_LinkTechnology{
    DMTF_LinkTechnology_Unknown=0,
    DMTF_LinkTechnology_Other=1,
    DMTF_LinkTechnology_Ethernet=2,
    DMTF_LinkTechnology_IB=3,
    DMTF_LinkTechnology_FC=4,
    DMTF_LinkTechnology_FDDI=5,
    DMTF_LinkTechnology_ATM=6,
    DMTF_LinkTechnology_Token_Ring=7,
    DMTF_LinkTechnology_Frame_Relay=8,
    DMTF_LinkTechnology_Infrared=9,
    DMTF_LinkTechnology_BlueTooth=10,
    DMTF_LinkTechnology_Wireless_LAN=11,
}DMTF_LinkTechnology;

typedef enum _DMTF_PortType{
    DMTF_PortType_Unknown=0,
    DMTF_PortType_Other=1,
    DMTF_PortType_Not_Applicable=2,
    /*DMTF_PortType_DMTF_Reserved=3..15999,*/
    /*DMTF_PortType_Vendor_Reserved=16000..65535,*/
}DMTF_PortType;

typedef enum _DMTF_UsageRestriction{
    DMTF_UsageRestriction_Unknown=0,
    DMTF_UsageRestriction_Front_end_only=2,
    DMTF_UsageRestriction_Back_end_only=3,
    DMTF_UsageRestriction_Not_restricted=4,
}DMTF_UsageRestriction;

#endif /*__DMTF_H__*/
