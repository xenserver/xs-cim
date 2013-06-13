// Copyright (C) 2006 IBM Corporation
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
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include "cmpitrace.h"
#include "cmpiutil.h"
#include "providerinterface.h"

#include "ProxyHelper.h"
#include "Xen_Job.h"


/*=============================================================================
 * This is the CMPI provider interface through which the CIMOM calls the 
 * providers. This inteface in turn looks up the appropriate backend xen
 * provider, loads and calls it.
 *============================================================================*/

/* Global CMPI broker for instance providers */
static const CMPIBroker *_BROKER;

/* Forward declarations */
const XenProviderInstanceFT* prov_pxy_load_xen_instance_provider(
    const CMPIBroker *broker,
    const char *classname
    );
const XenProviderMethodFT* prov_pxy_load_xen_method_provider(
    const CMPIBroker *broker,
    const char *classname
    );
/* Internal functions */
/*****************************************************************************
 * Convert an CIM object path to an CIM instance. Set the key properties
 * of the instance from the object path.
 *
 * @param in op - CIM object path (reference) identifying an object
 * @param out inst - new CIM instance whose key properties have been set
 * @return int 0 on error, 1 on success
 *****************************************************************************/
static int op2inst(
    const CMPIObjectPath* op, 
    CMPIInstance** inst
    )
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    CMPIString* keyname;
    int numkeys, i;
    CMPIData key;
    int rc = 0;

    /* Convert CMPIObjectPath to CMPIInstance. */
    *inst = CMNewInstance(_BROKER, op, &status);
    if (status.rc != CMPI_RC_OK) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: failed to create new CIMPIInstance"));
        goto exit;
    }

    /* Set the CMPIInstance key properties from the CMPIObjectPath keys. */
    numkeys = CMGetKeyCount(op, &status);
    if (status.rc != CMPI_RC_OK) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: failed to count keys"));
        goto exit;
    }
    for (i=0; i<numkeys; i++) {
        key = CMGetKeyAt(op, i, &keyname, &status);
        if (status.rc != CMPI_RC_OK) {
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: failed to retrieve key at position %d/%d", i, numkeys));
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("CMGetKeyAt failed with '%s'",status.msg)); 
            goto exit;
	}
        status = CMSetProperty(*inst, CMGetCharPtr(keyname), &(key.value), key.type);
        if (status.rc != CMPI_RC_OK) {
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: failed to set property"));
            goto exit;
	}
    }
    rc = 1;
    exit:
    return rc;
}
/*****************************************************************************
 * Get a xen resource based on an CIM object path (reference) 
 * identifying a xen object.
 *
 * @param in op - CIM object path (reference) identifying an object
 * @param in caller_ctx - caller's credentials and such
 * @param in ft - xen provider's function table
 * @param in properties - array of properties that caller cares about
 * @param out res - xen resource identified by the CIM reference
 * @return int 0 on error, 1 on success
 *****************************************************************************/
static int getres4op(
    CMPIObjectPath* op, 
    struct xen_call_context *caller_ctx,
    const XenProviderInstanceFT *ft,
    const char** properties,
    void** res
    )
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    void* resId = NULL;
    *res = NULL;
    CMPIInstance* inst;
    int rc = 0;

    if (!op2inst(op, &inst)){
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: cannot convert op2inst"));
        goto exit;
    }

    /* Get the ResourceId for the instance */
    status.rc = prov_pxy_extractid(_BROKER, ft, inst, &resId);
    if (status.rc != CMPI_RC_OK) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: cannot extract ID"));
        goto exit;
    }

    /* First try to get the target resource directly. */
    status.rc = prov_pxy_get(_BROKER, ft, resId, caller_ctx, properties, res);
    if (status.rc != CMPI_RC_OK) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: cannot get object by ID"));
        goto exit;
    }
    rc = 1;

    exit:
    if (resId)
        prov_pxy_releaseid(ft, resId);
    if(rc == 0 && *res != NULL) {
        prov_pxy_release(ft, *res);
        *res = NULL;
    }
    return rc;
}
/*****************************************************************************
 * CMPI interace function.
 * Cleanup routine called by the CIMOM to perform any cleanup before unload
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @param in terminating - provider is terminating
 * @return CMPIStatus error codes
 *****************************************************************************/
CMPIStatus XenCommonCleanup(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    CMPIBoolean terminating)
{
    CMPIStatus status = {CMPI_RC_OK, NULL}; 
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("CMPILIFYInstance_cleanup"));

    /* Run resource provider's unload(). */
    if (prov_pxy_uninit(terminating == CMPI_true) != CMPI_RC_OK)
        CMSetStatusWithChars(_BROKER, &status, (terminating == CMPI_true)?
            CMPI_RC_ERR_FAILED : CMPI_RC_DO_NOT_UNLOAD,
            "CMPILIFY unload() failed");
    return status;
}
/*****************************************************************************
 * Common code to enumerate both CIM references and CIM instances
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @param out result - results containing enumeration
 * @param in ref - CIM reference to the CIM object being enumerated (classname)
 * @param in properties - properties that caller cares about, if CIM instance enum
 * @param in refs_only - references or instanes
 * @return CMPIStatus error codes
 *****************************************************************************/
CMPIStatus enum_call(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char** properties,
    bool refs_only
    )
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    void* resList;
    void* res;
    char* ns;
    unsigned int found = 0;
    CMPIObjectPath* op;
    CMPIInstance* inst;

    _SBLIM_ENTER("CMPILIFYInstance_enumInstanceNames");
    CMPIString *cn = CMGetClassName(ref, &status);
    char *classname = CMGetCharPtr(cn);

    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)) {
        goto exit;
    }
    const XenProviderInstanceFT *ft = prov_pxy_load_xen_instance_provider(_BROKER, CMGetCharPtr(cn));
    if(ft == NULL) {
        CMSetStatus(&status, CMPI_RC_ERR_NOT_SUPPORTED);
        goto exit;
    }

    /* Get list of resources. */
    if (prov_pxy_begin(_BROKER, ft, classname, ctx, refs_only, NULL, &resList) != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED, 
            "CMPILIFY begin() failed");
        goto exit;
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("enumerate resource"));
    /* Enumerate resources and return CMPIObjectPath for each. */
    ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));
    while (1) {
        /* Create new CMPIObjectPath for next resource. */
        op = CMNewObjectPath(_BROKER, ns, classname, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, 
                         ("ERROR: CMNewObjectPath failed with %d", status.rc));
            break;
        }
        /* Create new CMPIInstance for resource. */
        inst = CMNewInstance(_BROKER, op, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, 
                         ("ERROR: CMNewInstance  failed with %d", status.rc));
            break;
        }
	CMPIrc rc = CMPI_RC_OK;
	rc = prov_pxy_getnext(ft, resList, NULL, &res);
        /* Get the next resource using the resource provider's getNext(). */
        if (rc != CMPI_RC_OK) {
	  if (rc == CMPI_RC_ERR_NOT_FOUND) {
	    /* We have reached the end of our resource list and have not more
	     * objects to enumerate. We should break out of this while loop.*/
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("End of resource list %d", rc));
            break;
	  }
	  /* In this case, we have encountered a failure, and so should continue onto
	   * the next object. A failure may have been due to an object being removed
	   * by someone else, which is a valid case and should not cause other failures.*/
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error enumerating object, Continuing to process remaining resources"));
	    continue; /* Continue to next object in resource list */
	 
	}
        /* If specified, set the property filter for CMPIInstance. */
        if (properties) {
            status = CMSetPropertyFilter(inst, properties, NULL);
            if (status.rc != CMPI_RC_OK) {
                CMSetStatus(&status, CMPI_RC_ERR_FAILED);
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, 
                             ("ERROR: CMSetPropertyFilter failed with %d", status.rc));
                break;
            }
        }
        /* Set CMPIInstance properties from resource data. */
        status.rc = prov_pxy_setproperties(ft, inst, res, NULL);
        prov_pxy_release(ft, res);
        if (status.rc != CMPI_RC_OK) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, 
                         ("ERROR: Set properties failed with %d", status.rc));
            CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED, 
                                 "CMPILIFY setproperties() failed");
            continue; /* we skip over this one */
        }

        /* Get the CMPIObjectPath for this CMPIInstance. */
        op = CMGetObjectPath(inst, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
            break;
        }
        status = CMSetNameSpace(op, ns);
        if (status.rc != CMPI_RC_OK) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                         ("Setnamespace failed with %d", status.rc));
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
            break;
        }

        if (refs_only) {
            /* Return the CMPIObjectPath for the resource. */
            status = CMReturnObjectPath(rslt, op);

        }
        else {
            /* Return the CMPIInstance for the resource. */
            status = CMReturnInstance(rslt, inst);
        }
        if (status.rc != CMPI_RC_OK) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                         ("CMReturnInstance failed with %d", status.rc));
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
            break;
        }

        found++;
    } /* while() */
    prov_pxy_end(ft, resList);

    /* Check if enumeration finished OK. */
    if (found) {
        if ((status.rc == CMPI_RC_OK) || (status.rc == CMPI_RC_ERR_NOT_FOUND)) {
            CMReturnDone(rslt);
            CMSetStatus(&status, CMPI_RC_OK);
        }
    }

    exit:
    if (ctx)
        xen_utils_free_call_context(ctx);

    _SBLIM_RETURNSTATUS(status);

}
/*****************************************************************************
 * CMPI interface function
 * Enumerate all CIM references for a particular class
 *****************************************************************************/
CMPIStatus XenCommonEnumInstanceNames(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref)
{
    return enum_call(mi, cmpi_ctx, rslt, ref, NULL, true);
}
/*****************************************************************************
 * CMPI interface function
 * Enumerate all CIM instances for a particular class
 *****************************************************************************/
CMPIStatus XenCommonEnumInstances(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char** properties)
{
    return enum_call(mi, cmpi_ctx, rslt, ref, properties, false);
}
/*****************************************************************************
 * CMPI interface function
 * Gets a full CIM instance identified by teh CIM objectpath (reference)
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @param out result - result containing instance
 * @param in ref - CIM reference to the CIM object being acquired (classname and keys)
 * @param in properties - properties that caller cares about, if CIM instance enum
 * @return CMPIStatus error codes
 *****************************************************************************/
CMPIStatus XenCommonGetInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char** properties)
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    void* res = NULL;
    CMPIInstance* inst;
    _SBLIM_ENTER("CMPILIFYInstance_getInstance");
    CMPIString *cn = CMGetClassName(ref, &status);


    /* Create new CMPIInstance for resource. */
    inst = CMNewInstance(_BROKER, ref, &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
        CMSetStatus(&status, CMPI_RC_ERR_FAILED);
        goto exit;
    }

    /* If specified, set the property filter for CMPIInstance. */
    if (properties) {
        status = CMSetPropertyFilter(inst, properties, NULL);
        if (status.rc != CMPI_RC_OK) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
            goto exit;
        }
    }

    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("xen_utils_get_call_context failed"));
        goto exit;
    }

    const XenProviderInstanceFT *ft = prov_pxy_load_xen_instance_provider(_BROKER, CMGetCharPtr(cn));
    if(ft == NULL) {
        CMSetStatus(&status, CMPI_RC_ERR_NOT_SUPPORTED);
        goto exit;
    }
    /* Get the target resource. */ 
    if (!getres4op((CMPIObjectPath*)ref, ctx, ft, properties, &res)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("getres4op failed"));
        CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);
        goto exit;
    }
    /* Set CMPIInstance properties from resource data. */
    status.rc = prov_pxy_setproperties(ft, inst, res, properties);
    prov_pxy_release(ft, res);
    if (status.rc != CMPI_RC_OK) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("SetProperties failed with %d", status.rc));
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPILIFY setproperties() failed");
        goto exit;
    }
    /* Return the CMPIInstance for the resource. */
    status = CMReturnInstance(rslt, inst);
    if (status.rc != CMPI_RC_OK) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("CMSeturnInstance failed with %d", status.rc));
        CMSetStatus(&status, CMPI_RC_ERR_FAILED);
        goto exit;
    }

    CMReturnDone(rslt);
    exit:
    if (ctx)
        xen_utils_free_call_context(ctx);

    _SBLIM_RETURNSTATUS(status);
}
/*****************************************************************************
 * CMPI interface function
 * Create a backend xen resource based on the CIM instance passed in
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @param out result - result containing instance
 * @param in ref - CIM reference to the CIM object being acquired 
 *                 (classname and keys)
 * @param in inst - CIM instanec with all properties needed to create a new xen 
 *                  instance
 * @return CMPIStatus error codes
 *****************************************************************************/
CMPIStatus XenCommonCreateInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const CMPIInstance* inst
    )
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    void* res = NULL;
    void* resId = NULL;
    CMPIString *cn = CMGetClassName(ref, &status);

    _SBLIM_ENTER("CMPILIFYInstance_createInstance");

    const XenProviderInstanceFT *ft = prov_pxy_load_xen_instance_provider(_BROKER, CMGetCharPtr(cn));
    if((ft == NULL) || ft->xen_resource_add == NULL) {
        status.rc = CMPI_RC_ERR_NOT_SUPPORTED;
        goto exit;
    }

    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)) {
        goto exit;
    }

    /* Check if target resource already exists. */
    if (getres4op((CMPIObjectPath*)ref, ctx, ft, NULL, &res)) {
        prov_pxy_release(ft, res);
        CMSetStatus(&status, CMPI_RC_ERR_ALREADY_EXISTS);
        goto exit;
    }
    /* Get the ResourceId for the new resource instance. */
    if (prov_pxy_extractid(_BROKER, ft, (CMPIInstance*)inst, &resId) != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPILIFY extractid() failed");
        goto exit;
    }
    /* Create a new resource from the CMPIInstance properties. */
    status.rc = prov_pxy_extract(_BROKER, ft, (CMPIInstance*)inst, NULL, &res);
    if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED,
            "CMPILIFY extract() unsupported");
        goto exit;
    }
    else if (status.rc != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPILIFY extract() failed");
        goto exit;
    }
    /* Add the target resource. */
    status.rc = prov_pxy_add(_BROKER, ft, resId, ctx, res);
    prov_pxy_release(ft, res);
    if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED, 
            "CMPILIFY add() unsupported");
    }
    else if (status.rc != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPILIFY add() failed");
    }

    exit:
    if (resId)
        prov_pxy_releaseid(ft, resId);
    if (ctx) xen_utils_free_call_context(ctx);

    _SBLIM_RETURNSTATUS(status);
}
/*****************************************************************************
 * CMPI interface function
 * Modify a backend xen resource based on the CIM instance passed in
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @param out result - result containing instance
 * @param in ref - CIM reference to the CIM object being acquired 
 *                 (classname and keys)
 * @param in inst - CIM instanec with all properties needed to create a new xen 
 *                  instance
 * @param properties - CIM instance properties that caller cares about
 * @return CMPIStatus error codes
 *****************************************************************************/
CMPIStatus XenCommonModifyInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const CMPIInstance* inst,
    const char** properties)
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    void* res = NULL;
    void* resId = NULL;
    CMPIString *cn = CMGetClassName(ref, &status);

    _SBLIM_ENTER("CMPILIFYInstance_modifyInstance");

    const XenProviderInstanceFT *ft = prov_pxy_load_xen_instance_provider(_BROKER, CMGetCharPtr(cn));
    if(ft == NULL || ft->xen_resource_modify == NULL) {
        status.rc = CMPI_RC_ERR_NOT_SUPPORTED;
        goto exit;
    }
    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)) {
        goto exit;
    }

    /* Get the target resource. */
    if (!getres4op((CMPIObjectPath*)ref, ctx, ft, NULL, &res)) {
        CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);
        goto exit;
    }
    prov_pxy_release(ft, res);
    res = NULL;
    /* Get the ResourceId for the new resource instance. */
    if (prov_pxy_extractid(_BROKER, ft, (CMPIInstance*)inst, &resId) != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPILIFY extractid() failed");
        goto exit;
    }

    /* Create a new resource from the CMPIInstance properties. */
    status.rc = prov_pxy_extract(_BROKER, ft, (CMPIInstance*)inst, NULL, &res);
    if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED,
            "CMPILIFY extract() unsupported");
        goto exit;
    }
    else if (status.rc != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPILIFY extract() failed");
        goto exit;
    }

    /* Modify the target resource. */
    status.rc = prov_pxy_modify(_BROKER, ft, resId, ctx, res, properties);
    if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED,
            "CMPILIFY modify() unsupported");
    }
    else if (status.rc != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPILIFY modify() failed");
    }

exit:
    if (res)
        prov_pxy_release(ft, res);
    if (resId)
        prov_pxy_releaseid(ft, resId);
    if (ctx)
        xen_utils_free_call_context(ctx);
    _SBLIM_RETURNSTATUS(status);
}
/*****************************************************************************
 * CMPI interface function
 * Delete a backend xen resource based on the CIM reference passed in
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @param in result - result containing instance
 * @param in ref - CIM reference to the CIM object being acquired 
 *                 (classname and keys)
 * @return CMPIStatus error codes
 *****************************************************************************/
CMPIStatus XenCommonDeleteInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref)
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    void* res = NULL;
    void* resId = NULL;
    CMPIInstance* inst;
    CMPIString *cn = CMGetClassName(ref, &status);

    _SBLIM_ENTER("CMPILIFYInstance_deleteInstance");

    const XenProviderInstanceFT *ft = prov_pxy_load_xen_instance_provider(_BROKER, CMGetCharPtr(cn));
    if(ft == NULL || ft->xen_resource_delete == NULL) {
        status.rc = CMPI_RC_ERR_NOT_SUPPORTED;
        goto exit;
    }
    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)) {
        goto exit;
    }
    /* Get the target resource. */
    if (!getres4op((CMPIObjectPath*)ref, ctx, ft, NULL, &res)) {
        CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);
        goto exit;
    }
    prov_pxy_release(ft, res);
    res = NULL;

    /* Convert the ref CMPIObjectPath to (partial) CMPIInstance. */
    if (!op2inst(ref, &inst)) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "Internal error: op2inst() failed");
        goto exit;
    }
    /* Get the ResourceId for the new resource instance. */
    if (prov_pxy_extractid(_BROKER, ft, (CMPIInstance*)inst,  &resId) != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPLIFY extractid() failed");
        goto exit;
    }
    /* Delete the target resource. */
    status.rc = prov_pxy_delete(_BROKER, ft, resId, ctx);
    if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED,
            "CMPLIFY delete() unsupported");
    }
    else if (status.rc != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "CMPLIFY delete() failed");
    }

exit:
    if (resId) prov_pxy_releaseid(ft, resId);
    if (ctx) xen_utils_free_call_context(ctx);

    _SBLIM_RETURNSTATUS(status);
}
/*****************************************************************************
 * CMPI interface function
 * Query based CIM instance enumeration (WQL queries supported)
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @param out result - result containing instance
 * @param in ref - CIM reference to the CIM object being queried
 *                 (classname and certain properties)
 * @param in query - actual WQL query string
 * @param in lang - query language used ('WQL' only)
 * @return CMPIStatus error codes
 *****************************************************************************/
CMPIStatus XenCommonExecQuery(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char* query, 
    const char* lang)
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    void* resList;
    void* res;
    char* ns;
    unsigned int found = 0;
    CMPISelectExp* expr;
    CMPIObjectPath* op;
    CMPIInstance* inst;
    CMPIBoolean match;
    CMPIString *cn = CMGetClassName(ref, &status);
    char *classname = CMGetCharPtr(cn);

    _SBLIM_ENTER("CMPILIFYInstance_execQuery");

    const XenProviderInstanceFT *ft = prov_pxy_load_xen_instance_provider(_BROKER, CMGetCharPtr(cn));
    if(ft == NULL) {
        CMSetStatus(&status, CMPI_RC_ERR_NOT_SUPPORTED);
        goto exit;
    }

    /* Create select expression from query. */
    expr = CMNewSelectExp(_BROKER, query, lang, NULL, &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullObject(expr)) {
        CMSetStatus(&status, CMPI_RC_ERR_INVALID_QUERY);
        goto exit;
    }
    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)) {
        goto exit;
    }
    /* Get list of resources. */
    if (prov_pxy_begin(_BROKER, ft, classname, ctx, NULL, false, &resList) != CMPI_RC_OK) {
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
            "begin() failed");
        goto exit;
    }
    /* Enumerate resources and return CMPIObjectPath for each. */
    ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));
    while (1) {
        /* Create new CMPIObjectPath for next resource. */
        op = CMNewObjectPath(_BROKER, ns, classname, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Failing to create new object path"));
            break;
        }
        /* Create new CMPIInstance for resource. */
        inst = CMNewInstance(_BROKER, op, &status);
        if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Failing to create new CMPIInstance for resource"));
            break;
        }

	CMPIrc rc = CMPI_RC_OK;
	rc = prov_pxy_getnext(ft, resList, NULL, &res);

        /* Get the next resource using the resource provider's getNext(). */
        if (rc != CMPI_RC_OK) { 
	  if (rc == CMPI_RC_ERR_NOT_FOUND) {
	    /* We have reached the end of our resource list and have not more
	     * objects to enumerate. We should break out of this while loop.*/
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("End of resource list %d", rc));
   	    break; 
	  }
	  /* In this case, we have encountered a failure, and so should continue onto
	   * the next object. A failure may have been due to an object being removed
	   * by someone else, which is a valid case and should not cause other failures.*/
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error enumerating object, Continuing to process remaining resources"));
	    continue; /* Continue to next object in resource list */
	}

        /* Set CMPIInstance properties from resource data. */
        status.rc = prov_pxy_setproperties(ft, inst, res, NULL);
        prov_pxy_release(ft, res);
        if (status.rc != CMPI_RC_OK) {
            CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                "setproperties() failed");
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Set Properties failed"));
            break;
        }
        /* Evaluate the select expression against this CMPIInstance. */
        match = CMEvaluateSelExp(expr, inst, &status);
        if (status.rc != CMPI_RC_OK) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("CMPI RC Errror matching expression"));
            break;
        }
        /* Return the CMPIInstance for the resource if it match the query. */
        if (match) {
            status = CMReturnInstance(rslt, inst);
            if (status.rc != CMPI_RC_OK) {
                CMSetStatus(&status, CMPI_RC_ERR_FAILED);
		_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("error returning instance"));
                break;
            }
            found++;
        }
    } /* while() */
    prov_pxy_end(ft, resList);

    /* Check if enumeration finished OK. */
    if (found) {
        if ((status.rc == CMPI_RC_OK) || (status.rc == CMPI_RC_ERR_NOT_FOUND)) {
            CMReturnDone(rslt);
            CMSetStatus(&status, CMPI_RC_OK);
        }
    }

    exit:
    if (ctx)
        xen_utils_free_call_context(ctx);
    _SBLIM_RETURNSTATUS(status);
}
/*****************************************************************************
 * CMPI interface function
 * Initialization routine for the CIM instance provider
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @return CMPIStatus error codes
 *****************************************************************************/
static void InstanceInitialize(
    CMPIInstanceMI * self,      /* [in] Handle to this provider (i.e. 'self'). */
    const CMPIContext * context)        /* [in] Additional context info, if any. */
{
    _SBLIM_ENTER("Xen Common Provider Initializer");
    _SBLIM_TRACE(2, ("--- self=\"%s\"", self->ft->miName));

    prov_pxy_init();

    _SBLIM_RETURN();
}

/* Initialize the CMPI function pointers for this provider and run the initalizer */
CMInstanceMIStub(XenCommon, Xen_ProviderCommon, _BROKER, InstanceInitialize(&mi, ctx));

/*=============================================================================
 *  Method provider interface. 
 * An InvokeMethod call on a CIM Object lands here. We lookup the right backend
 * provider to load and let it handle it.
 *=============================================================================*/

/* Global CMPI broker for method providers */
static const CMPIBroker *_BROKER_M;

/*****************************************************************************
 * CMPI interface function
 * Initialization routine for the Method provider
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @return CMPIStatus error codes
 *****************************************************************************/
static void XenCommonMethodInitialize(
    CMPIMethodMI * self,         /* [in] Handle to this provider (i.e. 'self'). */
    const CMPIContext * context)         /* [in] Additional context info, if any. */
{
    _SBLIM_ENTER("XenCommonMethodInitialize");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));

    /* Initialize xen-api for use in method invocations. */
    xen_utils_xen_init();
    jobs_initialize();

    _SBLIM_RETURN();
}
/*****************************************************************************
 * CMPI interface function
 * Cleanup required before the provider gets unloaded
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @return CMPIStatus error codes
 *****************************************************************************/
static CMPIStatus XenCommonMethodCleanup(
    CMPIMethodMI * self,          /* [in] Handle to this provider (i.e. 'self'). */
    const CMPIContext * context,          /* [in] Additional context info, if any. */
    CMPIBoolean terminating)   /* [in] True if MB is terminating */
{
    (void)terminating;
    CMPIStatus status = { CMPI_RC_OK, NULL};    /* Return status of CIM operations. */

    _SBLIM_ENTER("XenCommonMethodInitialize");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));

    if(!jobs_running()) {
        jobs_uninitialize();
        xen_utils_xen_close(); /* clean up xen-api */
    }
    else
        status.rc = CMPI_RC_DO_NOT_UNLOAD; /* Dont unload the provider yet */

    _SBLIM_RETURNSTATUS(status);
}
/*****************************************************************************
 * CMPI interface function
 * Looks up the backend xen provider based on the classname passed in and
 * calls INvokeMethod on it.
 *
 * @param in mi - CMPI instance function inteface
 * @param in caller_ctx - caller's context
 * @param out results - methods results, if any
 * @param in ref - object reference whose method is being invoked
 * @param in methodname - name of the method being invoked
 * @param in argsin - input argument list for the method (if any)
 * @param out argsout - output argument list from the method (if any)
 * @return CMPIStatus error codes
 *****************************************************************************/
static CMPIStatus XenCommonInvokeMethod(
    CMPIMethodMI * self,                  /* [in] Handle to this provider (i.e. 'self') */
    const CMPIContext * cmpi_context,     /* [in] Additional context info, if any */
    const CMPIResult * results,           /* [out] Results of this operation */
    const CMPIObjectPath * ref,           /* [in] Contains the CIM namespace, classname and desired object path */
    const char * methodname,              /* [in] Name of the method to apply against the reference object */
    const CMPIArgs * argsin,              /* [in] Method input arguments */
    CMPIArgs * argsout)                   /* [in] Method output arguments */
{
    CMPIStatus status = { CMPI_RC_ERR_METHOD_NOT_FOUND, NULL};

    CMPIString *cn = CMGetClassName(ref, &status);
    char *classname = CMGetCharPtr(cn);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("XenCommonInvokeMethod for %s", classname));
    const XenProviderMethodFT *ft = prov_pxy_load_xen_method_provider(_BROKER_M, classname);
    if(ft)
        status = ft->xen_resource_invoke_method(self, _BROKER_M, cmpi_context, results, ref, methodname, argsin, argsout);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("XenCommonInvokeMethod returned %d", status.rc));

    return status;
}

CMMethodMIStub(XenCommon, Xen_ProviderCommon, _BROKER_M, XenCommonMethodInitialize(&mi, ctx));
