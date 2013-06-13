// Copyright (C) 2007 Citrix Systems Inc.
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
// Description:
// ============================================================================

#include <string.h>

#include "cmpidt.h"
#include "cmpift.h"
#include "cmpimacs.h"

#include "cmpilify.h"

#include "cmpitrace.h"

CMPIrc load()
{
    return CMPI_RC_OK;
}

CMPIrc unload(
    const int terminating
    )
{
    return CMPI_RC_OK;
}

CMPIrc getlhsquery(
    const CMPIInstance* rhsinstance,
    char** query,
    char** lang)
{
    CMPIStatus status;
    char buf[256];
    CMPIData data = CMGetProperty(rhsinstance, "InstanceID", &status);
    *lang = strdup("WQL");
    sprintf(buf, "SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID = %s", CMGetCharPtr(data.value.string));
    *query = strdup(buf);
    return CMPI_RC_OK;
}

CMPIrc getrhsquery(
    const CMPIInstance* lhsinstance,
    char** query,
    char** lang)
{
    CMPIStatus status;
    char buf[256];
    CMPIData data = CMGetProperty(lhsinstance, "InstanceID", &status);
    *lang = strdup("WQL");
    sprintf(buf, "SELECT * FROM CIM_ResourceAllocationSettingData WHERE InstanceID = %s", CMGetCharPtr(data.value.string));
    *query = strdup(buf);
    return CMPI_RC_OK;
}

CMPIrc setassocproperties(const CMPIInstance *associnstance)
{
    (void)associnstance;
     return CMPI_RC_OK;
}

CMPILIFYAssociationMIStub(,Xen_ElementSettingData, 
                          "CIM_ResourceAllocationSettingData", "SettingData", "root/cimv2", 
                          "CIM_ResourceAllocationSettingData", "ManagedElement", "root/cimv2")
