// ***** Generated by Codegen *****
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

#ifndef __XEN_ALLOCATION_CAPABILITIES_H__
#define __XEN_ALLOCATION_CAPABILITIES_H__

/* Values for the various properties of the class */
typedef enum _RequestTypesSupported{
    RequestTypesSupported_Unknown=0,
    RequestTypesSupported_Specific=2,
    RequestTypesSupported_General=3,
    RequestTypesSupported_Both=4,
    /*RequestTypesSupported_DMTF_reserved=..,*/
    /*RequestTypesSupported_Vendor_Reserved=0x8000..0xFFFF,*/
}RequestTypesSupported;

typedef enum _SharingMode{
    SharingMode_Unknown=0,
    SharingMode_Other=1,
    SharingMode_Dedicated=2,
    SharingMode_Shared=3,
    /*SharingMode_DMTF_reserved=..,*/
    /*SharingMode_Vendor_Reserved=0x8000..0xFFFF,*/
}SharingMode;

typedef enum _SupportedAddStates{
    SupportedAddStates_Unknown=0,
    SupportedAddStates_Other=1,
    SupportedAddStates_Enabled=2,
    SupportedAddStates_Disabled=3,
    SupportedAddStates_Shutting_Down=4,
    SupportedAddStates_Not_Applicable=5,
    SupportedAddStates_Enabled_but_Offline=6,
    SupportedAddStates_In_Test=7,
    SupportedAddStates_Deferred=8,
    SupportedAddStates_Quiesce=9,
    SupportedAddStates_Starting=10,
    SupportedAddStates_Paused=11,
    SupportedAddStates_Suspended=12,
    /*SupportedAddStates_DMTF_Reserved=..,*/
    /*SupportedAddStates_Vendor_Reserved=0x8000..0xFFFF,*/
}SupportedAddStates;

typedef enum _SupportedRemoveStates{
    SupportedRemoveStates_Unknown=0,
    SupportedRemoveStates_Other=1,
    SupportedRemoveStates_Enabled=2,
    SupportedRemoveStates_Disabled=3,
    SupportedRemoveStates_Shutting_Down=4,
    SupportedRemoveStates_Not_Applicable=5,
    SupportedRemoveStates_Enabled_but_Offline=6,
    SupportedRemoveStates_In_Test=7,
    SupportedRemoveStates_Deferred=8,
    SupportedRemoveStates_Quiesce=9,
    SupportedRemoveStates_Starting=10,
    SupportedRemoveStates_Paused=11,
    SupportedRemoveStates_Suspended=12,
    /*SupportedRemoveStates_DMTF_Reserved=..,*/
    /*SupportedRemoveStates_Vendor_Reserved=0x8000..0xFFFF,*/
}SupportedRemoveStates;

typedef enum _CreateGoalSettings{
    CreateGoalSettings_Success=0,
    CreateGoalSettings_Not_Supported=1,
    CreateGoalSettings_Unknown=2,
    CreateGoalSettings_Timeout=3,
    CreateGoalSettings_Failed=4,
    CreateGoalSettings_Invalid_Parameter=5,
    CreateGoalSettings_Alternative_Proposed=6,
    /*CreateGoalSettings_DMTF_Reserved=..,*/
    /*CreateGoalSettings_Vendor_Specific=32768..65535,*/
}CreateGoalSettings;


#endif /*__XEN_ALLOCATION_CAPABILITIES_H__*/
