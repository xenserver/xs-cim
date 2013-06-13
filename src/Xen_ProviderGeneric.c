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

/* Common declarations for each CMPI "Cimpler" instance provider */
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#include <cmpidt.h>
#include <cmpimacs.h>

#include "cmpilify.h"
#include "cmpitrace.h"

static const CMPIInstanceMI* mi;

#define _BROKER (((CMPILIFYInstanceMI*)(mi->hdl))->brkr)
#define _CLASS (((CMPILIFYInstanceMI*)(mi->hdl))->cn)
//#define _KEYS (((CMPILIFYInstanceMI*)(mi->hdl))->kys)

#include <stdlib.h>
#include "xen_utils.h"
#include "provider_common.h"

/* C structs to store the data for all resources. */

/* The provider_resource structure is the individual resource handled
 * by the provider. The provider can add their own specific resource
 * by defining PROVIDER_RES_REC  */
typedef struct
{
    const char *classname;
    PROVIDER_RES_REC;
    xen_utils_session *session;
    bool cleanupsession;
} provider_resource;

/* The provider_resource_list is the list of resources enumerated
 * by the provider. The list definition can be augmented by 
 * defining the PROVIDER_RES_LIST in the imeplementation file */
typedef struct
{
    const char *classname;
    PROVIDER_RES_LIST;  
    int current_resource; /* index of the current resource - used during enumeration */
    xen_utils_session *session;
} provider_resource_list;

const char* get_key_property(const char *classname);
const char** get_keys(const char *classname);
CMPIrc 
resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst);
static CMPIrc 
delete_resource(
    xen_utils_session *session, 
    char *inst_id);
static CMPIrc 
modify_resource(
    const void *res_id, 
    const void *modified_res,
    const char **properties, 
    CMPIStatus status, 
    char *inst_id, 
    xen_utils_session *session);
static CMPIrc 
extract_resource(
    void **res, const 
    CMPIInstance *inst, 
    const char **properties);
static CMPIrc 
enum_xen_resource_list(
    xen_utils_session *session, 
    provider_resource_list *resources);
static CMPIrc 
cleanup_xen_resource_list(
    provider_resource_list *resources);
static CMPIrc 
getnext_xen_resource_record(
    provider_resource_list *resources_list, 
    xen_utils_session *session, 
    provider_resource *prov_res);
void 
cleanup_xen_resource_record(
    provider_resource *prov_res);
#ifdef  READ_ONLY_PROVIDER
static CMPIrc 
get_xen_resource_record_from_id(
    xen_utils_session *session,
    provider_resource *prov_res);
#else
static CMPIrc 
get_xen_resource_record_from_id(
    char *res_uuid,
    xen_utils_session *session,
    provider_resource *prov_res);
#endif
static CMPIrc 
add_xen_resource();

/* CMPILIFY abstraction methods. Look at cmpilify.c on how these get called  */
/* Load gets called when the provider is first loaded by the CIMOM */
bool g_bProviderLoaded = false;
static CMPIrc load()
{
    /* Initialized Xen session object. */
    xen_utils_xen_init();
    g_bProviderLoaded = true;
    return CMPI_RC_OK;
}

/* Unload gets called when the provider is unloaded by the CIMOM */
static CMPIrc unload(const int terminating)
{
    (void)terminating;

    /* Close Xen session object. */
    if(g_bProviderLoaded) {
        /* this is to overcome a bug, where the CIMOM calls unload on the instanceMI
           over and over again, if this provider's methodMI interface returns
           DO_NOT_UNLOAD */
        xen_utils_xen_close();
        g_bProviderLoaded = false;
    }
    return CMPI_RC_OK;
}

/* Begin gets called when cmpilify wants to begin enumerating
   the backend resources represented by this class */
static CMPIrc begin(void **res_list, 
                    const char *classname, 
                    void *ctx, 
                    const char **properties)
{
    CMPIrc rc = CMPI_RC_OK;
    provider_resource_list *resources = NULL;
    xen_utils_session *session = NULL;
    (void)properties;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Begin enumerating %s", classname));
    if(res_list == NULL)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error res_list = NULL"));
        return CMPI_RC_ERR_FAILED;
    }

    if(!xen_utils_validate_session(&session, ctx))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- Unable to establish connection with Xen"));
        return CMPI_RC_ERR_FAILED;
    }
    resources = (provider_resource_list *)calloc(1, sizeof(provider_resource_list));
    if(resources == NULL)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Could not allocate memory for resources"));
        return CMPI_RC_ERR_FAILED;
    }
    resources->classname = classname;

    /* Make Xen call to populate the resources list */
    rc = enum_xen_resource_list(session,resources);
    if(rc != CMPI_RC_OK)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error Did not get xen resource list"));       
        goto Error;
    }
    resources->session = session;
    *res_list = (void *)resources;
    return rc;

Error:
    xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    cleanup_xen_resource_list(resources);
    xen_utils_cleanup_session(session);         
    if(resources)
        free(resources);

    return CMPI_RC_ERR_FAILED;
}

/* End gets called when cmpilify wants to end enumerating
   the backend resources represented by this class */
static void end(void *res_list)
{
    provider_resource_list *resources = (provider_resource_list *)res_list;
    if(resources)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("End enumerating %s", resources->classname));
        xen_utils_cleanup_session(resources->session);
        cleanup_xen_resource_list(resources);
        free(resources);
    }
}

/* End gets called when cmpilify wants to get the next backend resource */
static CMPIrc getnext(void *res_list, void **res, const char **properties)
{
    CMPIrc rc = CMPI_RC_OK;
    provider_resource_list *resources_list = (provider_resource_list *)res_list;
    (void)properties;
    if(resources_list == NULL || res == NULL)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error XenProviderGeneric:getnext:resource_list or res is NULL"));
        return CMPI_RC_ERR_FAILED;
    }

    /* Get the current resource record. */
    RESET_XEN_ERROR(resources_list->session->xen);
    provider_resource *prov_res = calloc(1, sizeof(provider_resource));
    if(prov_res == NULL)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error calloc failed"));
        return CMPI_RC_ERR_FAILED;
    }

    prov_res->classname = resources_list->classname;
    rc = getnext_xen_resource_record(resources_list, resources_list->session, prov_res);
    if(rc != CMPI_RC_OK)
    {
        if(rc != CMPI_RC_ERR_NOT_FOUND)
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_WARNING, ("Error getnext OK not received "));
        cleanup_xen_resource_record(prov_res);
        free(prov_res);
        return rc;
    }

    prov_res->cleanupsession = false;
    prov_res->session = resources_list->session;
    *res = (void *)prov_res;
    return CMPI_RC_OK;
}

/* This version of get is used by readonly profiles.
 * It has just two parameters instead of three.
 */

#ifdef  READ_ONLY_PROVIDER
static CMPIrc get(struct xen_call_context * caller_id, void **res, const char** properties)
{
    CMPIrc rc = CMPI_RC_OK;
    xen_utils_session *session = NULL;
    (void)properties;

    if(!xen_utils_validate_session(&session, caller_id))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- Unable to establish connection with Xend"));
        return CMPI_RC_ERR_FAILED;
    }

    provider_resource *prov_res = calloc(1, sizeof(provider_resource));
    //prov_res->classname = resources_list->classname;
    rc = get_xen_resource_record_from_id(session,prov_res);
    if(rc != CMPI_RC_OK)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error get(): OK not received from get_xen_resource_record_from_id"));
        cleanup_xen_resource_record(prov_res);
        return rc;
    }
    prov_res->session = session;
    *res = (void *)prov_res;
    return CMPI_RC_OK;
}
#else

/* Get gets called when cmpilify wants to get a backend resource 
 * identified by the keys passed in */
static CMPIrc get(const void *res_id, 
                  struct xen_call_context * caller_id, 
                  void **res, 
                  const char **properties)
{
    CMPIInstance *inst = (CMPIInstance *)res_id;
    xen_utils_session *session = NULL;
    CMPIData data;
    static CMPIrc rc = CMPI_RC_OK;
    char *res_uuid=NULL;
    CMPIStatus status = {CMPI_RC_OK, NULL};
    (void)properties;

    CMPIObjectPath *op = CMGetObjectPath(inst, &status);
    CMPIString *cn = CMGetClassName(op, &status);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Get() for %s", CMGetCharPtr(cn)));
    if(CMIsNullObject(inst) || res == NULL)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error get(): inst or res is NULL"));
        return CMPI_RC_ERR_FAILED;
    }

    const char *key_prop = get_key_property(CMGetCharPtr(cn));
    data = CMGetProperty(inst, key_prop ,&status); 
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(data))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("V-Error get(): macro data not fetched"));
        return CMPI_RC_ERR_INVALID_PARAMETER;
    }

    /* Extract the resource identifier string from the CMPIString. */
    res_uuid = CMGetCharPtr(data.value.string);
    if(res_uuid == NULL)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("Unable to extrace resource identifier string"));
        return CMPI_RC_ERR_FAILED;
    }

    if(!xen_utils_validate_session(&session, caller_id))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("Unable to establish connection with Xen"));
        return CMPI_RC_ERR_FAILED;
    }

    provider_resource *prov_res = calloc(1, sizeof(provider_resource));
    if(prov_res == NULL)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Out of memory"));
        return CMPI_RC_ERR_FAILED;
    }
    prov_res->classname = CMGetCharPtr(cn);

    rc = get_xen_resource_record_from_id(res_uuid, session, prov_res);
    if(rc != CMPI_RC_OK)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error get(): get_xen_resource_record_from_id failed"));
        cleanup_xen_resource_record(prov_res);
        free(prov_res);
        return rc;
    }
    prov_res->cleanupsession = true;
    prov_res->session = session;
    *res = (void *)prov_res;
    return CMPI_RC_OK;
}
#endif

/* Release gets called when cmpilify wants to release a single backend resource */
static void release(void *res)
{
    provider_resource *prov_res = (provider_resource *)res;
    if(prov_res)
    {
        if(prov_res->cleanupsession)
        {
            xen_utils_cleanup_session(prov_res->session);
        }
        cleanup_xen_resource_record(prov_res);
        if(prov_res)
        {
            free(prov_res);
        }
    }
}

/* Add gets called when cmpilify wants to Add a backend resource */
static CMPIrc add(const void *res_id, struct xen_call_context *caller_id, const void *res)
{
    (void)res_id;
    (void)res;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Add an instance"));
    return add_xen_resource();
}

/* Add gets called when cmpilify wants to delete a backend resource */
static CMPIrc delete(const void *res_id, struct xen_call_context *caller_id)
{
    CMPIInstance *inst = (CMPIInstance *)res_id;
    CMPIData data;
    CMPIStatus status = {CMPI_RC_OK, NULL};

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Delete an instance"));
    CMPIObjectPath *op = CMGetObjectPath(inst, &status);
    CMPIString *cn = CMGetClassName(op, &status);

    xen_utils_session *session = NULL;   
    if(CMIsNullObject(inst))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Invalid parameter"));
        return CMPI_RC_ERR_FAILED;
    }

    const char *key_prop = get_key_property(CMGetCharPtr(cn));
    data = CMGetProperty(inst, key_prop ,&status); 
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(data))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get property"));
        return CMPI_RC_ERR_FAILED;
    }

    char *inst_id = CMGetCharPtr(data.value.string);       
    if((inst_id == NULL) || (*inst_id == '\0'))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get inst id"));
        return CMPI_RC_ERR_FAILED;
    }

    if(!xen_utils_validate_session(&session, caller_id))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Unable to establish connection with Xen"));  
        return CMPI_RC_ERR_FAILED;                             
    }
    return delete_resource(session, inst_id);
    /* get the object and delete it */
}

/* Add gets called when cmpilify wants to modify a backend resource */
static CMPIrc modify(
    const void *res_id,
    struct xen_call_context *caller_id, 
    const void *modified_res,
    const char **properties)
{
    (void)properties;
    CMPIData data;
    CMPIStatus status = {CMPI_RC_OK, NULL};
    char *inst_id;
    xen_utils_session *session = NULL;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Modify an Instance"));
    CMPIInstance *inst = (CMPIInstance *)res_id;
    if(CMIsNullObject(inst))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("input parameter res_id is invalid"));
        return CMPI_RC_ERR_FAILED;
    }
    CMPIObjectPath *op = CMGetObjectPath(inst, &status);
    CMPIString *cn = CMGetClassName(op, &status);

    /* Get target resource */
    const char *key_prop = get_key_property(CMGetCharPtr(cn));
    data = CMGetProperty(inst, key_prop, &status); 
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(data))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("Could not get target resource"));
        return CMPI_RC_ERR_FAILED;
    }
    inst_id = CMGetCharPtr(data.value.string);
    if((inst_id == NULL) || (*inst_id == '\0'))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("Could not get inst id"));
        return CMPI_RC_ERR_FAILED;
    }
    if(!xen_utils_validate_session(&session, caller_id))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("--- Unable to establish connection with Xend"));
        return CMPI_RC_ERR_FAILED;
    }

    return(modify_resource(res_id, modified_res, properties, status, inst_id, session));
}


/* setproperties gets called when cmpilify wants to set the properties of a
 * the CIMInstance represented by this class with data from the backend
 * resource 
 */
static CMPIrc setproperties(CMPIInstance *inst, 
    const void *res,
    const char **properties)
{
    provider_resource *resource = (provider_resource *) res;
    if(res == NULL || CMIsNullObject(inst))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Invalid input parameter"));
        return CMPI_RC_ERR_FAILED;
    }

    RESET_XEN_ERROR(resource->session->xen);
    /* Setup a filter to only return the desired properties. */

    const char **keys = get_keys(resource->classname);
    CMSetPropertyFilter(inst, properties, keys);
    return resource_set_properties(resource, inst);
}


/*
 * Set resource data from the CMPIInstance properties.  Only needs to
 * be implemented if add() and/or modify() are supported.
 */
static CMPIrc extract(void **res, const CMPIInstance *inst,
    const char **properties)
{
    /* Provider specific implementation may be done in the extract_resource function */
    return  extract_resource(res, inst, properties); 
}


/* Get resource id from CMPIInstance properties. */
// todo: check this
static CMPIrc extractid(void **res_id, const CMPIInstance* inst)
{
    *res_id = (void *)inst;
    return CMPI_RC_OK;
}


/* Release resource id created in resId4inst(). */
static void releaseid(void* res_id)
{
    (void)res_id;
}

