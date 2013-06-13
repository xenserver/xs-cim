/*
 * cmpilify.h
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

#ifndef _CMPILIFY_H
#define _CMPILIFY_H

#include <cmpidt.h>
#include <cmpimacs.h>
#include <stdio.h>

#ifndef CMPI_EXTERN_C
  #ifdef __cplusplus
    #define CMPI_EXTERN_C extern "C"
  #else
    #define CMPI_EXTERN_C
  #endif
#endif

#define XEN_CLASS_NAMESPACE "root/cimv2"

struct xen_call_context
{
     char *user;
     char *pw;
};

int xen_utils_get_call_context(
    const CMPIContext *cmpi_ctx, 
    struct xen_call_context **ctx,
    CMPIStatus* status
    );

void xen_utils_free_call_context(
    struct xen_call_context *ctx
    );
/* ------------------------------------------------------------------------- */
/* Generic CMPILIFY instance provider abstract resource API.                 */
/* ------------------------------------------------------------------------- */
typedef struct {
   CMPIrc (*load)();
   CMPIrc (*unload)(const int terminating);
   CMPIrc (*begin)(void** enm, const char* classname,
                   struct xen_call_context *caller_id, const char** properties);
   void   (*end)(void* enm);
   CMPIrc (*getnext)(void* enm, void** inst, const char** properties);
   CMPIrc (*get)(void *id, struct xen_call_context * caller_id, 
                 void** inst, const char** properties);
   void   (*release)(void* inst);
   CMPIrc (*add)(const void* id, struct xen_call_context *caller_id, const void* inst);
   CMPIrc (*delete)(const void* id, struct xen_call_context *caller_id);
   CMPIrc (*modify)(const void* id, struct xen_call_context *caller_id, 
                    const void* inst, const char** properties);
   CMPIrc (*setproperties)(CMPIInstance* instance, const void* inst,
                           const char** properties);
   CMPIrc (*extract)(void** inst, const CMPIInstance* instance,
                     const char** properties);
   CMPIrc (*extractid)(void** id, const CMPIInstance* instance);
   void   (*releaseid)(void* id);
} CMPILIFYInstanceMIFT;

/* ------------------------------------------------------------------------- */
/* Generic CMPILIFY instance provider encapsulated object.                   */
/* ------------------------------------------------------------------------- */
typedef struct {
   void* hdl;                  /* Handle to implementation-specific data. */
   const CMPIBroker* brkr;     /* CIMOM handle, initialized on provider load. */
   const CMPIContext* ctx;     /* Caller's context */
   char* cn;                   /* Instance provider's CIM class. */
   CMPILIFYInstanceMIFT* ft;   /* Abstract resource API function table. */
} CMPILIFYInstanceMI;

/* ------------------------------------------------------------------------- */
/* Generic CMPILIFY instance provider intrinsic CMPI functions.              */
/* ------------------------------------------------------------------------- */
CMPIStatus CMPILIFYInstance_cleanup
        (CMPIInstanceMI* mi, const CMPIContext* ctx, CMPIBoolean terminating);

CMPIStatus CMPILIFYInstance_enumInstanceNames
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref);

CMPIStatus CMPILIFYInstance_enumInstances
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char** properties);

CMPIStatus CMPILIFYInstance_getInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char** properties);

CMPIStatus CMPILIFYInstance_createInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const CMPIInstance* inst);

CMPIStatus CMPILIFYInstance_modifyInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const CMPIInstance* inst,
        const char** properties);

CMPIStatus CMPILIFYInstance_deleteInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref);

CMPIStatus CMPILIFYInstance_execQuery
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char* lang, const char* query);

/* ------------------------------------------------------------------------- */
/* Macro to generate _Create_InstanceMI entry point, setup function table to */
/* the shared CMPILIFY instance provider intrinsic CMPI functions, and setup */
/* function table to the provider-specific resource access API functions.    */
/* ------------------------------------------------------------------------- */
#define CMPILIFYInstanceMIStub(pn,mi) \
static CMPILIFYInstanceMIFT _CMPILIFYMIFT = { \
   load, \
   unload, \
   begin, \
   end, \
   getnext, \
   get, \
   release, \
   add, \
   delete, \
   modify, \
   setproperties, \
   extract, \
   extractid, \
   releaseid, \
}; \
\
static CMPILIFYInstanceMI _CMPILIFYMI = { \
   NULL, \
   NULL, \
   NULL, \
   NULL, \
   &_CMPILIFYMIFT, \
}; \
\
static CMPIInstanceMIFT _CMPIMIFT = { \
   CMPICurrentVersion, \
   CMPICurrentVersion, \
   #pn, \
   CMPILIFYInstance_cleanup, \
   CMPILIFYInstance_enumInstanceNames, \
   CMPILIFYInstance_enumInstances, \
   CMPILIFYInstance_getInstance, \
   CMPILIFYInstance_createInstance, \
   CMPILIFYInstance_modifyInstance, \
   CMPILIFYInstance_deleteInstance, \
   CMPILIFYInstance_execQuery, \
}; \
\
CMPI_EXTERN_C \
CMPIInstanceMI* pn##_Create_InstanceMI(const CMPIBroker* brkr, \
                                       const CMPIContext* cmpi_ctx, \
                                       CMPIStatus* rc) { \
    static CMPIInstanceMI _CMPIMI = { \
       (void*)&_CMPILIFYMI, \
       &_CMPIMIFT, \
    }; \
    mi = &_CMPIMI; \
    ((CMPILIFYInstanceMI*)(mi->hdl))->brkr = brkr; \
    ((CMPILIFYInstanceMI*)(mi->hdl))->ctx = cmpi_ctx; \
    if (((CMPILIFYInstanceMI*)(mi->hdl))->ft->load() != CMPI_RC_OK) \
       mi = NULL; \
   return (CMPIInstanceMI*)mi; \
}; \

/* ------------------------------------------------------------------------- */
/* Optimized CMPILIFY 1RO instance provider abstract resource API.           */
/* ------------------------------------------------------------------------- */
typedef struct {
   CMPIrc (*load)();
   CMPIrc (*unload)(const int terminating);
   CMPIrc (*get)(struct xen_call_context *id, void** inst, const char** properties);
   void (*release)(void* inst);
   CMPIrc (*setproperties)(CMPIInstance* instance, const void* inst,
                           const char** properties);
} CMPILIFYInstance1ROMIFT;

/* ------------------------------------------------------------------------- */
/* Optimized CMPILIFY 1RO instance provider encapsulated object.             */
/* ------------------------------------------------------------------------- */
typedef struct {
   void* hdl;                   /* Handle to implementation-specific data. */
   const CMPIBroker* brkr;      /* CIMOM handle, initialized on provider load. */
   const CMPIContext* ctx;      /* Caller's context  */
   char* cn;                    /* Instance provider's CIM class. */
   CMPILIFYInstance1ROMIFT* ft; /* Abstract resource API function table. */
} CMPILIFYInstance1ROMI;

/* ------------------------------------------------------------------------- */
/* Optimized CMPILIFY 1RO instance provider intrinsic CMPI functions.        */
/* ------------------------------------------------------------------------- */
CMPIStatus CMPILIFYInstance1RO_cleanup
        (CMPIInstanceMI* mi, const CMPIContext* ctx, CMPIBoolean terminating);

CMPIStatus CMPILIFYInstance1RO_enumInstanceNames
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref);

CMPIStatus CMPILIFYInstance1RO_enumInstances
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char** properties);

CMPIStatus CMPILIFYInstance1RO_getInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char** properties);

CMPIStatus CMPILIFYInstance1RO_createInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const CMPIInstance* inst);

CMPIStatus CMPILIFYInstance1RO_modifyInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const CMPIInstance* inst,
        const char** properties);

CMPIStatus CMPILIFYInstance1RO_deleteInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref);

CMPIStatus CMPILIFYInstance1RO_execQuery
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char* lang, const char* query);

/* ------------------------------------------------------------------------- */
/* Macro to generate _Create_InstanceMI entry point, setup function table to */
/* the shared CMPILIFY instance provider intrinsic CMPI functions, and setup */
/* function table to the provider-specific resource access API functions.    */
/* ------------------------------------------------------------------------- */
#define CMPILIFYInstance1ROMIStub(pn,mi) \
static CMPILIFYInstance1ROMIFT _CMPILIFYMIFT = { \
   load, \
   unload, \
   get, \
   release, \
   setproperties, \
}; \
\
static CMPILIFYInstance1ROMI _CMPILIFYMI = { \
   NULL, \
   NULL, \
   NULL, \
   NULL, \
   &_CMPILIFYMIFT, \
}; \
\
static CMPIInstanceMIFT _CMPIMIFT = { \
   CMPICurrentVersion, \
   CMPICurrentVersion, \
   #pn, \
   CMPILIFYInstance1RO_cleanup, \
   CMPILIFYInstance1RO_enumInstanceNames, \
   CMPILIFYInstance1RO_enumInstances, \
   CMPILIFYInstance1RO_getInstance, \
   CMPILIFYInstance1RO_createInstance, \
   CMPILIFYInstance1RO_modifyInstance, \
   CMPILIFYInstance1RO_deleteInstance, \
   CMPILIFYInstance1RO_execQuery, \
}; \
\
CMPI_EXTERN_C \
CMPIInstanceMI* pn##_Create_InstanceMI(const CMPIBroker* brkr, \
                                       const CMPIContext* cmpi_ctx, \
                                       CMPIStatus* rc) { \
   static CMPIInstanceMI _CMPIMI = { \
      (void*)&_CMPILIFYMI, \
      &_CMPIMIFT, \
   }; \
   mi = &_CMPIMI; \
   ((CMPILIFYInstance1ROMI*)(mi->hdl))->brkr = brkr; \
   ((CMPILIFYInstance1ROMI*)(mi->hdl))->ctx = cmpi_ctx; \
   if (((CMPILIFYInstance1ROMI*)(mi->hdl))->ft->load() != CMPI_RC_OK) \
      mi = NULL; \
   return (CMPIInstanceMI*)mi; \
};


/*****************************************************************************/

/* ------------------------------------------------------------------------- */
/* Generic CMPILIFY association provider API.                                */
/* ------------------------------------------------------------------------- */
typedef struct {
   CMPIrc (*load)();
   CMPIrc (*unload)(const int terminating);
   CMPIrc (*getlhsquery)(const CMPIInstance* rhsinstance, char** query,
                         char** lang);
   CMPIrc (*getrhsquery)(const CMPIInstance* lhsinstance, char** query,
                         char** lang);
   CMPIrc (*setassocproperties)(const CMPIInstance* associnstnace);

} CMPILIFYAssociationMIFT;

/* ------------------------------------------------------------------------- */
/* Generic CMPILIFY association provider encapsulated object.                */
/* ------------------------------------------------------------------------- */
typedef struct {
   void* hdl;                     /* Handle to implementation-specific data. */
   const CMPIBroker* brkr;    /* CIMOM handle, initialized on provider load. */
   const CMPIContext *ctx;                               /* Caller's context */
   char* cn;                            /* Association provider's CIM class. */
   char* lhscn;                                 /* Left-hand-side CIM class. */
   char* lhsrol;                 /* Left-hand-side role/assoc property name. */
   char* lhsns;                    /* Namespace of left-hand-side CIM class. */
   char* rhscn;                                /* Right-hand-side CIM class. */
   char* rhsrol;                /* Right-hand-side role/assoc property name. */
   char* rhsns;                   /* Namespace of right-hand-side CIM class. */
   CMPILIFYAssociationMIFT* ft;  /* Abstract association API function table. */
} CMPILIFYAssociationMI;

/* ------------------------------------------------------------------------- */
/* Generic CMPILIFY association/instance provider intrinsic CMPI functions.  */
/* ------------------------------------------------------------------------- */
CMPIStatus CMPILIFYAssociation_cleanup
        (CMPIInstanceMI* mi, const CMPIContext* ctx, CMPIBoolean terminating);

CMPIStatus CMPILIFYAssociation_enumInstanceNames
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref);

CMPIStatus CMPILIFYAssociation_enumInstances
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char** properties);

CMPIStatus CMPILIFYAssociation_getInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char** properties);

CMPIStatus CMPILIFYAssociation_createInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const CMPIInstance* inst);

CMPIStatus CMPILIFYAssociation_modifyInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const CMPIInstance* inst,
        const char** properties);

CMPIStatus CMPILIFYAssociation_deleteInstance
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref);

CMPIStatus CMPILIFYAssociation_execQuery
        (CMPIInstanceMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* ref, const char* lang, const char* query);


CMPIStatus CMPILIFYAssociation_associationCleanup
        (CMPIAssociationMI* mi, const CMPIContext* ctx,
        CMPIBoolean terminating);

CMPIStatus CMPILIFYAssociation_associators
        (CMPIAssociationMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* op, const char* assocClass,
        const char* resultClass, const char* role, const char* resultRole,
        const char** properties);

CMPIStatus CMPILIFYAssociation_associatorNames
        (CMPIAssociationMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* op, const char* assocClass,
        const char* resultClass, const char* role, const char* resultRole);

CMPIStatus CMPILIFYAssociation_references
        (CMPIAssociationMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* op, const char* resultClass, const char* role,
        const char** properties);

CMPIStatus CMPILIFYAssociation_referenceNames
        (CMPIAssociationMI* mi, const CMPIContext* ctx, const CMPIResult* rslt,
        const CMPIObjectPath* op, const char* resultClass, const char* role);

/* ------------------------------------------------------------------------- */
/* Macro to generate _Create_InstanceMI and _Create_AssociationMI entry      */
/* points, setup function tables to the shared CMPILIFY instance/association */
/* provider intrinsic CMPI functions, and setup function table to the        */
/* provider-specific association API functions.                              */
/* ------------------------------------------------------------------------- */
#define CMPILIFYAssociationMIStub(cn,pn,lhscn,lhsrol,lhsns,rhscn,rhsrol,rhsns) \
static CMPILIFYAssociationMIFT _CMPILIFYMIFT = { \
   load, \
   unload, \
   getlhsquery, \
   getrhsquery, \
   setassocproperties, \
}; \
\
static CMPILIFYAssociationMI _CMPILIFYMI = { \
   NULL, \
   NULL, \
   NULL, \
   #cn, \
   #lhscn, \
   #lhsrol, \
   #lhsns, \
   #rhscn, \
   #rhsrol, \
   #rhsns, \
   &_CMPILIFYMIFT, \
}; \
\
static CMPIInstanceMIFT _CMPIINSTMIFT = { \
   CMPICurrentVersion, \
   CMPICurrentVersion, \
   #pn, \
   CMPILIFYAssociation_cleanup, \
   CMPILIFYAssociation_enumInstanceNames, \
   CMPILIFYAssociation_enumInstances, \
   CMPILIFYAssociation_getInstance, \
   CMPILIFYAssociation_createInstance, \
   CMPILIFYAssociation_modifyInstance, \
   CMPILIFYAssociation_deleteInstance, \
   CMPILIFYAssociation_execQuery, \
}; \
\
CMPI_EXTERN_C \
CMPIInstanceMI* pn##_Create_InstanceMI(const CMPIBroker* brkr, \
                                       const CMPIContext* cmpi_ctx, \
                                       CMPIStatus* rc) { \
   static CMPIInstanceMI _CMPIMI = { \
      (void*)&_CMPILIFYMI, \
      &_CMPIINSTMIFT, \
   }; \
   CMPIInstanceMI *mi = &_CMPIMI; \
   ((CMPILIFYAssociationMI*)(mi->hdl))->brkr = brkr; \
   ((CMPILIFYAssociationMI*)(mi->hdl))->ctx = cmpi_ctx; \
   if (((CMPILIFYAssociationMI*)(mi->hdl))->ft->load() != CMPI_RC_OK) \
      mi = NULL; \
   return (CMPIInstanceMI*)mi; \
}; \
\
static CMPIAssociationMIFT _CMPIASSOCMIFT = { \
   CMPICurrentVersion, \
   CMPICurrentVersion, \
   #pn, \
   CMPILIFYAssociation_associationCleanup, \
   CMPILIFYAssociation_associators, \
   CMPILIFYAssociation_associatorNames, \
   CMPILIFYAssociation_references, \
   CMPILIFYAssociation_referenceNames, \
}; \
\
CMPI_EXTERN_C \
CMPIAssociationMI* pn##_Create_AssociationMI(const CMPIBroker* brkr, \
                                             const CMPIContext* cmpi_ctx, \
                                             CMPIStatus* rc) { \
   static CMPIAssociationMI _CMPIMI = { \
      (void*)&_CMPILIFYMI, \
      &_CMPIASSOCMIFT, \
   }; \
   CMPIAssociationMI* mi = &_CMPIMI;                                    \
   ((CMPILIFYAssociationMI*)(mi->hdl))->brkr = brkr; \
   ((CMPILIFYAssociationMI*)(mi->hdl))->ctx = cmpi_ctx; \
   if (((CMPILIFYAssociationMI*)(mi->hdl))->ft->load() != CMPI_RC_OK) \
      mi = NULL; \
   return (CMPIAssociationMI*)mi; \
};


#endif // _CMPILIFY_H
