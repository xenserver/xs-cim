#ifndef _PROVIDERINTERFACE_H
#define _PROVIDERINTERFACE_H

#include <cmpidt.h>
#include <cmpimacs.h>
#include <cmpitrace.h>
#include <stdio.h>
#include <xen_utils.h>
#include <provider_common.h>
#include <dmtf.h>

#ifndef CMPI_EXTERN_C
  #ifdef __cplusplus
    #define CMPI_EXTERN_C extern "C"
  #else
    #define CMPI_EXTERN_C
  #endif
#endif

typedef struct
{
    const CMPIBroker *broker;
    const char *classname;      /* Name of the CIM class the provider is working with */
    xen_utils_session *session; /* xen session */
    bool cleanupsession;        /* should the session be cleaned up or not after the method is done */
    void *ctx;                  /* provider specific resource */
    bool ref_only;              /* just get the key properties */
} provider_resource;

typedef struct
{
    const CMPIBroker *broker;
    const char *classname;      /* Name of the CIM class the provider is working with */
    int current_resource;       /* index of the current resource - used during enumeration */
    xen_utils_session *session; /* xen session */
    void *ctx;                  /* provider specific resource */
    bool ref_only;              /* just get the key properties */
} provider_resource_list;

/* ------------------------------------------------------------------------- */
/* Generic instance provider abstract resource API.                 */
/* ------------------------------------------------------------------------- */
typedef struct {
    const char *(*xen_resource_get_key_property)(
                const CMPIBroker *broker,
                const char *classname);
    const char **(*xen_resource_get_keys)(
                const CMPIBroker *broker,
                const char *classname);
    CMPIrc (*xen_resource_list_enum)(
                xen_utils_session *session, 
                provider_resource_list *resources);
    CMPIrc (*xen_resource_list_cleanup)(
                provider_resource_list *resources);
    CMPIrc (*xen_resource_record_getnext)(
                provider_resource_list *resource_list,
                xen_utils_session *session,
                provider_resource *prov_res);
    CMPIrc (*xen_resource_record_cleanup)(
                provider_resource *prov_res);
    CMPIrc (*xen_resource_record_get_from_id)(
                char *res_uuid,
                xen_utils_session *session,
                provider_resource *prov_res);
    CMPIrc (*xen_resource_set_properties)(
                provider_resource *resource, 
                CMPIInstance *inst);
    CMPIrc (*xen_resource_add)(
                const CMPIBroker *broker,
                xen_utils_session *session,
                const void *res_id);
    CMPIrc (*xen_resource_delete)(
                const CMPIBroker *broker,
                xen_utils_session *session, 
                const char *inst_id);
    CMPIrc (*xen_resource_modify)(
                const CMPIBroker *broker,
                const void *res_id, 
                const void *modified_res,
                const char **properties, 
                CMPIStatus status, 
                char *inst_id, 
                xen_utils_session *session);
    CMPIrc (*xen_resource_extract)(
                void **res, 
                const CMPIInstance *inst, 
                const char **properties);
} XenProviderInstanceFT;

/* ------------------------------------------------------------------------- */
/* Function table to the provider-specific resource access API functions.    */
/* ------------------------------------------------------------------------- */
#define XenInstanceMIStub(pn) \
static XenProviderInstanceFT _XenInstanceProviderFT = { \
   xen_resource_get_key_property,\
   xen_resource_get_keys,\
   xen_resource_list_enum, \
   xen_resource_list_cleanup, \
   xen_resource_record_getnext, \
   xen_resource_record_cleanup, \
   xen_resource_record_get_from_id, \
   xen_resource_set_properties, \
   NULL, \
   NULL, \
   NULL, \
   NULL, \
}; \
\
CMPI_EXTERN_C XenProviderInstanceFT* pn##_Load_Instance_Provider()\
{\
\
   return &_XenInstanceProviderFT;\
}

#define XenFullInstanceMIStub(pn) \
static XenProviderInstanceFT _XenInstanceProviderFT = { \
   xen_resource_get_key_property,\
   xen_resource_get_keys,\
   xen_resource_list_enum, \
   xen_resource_list_cleanup, \
   xen_resource_record_getnext, \
   xen_resource_record_cleanup, \
   xen_resource_record_get_from_id, \
   xen_resource_set_properties, \
   xen_resource_add, \
   xen_resource_delete, \
   xen_resource_modify, \
   xen_resource_extract, \
}; \
\
CMPI_EXTERN_C XenProviderInstanceFT* pn##_Load_Instance_Provider()\
{\
\
   return &_XenInstanceProviderFT;\
}

/* ------------------------------------------------------------------------- */
/* Function table to the provider-specific resource access API functions.    */
/* ------------------------------------------------------------------------- */
typedef struct {
    CMPIStatus (*xen_resource_invoke_method)(
        CMPIMethodMI * self,               /* [in] Handle to this provider (i.e. 'self') */
        const CMPIBroker * broker,         /* [in] Broker to handle all CMPI calls */
        const CMPIContext * cmpi_context,  /* [in] Additional context info, if any */
        const CMPIResult * results,        /* [out] Results of this operation */
        const CMPIObjectPath * ref,        /* [in] Contains the CIM namespace, classname and desired object path */
        const char * methodname,           /* [in] Name of the method to apply against the reference object */
        const CMPIArgs * argsin,           /* [in] Method input arguments */
        CMPIArgs * argsout);               /* [in] Method output arguments */
} XenProviderMethodFT;

#define XenMethodMIStub(pn) \
static XenProviderMethodFT _XenMethodProviderFT = { \
   xen_resource_invoke_method \
}; \
\
CMPI_EXTERN_C XenProviderMethodFT* pn##_Load_Method_Provider()\
{\
\
   return &_XenMethodProviderFT;\
}

#endif // _PROVIDERINTERFACE_H
