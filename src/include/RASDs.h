// Copyright (C) 2006 IBM Corporation
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
// Authors:       Dr. Gareth S. Bestor, <bestor@us.ibm.com>
// Contributors:  Jim Fehlig, <jfehlig@novell.com>
// Description:
// ============================================================================
//

#ifndef __RASDS_H__
#define __RASDS_H__

#include <cmpidt.h>
#include "xen_utils.h"

typedef enum {
    resource_add = 0,
    resource_delete,
    resource_modify
} vm_resource_operation;

int proc_rasd_to_vm_rec(const CMPIBroker* broker, CMPIInstance *proc_rasd, xen_vm_record *vm_rec, vm_resource_operation op, CMPIStatus  *status);
int proc_rasd_from_vm_rec(const CMPIBroker* broker, xen_utils_session *session, CMPIInstance *inst, xen_vm_record * vm_rec);
int proc_rasd_modify(xen_utils_session *session, xen_vm vm,  xen_vm_record *vm_rec_template);
void set_processor_defaults(xen_vm_record *vm_rec);
CMPIObjectPath *proc_rasd_create_ref(const CMPIBroker *broker, const char *name_space, xen_utils_session *session, xen_vm_record *vm_rec);

int memory_rasd_to_vm_rec(const CMPIBroker* broker, CMPIInstance *mem_rasd, xen_vm_record *vm_rec, vm_resource_operation op, CMPIStatus *status);
int memory_rasd_from_vm_rec(const CMPIBroker* broker, xen_utils_session *session, CMPIInstance* inst,xen_vm_record *vm_rec);
int memory_rasd_modify(xen_utils_session *session, xen_vm vm,  xen_vm_record *vm_rec_template);
void set_memory_defaults(xen_vm_record *vm_rec);
CMPIObjectPath *memory_rasd_create_ref(const CMPIBroker *broker, const char *name_space, xen_utils_session *session, xen_vm_record *vm_rec);

int disk_rasd_to_vbd(const CMPIBroker* broker, xen_utils_session* session, CMPIInstance *disk_rasd, xen_vbd_record **vbd_rec, xen_vdi_record **vdi_rec, xen_sr  *sr, CMPIStatus *status);
int disk_rasd_from_vbd(const CMPIBroker* broker, xen_utils_session *session, CMPIInstance *inst, xen_vm_record *vm_rec, xen_vbd_record *vbd_rec, xen_vdi_record *vdi_rec);
int disk_rasd_modify(xen_utils_session *session, xen_vbd_record *vbd_rec_template, xen_vdi_record *vdi_rec_template);
CMPIObjectPath *disk_rasd_create_ref(const CMPIBroker *broker, const char *name_space, xen_utils_session *session, xen_vm_record *vm_rec, xen_vbd vbd);

int network_rasd_to_vif(const CMPIBroker* broker, xen_utils_session *session, CMPIInstance *nic_rasd, bool strict_checks, xen_vif_record **vif_rec, CMPIStatus *status);
int network_rasd_from_vif(const CMPIBroker* broker, xen_utils_session *session, xen_vif_record* vif, CMPIInstance* inst);
int network_rasd_modify(xen_utils_session* session, xen_vif_record *vif_rec_template);
CMPIObjectPath *network_rasd_create_ref(const CMPIBroker *broker, const char *name_space, xen_utils_session *session, xen_vm_record *vm_rec, xen_vbd vbd);

int console_rasd_to_xen_console_rec(const CMPIBroker *broker, CMPIInstance *con_rasd, xen_console_record **con_rec, CMPIStatus *status);
int console_rasd_from_xen_console_rec(const CMPIBroker* broker, xen_utils_session *session, CMPIInstance *inst, xen_console_record* con_rec);
CMPIObjectPath* console_rasd_create_ref(const CMPIBroker *broker, const char *name_space, xen_utils_session *session, xen_vm_record *vm_rec, xen_console con);
int xen_console_rec_to_console_rasd(const CMPIBroker* broker, xen_utils_session *session, CMPIInstance *inst, xen_console_record* con_rec);

CMPIObjectPath *disk_image_create_ref(const CMPIBroker *broker, const char *name_space, xen_utils_session *session, char *sr_uuid, char *vdi_uuid);

bool host_network_port_rasd_parse(
    const CMPIBroker *broker, 
    xen_utils_session *session,
    CMPIData *setting_data,
    xen_pif_record_set **pif_rec_set,
    bool *bonded_set,
    CMPIStatus *status
    );

#endif      /* __RASDS_H__ */
