#!/usr/bin/python
# Copyright (C) 2008 Citrix Systems Inc.
#
#    This library is free software; you can redistribute it and/or
#    modify it under the terms of the GNU Lesser General Public
#    License as published by the Free Software Foundation; either
#    version 2.1 of the License, or (at your option) any later version.
#
#    This library is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#    Lesser General Public License for more details.
#
#    You should have received a copy of the GNU Lesser General Public
#    License along with this library; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
import sys
import pywbem
import time

#
# Generates skeleton code (and header) for the CIM Class provider fo any given CIM
# class. It does so by making a CIM GetClass() request to the local CIMOM and 
# inspecting the CIMClass object returned back. 
# 
# For this to work, the MOF file corresponding to
# the class must already have been compiled into the CIMOM's repository.
# 
def usage ():
    print 'usage: python codegen.py <classname> {optional:<prefix>}'
    print 'ex: python codegen.py Xen_VirtualSystemManagementService'
    print 'Note: For codegen to work, the MOF file needs to have been registered with the CIMOM.'
    print '      For new classes, create a MOF file, add it to the schema directory,'
    print '      add the mof path to Makefile.am and run "make postinstall" to add it to the'
    print '      CIMOM\'s repository. Restart the CIMOM and you are good go with codegen.'

def generate_instance_provider_code(class_obj, prefix=None):
    keyProps = [p for p in class_obj.properties.values() \
                    if 'key' in p.qualifiers]
    props = class_obj.properties.values()
    props.sort()

    code = get_instance_provider_code_template()
    hdr = get_header_template()

    val_map_code = ''
    prop_code = ''
    for prop in props:
        val_map_code += generate_value_map_code(prop, prop.type, prefix)
        prop_code += generate_prop_code(prop)
    for method in class_obj.methods.values():
        val_map_code += generate_value_map_code(method, method.return_type, prefix) # return values

    code = code.replace('<PROPERTIES>', prop_code)
    code = code.replace('<KEYS>', generate_keys_code(keyProps))
    code = code.replace('<CLASSNAME>', class_obj.classname)
    code = code.replace('<PROVIDERNAME>', class_obj.classname)

    hdr = hdr.replace('<CLASSNAME_UPPER>', class_obj.classname.upper())
    hdr = hdr.replace('<VALUEMAP>', val_map_code)

    return (code, hdr)

def generate_method_provider_code (class_obj):
    code = ''
    total_methods = len(class_obj.methods.values())
    if total_methods > 1:
        method_code = ''
        method_number = 0
        for method in class_obj.methods.values():
            method_code += generate_method_code(method, method_number, total_methods)
            method_number += 1
        code = get_method_provider_template()
        code = code.replace('<METHODS>', method_code)
        code = code.replace('<PROVIDERNAME>', class_obj.classname)
    return code

def generate_method_code (method, number, total):
    inParams = [ p for p in method.parameters.values() if \
            'in' in p.qualifiers and p.qualifiers['in'].value ]
    outParams = [ p for p in method.parameters.values() if \
            'out' in p.qualifiers and p.qualifiers['out'].value ]
    if_clause = 'if'
    if number != 0:
        if_clause = '    else if'
    code = '''%s(strcmp(methodname, "%s") == 0) {\n''' % (if_clause, method.name)

    for inParam in inParams:
        cmpi_type = convert_type_to_CMPI_type(inParam)
        code += '''        if(!_GetArgument(_BROKER_M, argsin, "%s", %s, &argdata, &status))
        {
             /* return an error */
             goto Exit;
        }\n'''% (inParam.name, cmpi_type)

    for outParam in outParams:
        outParam_type = convert_type_to_CMPI_type(outParam)
        if outParam.type == 'reference':
            outParam_type = 'CMPI_ref'
        code += '''        CMPIObjectPath* %s_instance_op = NULL;
        CMAddArg(argsout, "%s", (CMPIValue *)&%s_instance_op, %s);\n''' % (outParam.name.lower(), outParam.name, outParam.name.lower(), outParam_type)
    code += '''    }\n'''
    return code

def generate_keys_code (keyprops):
    key_prop_code = ''
    for key in keyprops:
        key_prop_code += '"%s",' % key.name
    key_prop_code = key_prop_code.rstrip(',')
    return key_prop_code

def generate_value_map_code (prop, prop_type, prefix=None):
    vals = []
    if 'valuemap' in prop.qualifiers:
        vm = prop.qualifiers['valuemap'].value
        if 'values' in prop.qualifiers:
            vals = prop.qualifiers['values'].value
        else:
            import copy
            vals = copy.deepcopy(vm) # so we dont modify both below
    else:
        return ''

    cnt = 0
    # string values will be #defines and integer values will be enums
    val_code = '\n'
    if prop_type != 'string':
        if prefix == None:
            val_code += 'typedef enum _%s{\n' % prop.name
        else:
            val_code += 'typedef enum _%s_%s{\n' % (prefix, prop.name)

    while cnt != len(vals):
        vals[cnt] = vals[cnt].replace('/', '_') # '/' is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace(' ', '_') # space is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace('-', '_') # '-' is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace('(', '_') # '(' is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace(')', '_') # ')' is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace('+', '_') # '+' is an invalid character for a C constant or enum  
        vals[cnt] = vals[cnt].replace('.', '_') # space is an invalid character for a C constant or enum
        if prop_type == 'string':
            if prefix == None:
                val_code += '#define %s_%s "%s"\n' % (prop.name, vals[cnt], vm[cnt])
            else:
                val_code += '#define %s_%s_%s "%s"\n' % (prefix, prop.name, vals[cnt], vm[cnt])
        else:
            if vm[cnt].find('..') != -1:
                if prefix == None:
                    val_code += '    /*%s_%s=%s,*/\n' % (prop.name, vals[cnt], vm[cnt])
                else:
                    val_code += '    /*%s_%s_%s=%s,*/\n' % (prefix, prop.name, vals[cnt], vm[cnt])
            else:
                if prefix == None:
                    val_code += '    %s_%s=%s,\n' % (prop.name, vals[cnt], vm[cnt])
                else:
                    val_code += '    %s_%s_%s=%s,\n' % (prefix, prop.name, vals[cnt], vm[cnt])
        cnt += 1

    if prop_type != 'string':
        if prefix == None:
            val_code += '}%s;\n' % prop.name
        else:
            val_code += '}%s_%s;\n' % (prefix, prop.name)

    return val_code

def generate_prop_code (prop):
    prop_code = ''
    cmpi_type = convert_type_to_CMPI_type(prop)
    address_required = '&'
    if prop.type == 'string':
        address_required = ''
    if prop.is_array == True:
        prop_code = '''    //CMPIArray *arr = CMNewArray(_BROKER, 1, %s, NULL);\n    //CMSetArrayElementAt(arr, 0, (CMPIValue *)%s<value>, %s);\n    //CMSetProperty(inst, "%s",(CMPIValue *)&arr, %sA);\n''' % (cmpi_type, address_required, cmpi_type, prop.name, cmpi_type)
    else:
        if prop.type == 'string':
            prop_code = '    //CMSetProperty(inst, "%s",(CMPIValue *)<value>, %s);\n' % (prop.name, cmpi_type)
        elif prop.type == 'datetime':
            prop_code = '''    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);\n    //CMSetProperty(inst, "%s",(CMPIValue *)&date_time, CMPI_dateTime);\n''' % prop.name
        else:
            prop_code = '    //CMSetProperty(inst, "%s",(CMPIValue *)&<value>, %s);\n' % (prop.name, cmpi_type)
    return prop_code


def convert_type_to_CMPI_type (prop):
    prop_type = 'CMPI_%s' % prop.type
    if prop.type == 'string':
        prop_type = 'CMPI_chars'
    elif prop.type == 'datetime':
        prop_type = 'CMPI_dateTime'
    elif prop.type == 'reference':
        prop_type = 'CMPI_ref'
    return prop_type

def get_instance_provider_code_template ():
    return '''// Copyright (C) 2008 Citrix Systems Inc
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

#include <cmpidt.h>
#include <cmpimacs.h>
#include <stdlib.h>

#include "cmpilify.h"
#include "cmpitrace.h"
#include "./include/<CLASSNAME>.h"
#include "xen_utils.h"
#include "provider_common.h"


/****************** INSTANCE PROVIDER **************************************/
/* Common declarations for each CMPI "Cimpler" instance provider */
static const CMPIInstanceMI* mi;
#define _BROKER (((CMPILIFYInstanceMI*)(mi->hdl))->brkr)
#define _CLASS (((CMPILIFYInstanceMI*)(mi->hdl))->cn)
#define _KEYS (((CMPILIFYInstanceMI*)(mi->hdl))->kys)


/* C structs to store the data for all resources. */
typedef struct {
    /*TODO: YOUR RESOURCE LIST DEFINITION GOES HERE*/
    int current_resource; /* index of the current resource - used during enumeration */
    xen_utils_session *session; /* session to be used */
} provider_resource_list;

typedef struct {
    /*TODO: YOUR RESOURCE DEFINITION GOES HERE*/
    xen_utils_session *session;
    bool cleanupsession; /* release the above session or not */
} provider_resource;

/* Name of the class implemented by this instance provider. */
static char * _CLASSNAME = "<CLASSNAME>";

/* Keys that uniquely identify the objects of this class */
static const char *keys[] = {<KEYS>};

/* CMPILIFY abstraction methods. Look at cmpilify.c on how these get called  */
/* Load gets called when the provider is first loaded by the CIMOM */
static CMPIrc load()
{
    xen_utils_xen_init2();
    return CMPI_RC_OK;
}

/* Unload gets called when the provider is unloaded by the CIMOM */
static CMPIrc unload(const int terminating)
{
    (void)terminating;
    
    xen_utils_xen_close2();
    return CMPI_RC_OK;
}

/* Begin gets called when cmpilify wants to begin enumerating
   the backend resources represented by this class */
static CMPIrc begin(void **res_list, const char **properties)
{
    provider_resource_list *resources = NULL;
    xen_utils_session *session = NULL;
    (void)properties;
    
    if (res_list == NULL)
        return CMPI_RC_ERR_FAILED;
    
    if (!xen_utils_validate_session(&session)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Unable to establish connection with Xen"));
        return CMPI_RC_ERR_FAILED;
    }
    
    /* TODO Make Xen call to populate the resources list */
    /* xen_XXX_set *XXX_set;
    if(!xen_XXX_get_all(session->xen, &XXX_set))
        goto Error; */

    resources = (provider_resource_list *)calloc(1, sizeof(provider_resource_list));
    if (resources == NULL)
        return CMPI_RC_ERR_FAILED;
    resources->session = session;

    *res_list = (void *)resources;
    return CMPI_RC_OK;
    
Error:
    _SBLIM_TRACE_FUNCTION(_SBLIM_TRACE_LEVEL_ERROR,
                          xen_utils_trace_error(session->xen));
    
    /* TODO Free any Xen resources that might be left around */
    return CMPI_RC_ERR_FAILED;
}

/* End gets called when cmpilify wants to end enumerating
   the backend resources represented by this class */
static void end(void *res_list)
{
    provider_resource_list *resources = (provider_resource_list *)res_list;
    
    if (resources) {
        /* TODO Free any resources from xen */
        xen_utils_cleanup_session(resources->session);
        free(resources);
    }
}

/* End gets called when cmpilify wants to get the next backend resource */
static CMPIrc getnext(void *res_list, void **res, const char **properties)
{
    provider_resource_list *resources_list = (provider_resource_list *)res_list;
    (void)properties;
    
    if (resources_list == NULL || res == NULL)
        return CMPI_RC_ERR_FAILED;
    
    /* Are there any resources or has end of list of resources been reached? */
    if (resources_list-><resource> == NULL ||
       resources_list->current_resource == resources_list-><resource>->size)
        return CMPI_RC_ERR_NOT_FOUND;
    
    /* Get the current resource record. */
    resources_list->session->xen->ok = true;

    /* TODO: Call xen to get the resource record */
    /* if(!xen_XXX_get_record(resources->session->xen, &xx_rec,
            resources_list-><resource>->contents[resources_list->current_resource]
            ))
    {
        xen_utils_trace_error(session->xen);
        return CMPI_RC_ERR_FAILED;
    }
    */
    
    provider_resource *prov_res = calloc(1, sizeof(provider_resource));
    if(prov_res == NULL)
        return CMPI_RC_ERR_FAILED;

    prov_res->session = resources_list->session;
    prov_res->cleanupsession = false;

    /* TODO: fill in the provider resuorce with the data */
    //resources_list-><resource>->contents[resources_list->current_resource++] = NULL; /* do not delete this */
    *res = (void *)prov_res;
    return CMPI_RC_OK;
}

/* Get gets called when cmpilify wants to get a backend resource 
 * identified by the keys passed in */
static CMPIrc get(const void *res_id, void **res, const char **properties)
{
    CMPIInstance *inst = (CMPIInstance *)res_id;
    xen_utils_session *session = NULL;
    CMPIData data;
    char *res_uuid=NULL;
    CMPIStatus status = {CMPI_RC_OK, NULL};
    (void)properties;
    
    if (CMIsNullObject(inst) || res == NULL)
        return CMPI_RC_ERR_FAILED;
    
    data = CMGetProperty(inst, <KEY>, &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(data))
        return CMPI_RC_ERR_INVALID_PARAMETER;
    
    /* Extract the resource identifier string from the CMPIString. */
    res_uuid = CMGetCharPtr(data.value.string);
    if (res_uuid == NULL)
        return CMPI_RC_ERR_FAILED;

    /* if a substring has to be extracted out of the res id */
    //char buf[MAX_INSTANCEID_LEN];
    //_CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf));
    //_CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));

    if (!xen_utils_validate_session(&session)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                   ("--- Unable to establish connection with Xen"));
        return CMPI_RC_ERR_FAILED;
    }
    
    /* TODO : Get the resource based on the res_uuid*/
    /* if(!xen_XXX_get_by_uuid(session->xen, &XX, res_uuid) ||
          !xen_XXX_get_record(session->xen, &XX_rec, XX))
    {
        _SBLIM_TRACE_FUNCTION(_SBLIM_TRACE_LEVEL_ERROR, 
                              ("coundlnt find XX for %s: Error",
                              res_uuid));
        return CMPI_RC_ERR_NOT_FOUND;

    } */
    
    provider_resource *prov_res = calloc(1, sizeof(provider_resource));
    if(prov_res == NULL)
        return CMPI_RC_ERR_FAILED;

    prov_res->session = session;
    prov_res->cleanupsession = true;

    /* TODO : Fill in the provider resource with the backend data */
    
    *res = (void *)prov_res;
    return CMPI_RC_OK;
}

/* Release gets called when cmpilify wants to release a single backend resource */
static void release(void *res)
{
    provider_resource *prov_res = (provider_resource *)res;
    if(prov_res)
    {
        /* TODO Free resource */
        if(prov_res->cleanupsession)
            xen_utils_cleanup_session(prov_res->session);
        free(prov_res);
    }
}

/* Add gets called when cmpilify wants to Add a backend resource */
static CMPIrc add(const void *res_id, const void *res)
{
    (void)res_id;
    (void)res;
    
    return CMPI_RC_ERR_NOT_SUPPORTED; /* unsupported */
}

/* Add gets called when cmpilify wants to delete a backend resource */
static CMPIrc delete(const void *res_id)
{
    CMPIInstance *inst = (CMPIInstance *)res_id;
    CMPIData data;
    CMPIStatus status = {CMPI_RC_OK, NULL};
    
    return CMPI_RC_ERR_NOT_SUPPORTED;
#if NOT_SUPPORTED    
    xen_utils_session *session = NULL;

    if (CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;
    
    /* find the backend resource based on the following:
    <KEYS> */
    data = CMGetProperty(inst, <KEY>, &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(data))
        return CMPI_RC_ERR_FAILED;
    
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(data))
        return CMPI_RC_ERR_FAILED;
    
    /* Extract the resource id string from the CMPIString. */
    char *inst_id = CMGetCharPtr(data.value.string);
    if ((inst_id == NULL) || (*inst_id == '\0'))
        return CMPI_RC_ERR_FAILED;
    
    if (!xen_utils_validate_session(&session)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Unable to establish connection with Xen"));
        return CMPI_RC_ERR_FAILED;
    }
    /* get the object and delete it */
    
    return CMPI_RC_OK;
#endif // NOT_SUPPORTED
}

/* Add gets called when cmpilify wants to modify a backend resource */
static CMPIrc modify(const void *res_id, const void *modified_res,
                     const char **properties)
{
    (void)res_id;
    (void)modified_res;
    (void)properties;
    
    return CMPI_RC_ERR_NOT_SUPPORTED; /* unsupported */
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
    if (res == NULL || CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;
    resource->session->xen->ok = true;
    
    /* Setup a filter to only return the desired properties. */
    CMSetPropertyFilter(inst, properties, keys);
    
    /* Populate the instance's properties with the backend data */
    <PROPERTIES>
    
    return CMPI_RC_OK;
}


/*
 * Set resource data from the CMPIInstance properties.  Only needs to
 * be implemented if add() and/or modify() are supported.
 */
static CMPIrc extract(void **res, const CMPIInstance *inst,
                      const char **properties)
{
    (void)res;
    (void)inst;
    (void)properties;
    
    return CMPI_RC_ERR_NOT_SUPPORTED;  /* unsupported */
}


/* Get resource id from CMPIInstance properties. */
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


/* Setup CMPILIFY function tables and instance provider entry point.*/
/* CMPILIFYInstanceMIStub(<CLASS>,<PROVIDER_NAME>,<keys>,<CMPIInstanceMI_HANDLE>) */
CMPILIFYInstanceMIStub(<CLASSNAME>, <PROVIDERNAME>, keys, mi)

'''

def get_method_provider_template ():
    return '''

/************** METHOD PROVIDER **********************************************/

/* Common declarations for CMPILIFY method provider */
static const CMPIBroker* _BROKER_M; 

/* 
 * MethodCleanup()
 * Perform any necessary cleanup immediately before this provider is unloaded.
 */
static CMPIStatus MethodCleanup(
                CMPIMethodMI * self,          /* [in] Handle to this provider (i.e. 'self'). */
                const CMPIContext * context,          /* [in] Additional context info, if any. */
                CMPIBoolean terminating)   /* [in] True if MB is terminating */
{
    (void)terminating;
    CMPIStatus status = { CMPI_RC_OK, NULL };    /* Return status of CIM operations. */
    
    _SBLIM_ENTER("MethodCleanup");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, 
        ("--- self=\\"%s\\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, 
        ("--- context=\\"%s\\"", CMGetCharPtr(CDToString(_BROKER_M, context, NULL))));
    
    if (session) {
        xen_utils_xen_close(session);
        session = NULL;
    }
    _SBLIM_RETURNSTATUS(status);
}

/* 
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 */
static CMPIStatus InvokeMethod(
                CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
                const CMPIContext * context,    /* [in] Additional context info, if any */
                const CMPIResult * results,     /* [out] Results of this operation */
                const CMPIObjectPath * reference, /* [in] Contains the CIM namespace, classname and desired object path */
                const char * methodname,          /* [in] Name of the method to apply against the reference object */
                const CMPIArgs * argsin,          /* [in] Method input arguments */
                CMPIArgs * argsout)             /* [in] Method output arguments */
{
    CMPIStatus status = {CMPI_RC_OK, NULL};      /* Return status of CIM operations. */
    char * namespace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    unsigned long rc = 0;
    CMPIData argdata;
    xen_utils_session * session = NULL;
    char error_msg[XEN_UTILS_ERROR_BUF_LEN];
    
    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\\"%s\\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\\"%s\\"", CMGetCharPtr(CDToString(_BROKER_M, context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\\"%s\\"", CMGetCharPtr(CDToString(_BROKER_M, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\\"%s\\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\\"%s\\"", namespace));
    
    if (!xen_utils_validate_session(&session)) {
        CMSetStatusWithChars(_BROKER_M, &status, 
            CMPI_RC_ERR_METHOD_NOT_AVAILABLE, "Unable to connect to Xen");
        goto Exit;
    }
    
    if (strcmp(namespace, HOST_INSTRUMENTATION_NS) == 0) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("--- \\"%s\\" is not a valid namespace for %s", namespace, _CLASSNAME));
        CMSetStatusWithChars(_BROKER_M, &status, CMPI_RC_ERR_INVALID_NAMESPACE, 
            "Invalid namespace specified for Xen_ComputerSystem");
        goto Exit;
    }

    int argcount = CMGetArgCount(argsin, NULL);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- argsin=%d", argcount));
    
    argdata = CMGetKey(reference, <KEY>, &status);
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt find UUID of the VM to invoke method on"));
        goto Exit;
    }
    char *res_id = strdup(CMGetCharPtr(argdata.value.string));


    /* TODO : Find the backend resource based on the resource id */
    /* Check that the method has the correct number of arguments. */

    <METHODS> 
    
    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);

Exit:
    /* TODO : Free any resources here */
    _SBLIM_RETURNSTATUS(status);
}

/* 
 * MethodInitialize()
 * Perform any necessary initialization immediately after the 
 * method provider is first loaded.
 */
static void MethodInitialize(
    CMPIMethodMI * self,		    /* [in] Handle to this provider (i.e. 'self'). */
    const CMPIContext * context)	/* [in] Additional context info, if any. */
{
    _SBLIM_ENTER("MethodInitialize");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\\"%s\\"", self->ft->miName));
    _SBLIM_TRACE(2, ("--- context=\\"%s\\"", CMGetCharPtr(CDToString(_BROKER_M, context, NULL))));

    /* Initialized Xen session object. */
    if (session == NULL)
        xen_utils_xen_init(&session);
   
    _SBLIM_RETURN();
}

/* CMPI Method provider function table setup */
CMMethodMIStub( , <PROVIDERNAME>, _BROKER_M, MethodInitialize(&mi, ctx));

'''
def get_header_template ():
    return '''// ***** Generated by Codegen *****
// Copyright (C) 2008 Citrix Systems Inc
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

#ifndef __<CLASSNAME_UPPER>_H__
#define __<CLASSNAME_UPPER>_H__

/* Values for the various properties of the class */

<VALUEMAP>

#endif /*__<CLASSNAME_UPPER>_H__*/
'''

if __name__ == '__main__':
    if (len(sys.argv) < 2) or (len(sys.argv) > 3):
        usage()
        sys.exit()

    #if os.path.isfile('%s.c' % class_obj.classname):
    #    print '%s.c already exists. Do you want to overwrite it ?' class_obj.classname
    #    sys.exit(1)
        
    conn = pywbem.WBEMConnection('localhost', ('', ''))
    params = {'LocalOnly':False, 'IncludeQualifiers':True, 'IncludeClassOrigin':True}
    class_obj = conn.GetClass(sys.argv[1], **params)

    prefix = None
    if len(sys.argv) == 3:
        prefix = sys.argv[2]
    method_code = generate_method_provider_code(class_obj)
    fsrc = open('%s.c.new' % class_obj.classname, 'w')
    fhdr = open('%s.h.new' % class_obj.classname, 'w')
    (code, hdr) = generate_instance_provider_code(class_obj, prefix)
    fsrc.write(code)
    fsrc.write(method_code)
    fhdr.write(hdr)
    fsrc.close()
    fhdr.close()

