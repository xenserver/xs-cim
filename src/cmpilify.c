/*
 * cmpilify.c
 *
 * © Copyright IBM Corp. 2007
 *
 * THIS FILE IS PROVIDED UNDER THE TERMS OF THE COMMON PUBLIC LICENSE
 * ("AGREEMENT"). ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS FILE
 * CONSTITUTES RECIPIENTS ACCEPTANCE OF THE AGREEMENT.
 * You can obtain a current copy of the Common Public License from
 * http://oss.software.ibm.com/developerworks/opensource/license-cpl.html
 *
 * Author:         Dr. Gareth S. Bestor <bestor@us.ibm.com>
 * Contributors:
 * Interface Type: Common Manageability Programming Interface (CMPI)
 * Description:    Generic CMPILIFY instance provider.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include "cmpilify.h"
#include "cmpitrace.h"
#include "cmpiutil.h"


#define _BROKER (((CMPILIFYInstanceMI*)(mi->hdl))->brkr)
#define _CLASS (((CMPILIFYInstanceMI*)(mi->hdl))->cn)
#define _KEYS (((CMPILIFYInstanceMI*)(mi->hdl))->kys)
#define _FT (((CMPILIFYInstanceMI*)(mi->hdl))->ft)

static int op2inst(
    const CMPIObjectPath* op, 
    CMPIInstance** inst,
    CMPIInstanceMI* mi)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   CMPIString* keyname;
   int numkeys, i;
   CMPIData key;
   int rc = 0;

   /* Convert CMPIObjectPath to CMPIInstance. */
   *inst = CMNewInstance(_BROKER, op, &status);
   if (status.rc != CMPI_RC_OK) goto exit;

   /* Set the CMPIInstance key properties from the CMPIObjectPath keys. */
   numkeys = CMGetKeyCount(op, &status);
   if (status.rc != CMPI_RC_OK) goto exit;
   for (i=0; i<numkeys; i++) {
      key = CMGetKeyAt(op, i, &keyname, &status);
      if (status.rc != CMPI_RC_OK) goto exit;
      status = CMSetProperty(*inst, CMGetCharPtr(keyname),
                             &(key.value), key.type);
      if (status.rc != CMPI_RC_OK) goto exit;
   }
   rc = 1;
 exit:
   return rc;
}

/* ------------------------------------------------------------------------- */

static int getres4op(
    void** res, 
    CMPIObjectPath* op, 
    struct xen_call_context *caller_ctx,
    CMPIInstanceMI* mi, 
	const char** properties)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   void* resId = NULL;
   *res = NULL;
   CMPIInstance* inst;
   int rc = 0;

   if (!op2inst(op,&inst,mi)) goto exit;

   /* Get the ResourceId for the instance */
   status.rc = _FT->extractid(&resId, inst);
   if (status.rc != CMPI_RC_OK) goto exit;

   /* First try to get the target resource directly. */
   status.rc = _FT->get(resId, caller_ctx, res, properties);
   if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED) {
      /* FIXME - if get() unsupported then enumerate and look for match */
      goto exit;
   } else if (status.rc != CMPI_RC_OK) {
      goto exit;
   } 
   rc = 1;

 exit:
   if (resId) _FT->releaseid(resId);
   return rc;
}

/* ------------------------------------------------------------------------- *
 * Shared CMPILIFY CMPI instance provider functions.     
 * These are exported as entry points to each provider.                    
 * ------------------------------------------------------------------------- */
CMPIStatus CMPILIFYInstance_cleanup(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    CMPIBoolean terminating)
{
   CMPIStatus status = {CMPI_RC_OK, NULL}; 

   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("CMPILIFYInstance_cleanup"));

   /* Run resource provider's unload(). */
   if (_FT->unload(terminating == CMPI_true) != CMPI_RC_OK)
      CMSetStatusWithChars(_BROKER, &status, (terminating == CMPI_true)?
                           CMPI_RC_ERR_FAILED : CMPI_RC_DO_NOT_UNLOAD,
                           "CMPILIFY unload() failed");

   return status;
}

/*****************************************************************************
 * CMPILIFYInstance_enumInstanceNames
 *     Enumerate all CIM references for a particular class
 *****************************************************************************/
CMPIStatus CMPILIFYInstance_enumInstanceNames(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   void* resList;
   void* res;
   char* ns;
   unsigned int found = 0;
   CMPIObjectPath* op;
   CMPIInstance* inst;

   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance_enumInstanceNames");

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get list of resources. */
   if (_FT->begin(&resList, _CLASS, ctx, NULL) != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED, 
                           "CMPILIFY begin() failed");
      goto exit;
   }

   /* Enumerate resources and return CMPIObjectPath for each. */ 
   ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));
   while (1) {
      /* Create new CMPIObjectPath for next resource. */
      op = CMNewObjectPath(_BROKER, ns, _CLASS, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }

      /* Create new CMPIInstance for resource. */
      inst = CMNewInstance(_BROKER, op, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }

      /* Get the next resource using the resource provider's getNext(). */
      if (_FT->getnext(resList, &res, NULL) != CMPI_RC_OK)
         break; /* Normal loop exit when CMPI_RC_ERR_NOT_FOUND. */

      /* Set CMPIInstance properties from resource data. */
      status.rc = _FT->setproperties(inst, res, NULL);
      _FT->release(res);
      if (status.rc != CMPI_RC_OK) {
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
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }

      /* Return the CMPIObjectPath for the resource. */
      status = CMReturnObjectPath(rslt, op);
      if (status.rc != CMPI_RC_OK) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }
      found++;
   } /* while() */
   _FT->end(resList);

   /* Check if enumeration finished OK. */
   if (found) {
      if ((status.rc == CMPI_RC_OK) || (status.rc == CMPI_RC_ERR_NOT_FOUND)) {
         CMReturnDone(rslt);
         CMSetStatus(&status, CMPI_RC_OK);
      }
   }

 exit:
     if(ctx)
         xen_utils_free_call_context(ctx);

    _SBLIM_RETURNSTATUS(status);
}

/*****************************************************************************
 * CMPILIFYInstance_enumInstance
 *     Enumerate all CIM instances for a particular class
 *****************************************************************************/
CMPIStatus CMPILIFYInstance_enumInstances(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char** properties)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   void* resList;
   void* res;
   char* ns;
   unsigned int found = 0;
   CMPIObjectPath* op;
   CMPIInstance* inst;

   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance_enumInstances");

   /* CMPIPrincipal contains the username and password */
   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get list of resources. */
   if (_FT->begin(&resList, _CLASS, ctx, NULL) != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY begin() failed");
      goto exit;
   }
 
   /* Enumerate resources and return CMPIObjectPath for each. */
   ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));
   while (1) {
      /* Create new CMPIObjectPath for next resource. */
      op = CMNewObjectPath(_BROKER, ns, _CLASS, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }

      /* Create new CMPIInstance for resource. */
      inst = CMNewInstance(_BROKER, op, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }

      /* Get the next resource using the resource provider's getNext(). */
      if (_FT->getnext(resList, &res, NULL) != CMPI_RC_OK)
         break; /* Normal loop exit when CMPI_RC_ERR_NOT_FOUND. */

      /* If specified, set the property filter for CMPIInstance. */
      if (properties) {
         status = CMSetPropertyFilter(inst, properties, NULL);
         if (status.rc != CMPI_RC_OK) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
            break;
         }
      }

      /* Set CMPIInstance properties from resource data. */
      status.rc = _FT->setproperties(inst, res, NULL);
      _FT->release(res);
      if (status.rc != CMPI_RC_OK) {
         CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                              "CMPILIFY setproperties() failed");
         break;
      }

      /* Return the CMPIInstance for the resource. */
      status = CMReturnInstance(rslt, inst);
      if (status.rc != CMPI_RC_OK) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }
      found++;
   } /* while() */
   _FT->end(resList);

   /* Check if enumeration finished OK. */
   if (found) {
      if ((status.rc == CMPI_RC_OK) || (status.rc == CMPI_RC_ERR_NOT_FOUND)) {
         CMReturnDone(rslt);
         CMSetStatus(&status, CMPI_RC_OK);
      }
   }

 exit:
     if(ctx)
         xen_utils_free_call_context(ctx);
    
     _SBLIM_RETURNSTATUS(status);
}

/*****************************************************************************
 * CMPILIFYInstance_getInstance
 *     Get a CIM object instance given its reference (keys)
 *****************************************************************************/
CMPIStatus CMPILIFYInstance_getInstance(
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
   _CLASS = CMGetCharPtr(cn);


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
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get the target resource. */ 
   if (!getres4op(&res, (CMPIObjectPath*)ref, ctx, mi, properties)) {
      CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);
      goto exit;
   }

   /* Set CMPIInstance properties from resource data. */
   status.rc = _FT->setproperties(inst, res, properties);
   _FT->release(res);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY setproperties() failed");
      goto exit;
   }

   /* Return the CMPIInstance for the resource. */
   status = CMReturnInstance(rslt, inst);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   CMReturnDone(rslt);
 exit:
     if(ctx)
         xen_utils_free_call_context(ctx);

   _SBLIM_RETURNSTATUS(status);
}

/*****************************************************************************
 * CMPILIFYInstance_createInstance
 *     Create a new CIM object instance
 *****************************************************************************/
CMPIStatus CMPILIFYInstance_createInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const CMPIInstance* inst)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   void* res = NULL;
   void* resId = NULL;


   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance_createInstance");

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Check if target resource already exists. */
   if (getres4op(&res, (CMPIObjectPath*)ref, ctx, mi, NULL)) {
      _FT->release(res);
      CMSetStatus(&status, CMPI_RC_ERR_ALREADY_EXISTS);
      goto exit;
   }

   /* Get the ResourceId for the new resource instance. */
   if (_FT->extractid(&resId, (CMPIInstance*)inst) != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY extractid() failed");
      goto exit;
   }

   /* Create a new resource from the CMPIInstance properties. */
   status.rc = _FT->extract(&res, (CMPIInstance*)inst, NULL);
   if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED,
                           "CMPILIFY extract() unsupported");
      goto exit;
   } else if (status.rc != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY extract() failed");
      goto exit;
   }

   /* Add the target resource. */
   status.rc = _FT->add(resId, ctx, res);
   _FT->release(res);
   if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED)
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED, 
                           "CMPILIFY add() unsupported");
   else if (status.rc != CMPI_RC_OK)
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY add() failed");

 exit:
   if (resId) _FT->releaseid(resId);
   if(ctx) xen_utils_free_call_context(ctx);

   _SBLIM_RETURNSTATUS(status);
}

/*****************************************************************************
 * CMPILIFYInstance_modifyInstance
 *     Modify a CIM object instance
 *****************************************************************************/
CMPIStatus CMPILIFYInstance_modifyInstance(
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
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance_modifyInstance");

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get the target resource. */
   if (!getres4op(&res, (CMPIObjectPath*)ref, ctx, mi, NULL)) {
      CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);
      goto exit;
   }
   _FT->release(res);
   res = NULL;

   /* Get the ResourceId for the new resource instance. */
   if (_FT->extractid(&resId, (CMPIInstance*)inst) != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY extractid() failed");
      goto exit;
   }

   /* Create a new resource from the CMPIInstance properties. */
   status.rc = _FT->extract(&res, (CMPIInstance*)inst, NULL);
   if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED,
                           "CMPILIFY extract() unsupported");
      goto exit;
   } else if (status.rc != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY extract() failed");
      goto exit;
   }

   /* Modify the target resource. */
   status.rc = _FT->modify(resId, ctx, res, properties);
   if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED)
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED,
                           "CMPILIFY modify() unsupported");
   else if (status.rc != CMPI_RC_OK)
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY modify() failed");

 exit:
   if (res) _FT->release(res);
   if (resId) _FT->releaseid(resId);
   if(ctx) xen_utils_free_call_context(ctx);
   _SBLIM_RETURNSTATUS(status);
}

/*****************************************************************************
 * CMPILIFYInstance_modifyInstance
 *     Delete a CIM object instance
 *****************************************************************************/
CMPIStatus CMPILIFYInstance_deleteInstance(
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
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance_deleteInstance");

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get the target resource. */
   if (!getres4op(&res, (CMPIObjectPath*)ref, ctx, mi, NULL)) {
      CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);
      goto exit;
   }

   _FT->release(res);
   res = NULL;

   /* Convert the ref CMPIObjectPath to (partial) CMPIInstance. */
   if (!op2inst(ref, &inst, mi)) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "Internal error: op2inst() failed");
      goto exit;
   }

   /* Get the ResourceId for the new resource instance. */
   if (_FT->extractid(&resId, (CMPIInstance*)inst) != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPLIFY extractid() failed");
      goto exit;
   }

   /* Delete the target resource. */
   status.rc = _FT->delete(resId, ctx);
   if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED)
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_NOT_SUPPORTED,
                           "CMPLIFY delete() unsupported");
   else if (status.rc != CMPI_RC_OK)
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPLIFY delete() failed");

 exit:
   if (resId) _FT->releaseid(resId);
   if(ctx) xen_utils_free_call_context(ctx);
   
   _SBLIM_RETURNSTATUS(status);
}

/*****************************************************************************
 * CMPILIFYInstance_execQuery
 *     Execute a WQL or CQL query,
 *     Intended to get a filtered list of objects
 *****************************************************************************/
CMPIStatus CMPILIFYInstance_execQuery(
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
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance_execQuery");

   /* Create select expression from query. */
   expr = CMNewSelectExp(_BROKER, query, lang, NULL, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(expr)) {
      CMSetStatus(&status, CMPI_RC_ERR_INVALID_QUERY);
      goto exit;
   }

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get list of resources. */
   if (_FT->begin(&resList, _CLASS, ctx, NULL) != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "begin() failed");
      goto exit;
   }

   /* Enumerate resources and return CMPIObjectPath for each. */
   ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));
   while (1) {
      /* Create new CMPIObjectPath for next resource. */
      op = CMNewObjectPath(_BROKER, ns, _CLASS, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }

      /* Create new CMPIInstance for resource. */
      inst = CMNewInstance(_BROKER, op, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }

      /* Get the next resource using the resource provider's getNext(). */
      if (_FT->getnext(resList, &res, NULL) != CMPI_RC_OK)
         break; /* Normal loop exit when CMPI_RC_ERR_NOT_FOUND. */

      /* Set CMPIInstance properties from resource data. */
      status.rc = _FT->setproperties(inst, res, NULL);
      _FT->release(res);
      if (status.rc != CMPI_RC_OK) {
         CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                              "setproperties() failed");
         break;
      }

      /* Evaluate the select expression against this CMPIInstance. */
      match = CMEvaluateSelExp(expr, inst, &status);
      if (status.rc != CMPI_RC_OK) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         break;
      }

      /* Return the CMPIInstance for the resource if it match the query. */
      if (match) {
         status = CMReturnInstance(rslt, inst);
         if (status.rc != CMPI_RC_OK) {
            CMSetStatus(&status, CMPI_RC_ERR_FAILED);
            break;
         }
         found++;
      }
   } /* while() */
   _FT->end(resList);

   /* Check if enumeration finished OK. */
   if (found) {
      if ((status.rc == CMPI_RC_OK) || (status.rc == CMPI_RC_ERR_NOT_FOUND)) {
         CMReturnDone(rslt);
         CMSetStatus(&status, CMPI_RC_OK);
      }
   }

 exit:
     if(ctx)
         xen_utils_free_call_context(ctx);
   _SBLIM_RETURNSTATUS(status);
}


/*****************************************************************************
 * The following functions are entry points for read-only CIM classes
 * that don't imlpement the create/modify/delete operations
 *****************************************************************************/
/* Need to refine the shortcuts to match the different CMPILIFYInstanceMI */
#undef _BROKER
#define _BROKER (((CMPILIFYInstance1ROMI*)(mi->hdl))->brkr)
#undef _CLASS
#define _CLASS (((CMPILIFYInstance1ROMI*)(mi->hdl))->cn)
#undef _KEYS
#define _KEYS (((CMPILIFYInstance1ROMI*)(mi->hdl))->kys)
#undef _FT
#define _FT (((CMPILIFYInstance1ROMI*)(mi->hdl))->ft)

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYInstance1RO_cleanup(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    CMPIBoolean terminating)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   _SBLIM_ENTER("CMPILIFYInstance1RO_cleanup");

   /* Run resource provider's unload(). */
   if (_FT->unload(terminating == CMPI_true) != CMPI_RC_OK)
      CMSetStatusWithChars(_BROKER, &status, (terminating == CMPI_true)?
                           CMPI_RC_ERR_FAILED : CMPI_RC_DO_NOT_UNLOAD,
                           "CMPILIFY unload() failed");

   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYInstance1RO_enumInstanceNames(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   void* res = NULL;
   CMPIInstance* inst;
   CMPIObjectPath* op;
   char* ns;

   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);

   _SBLIM_ENTER("CMPILIFYInstance1RO_enumInstanceNames");

   ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));

   /* Create the new CMPIInstance. */
   inst = CMNewInstance(_BROKER, ref, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get the instance data. */
   status.rc = _FT->get(ctx, &res,  NULL);
   if (status.rc != CMPI_RC_OK)
      goto exit;

   /* Set the CMPIInstance properties from the instance data. */
   status.rc = _FT->setproperties(inst, res, NULL);
   _FT->release(res);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY setproperties() failed");
      goto exit;
   }

   /* Get the CMPIObjectPath for this CMPIInstance. */
   op = CMGetObjectPath(inst, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }
   status = CMSetNameSpace(op, ns);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* Return the CMPIObjectPath for the resource. */
   status = CMReturnObjectPath(rslt, op);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   CMReturnDone(rslt);
 exit:
     if(ctx) xen_utils_free_call_context(ctx);
     
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYInstance1RO_enumInstances(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char** properties)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   void* res = NULL;
   CMPIInstance* inst;
   char* ns;

   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance1RO_enumInstances");

   ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));

   /* Create the new CMPIInstance. */
   inst = CMNewInstance(_BROKER, ref, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* If specified, set the property filter. */
   if (properties) {
      status = CMSetPropertyFilter(inst, properties, NULL);
      if (status.rc != CMPI_RC_OK) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         goto exit;
      }
   }

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get the instance data. */
   status.rc = _FT->get(ctx, &res, properties);
   if (status.rc != CMPI_RC_OK)
      goto exit;

   /* Set the CMPIInstance properties from the instance data. */
   status.rc = _FT->setproperties(inst, res, properties);
   _FT->release(res);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY setproperties() failed");
      goto exit;
   }

   /* Return the CMPIInstance for the resource. */
   status = CMReturnInstance(rslt, inst);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   CMReturnDone(rslt);
 exit:
     if(ctx) xen_utils_free_call_context(ctx);
     
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYInstance1RO_getInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char** properties)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   void* res = NULL;
   CMPIInstance* inst;
   CMPIObjectPath* op;
   char* ns;

   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance1RO_getInstance");

   ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));

   /* Create the new CMPIInstance. */
   inst = CMNewInstance(_BROKER, ref, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* If specified, set the property filter. */
   if (properties) {
      status = CMSetPropertyFilter(inst, properties, NULL);
      if (status.rc != CMPI_RC_OK) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         goto exit;
      }
   }

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get the instance data. */
   status.rc = _FT->get(ctx, &res, properties);
   if (status.rc != CMPI_RC_OK)
      goto exit;

   /* Set the CMPIInstance properties from the instance data. */
   status.rc = _FT->setproperties(inst, res, properties);
   _FT->release(res);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY setproperties() failed");
      goto exit;
   }

   /* Get the CMPIObjectPath for this CMPIInstance. */
   op = CMGetObjectPath(inst, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }
   status = CMSetNameSpace(op, ns);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* Check the CMPIInstance matches the requested reference. */
   if (!_CMSameObject(op, (CMPIObjectPath*)ref)) {
      CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);
      goto exit;
   }
 
   /* Return the CMPIInstance for the resource. */
   status = CMReturnInstance(rslt, inst);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   CMReturnDone(rslt);
 exit:
     if(ctx) xen_utils_free_call_context(ctx);
     
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYInstance1RO_createInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const CMPIInstance* inst)
{

    CMPIStatus status = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
    CMPIString *cn = CMGetClassName(ref, &status);
    _CLASS = CMGetCharPtr(cn);
    _SBLIM_ENTER("CMPILIFYInstance1RO_createInstance - NOT SUPPORTED");
    _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYInstance1RO_modifyInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const CMPIInstance* inst,
    const char** properties)
{
    CMPIStatus status = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
    CMPIString *cn = CMGetClassName(ref, &status);
    _CLASS = CMGetCharPtr(cn);
    _SBLIM_ENTER("CMPILIFYInstance1RO_modifyInstance - NOT SUPPORTED");
    _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYInstance1RO_deleteInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref)
{

    CMPIStatus status = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
    CMPIString *cn = CMGetClassName(ref, &status);
    _CLASS = CMGetCharPtr(cn);
    _SBLIM_ENTER("CMPILIFYInstance1RO_deleteInstance - NOT SUPPORTED");
    _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYInstance1RO_execQuery(
    CMPIInstanceMI* mi, 
    const CMPIContext* cmpi_ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char* query, 
    const char* lang)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   void* res;
   char* ns;
   CMPISelectExp* expr;
   CMPIInstance* inst;
   CMPIBoolean match;

   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYInstance1RO_execQuery");

   /* Create select expression from query. */
   expr = CMNewSelectExp(_BROKER, query, lang, NULL, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(expr)) {
      CMSetStatus(&status, CMPI_RC_ERR_INVALID_QUERY);
      goto exit;
   }

   ns = CMGetCharPtr(CMGetNameSpace(ref, NULL));

   /* Create the new CMPIInstance. */
   inst = CMNewInstance(_BROKER, ref, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(inst)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   struct xen_call_context *ctx = NULL;
   if(!xen_utils_get_call_context(cmpi_ctx, &ctx, &status)){
       goto exit;
   }

   /* Get the instance data. */
   status.rc = _FT->get(ctx, &res, NULL);
   if (status.rc != CMPI_RC_OK)
      goto exit;

   /* Set the CMPIInstance properties from the instance data. */
   status.rc = _FT->setproperties(inst, res, NULL);
   _FT->release(res);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                           "CMPILIFY setproperties() failed");
      goto exit;
   }

   /* Evaluate the select expression against this CMPIInstance. */
   match = CMEvaluateSelExp(expr, inst, &status);
   if (status.rc != CMPI_RC_OK) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* Return the CMPIInstance for the resource if it match the query. */
   if (match) {
      status = CMReturnInstance(rslt, inst);
      if (status.rc != CMPI_RC_OK) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         goto exit;
      }
      CMSetStatus(&status, CMPI_RC_OK); 
   } else
      CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);

   CMReturnDone(rslt);
 exit:
     if(ctx) xen_utils_free_call_context(ctx);
    
   _SBLIM_RETURNSTATUS(status);
}


/*****************************************************************************
 * The following functions are entry points for association CIM classes
 * Associations are special classes that keep track of relationships between
 * objects - such as getting a list of virtual disk (Xen_Disk) associated with
 * a VM (Xen_ComputerSystem) 
 *****************************************************************************/
/* Need to refine the shortcuts to match the different CMPILIFYAssociationMI */
#undef _BROKER
#define _BROKER (((CMPILIFYAssociationMI*)(mi->hdl))->brkr)
#undef _CLASS
#define _CLASS (((CMPILIFYAssociationMI*)(mi->hdl))->cn)
#undef _FT
#define _FT (((CMPILIFYAssociationMI*)(mi->hdl))->ft)
#define _LHSCLASS (((CMPILIFYAssociationMI*)(mi->hdl))->lhscn)
#define _LHSROLE (((CMPILIFYAssociationMI*)(mi->hdl))->lhsrol)
#define _LHSNAMESPACE (((CMPILIFYAssociationMI*)(mi->hdl))->lhsns)
#define _RHSCLASS (((CMPILIFYAssociationMI*)(mi->hdl))->rhscn)
#define _RHSROLE (((CMPILIFYAssociationMI*)(mi->hdl))->rhsrol)
#define _RHSNAMESPACE (((CMPILIFYAssociationMI*)(mi->hdl))->rhsns)

static CMPIStatus gettargetinfo(
    CMPIAssociationMI* mi, 
    const CMPIContext* ctx, 
    const CMPIObjectPath* op, 
    const char* assocClass,
    const char* resultClass, 
    const char* role, 
    const char* resultRole,
    char** trgcn, 
    char** trgns, 
    char** query, 
    char** lang)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   char* srccn;
   char* srcns;
   CMPIObjectPath* tmpop;
   CMPIInstance* srcinst;

   _SBLIM_ENTER("gettargetinfo");

   srccn = CMGetCharPtr(CMGetClassName(op, &status));
   srcns = CMGetCharPtr(CMGetNameSpace(op, &status));

   /* Check assocClass filter. */
   if (assocClass) {
      tmpop = CMNewObjectPath(_BROKER, srcns, _CLASS, &status);
      if (!CMClassPathIsA(_BROKER, tmpop, assocClass, &status)) {
         CMSetStatus(&status, CMPI_RC_ERR_NOT_SUPPORTED);
         goto exit;
      }
   }

   /* Get the source instance. */
   srcinst = CBGetInstance(_BROKER, ctx, op, NULL, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(srcinst)) {
      CMSetStatus(&status, CMPI_RC_ERR_NOT_FOUND);
      goto exit;
   }

   /* Determine target class: LHS or RHS. */
   tmpop = CMNewObjectPath(_BROKER, srcns, srccn, &status);
   if (CMClassPathIsA(_BROKER, tmpop, _LHSCLASS, &status)) {
      *trgcn = _RHSCLASS;
      *trgns = _RHSNAMESPACE;

      /* Check role & resultRole filter. */
      if ((role && (strcmp(role, _LHSROLE) != 0)) ||
          (resultRole && (strcmp(resultRole, _RHSROLE) != 0))) {
         CMSetStatus(&status, CMPI_RC_ERR_NOT_SUPPORTED);
         goto exit;
      }

      /* Get the query for the RHS target class. */
      if ((_FT->getrhsquery(srcinst, query, lang) != CMPI_RC_OK) ||
          !*query || !*lang) {
         CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                              "CMPILIFY getrhsquery() failed");
         goto exit;
      }

   } else if (CMClassPathIsA(_BROKER, tmpop, _RHSCLASS, &status)) {
      *trgcn = _LHSCLASS;
      *trgns = _LHSNAMESPACE;

      /* Check role & resultRole filter. */
      if ((role && (strcmp(role, _RHSROLE) != 0)) ||
          (resultRole && (strcmp(resultRole, _LHSROLE) != 0))) {
         CMSetStatus(&status, CMPI_RC_ERR_NOT_SUPPORTED);
         goto exit;
      }

      /* Get the query for the LHS target class. */
      if ((_FT->getlhsquery(srcinst, query, lang) != CMPI_RC_OK) ||
          !*query || !*lang) {
         CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED,
                              "CMPILIFY getlhsquery() failed");
         goto exit;
      }
   } else {
      CMSetStatus(&status, CMPI_RC_ERR_NOT_SUPPORTED);
      goto exit;
   }

 exit:
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_cleanup(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    CMPIBoolean terminating)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   _SBLIM_ENTER("CMPILIFYAssociation_cleanup");

   /* Run resource provider's unload(). */
   if (_FT->unload(terminating == CMPI_true) != CMPI_RC_OK)
      CMSetStatusWithChars(_BROKER, &status, (terminating == CMPI_true)?
                           CMPI_RC_ERR_FAILED : CMPI_RC_DO_NOT_UNLOAD,
                           "CMPILIFY unload() failed");

   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_enumInstanceNames(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   CMPIEnumeration* ops;
   CMPIObjectPath* op;
   CMPIData dt;
   CMPIObjectPath* tmpop;
   CMPIEnumeration* enm;
   CMPIData data;


   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYAssociation_enumInstanceNames");

   /* Determine target class: LHS or RHS. */
   op = CMNewObjectPath(_BROKER, _LHSNAMESPACE, _LHSCLASS, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* Get list of all LHS objectpaths. */
   ops = CBEnumInstanceNames(_BROKER, ctx, op, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(ops)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* For each LHS instance, get all references. */
   while (CMHasNext(ops, &status)) {
      dt = CMGetNext(ops, &status);
      op = dt.value.ref;
      fprintf(stderr,"op=%s\n",CMGetCharPtr(CDToString(_BROKER,op,NULL)));

      enm = CBReferenceNames(_BROKER, ctx, op, _RHSCLASS, _LHSROLE, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(enm)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         goto exit;
      }
      if (!CMHasNext(enm,NULL))
         fprintf(stderr,"CBReferenceNames(op, resultClass=%s, role=%s) returned nothing!\n", _RHSCLASS,_LHSROLE);

      while (CMHasNext(enm, &status)) {
         data = CMGetNext(enm, &status);
         tmpop = data.value.ref;

         if (CMClassPathIsA(_BROKER, tmpop, _CLASS, &status))
            CMReturnObjectPath(rslt, tmpop);
      }
   }

   CMReturnDone(rslt);
 exit:
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_enumInstances(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char** properties)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   CMPIEnumeration* ops;
   CMPIObjectPath* op;
   CMPIData dt;
   CMPIObjectPath* tmpop;
   CMPIInstance* tmpinst;
   CMPIEnumeration* enm;
   CMPIData data;

   CMPIString *cn = CMGetClassName(ref, &status);
   _CLASS = CMGetCharPtr(cn);
   _SBLIM_ENTER("CMPILIFYAssociation_enumInstances");

   /* Determine target class: LHS or RHS. */
   op = CMNewObjectPath(_BROKER, _LHSNAMESPACE, _LHSCLASS, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(op)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* Get list of all LHS objectpaths. */
   ops = CBEnumInstanceNames(_BROKER, ctx, op, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(ops)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   /* For each LHS instance, get all references. */
   while (CMHasNext(ops, &status)) {
      dt = CMGetNext(ops, &status);
      op = dt.value.ref;
      fprintf(stderr,"op=%s\n",CMGetCharPtr(CDToString(_BROKER,op,NULL)));

      enm = CBReferences(_BROKER, ctx, op, _RHSCLASS, _LHSROLE, properties,
                         &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(enm)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         goto exit;
      }
      if (!CMHasNext(enm,NULL))
         fprintf(stderr,"CBReferences(op, resultClass=%s, role=%s) returned nothing!\n", _RHSCLASS,_LHSROLE);

      while (CMHasNext(enm, &status)) {
         data = CMGetNext(enm, &status);
         tmpinst = data.value.inst;

         tmpop = CMGetObjectPath(tmpinst, &status);
         if (CMClassPathIsA(_BROKER, tmpop, _CLASS, &status))
            CMReturnInstance(rslt, tmpinst);
      }
   }

   CMReturnDone(rslt);
 exit:
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_getInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char** properties)
{
    CMPIStatus status = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
    _SBLIM_ENTER("CMPILIFYAssociation_getInstance - NOT SUPPORTED");
    _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_createInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const CMPIInstance* inst)
{
    CMPIStatus status = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
    _SBLIM_ENTER("CMPILIFYAssociation_createInstance - NOT SUPPORTED");
    _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_modifyInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const CMPIInstance* inst,
    const char** properties)
{
    CMPIStatus status = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
    _SBLIM_ENTER("CMPILIFYAssociation_modifyInstance - NOT SUPPORTED");
    _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_deleteInstance(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref)
{
    CMPIStatus status = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
    _SBLIM_ENTER("CMPILIFYAssociation_deleteInstance - NOT SUPPORTED");
    _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_execQuery(
    CMPIInstanceMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* ref, 
    const char* lang, 
    const char* query)
{
    CMPIStatus status = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
   _SBLIM_ENTER("CMPILIFYAssociation_execQuery - NOT SUPPORTED");
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_associationCleanup(
    CMPIAssociationMI* mi, 
    const CMPIContext* ctx,
    CMPIBoolean terminating)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};

   _SBLIM_ENTER("CMPILIFYAssociation_associationCleanup");
   if (_FT->unload(terminating == CMPI_true) != CMPI_RC_OK)
      CMSetStatusWithChars(_BROKER, &status, (terminating == CMPI_true)?
                           CMPI_RC_ERR_FAILED : CMPI_RC_DO_NOT_UNLOAD,
                           "CMPILIFY unload() failed");
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_associators(
    CMPIAssociationMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* op, 
    const char* assocClass,
    const char* resultClass, 
    const char* role, 
    const char* resultRole,
    const char** properties)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   char* trgcn;
   char* trgns;
   char* query = NULL;
   char* lang = NULL;
   CMPIObjectPath* tmpop;
   CMPIEnumeration* enm;
   CMPIData data;
   CMPIInstance* tmpinst;

   _SBLIM_ENTER("CMPILIFYAssociation_associators");
   status = gettargetinfo(mi,ctx,op,assocClass,resultClass,role,resultRole,
                          &trgcn, &trgns, &query, &lang);
   if (status.rc != CMPI_RC_OK) {
      /* Filter checks failed so just return OK w/o results. */
      if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED)
         CMSetStatus(&status, CMPI_RC_OK);
      goto exit;
   }

   tmpop = CMNewObjectPath(_BROKER, trgns, trgcn, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(tmpop)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   enm = CBExecQuery(_BROKER, ctx, tmpop, query, lang, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(enm)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }
 
   while (CMHasNext(enm, &status)) {
      data = CMGetNext(enm, &status);
      tmpinst = data.value.inst;
      tmpop = CMGetObjectPath(tmpinst, &status);

      /* Check resultClass filter. */
      if (!resultClass || CMClassPathIsA(_BROKER, tmpop, resultClass, &status)) 
         CMReturnInstance(rslt, tmpinst);
   }
 
   CMReturnDone(rslt);
 exit:
   if (query) free(query);
   if (lang) free(lang);
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_associatorNames(
    CMPIAssociationMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* op, 
    const char* assocClass,
    const char* resultClass, 
    const char* role, 
    const char* resultRole)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   char* trgcn;
   char* trgns;
   char* query = NULL;
   char* lang = NULL;
   CMPIObjectPath* tmpop;
   CMPIEnumeration* enm;
   CMPIData data;
   CMPIInstance* tmpinst;

   _SBLIM_ENTER("CMPILIFYAssociation_associatorNames");
   status = gettargetinfo(mi,ctx,op,assocClass,resultClass,role,resultRole,
                          &trgcn, &trgns, &query, &lang);
   if (status.rc != CMPI_RC_OK) {
      /* Filter checks failed so just return OK w/o results. */
      if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED)
         CMSetStatus(&status, CMPI_RC_OK);
      goto exit;
   }

   tmpop = CMNewObjectPath(_BROKER, trgns, trgcn, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(tmpop)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   enm = CBExecQuery(_BROKER, ctx, tmpop, query, lang, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(enm)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   while (CMHasNext(enm, &status)) {
      data = CMGetNext(enm, &status);
      tmpinst = data.value.inst;
      tmpop = CMGetObjectPath(tmpinst, &status);
      status = CMSetNameSpace(tmpop,trgns);

      /* Check resultClass filter. */
      if (!resultClass || CMClassPathIsA(_BROKER, tmpop, resultClass, &status))
         CMReturnObjectPath(rslt, tmpop);
   }

   CMReturnDone(rslt);
 exit:
   if (query) free(query);
   if (lang) free(lang);
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_references(
    CMPIAssociationMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* op, 
    const char* resultClass, 
    const char* role,
    const char** properties)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   char* trgcn;
   char* trgns;
   char* query = NULL;
   char* lang = NULL;
   CMPIObjectPath* tmpop;
   CMPIEnumeration* enm;
   CMPIData data;
   CMPIInstance* tmpinst;
   char* ns;
   CMPIObjectPath* assocop;
   CMPIInstance* associnst;

   _SBLIM_ENTER("CMPILIFYAssociation_references");
   status = gettargetinfo(mi,ctx,op,NULL,resultClass,role,NULL,
                          &trgcn, &trgns, &query, &lang);
   if (status.rc != CMPI_RC_OK) {
      /* Filter checks failed so just return OK w/o results. */
      if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED)
         CMSetStatus(&status, CMPI_RC_OK);
      goto exit;
   }

   tmpop = CMNewObjectPath(_BROKER, trgns, trgcn, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(tmpop)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   enm = CBExecQuery(_BROKER, ctx, tmpop, query, lang, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(enm)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   ns = CMGetCharPtr(CMGetNameSpace(op, &status));

   while (CMHasNext(enm, &status)) {
      data = CMGetNext(enm, &status);
      tmpinst = data.value.inst;
      tmpop = CMGetObjectPath(tmpinst, &status);
      status = CMSetNameSpace(tmpop,trgns);

      /* Check resultClass filter. */
      if (resultClass && !CMClassPathIsA(_BROKER, tmpop, resultClass, &status))
         continue;

      /* Create association instance. */
      assocop = CMNewObjectPath(_BROKER, ns, _CLASS, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(assocop)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         goto exit;
      }

      associnst = CMNewInstance(_BROKER, assocop, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(associnst)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         goto exit;
      }

      /* Assign association references appropriately. */
      if (strcmp(trgcn, _LHSCLASS) == 0) {
         CMSetProperty(associnst, _LHSROLE, (CMPIValue*)&tmpop, CMPI_ref);
         CMSetProperty(associnst, _RHSROLE, (CMPIValue*)&op, CMPI_ref);
      } else {
         CMSetProperty(associnst, _LHSROLE, (CMPIValue*)&op, CMPI_ref);
         CMSetProperty(associnst, _RHSROLE, (CMPIValue*)&tmpop, CMPI_ref);
      }
      _FT->setassocproperties(associnst);
      CMReturnInstance(rslt, associnst);
   }

   CMReturnDone(rslt);
 exit:
   if (query) free(query);
   if (lang) free(lang);
   _SBLIM_RETURNSTATUS(status);
}

/* ------------------------------------------------------------------------- */

CMPIStatus CMPILIFYAssociation_referenceNames(
    CMPIAssociationMI* mi, 
    const CMPIContext* ctx, 
    const CMPIResult* rslt,
    const CMPIObjectPath* op, 
    const char* resultClass, 
    const char* role)
{
   CMPIStatus status = {CMPI_RC_OK, NULL};
   char* trgcn;
   char* trgns;
   char* query = NULL;
   char* lang = NULL;
   CMPIObjectPath* tmpop;
   CMPIEnumeration* enm;
   CMPIData data;
   CMPIInstance* tmpinst;
   char* ns;
   CMPIObjectPath* assocop;

   _SBLIM_ENTER("CMPILIFYAssociation_referenceNames");
   status = gettargetinfo(mi,ctx,op,NULL,resultClass,role,NULL,
                          &trgcn, &trgns, &query, &lang);
   if (status.rc != CMPI_RC_OK) {
      /* Filter checks failed so just return OK w/o results. */
      if (status.rc == CMPI_RC_ERR_NOT_SUPPORTED)
         CMSetStatus(&status, CMPI_RC_OK);
      goto exit;
   }

   tmpop = CMNewObjectPath(_BROKER, trgns, trgcn, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(tmpop)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   enm = CBExecQuery(_BROKER, ctx, tmpop, query, lang, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(enm)) {
      CMSetStatus(&status, CMPI_RC_ERR_FAILED);
      goto exit;
   }

   ns = CMGetCharPtr(CMGetNameSpace(op, &status));

   while (CMHasNext(enm, &status)) {
      data = CMGetNext(enm, &status);
      tmpinst = data.value.inst;
      tmpop = CMGetObjectPath(tmpinst, &status);
      status = CMSetNameSpace(tmpop,trgns);

      /* Check resultClass filter. */
      if (resultClass && !CMClassPathIsA(_BROKER, tmpop, resultClass, &status))
         continue;

      /* Create association objectpath. */
      assocop = CMNewObjectPath(_BROKER, ns, _CLASS, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(assocop)) {
         CMSetStatus(&status, CMPI_RC_ERR_FAILED);
         goto exit;
      }

      /* Assign association references appropriately. */
      if (strcmp(trgcn, _LHSCLASS) == 0) {
         CMAddKey(assocop, _LHSROLE, (CMPIValue*)&tmpop, CMPI_ref);
         CMAddKey(assocop, _RHSROLE, (CMPIValue*)&op, CMPI_ref);
      } else {
         CMAddKey(assocop, _LHSROLE, (CMPIValue*)&op, CMPI_ref);
         CMAddKey(assocop, _RHSROLE, (CMPIValue*)&tmpop, CMPI_ref);
      }
      CMReturnObjectPath(rslt, assocop);
   }

   CMReturnDone(rslt);
 exit:
   if (query) free(query);
   if (lang) free(lang);
   _SBLIM_RETURNSTATUS(status);
}
