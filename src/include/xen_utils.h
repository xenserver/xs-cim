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
// Authors:        Jim Fehlig, <jfehlig@novell.com>
// Contributors:
// Description:    Utilitiy functions built on top of libxen for use in all
//                 providers.
// ============================================================================

#if !defined(__XEN_UTILS_H__)
#define __XEN_UTILS_H__

#include <string.h>
#include <time.h>
#include <xen/api/xen_all.h>

#include <cmpidt.h>
#include <cmpimacs.h>
#include <cmpilify.h>
#include <curl/curl.h>

#include "Xen_KVP.h"

#define GUID_STRLEN 36
/*
 * A structure for encapsulating a Xen session.  Providers should
 * retrieve an instance of this object using xen_utils_xen_init() when
 * first loaded into the cimom.  xen_utils_xen_close() should be called
 * when cimom invokes the provider's Cleanup() method.
 */
#define MAX_HOST_URL_LEN 100
typedef struct {
    xen_session *xen;
    char host_url[MAX_HOST_URL_LEN];
    xen_host host;
    CURL *curl_handle;
} xen_utils_session;


enum domain_choice {
    vms_only=0,
    templates_only,
    snapshots_only,
    all
};

/*
 * A structure for encapsulating domain resources.
 */
typedef struct {
    xen_vm_set *domains;         /* List of domains */
    unsigned int numdomains;     /* Totoal number of domains */
    unsigned int currentdomain;  /* Current domain in the list */
    enum domain_choice choice; /* do we want to enumerate templates/vms/snapshots/all */
} xen_domain_resources;

    #define INSTANCEID_SEPARATOR_CHAR '/' /* used to spearate out the elements that make up an instanceID string */
    #define XEN_UTILS_ERROR_BUF_LEN   512


/*
 * Macro for populating a char buffer with error messages contained
 * in xen session object.
 * buf is expected to be of length XEN_UTILS_ERROR_BUF_LEN
 */
    #define XEN_UTILS_GET_ERROR_STRING(buf, session)                           \
{                                                                          \
   memset(buf, 0, XEN_UTILS_ERROR_BUF_LEN);                                \
   int ndx;                                                                \
   for (ndx = 0; ndx < session->error_description_count; ndx++)            \
   {                                                                       \
      strncat(buf, session->error_description[ndx],                        \
              XEN_UTILS_ERROR_BUF_LEN - (strlen(buf) + 1));                \
   }                                                                       \
}                                                                          \

/*
 * This macro can be used to generate functions for concatenating lists of
 * Xen API reference objects (UUIDs).  This macro is used in xen_utils.c to
 * generate the xen_*_set_concat() functions declared below.
 */
    #define XEN_UTILS_REF_LIST_CONCAT(type__)                                       \
int type__ ## _set_concat(type__ ## _set **target, type__ ## _set *source)      \
{                                                                               \
   int total_size;                                                              \
   int i, j;                                                                    \
   /* Nothing to concatenate if source is empty */                              \
   if (source == NULL || source->size == 0)                                     \
      return 1;                                                                 \
   /* If targe is empty, assign source to target */                             \
   if (*target == NULL) {                                                       \
      *target = source;                                                         \
      return 1;                                                                 \
   }                                                                            \
   /* realloc memory and append source to target */                             \
   total_size = (*target)->size + source->size;                                 \
   *target = realloc(*target, sizeof(type__ ## _set) +                          \
                     (total_size * sizeof(type__)));                            \
   if (*target == NULL)                                                         \
      return 0;                                                                 \
   for (j = (*target)->size, i = 0; i < source->size; i++) {                    \
      (*target)->contents[j + i] = source->contents[i];                         \
      source->contents[i] = NULL;                                               \
   }                                                                            \
   (*target)->size = total_size;                                                \
   /* Free source list - it has been copied to target */                        \
   type__ ## _set_free(source);                                                 \
   return 1;                                                                    \
}


int xen_vm_set_concat(xen_vm_set **target, xen_vm_set *source);
int xen_vdi_set_concat(xen_vdi_set **target, xen_vdi_set *source);
int xen_vbd_set_concat(xen_vbd_set **target, xen_vbd_set *source);
int xen_vif_set_concat(xen_vif_set **target, xen_vif_set *source);


/*
 * This macro can be used to generate functions for adding a Xen API
 * reference objects (UUIDs) to a list of UUIDs.  Currently, libxenapi
 * does not support growing lists.  For now, the macro is used in xen_utils.c to
 * generate the xen_*_set_add() functions declared below.
 *
 * TODO:
 * Submit this upstream so xen_*_set_add functions are available in
 * the c-bindings?
 */
    #define XEN_UTILS_REF_LIST_ADD(type__)                                          \
int type__ ## _set_add(type__ ## _set *list, type__  device)                    \
{                                                                               \
   if (list == NULL) {                                                          \
      list = type__ ## _set_alloc(1);                                           \
      if (list == NULL)                                                         \
         return 0;                                                              \
      list->size = 1;                                                           \
      list->contents[0] = device;                                               \
      return 1;                                                                 \
   }                                                                            \
                                                                                \
   /* List is not empty.  Grow the list and add the new device */               \
   int new_len = sizeof(type__ ## _set) + ((list->size + 1) * sizeof(type__));  \
   list = realloc(list, new_len);                                               \
   if (list == NULL)                                                            \
      return 0;                                                                 \
   list->contents[list->size] = device;                                         \
   list->size++;                                                                \
   return 1;                                                                    \
}


int xen_vm_set_add(xen_vm_set *list, xen_vm vm);
int xen_vdi_set_add(xen_vdi_set *list, xen_vdi vdi);
int xen_vbd_set_add(xen_vbd_set *list, xen_vbd vbd);
int xen_vif_set_add(xen_vif_set *list, xen_vif vif);


/*
 * This macro can be used to generate functions for adding a Xen API
 * object to a list of such objects.  Currently, libxenapi does not
 * support growing lists.  For now, the macro is used in xen_utils.c to
 * generate the xen_*_set_add() functions declared below.
 *
 * TODO:
 * 1. It is very similar to above :-).  I can't figure out a clean way to
 *    handle slight difference (second parameter of the generated function).
 * 
 * 2. Submit this upstream so xen_*_set_add functions are available in
 *    the c-bindings?
 */
    #define XEN_UTILS_DEV_LIST_ADD(type__)                                          \
int type__ ## _set_add(type__ ## _set *list, type__  *device)                   \
{                                                                               \
   if (list == NULL) {                                                          \
      list = type__ ## _set_alloc(1);                                           \
      if (list == NULL)                                                         \
         return 0;                                                              \
      list->size = 1;                                                           \
      list->contents[0] = device;                                               \
      return 1;                                                                 \
   }                                                                            \
                                                                                \
   /* List is not empty.  Grow the list and add the new device */               \
   int new_len = sizeof(type__ ## _set) + ((list->size + 1) * sizeof(type__));  \
   list = realloc(list, new_len);                                               \
   if (list == NULL)                                                            \
      return 0;                                                                 \
   list->contents[list->size] = device;                                         \
   list->size++;                                                                \
   return 1;                                                                    \
}


int xen_vdi_record_set_add(xen_vdi_record_set *list, xen_vdi_record *device);
int xen_vbd_record_set_add(xen_vbd_record_set *list, xen_vbd_record *device);
int xen_vif_record_set_add(xen_vif_record_set *list, xen_vif_record *device);


/*
 * Initialize a session with Xen.  Providers should acquire a Xen
 * Session when loaded into the cimom, i.e. when the provider's
 * Initialize() method is invoked by the cimom.
 *
 * On success a xen_utils_session object is placed in parameter
 * session.  The object is connected and logged-in to the Xen Daemon
 *  and ready for use.
 *
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_xen_init2(xen_utils_session **session, struct xen_call_context *ctx);
int xen_utils_xen_init();


/*
 * Close Xen session.  Connection to Xen Daemon is closed and the
 * session handle is freed.  Providers should invoke this function
 * when their Cleanup() method is invoked by the cimom.
 */
void xen_utils_xen_close2(xen_utils_session *session);
void xen_utils_xen_close();

int xen_utils_get_call_context(const CMPIContext *cmpi_ctx, struct xen_call_context **ctx, CMPIStatus* status);
void xen_utils_free_call_context(struct xen_call_context *ctx);

/*
 * Validate xen session.  If sesssion is null, create one.
 * Session is ready for use on success.
 *
 * Returns a non-zero on success, 0 on failure.
 */
int xen_utils_validate_session(xen_utils_session **session, struct xen_call_context *ctx);
int xen_utils_cleanup_session(xen_utils_session *session); /* logout and free */
int xen_utils_free_session(xen_utils_session *session); /* free, dont logout */
int xen_utils_get_session(xen_utils_session **session, char *user, char *pw);
int xen_utils_get_remote_session(xen_utils_session **session, char *xen_host_ip_addr, char *remote_login_user, char *remote_login_pw);

int xen_utils_get_time(void);

/*
 * Retrieve the domain resources (a list of VMs) using the provided
 * session.
 * 
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_get_domain_resources(xen_utils_session *session,
    xen_domain_resources **resources,
    enum domain_choice temlates_or_vms);

/*
 * Free the list of domain resources.
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_free_domain_resources(xen_domain_resources *resources);

/*
 * Retrieve the next domain from the list of domain resources.
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_get_next_domain_resource(
    xen_utils_session *session,
    xen_domain_resources *resources,
    xen_vm *resource_handle,                                  
    xen_vm_record **resource_rec);

/*
 * Free the domain resource specified by resource.
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_free_domain_resource(
    xen_vm resource_handle,
    xen_vm_record *resource_rec);

/*
 * Retrieve a domain resource given a CIM_SettingData object path.
 * Domain record will be placed in out param "domain".
 * vm handle will be placed in out param "vm"
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_get_domain_from_sd_OP(xen_utils_session *session,
    xen_vm_record **domain,
    xen_vm *vm,
    const CMPIObjectPath *domOP);
int xen_utils_get_domain_from_instanceID(xen_utils_session *session,
    char *instanceID, 
    xen_vm_record**domain,
    xen_vm* vm );
int xen_utils_get_domain_from_uuid(xen_utils_session *session,
    const char *uuid, 
    xen_vm_record**domain,
    xen_vm* vm );

xen_host_record* xen_utils_get_domain_host(
    xen_utils_session *session,
    xen_vm_record *domain_rec
    );

void xen_utils_free_domain_host(
    xen_vm_record *domain_rec,
    xen_host_record *host_rec
    );
/*
 * Determine if the domain specified by name active.
 * If active, isActive will be set to 1, otherwise 0.
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_is_domain_active(
    xen_utils_session *session,
    const char *name, 
    int *isActive);

/*
 * Add a string to a string set.  
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_add_to_string_set(char *str, xen_string_set **set);
CMPIArray *xen_utils_convert_string_set_to_CMPIArray(
    const CMPIBroker *broker,
    xen_string_set *set
    );
xen_string_set *xen_utils_copy_to_string_set(char *str, char *delimiter);
char *xen_utils_flatten_string_set(xen_string_set *strset, char* delimiter);
char *xen_utils_flatten_CMPIArray(CMPIArray *arr);

xen_string_string_map *
xen_utils_convert_CMPIArray_to_string_string_map(CMPIArray *arr);

/*
 * Get value of key from map.
 * Returns pointer to value on success, NULL if key does not exist in map.
 */
char *xen_utils_get_from_string_string_map(
    xen_string_string_map *map,
    const char *key);

/* Free all entries in the string string map and set *map to NULL
 * Return 1 if successful, 0 if not
 */
int xen_utils_clear_string_string_map(
    xen_string_string_map **map);

/*
 * Add key/val to map.  If map contains key, update value for that key.
 * Returns non-zero on success, 0 on failure.  On failure, map is unchanged.
 */
int xen_utils_add_to_string_string_map(
    const char *key, 
    const char *val,
    xen_string_string_map **map);
int xen_utils_remove_from_string_string_map(
    char *key_to_remove,
    xen_string_string_map **map
    );

/*
 * Flatten a Xen API string-string map.  The flattened map will be in form
 * key0=value0,key1=value1,...,keyN=valueN
 * Returns char array containing flattened map on success, NULL on failure.
 * Caller is responsible for freeing memory.
 */
char *xen_utils_flatten_string_string_map(
    xen_string_string_map *map);

/*
* Create a string map from a 'flattened' string map - opposite of flatten
* Converts a string of form key0=value0,key1=value1,...keyN=valueN
* into a xen_string_string_map
* caller will free the map returned
*/
xen_string_string_map* xen_utils_convert_string_to_string_map(
    const char *str,
    const char *delimiter);
/* 
 * Convert a xen_string_string_map to a CMPIArray of strings of the form
 * 'key0:value0','key1:value1'..
 * returns the newly created CMPIArray 
 */
CMPIArray *xen_utils_convert_string_string_map_to_CMPIArray(
    const CMPIBroker *broker,
    xen_string_string_map *xen_map);

/* Epoch Time to CMPI date time conversion routines */
CMPIDateTime *xen_utils_time_t_to_CMPIDateTime(
    const CMPIBroker *broker, time_t time);

time_t xen_utils_CMPIDateTime_to_time_t(
    const CMPIBroker *broker, CMPIDateTime *time);

CMPIDateTime* xen_utils_CMPIDateTime_now(
    const CMPIBroker* broker);

/*
 * Trace the error descriptions found in xen session object.
 * This routine uses _sblim_trace function in cmpitrace interface
 * for actual tracing.  Output is to a location specified in the
 * cmpitrace module.
*/
void xen_utils_trace_error(xen_session *session, char *file, int line);
void xen_utils_set_status(const CMPIBroker *broker, CMPIStatus *status, int rc, char *default_msg, xen_session *session);
char* xen_utils_get_xen_error(xen_session *session);

xen_vm_set* xen_utils_enum_domains(
    xen_utils_session *session,
    enum domain_choice);

int xen_utils_log_domains(xen_vm_set *vm_set);

/* Determine the allocation units to use based on the string input.
 * return the units found in bytes or 0 on failure
 */
int64_t xen_utils_get_alloc_units(const char *allocStr);

/* Utility functions to parse embedded instances in mof format */
CMPIInstance *xen_utils_parse_embedded_instance(
    const CMPIBroker *broker, 
    const char *instanceStr);

int xen_utils_get_cmpi_instance(
    const CMPIBroker *broker,    /* in  */
    CMPIData *setting_data,      /* in  */
    CMPIObjectPath **objectpath, /* out */
    CMPIInstance **instance);    /* out */

int xen_utils_get_affectedsystem(
    const CMPIBroker *broker,   /* in */
    xen_utils_session *session, /* in */
    const CMPIArgs* argsin,     /* in */
    CMPIStatus *status,         /* in/out */
    xen_vm_record **vm_rec,     /* out */
    xen_vm *vm                  /* out */
    );
int xen_utils_get_vssd_param(
    const CMPIBroker *broker, 
    xen_utils_session *session,
    const CMPIArgs *argsin,
    const char *param_name,
    CMPIStatus *status,
    CMPIInstance** vssd_inst
    );
bool xen_utils_class_is_subclass_of(
    const CMPIBroker *broker,
    const char *class_to_check, 
    const char *superclass);

char *xen_utils_CMPIObjectPath_to_WBEM_URI(
    const CMPIBroker *broker,
    CMPIObjectPath *obj_path
    );
CMPIObjectPath *xen_utils_WBEM_URI_to_CMPIObjectPath(
    const CMPIBroker *broker,
    const char *wbem_uri
    );


/* Generic function for pulling the contents of a URL into a buffer */
long get_from_url(char* url, char** buffer);

/* KVP functions */
int initialise_kvp(kvp **kvp_obj);
int initialise_kvp_set(kvp_set** set);
int xen_utils_create_kvp(char *key, char *value, char *vm_uuid, kvp **kvp_obj);
int xen_utils_kvp_copy(kvp *orig, kvp** new);
Xen_KVP_RC xen_utils_get_kvp_store(char* url, char* vm_uuid, kvp_set **kvps);
int xen_utils_append_kvp_set(kvp_set *dest, kvp_set *src);
int xen_utils_free_kvp(kvp *kvp);
int xen_utils_free_kvpset(kvp_set *set);
Xen_KVP_RC xen_utils_delete_kvp(xen_utils_session *session, kvp *kvp_obj);
Xen_KVP_RC xen_utils_get_from_kvp_store(xen_utils_session *session, char *vm_uuid, char *key, char **value);
Xen_KVP_RC xen_utils_push_kvp(xen_utils_session *session, kvp *kvp_obj);
Xen_KVP_RC xen_utils_setup_kvp_channel(xen_utils_session *session, char *vm_uuid);
Xen_KVP_RC xen_utils_preparemigration_kvp_channel(xen_utils_session *session, char *vm_uuid);
Xen_KVP_RC xen_utils_finishmigration_kvp_channel(xen_utils_session *session, char *vm_uuid);

/* transfer VM record parsing routines */
char *xen_utils_get_value_from_transfer_record(char *dict, char *key);
char *xen_utils_get_uri_from_transfer_record(char *record);
xen_string_string_map *xen_utils_convert_transfer_record_to_string_map(char *transfer_record);

/*
 * Wild macro to support adding devices (vbd, vif) to as list of such
 * devices. To be used only where list of devices is
 * collected from the incoming RASDs.
 */
    #define ADD_DEVICE_TO_LIST(__list, __device, __type)                            \
{                                                                               \
   if (__list == NULL) {                                                        \
      __list = __type ## _set_alloc(1);                                         \
      if (__list == NULL) {                                                     \
         _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,                                 \
                      ("Cannot malloc memory for xen device settings list"));   \
         __type ## _free(__device);                                             \
         goto Exit;                                                             \
      }                                                                         \
      __list->contents[0] = __device;                                           \
      __device = NULL;                                                          \
   }                                                                            \
   /* List is not empty.  Grow the list and add the new device */               \
   else {                                                                       \
      int __idx;                                                                \
      __type ## _set *__newlist = __type ## _set_alloc(__list->size + 1);       \
      if (__newlist == NULL) {                                                  \
         _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,                                 \
             ("Cannot malloc memory for xen device settings list of size %d",   \
              __list->size));                                                   \
         __type ## _free(__device);                                             \
         goto Exit;                                                             \
      }                                                                         \
      for (__idx=0; __idx<__list->size; __idx++) {                              \
           __newlist->contents[__idx] = __list->contents[__idx];                \
           __list->contents[__idx] = NULL;                                      \
      }                                                                         \
      /* free old, copy new */                                                  \
      __type ## _set_free(__list);                                              \
      __list = __newlist;                                                       \
      __list->contents[__list->size-1] = __device;                              \
      __device = NULL;                                                          \
   }                                                                            \
}

#define RESET_XEN_ERROR( _session_ )    \
if(!_session_->ok) {                    \
    xen_session_clear_error(_session_); \
}

int _GetArgument(
    const CMPIBroker* broker,
    const CMPIArgs* argsin,
    const char* argument_name,
    const CMPIType argument_type,
    CMPIData* argument,
    CMPIStatus* status
    );

#endif /* __XEN_UTILS_H__ */
