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

#include "Xen_VirtualSwitchManagementService.h"
#include "Xen_VirtualSwitch.h"
#include "providerinterface.h"
#include "RASDs.h"

CMPIrc DefineSystem(
    const CMPIBroker *broker,
    const CMPIContext *context,
    const CMPIArgs *argsin,
    CMPIArgs *argsout,
    xen_utils_session *session,
    CMPIStatus *status
    );
CMPIrc DestroySystem(
    const CMPIBroker *broker,
    const CMPIContext *context,
    const CMPIArgs *argsin,
    CMPIArgs *argsout,
    xen_utils_session *session,
    CMPIStatus *status
    );
CMPIrc AddResourceSettings(
    const CMPIBroker *broker,
    const CMPIContext *context,
    const CMPIArgs *argsin,
    CMPIArgs *argsout,
    xen_utils_session *session,
    CMPIStatus *status
    );
CMPIrc RemoveResourceSettings(
    const CMPIBroker *broker,
    const CMPIContext *context,
    const CMPIArgs *argsin,
    CMPIArgs *argsout,
    xen_utils_session *session,
    CMPIStatus *status
    );
CMPIrc ModifyResourceSettings(
    const CMPIBroker *broker,
    const CMPIContext *context,
    const CMPIArgs *argsin,
    CMPIArgs *argsout,
    xen_utils_session *session,
    CMPIStatus *status
    );
CMPIrc ModifySystemSettings(
    const CMPIBroker *broker, 
    const CMPIContext *context, 
    const CMPIArgs *argsin, 
    CMPIArgs *argsout, 
    xen_utils_session *session, 
    CMPIStatus *status);

static xen_network *_get_affected_system(
    const CMPIBroker *broker, 
    const CMPIArgs* argsin, 
    xen_utils_session *session,
    CMPIStatus *status
    );
static xen_network *_get_affected_configuration(
    const CMPIBroker *broker, 
    const CMPIArgs* argsin, 
    xen_utils_session *session,
    CMPIStatus *status
    );
static CMPIrc _get_resource_settings(
    const CMPIBroker *broker, 
    const CMPIArgs* argsin, 
    xen_utils_session *session,
    xen_pif_record_set **pif_recs,
    bool *create_bond,
    CMPIStatus *status
    );
static bool _create_bond_or_vlan(
    const CMPIBroker *broker,
    xen_utils_session *session,
    xen_network net,
    xen_pif_record_set *pif_rec_set,
    bool create_bond);

static void _destroy_pif(
    xen_utils_session *session, 
    xen_pif pif
    );

/******************************************************************************
 * Provider export function
 * Execute an extrinsic method on the specified CIM instance.
 *****************************************************************************/
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,       /* [in] CMPI Factory services */
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

    int argcount = CMGetArgCount(argsin, NULL);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- argsin=%d", argcount));

    argdata = CMGetKey(reference, "Name", &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("Couldnt find the Virtual Switch Service to invoke method on"));
        goto Exit;
    }

    /* Check that the method has the correct number of arguments. */
    if (strcmp(methodname, "DefineSystem") == 0) {
        rc = DefineSystem(broker, context, argsin, argsout, session, &status);
    }
    else if (strcmp(methodname, "DestroySystem") == 0) {
        rc = DestroySystem(broker, context, argsin, argsout, session, &status);
    }
    else if (strcmp(methodname, "AddResourceSettings") == 0) {
        rc = AddResourceSettings(broker, context, argsin, argsout, session, &status);
    }
    else if (strcmp(methodname, "RemoveResourceSettings") == 0) {
        rc = RemoveResourceSettings(broker, context, argsin, argsout, session, &status);
    }
    else if (strcmp(methodname, "ModifyResourceSettings") == 0) {
        rc = ModifyResourceSettings(broker, context, argsin, argsout, session, &status);
#if NOTUSED
        CMPIObjectPath* job_instance_op = NULL;
        CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
        CMPIObjectPath* resultingresourcesettings_instance_op = NULL;
        CMAddArg(argsout, "ResultingResourceSettings", (CMPIValue *)&resultingresourcesettings_instance_op, CMPI_ref);
#endif
    }
    else if (strcmp(methodname, "ModifySystemSettings") == 0) {
        rc = ModifySystemSettings(broker, context, argsin, argsout, session, &status);
#if NOTUSED
        CMPIObjectPath* job_instance_op = NULL;
        CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
#endif
    }
    else
        status.rc = CMPI_RC_ERR_METHOD_NOT_FOUND;

    Exit:
    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);

    if(session)
        xen_utils_cleanup_session(session);
    if(ctx)
        xen_utils_free_call_context(ctx);

    _SBLIM_RETURNSTATUS(status);
}

/* CMPI Method provider function table setup */
XenMethodMIStub(Xen_VirtualSwitchManagementService)

/* Internal functions that implement the methods */
/******************************************************************************
* Method to create a virtual switch (network) 
* 1) If ResourceSettings are specified, they need to be of type 'EthernetConnection' 
* 2) If more than 2 HostnetworkPorts are connected to, they are automatically 'bonded'.
* 3) If no HostNetworkPort is connected to (absence of ResourceSettings), 
*    a virtual switch which is not connected to the outside world (internal) is created
* 4) If exactly one HostNetworkPort is specified in the ResourceSettings, it needs
*    to contain a VLAN tag, and a VLAN tagged network is created.
******************************************************************************/
CMPIrc DefineSystem(
    const CMPIBroker *broker,
    const CMPIContext *context,
    const CMPIArgs *argsin,
    CMPIArgs *argsout,
    xen_utils_session *session,
    CMPIStatus *status
    )
{
    CMPIInstance* vssd;
    xen_network_record *net_rec = NULL;
    xen_network net = NULL;
    xen_pif_record_set *pif_rec_set = NULL;
    int rc = Xen_VirtualSwitchManagementService_DefineSystem_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";

    /* SystemSettings defines all the settings required to create a switch */
    if (!xen_utils_get_vssd_param(broker, session, argsin, 
        "SystemSettings", status, 
        &vssd)) {
        error_msg = "ERROR: Error getting the 'SystemSettings' parameter";
        goto Exit;
    }
    if (!vssd_to_network_rec(broker, vssd, &net_rec, status)) {
        error_msg = "ERROR: Couldn't parse the 'SystemSettings' parameter ";
        goto Exit;
    }

    /* ResourceSettings is a list of all device names(s) to attach to the network */
    /* This can be NULL, if the caller is creating an internal network */
    bool create_bond = false;
    statusrc = _get_resource_settings(broker, argsin, session, &pif_rec_set, &create_bond, status);
    if ((statusrc != CMPI_RC_OK) && (statusrc != CMPI_RC_ERR_NOT_FOUND)) {
        error_msg = "ERROR: Couldn't get the 'ResourceSettings' parameter";
        goto Exit;
    }

    rc = Xen_VirtualSwitchManagementService_DefineSystem_Failed;
    statusrc = CMPI_RC_ERR_FAILED;

    if (!xen_network_create(session->xen, &net, net_rec))
        goto Exit;

    /* If devices were created create a VLAN (one device speciifed) or Bond (multiple devices) */
    if (pif_rec_set) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Creating 'External' or 'Bonded' network"));
        if (!_create_bond_or_vlan(broker, session, net, pif_rec_set, create_bond))
            goto Exit;
    }
    else {
        /* private network already created, nothing to do */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Creating 'Internal' network"));
    }

    /* output parameter is a CIM reference to the virtual switch */
    rc = Xen_VirtualSwitchManagementService_DefineSystem_Completed_with_No_Error;
    statusrc = CMPI_RC_OK;
    CMPIObjectPath *op = (CMPIObjectPath *)virtual_switch_create_ref(broker, session, net, status);
    CMAddArg(argsout, "ResultingSystem", (CMPIValue *)&op, CMPI_ref);

    Exit:

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    if (!session->xen->ok && (net != NULL)) {
        RESET_XEN_ERROR(session->xen);
        xen_network_destroy(session->xen, net);
    }
    if (pif_rec_set)
        xen_pif_record_set_free(pif_rec_set);
    if (net_rec)
        xen_network_record_free(net_rec);
    if (net)
        xen_network_free(net);

    return rc;
}

CMPIrc DestroySystem(
    const CMPIBroker *broker,
    const CMPIContext *context,
    const CMPIArgs *argsin,
    CMPIArgs *argsout,
    xen_utils_session *session,
    CMPIStatus *status
    )
{
    xen_network network = NULL;
    int rc = Xen_VirtualSwitchManagementService_DestroySystem_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";

    /* get the network to be deleted, in the 'AffectedSystem' parameter */
    network = _get_affected_system(broker, argsin, session, status);
    if (!network) {
        error_msg = "ERROR: Couldn't get the 'AffectedSystem' parameter";
        goto Exit;
    }

    /* Destroy all the pifs associated with this network, including the bonds */
    xen_pif_set *pif_set = NULL;
    if (xen_network_get_pifs(session->xen, &pif_set, network) && pif_set) {
        int i;
        for (i=0; i<pif_set->size; i++) {
            _destroy_pif(session, pif_set->contents[i]);
            RESET_XEN_ERROR(session->xen);
        }
        xen_pif_set_free(pif_set);
    }

    /* Now destroy the actual network */
    if (!xen_network_destroy(session->xen, network)) {
        rc = Xen_VirtualSwitchManagementService_DestroySystem_Failed;
        statusrc = CMPI_RC_ERR_FAILED;
    }
    else {
        rc = Xen_VirtualSwitchManagementService_DestroySystem_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
    }

    Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);

    if (network)
        xen_network_free(network);

    return rc;
}

CMPIrc AddResourceSettings(
    const CMPIBroker *broker, 
    const CMPIContext *context, 
    const CMPIArgs *argsin, 
    CMPIArgs *argsout, 
    xen_utils_session *session, 
    CMPIStatus *status)
{
    xen_network network = NULL;
    int rc = Xen_VirtualSwitchManagementService_AddResourceSettings_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    xen_pif_record_set *pif_rec_set = NULL;

    /* find the switch we need to add the vlan/bond to */
    network = _get_affected_configuration(broker, argsin, session, status);
    if (!network) {
        error_msg = "ERROR: Couldn't get the 'AffectedConfiguration' parameter";
        goto Exit;
    }

    /* get the device names for the new vlan/bond */
    bool create_bond = false;
    statusrc = _get_resource_settings(broker, argsin, session, &pif_rec_set, &create_bond, status);
    if (statusrc != CMPI_RC_OK) {
        error_msg = "ERROR: Couldnt parse/get the 'ResourceSettings' parameter";
        goto Exit;
    }

    /* create a vlan/bond and add it to the network */
    rc = Xen_VirtualSwitchManagementService_AddResourceSettings_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    if (pif_rec_set) {
        if (!_create_bond_or_vlan(broker, session, network, pif_rec_set, create_bond)) {
            error_msg = "ERROR: Couldn't create the bonded or a vlan tagged network";
            goto Exit;
        }
        rc = Xen_VirtualSwitchManagementService_AddResourceSettings_Completed_with_No_Error;
        statusrc = CMPI_RC_OK;
    }

    Exit:
    if (pif_rec_set)
        xen_pif_record_set_free(pif_rec_set);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);

    if (network)
        xen_network_free(network);

    return rc;
}

CMPIrc RemoveResourceSettings(
    const CMPIBroker *broker, 
    const CMPIContext *context, 
    const CMPIArgs *argsin, 
    CMPIArgs *argsout, 
    xen_utils_session *session, 
    CMPIStatus *status)
{
    int rc = Xen_VirtualSwitchManagementService_RemoveResourceSettings_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    xen_pif_record_set *pif_rec_set = NULL;

    /* get the pif(s) that are being removed */
    bool create_bond = false;
    statusrc = _get_resource_settings(broker, argsin, session, &pif_rec_set, &create_bond, status);
    if (statusrc != CMPI_RC_OK || (pif_rec_set == NULL)) {
        error_msg = "ERROR: Couldn't parse/get the 'ResourceSettings' parameter. RASD needs to have the 'InstanceID' property set";
        goto Exit;
    }

    rc = Xen_VirtualSwitchManagementService_RemoveResourceSettings_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    int i;

    /* try destroying them */
    for (i=0; i<pif_rec_set->size; i++) {
        xen_pif pif = NULL;
        if (xen_pif_get_by_uuid(session->xen,  &pif, pif_rec_set->contents[i]->uuid) && pif) {
            _destroy_pif(session, pif);
            xen_pif_free(pif);
            pif = NULL;
        }
        if (!session->xen->ok)
            goto Exit;
    }

    rc = Xen_VirtualSwitchManagementService_RemoveResourceSettings_Completed_with_No_Error;
    statusrc = CMPI_RC_OK;

    Exit:

    if (pif_rec_set)
        xen_pif_record_set_free(pif_rec_set);

    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

CMPIrc ModifyResourceSettings(
    const CMPIBroker *broker, 
    const CMPIContext *context, 
    const CMPIArgs *argsin, 
    CMPIArgs *argsout, 
    xen_utils_session *session, 
    CMPIStatus *status)
{
    int rc = Xen_VirtualSwitchManagementService_ModifyResourceSettings_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";
    xen_pif_record_set *pif_rec_set = NULL;

    /* get the pif(s) that are being modified - specified in the RASD passed in */
    bool create_bond = false;
    statusrc = _get_resource_settings(broker, argsin, session, &pif_rec_set, &create_bond, status);
    if (statusrc != CMPI_RC_OK || (pif_rec_set == NULL)) {
        error_msg = "ERROR: Couldn't parse/get the 'ResourceSettings' parameter. RASD needs to have the 'InstanceID' property set";
        goto Exit;
    }

    rc = Xen_VirtualSwitchManagementService_ModifyResourceSettings_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    int i;

    /* Modify the IP configuration, nothing else is modifyable on a PIF */
    for (i=0; i<pif_rec_set->size; i++) {
        xen_pif pif = NULL;
        xen_pif_record *pif_rec = pif_rec_set->contents[i];
        if (xen_pif_get_by_uuid(session->xen,  &pif, pif_rec->uuid) && pif) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Reconfiguring (mode:%d) (IP:%s), (mask:%s), (gw:%s), (dns:%s)", 
                pif_rec->ip_configuration_mode,
                pif_rec->ip, pif_rec->netmask, 
                pif_rec->gateway, pif_rec->dns))
                /* reconfigurae the IP configuration based on what's been specified in the new RASD */
            xen_pif_reconfigure_ip(session->xen, pif, pif_rec->ip_configuration_mode, 
                pif_rec->ip, pif_rec->netmask, pif_rec->gateway, 
                pif_rec->dns);
            xen_pif_record *current_pif_rec = NULL;
            if (xen_pif_get_record(session->xen, &current_pif_rec, pif)) {
                char *purpose_before = xen_utils_get_from_string_string_map(current_pif_rec->other_config, "management_purpose");
                char *purpose_now = xen_utils_get_from_string_string_map(pif_rec->other_config, "management_purpose");
                if (purpose_now && 
                    (pif_rec->ip_configuration_mode != XEN_IP_CONFIGURATION_MODE_NONE)) {
                    /* set the management purpose to the new value */
                    if ((purpose_before==NULL) || (strcmp(purpose_now, purpose_before) != 0)) {
                        xen_utils_add_to_string_string_map("management_purpose", purpose_now, &(current_pif_rec->other_config));
                        xen_pif_set_other_config(session->xen, pif, current_pif_rec->other_config);
                        xen_pif_set_disallow_unplug(session->xen, pif, true); 
                    }
                }
                else {
                    if (purpose_before != NULL) {
                        /* reset the contents of the management purpose */
                        xen_pif_remove_from_other_config(session->xen, pif, "management_purpose");
                        xen_pif_set_disallow_unplug(session->xen, pif, false);
                    }
                }
                xen_pif_record_free(current_pif_rec);
            }
            xen_pif_free(pif);
            pif = NULL;
        }
        if (!session->xen->ok)
            goto Exit;
    }
    rc = Xen_VirtualSwitchManagementService_ModifyResourceSettings_Completed_with_No_Error;
    statusrc = CMPI_RC_OK;

    Exit:

    if (pif_rec_set != NULL)
        xen_pif_record_set_free(pif_rec_set);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}

CMPIrc ModifySystemSettings(
    const CMPIBroker *broker, 
    const CMPIContext *context, 
    const CMPIArgs *argsin, 
    CMPIArgs *argsout, 
    xen_utils_session *session, 
    CMPIStatus *status)
{
    CMPIInstance* vssd = NULL;
    xen_network_record *net_rec = NULL;
    int rc = Xen_VirtualSwitchManagementService_ModifySystemSettings_Invalid_Parameter;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Unknown error";

    /* 'SystemSettings' defines all the settings required to modify a switch */
    /* This time it should include a valid InstanceID property to identify the switch 
       we need to mofify */
    if (!xen_utils_get_vssd_param(broker, session, argsin, "SystemSettings", status, &vssd)) {
        error_msg = "ERROR: Couldn't get the 'SystemSettings' parameter";
        goto Exit;
    }
    if (!vssd_to_network_rec(broker, vssd, &net_rec, status)) {
        error_msg = "ERROR: Couldn't parse the 'SystemSettings' parameter ";
        goto Exit;
    }

    /* change name-label and name-description if they are different */
    rc = Xen_VirtualSwitchManagementService_ModifySystemSettings_Failed;
    statusrc = CMPI_RC_ERR_FAILED;
    if(net_rec && net_rec->uuid) {
        xen_network network = NULL;
        if(xen_network_get_by_uuid(session->xen, &network, net_rec->uuid)){
            xen_network_record *old_rec = NULL;
            if(xen_network_get_record(session->xen, &old_rec, network)) {
                if(net_rec->name_label && 
                   (strcmp(net_rec->name_label, old_rec->name_label) != 0))
                    xen_network_set_name_label(session->xen, network, net_rec->name_label);
                if(net_rec->name_description &&
                   (strcmp(net_rec->name_description, old_rec->name_description) != 0))
                    xen_network_set_name_description(session->xen, network, net_rec->name_description);
                if(net_rec->other_config && net_rec->other_config->size > 0) {
                    int i;
                    for(i=0; i<net_rec->other_config->size; i++)
                        xen_utils_add_to_string_string_map(
                            net_rec->other_config->contents[i].key, 
                            net_rec->other_config->contents[i].val,
                            &old_rec->other_config);
                    xen_network_set_other_config(session->xen, network, old_rec->other_config);
                }
                xen_network_record_free(old_rec);
                if(session->xen->ok) {
                    rc = Xen_VirtualSwitchManagementService_ModifySystemSettings_Completed_with_No_Error;
                    statusrc = CMPI_RC_OK;
                }
            }
            xen_network_free(network);
        }
        xen_network_record_free(net_rec);
    }
    else {
        error_msg = "ERROR: The SystemSettings parameter should include a valid InstanceID";
    }

Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return rc;
}
/******************************************************************************
 * A Bunch of helper routines follow:
 *****************************************************************************/
/* Looks for and gets the 'AffectedSystem' parameter */
static xen_network *_get_affected_system(
    const CMPIBroker *broker, 
    const CMPIArgs* argsin, 
    xen_utils_session *session,
    CMPIStatus *status
    )
{
    CMPIData argdata;
    xen_network network = NULL;
    char *uuid = NULL;
    if (!_GetArgument(broker, argsin, "AffectedSystem", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "AffectedSystem", CMPI_string, &argdata, status)) {
            status->rc = CMPI_RC_ERR_INVALID_PARAMETER;
            goto Exit;
        }
        uuid = CMGetCharPtr(argdata.value.string);
    }
    else {
        CMPIData key = CMGetKey(argdata.value.ref, "Name", status);
        uuid = CMGetCharPtr(key.value.string);
    }

    if (!xen_network_get_by_uuid(session->xen, &network, uuid)) {
        status->rc = CMPI_RC_ERR_NOT_FOUND;
        goto Exit;
    }
    Exit:
    return network;
}
/* 
 * Read the the 'AffectedConfiguration' parameter which is of type 
 * Xen_VirtualSwitchSettingData.
 */
static xen_network *_get_affected_configuration(
    const CMPIBroker *broker, 
    const CMPIArgs* argsin, 
    xen_utils_session *session,
    CMPIStatus *status
    )
{
    CMPIData argdata;
    xen_network network = NULL;
    char buf[MAX_INSTANCEID_LEN];
    char *uuid = NULL;

    if (!_GetArgument(broker, argsin, "AffectedConfiguration", CMPI_ref, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "AffectedConfiguration", CMPI_string, &argdata, status)) {
            status->rc = CMPI_RC_ERR_INVALID_PARAMETER;
            goto Exit;
        }
        uuid = CMGetCharPtr(argdata.value.string);
    }
    else {
        CMPIData key = CMGetKey(argdata.value.ref, "InstanceID", status);
        if(key.type == CMPI_string) {
            _CMPIStrncpySystemNameFromID(buf, CMGetCharPtr(key.value.string), sizeof(buf)/sizeof(buf[0]));
            uuid = buf;
        }
    }

    if (!xen_network_get_by_uuid(session->xen, &network, uuid)) {
        status->rc = CMPI_RC_ERR_NOT_FOUND;
        goto Exit;
    }
    Exit:
    return network;
}

/* 
 * Reads the 'ResourceSettings' parameter - 
 * Identifies the the PIFs required by methods
 */
static CMPIrc _get_resource_settings(
    const CMPIBroker *broker, 
    const CMPIArgs* argsin, 
    xen_utils_session *session,
    xen_pif_record_set **pif_recs,
    bool *create_bond,
    CMPIStatus *status
    )
{
    *pif_recs = NULL;
    CMPIData argdata;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;

    /* Find if the 'ResourceSettings' parameter is available */
    if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_instanceA, &argdata, status)) {
        if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_stringA, &argdata, status)) {
            if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_refA, &argdata, status)) {
                if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_instance, &argdata, status)) { /* not an array, single resource case */
                    if (!_GetArgument(broker, argsin, "ResourceSettings", CMPI_string, &argdata, status)) { /* not an array, single resource case */
                        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("ResourceSettings not found"));
                        statusrc = CMPI_RC_ERR_NOT_FOUND;
                        goto Exit;
                    }
                }
            }
        }
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Checking  PIFs"));
    if ((CMPI_ARRAY & argdata.type) || (CMPI_instance & argdata.type) || 
        (CMPI_chars & argdata.type) || (CMPI_string & argdata.type)) {
        if (CMPI_ARRAY & argdata.type) {
            int num_resources = CMGetArrayCount(argdata.value.array, NULL);
            // If more than 1 network RASD was specified,
            // we are expected to create a bond
            if (num_resources > 1)
                *create_bond = true;
            int i;
            for (i = 0; i < num_resources; i++) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Getting ResourceSettings array element %d", i));
                CMPIData setting_data = CMGetArrayElementAt(argdata.value.array, i, status);
                if ((status->rc != CMPI_RC_OK) || CMIsNullValue(setting_data))
                    goto Exit;
                if (!host_network_port_rasd_parse(broker, session, &setting_data, pif_recs, create_bond, status))
                    goto Exit;
            }
        }
        else {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Parsing the RASD"));
            if (!host_network_port_rasd_parse(broker, session, &argdata, pif_recs, create_bond, status))
                goto Exit;
        }
        statusrc = CMPI_RC_OK;
    }
    Exit:
    if (statusrc  != CMPI_RC_OK) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("_get_resource_settings returning error %d", statusrc));
        if (*pif_recs) {
            xen_pif_record_set_free(*pif_recs);
            *pif_recs = NULL;
        }
    }
    return statusrc;
}
//
// From the pif record set passed in, find all pif records that belong to the same host
// so they can be bonded together. 
// 
// Mark that set, so that the next time we call this API, we dont get the same set.
//
static int _find_next_bondable_pif_set(
    const CMPIBroker *broker,
    xen_utils_session *session,
    xen_pif_record_set *pif_rec_set, 
    xen_pif_set **pif_set)
{
    int i = 0, rc = 0;
    char * host_uuid_to_check_for = NULL;

    if (!pif_rec_set)
        return 0;

    *pif_set = NULL;
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
        ("Finding PIFs (from %d PIFs) that can be bonded together....", pif_rec_set->size));

    for (i=0; i < pif_rec_set->size; i++) {
        if (pif_rec_set->contents[i]->host) {
            char *uuid = NULL;
            if (!xen_host_get_uuid(session->xen, &uuid, pif_rec_set->contents[i]->host->u.handle))
                goto Exit;
            if (!host_uuid_to_check_for)
                host_uuid_to_check_for = uuid;
            if (strcmp(uuid, host_uuid_to_check_for) == 0) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                    ("Found PIF (%s) for host %s", pif_rec_set->contents[i]->device, uuid));
                xen_pif pif = NULL;
                if (xen_pif_get_by_uuid(session->xen, &pif, pif_rec_set->contents[i]->uuid))
                    ADD_DEVICE_TO_LIST((*pif_set), pif, xen_pif);
                // mark this host off as 'already checked', by setting it to null
                xen_host_record_opt_free(pif_rec_set->contents[i]->host);
                pif_rec_set->contents[i]->host = NULL;
                rc = 1;
            }
            if (host_uuid_to_check_for != uuid)
                free(uuid);
        }
    }
    Exit:
    if (host_uuid_to_check_for)
        free(host_uuid_to_check_for);
    return rc;
}

static void _destroy_pif(
    xen_utils_session *session, 
    xen_pif pif
    )
{
    /* unplug the pif */
    xen_pif_unplug(session->xen, pif);
    xen_bond_set *bonds = NULL;

    /* If this pif is part of a bond, break the bond */
    if (xen_pif_get_bond_master_of(session->xen, &bonds, pif) && bonds) {
        int j;
        for (j=0; j<bonds->size; j++)
            xen_bond_destroy(session->xen, bonds->contents[j]);
        xen_bond_set_free(bonds);
    }

    /* now destroy the pif */
    xen_pif_destroy(session->xen, pif);
}

/* Create either a Bond (more than one pif record specified) or a VLAN (1 pif_record specified) */
static bool _create_bond_or_vlan(
    const CMPIBroker *broker,
    xen_utils_session *session,
    xen_network net,
    xen_pif_record_set *pif_rec_set,
    bool create_bond)
{
    bool success = true;
    if (!create_bond) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Creating 'External' VLAN network"));
        int i;
        for (i=0; i<pif_rec_set->size; i++) {
            xen_pif_record *pif_rec = pif_rec_set->contents[i];
            xen_pif pif = NULL;
            xen_vlan vlan = NULL;
            if(xen_pif_get_by_uuid(session->xen, &pif, pif_rec_set->contents[i]->uuid) && pif != NULL) {
                if (xen_vlan_create(session->xen, &vlan, pif, pif_rec->vlan, net)) {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Tagged VLAN for PIF %s(%s)", 
                                                           pif_rec_set->contents[i]->uuid,
                                                           pif_rec_set->contents[i]->device));
                    xen_vlan_free(vlan);
                }
                else {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VLAN network creation failed for %s(%s)", 
                                                           pif_rec_set->contents[i]->uuid,
                                                           pif_rec_set->contents[i]->device));
                    success = false;
                }
                xen_pif_free(pif);
                if(!success)
                    break;
            }
        }
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Creating 'Bonded' network"));
        // create a bond for the pifs specified, on every host 
        xen_bond bond = NULL;
        xen_pif_set *pif_set = NULL;
        // find the pifs on each host and bond them
        while (_find_next_bondable_pif_set(broker, session, pif_rec_set, &pif_set)) {
	  //'Balance-slb is the default bonding mode
	  if (!xen_bond_create(session->xen, &bond, net, pif_set, NULL, XEN_BOND_MODE_BALANCE_SLB, NULL)) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Bond creation failed"));
                success = false;
                break;
            }
            if (bond)
                xen_bond_free(bond);
            xen_pif_set_free(pif_set);
            pif_set = NULL;
        }
    }
    return success;
}


