#!/usr/bin/python
# Copyright (C) 2008-2009 Citrix Systems Inc.
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
    print 'usage: python codegen.py <classname> <remoteserver> <user> <pass> {optional: read-only}'
    print 'ex: \'python codegen.py Xen_VirtualSystemManagementService 192.168.2.100 root password\''
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
        val_map_code += generate_value_map_code(prop, prop.type,class_obj)
        prop_code += generate_prop_code(prop)
    for method in class_obj.methods.values():
        val_map_code += generate_value_map_code(method, method.return_type,class_obj) # return values

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
        code += '''        if(!_GetArgument(broker, argsin, "%s", %s, &argdata, &status))
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

def generate_value_map_code (prop, prop_type,class_obj):
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
            val_code += 'typedef enum _%s_%s{\n' % (class_obj.classname, prop.name)

    while cnt != len(vals):
        vals[cnt] = vals[cnt].replace('/', '_') # '/' is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace(' ', '_') # space is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace('-', '_') # '-' is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace('(', '_') # '(' is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace(')', '_') # ')' is an invalid character for a C constant or enum
        vals[cnt] = vals[cnt].replace('+', '_') # '+' is an invalid character for a C constant or enum  
        vals[cnt] = vals[cnt].replace('.', '_') # space is an invalid character for a C constant or enum
        if prop_type == 'string':
                val_code += '#define %s_%s_%s "%s"\n' % (class_obj.classname, prop.name, vals[cnt], vm[cnt])
        else:
            if vm[cnt].find('..') != -1:
                    val_code += '    /*%s_%s_%s=%s,*/\n' % (class_obj.classname, prop.name, vals[cnt], vm[cnt])
            else:
                    val_code += '    %s_%s_%s=%s,\n' % (class_obj.classname, prop.name, vals[cnt], vm[cnt])
        cnt += 1

    if prop_type != 'string':
            val_code += '}%s_%s;\n' % (class_obj.classname, prop.name)

    return val_code

def generate_prop_code (prop):
    prop_code = ''
    cmpi_type = convert_type_to_CMPI_type(prop)
    address_required = '&'
    if prop.type == 'string':
        address_required = ''
    if prop.is_array == True:
        prop_code = '''    //CMPIArray *arr = CMNewArray(resource->broker, 1, %s, NULL);\n    //CMSetArrayElementAt(arr, 0, (CMPIValue *)%s<value>, %s);\n    //CMSetProperty(inst, "%s",(CMPIValue *)&arr, %sA);\n''' % (cmpi_type, address_required, cmpi_type, prop.name, cmpi_type)
    else:
        if prop.type == 'string':
            prop_code = '    //CMSetProperty(inst, "%s",(CMPIValue *)<value>, %s);\n' % (prop.name, cmpi_type)
        elif prop.type == 'datetime':
            prop_code = '''    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, <time_value>);\n    //CMSetProperty(inst, "%s",(CMPIValue *)&date_time, CMPI_dateTime);\n''' % prop.name
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

#==========================================================

def get_instance_provider_code_template ():
    return '''// Copyright (C) 2008-2009 Citrix Systems Inc
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

/*#include other header files required by the provider */
#include "<CLASSNAME>.h"

/* TODO: Define any local resources here */
typedef struct _local_XX_resource {
    xen_XX XX;
    xen_XX_record *XX_rec;
} local_XX_resource;

static const char *classname = "<CLASSNAME>";    
static const char *keys[] = {<KEYS>}; 
static const char *key_property = <KEY>;

/******************************************************************************
 ************ Provider Export functions ************************************* 
 *****************************************************************************/
const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classnamestr
    )
{
    //if(xen_utils_class_is_subclass_of(_BROKER, classname, classnamestr))
    return key_property;
}

const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classnamestr
    )
{
    //if(xen_utils_class_is_subclass_of(_BROKER, classname, classnamestr))
    return keys;
}
/******************************************************************************
 * Function to enumerate a xen resource
 *
 * @param session - handle to a xen_utils_session object
 * @param resources - pointer to the provider_resource_list
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_list_enum(
       xen_utils_session *session, 
       provider_resource_list *resources
)
{
    xen_XX_set *XX_set = NULL;
    if (!xen_XX_get_all(session->xen, &XX_set))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = XX_set;
    return CMPI_RC_OK;
}
/******************************************************************************
 * Function to cleanup provider specific resource, this function is
 * called at various places in Xen_ProviderGeneric.c
 *
 * @param resources - handle to the provider_resource_list to be
 *    be cleaned up. Clean up the provider specific part of the
 *    resource.
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_list_cleanup(
       provider_resource_list *resources
)
{
    xen_XX_set *XX_set = (xen_XX_set *)resources->XX_set;
    if(XX_set)
        xen_XX_set_free(XX_set);
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Function to get the next xen resource in the resource list
 *
 * @param resources_list - handle to the provide_resource_list object
 * @param session - handle to the xen_utils_session object
 * @param prov_res - handle to the next provider_resource to be filled in.
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_record_getnext(
       provider_resource_list *resources_list,  /* in */
       xen_utils_session *session,              /* in */
       provider_resource *prov_res              /* in , out */
)
{
    xen_XX_set *XX_set = (xen_XX_set *)resources_list->ctx;
    if (XX_set == NULL || resources_list->current_resource == XX_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_XX_record *XX_rec = NULL;
    if (!xen_XX_get_record(
            session->xen,
            &XX_rec,
            XX_set->contents[resources_list->current_resource]
    ))
    {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    local_XX_resource *ctx = calloc(1, sizeof(local_XX_resource));
    if(ctx== NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->XX = XX_set->contents[resources_list->current_resource];
    ctx->XX_rec = XX_rec;
    XX_set->contents[resources_list->current_resource] = NULL; /* do not delete this*/
    prov_res->ctx = ctx;
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Function to cleanup the resource
 *
 * @param - provider_resource to be freed
 * @return CMPIrc error codes
****************************************************************************/
void cleanup_xen_resource_record(provider_resource *prov_res)
{
    local_XX_resource *ctx = (local_XX_resource *)prov_res->ctx;
    if(ctx) {
        if(ctx->XX_rec)
            xen_XX_record_free(ctx->XX_rec);
        if(ctx->XX)
            xen_XX_free(ctx->XX);
        free(ctx);
    }
}
/*****************************************************************************
 * Function to get a provider specific resource identified by an id
 *
 * @param res_uuid - resource identifier for the provider specific resource
 * @param session - handle to the xen_utils_session object
 * @param prov_res - provide_resource object to be filled in with the provider
 *                   specific resource
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc get_xen_resource_record_from_id(
       char *res_uuid, /* in */
       xen_utils_session *session, /* in */
       provider_resource *prov_res /* in , out */
)
{
    char buf[MAX_INSTANCEID_LEN];
    _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));
    //_CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf));
    xen_XX XX;
    xen_XX_record *XX_rec = NULL;
    if(!xen_XX_get_by_uuid(session->xen, &XX, buf) || 
       !xen_XX_get_record(session->xen, &XX_rec, XX))
    {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    local_XX_resource *ctx = calloc(1, sizeof(local_XX_resource));
    if(ctx== NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->XX = XX;
    ctx->XX_rec = XX_rec;
    prov_res->ctx = ctx;
    return CMPI_RC_OK;
}
#ifdef FULL_INSTANCE_PROVIDER 
// Use this only  if the CIM object CreateInstance, ModifyINstance
// and DEleteInstance
/*****************************************************************************
 * This function is called from add in the Xen_ProviderGeneric.c
 * The code specific to a provider may be implemented below.
 *
 * @param broker - CMPI Factory broker
 * @param session - xen session handle
 * @param res - resource to be added
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_add(
    const CMPIBroker *broker,
    xen_utils_session *session,
    void *res,
)
{
    return CMPI_RC_ERR_NOT_SUPPORTED;
}
/*****************************************************************************
 * Delete a provider specific resource identified by inst_id.
 *
 * @param session - handle to the xen_utils_session object
 * @param inst_id - resource identifier for the provider specific resource
 * @return CMPIrc error codes
****************************************************************************/
static CMPIrc xen_delete_resource(
    const CMPIBroker *broker,
    xen_utils_session *session, 
    char *inst_id
)
{
    return CMPI_RC_ERR_NOT_SUPPORTED;
    //If the following part is not required you may delete it. This is just a template
    //xen_XX XX_handle;
    //if(!xen_XX_get_by_uuid(session->xen, &XX_handle,inst_id)) {
    //     xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    //     return CMPI_RC_ERR_FAILED;
    //}
     
    //if(!xen_XX_destroy(session->xen, XX_handle)) {
    //    xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    //    return CMPI_RC_ERR_FAILED;
    //}
    //xen_XX_free(XX_handle);
    //
    //return CMPI_RC_OK;
}
/*****************************************************************************
 * Modify a provider specific resource identified by inst_id.
 *
 * @param res_id - pointer to a CMPIInstance that represents the CIM object
 *                 being modified
 * @param modified_res -
 * @param properties - list of properties to be used while modifying
 * @param session - handle to the xen_utils_session object
 * @param inst_id - resource identifier for the provider specific resource
 * @return CMPIrc error codes
*****************************************************************************/
static CMPIrc xen_modify_resource(
    const CMPIBroker *broker,
    const void *res_id, 
    const void *modified_res,
    const char **properties, 
    CMPIStatus status, 
    char *inst_id, 
    xen_utils_session *session
)
{
    return CMPI_RC_ERR_NOT_SUPPORTED;

    //If the following part is not required you may delete it. This is just a template

    //provider_resource *prov_resource = (provider_resource *)modified_res;
    //CMPIInstance *modified_inst;
    //CMPIData data;
    //char uuid[MAX_SYSTEM_NAME_LEN];
    //xen_XX_record *target_XX_rec;
    //char *tmp_str = NULL;
    //
    //if (prov_resource == NULL ||
    //    prov_resource->is_XX_record)
    //return CMPI_RC_ERR_FAILED;
    //
    //modified_inst = prov_resource->u.cmpi_inst;
    /* Extract the device uuid from InstanceID property. */
    //if (!_CMPIStrncpyDeviceNameFromID(uuid, inst_id, MAX_SYSTEM_NAME_LEN))
    //    return CMPI_RC_ERR_FAILED;
    //if (!xen_XX_get_record(session->xen, &target_XX_rec, (xen_XX)uuid)) {
    //    xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    //    goto Error;
    //}
    //Error:
    //if(tmp_str)
    //    free(tmp_str);
    //if(target_XX_rec)
    //    xen_XX_record_free(target_XX_rec);
    //return CMPI_RC_OK;
       
}
/************************************************************************
 * Function to extract resources
 *
 * @param res - provider specific resource to get values from
 * @param inst - CIM object whose properties are being set
 * @param properties - list of properties to be used while modifying
 * @return CMPIrc return values
*************************************************************************/
static CMPIrc xen_extract_resource(
    void **res, 
    const CMPIInstance *inst, 
    const char **properties
)
{
    /* Following is the default implementation*/
    (void)res;
    (void)inst;
    (void)properties;
    return CMPI_RC_ERR_NOT_SUPPORTED;  /* unsupported */

    /* Template for implementation when required */
    //  provider_resource *prov_resource;
    //
    //  (void)properties;
    /* Get memory for resource. */
    //  prov_resource = (provider_resource *)calloc(1, sizeof(provider_resource));
    //  if (prov_resource == NULL)
    //     return CMPI_RC_ERR_FAILED;
    //
    //  prov_resource->u.cmpi_inst = (CMPIInstance *)inst;
    //  *res = (void *)prov_resource;
    //  return CMPI_RC_OK;
}
#endif //FULL_INSTANCE_PROVIDER
/************************************************************************
 * Function that sets the properties of a CIM object with values from the
 * provider specific resource.
 *
 * @param resource - provider specific resource to get values from
 * @param inst - CIM object whose properties are being set
 * @return CMPIrc return values
*************************************************************************/
CMPIrc xen_resource_set_properties(provider_resource *resource, CMPIInstance *inst)
{
    <PROPERTIES>
    return CMPI_RC_OK;
}
/* CMPI instance provider function table setup */
#ifdef FULL_INSTANCE_PROVIDER
XenFullInstanceMIStub(<PROVIDERNAME>)
#else
XenInstanceMIStub(<PROVIDERNAME>)
#endif

'''
def get_method_provider_template ():
    return '''
/******************************************************************************
 * METHOD PROVIDER 
 * This interface gets invoked when caller calls the CIM class's 
 * extrinsic method
 *****************************************************************************/
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,       /* [in] CMPI Factory broker
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
    char error_msg[XEN_UTILS_ERROR_BUF_LEN];
    
    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\\"%s\\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\\"%s\\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\\"%s\\"", nameSpace));

    struct xen_call_context *ctx = NULL;
    if(!xen_utils_get_call_context(context, &ctx, &status)){
         goto Exit;
    }

    if (!xen_utils_validate_session(&session, ctx)) {
        CMSetStatusWithChars(_BROKER_M, &status, 
            CMPI_RC_ERR_METHOD_NOT_AVAILABLE, "Unable to connect to Xen");
        goto Exit;
    }
    
    if (strcmp(nameSpace, HOST_INSTRUMENTATION_NS) == 0) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("--- \\"%s\\" is not a valid namespace for %s", nameSpace, classname));
        CMSetStatusWithChars(_BROKER_M, &status, CMPI_RC_ERR_INVALID_NAMESPACE, 
            "Invalid namespace specified for Xen_ComputerSystem");
        goto Exit;
    }

    int argcount = CMGetArgCount(argsin, NULL);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- argsin=%d", argcount));
    
    argdata = CMGetKey(reference, key_property, &status);
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt find UUID of the VM to invoke method on"));
        goto Exit;
    }
    char *res_id = strdup(CMGetCharPtr(argdata.value.string));

    /* TODO : Find the backend resource based on the resource id */
    /* Check that the method has the correct number of arguments. */

    <METHODS> 
    else
        status.rc = CMPI_RC_ERR_METHOD_NOT_FOUND;
    
    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);

Exit:
    /* TODO : Free any resources here */
    _SBLIM_RETURNSTATUS(status);
}

/* CMPI Method provider function table setup */
XenMethodMIStub(<PROVIDERNAME>)

'''
def get_header_template ():
    return '''// ***** Generated by Codegen *****
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

#ifndef __<CLASSNAME_UPPER>_H__
#define __<CLASSNAME_UPPER>_H__

/* Values for the various properties of the class */

<VALUEMAP>

#endif /*__<CLASSNAME_UPPER>_H__*/
'''

if __name__ == '__main__':
    if (len(sys.argv) < 5):
        usage()
        sys.exit()

    #if os.path.isfile('%s.c' % class_obj.classname):
    #    print '%s.c already exists. Do you want to overwrite it ?' class_obj.classname
    #    sys.exit(1)

    classname = sys.argv[1]
    server = sys.argv[2]
    user = sys.argv[3]
    passwd = sys.argv[4]

    conn = pywbem.WBEMConnection('http://' + server, (user, passwd))
    params = {'LocalOnly':False, 'IncludeQualifiers':True, 'IncludeClassOrigin':True}
    class_obj = conn.GetClass(classname, **params)

    xen_class = None
    prefix = None

    if len(sys.argv) >= 6:
        xen_class = sys.argv[5]
    if len(sys.argv) >= 7:
        prefix = sys.argv[6]

    method_code = generate_method_provider_code(class_obj)
    fsrc = open('%s.c.new' % class_obj.classname, 'w')
    fhdr = open('%s.h.new' % class_obj.classname, 'w')
    (code, hdr) = generate_instance_provider_code(class_obj, prefix)

    if xen_class != None:
        code = code.replace('XX', xen_class)
        hdr = hdr.replace('XX', xen_class)
        method_code = method_code.replace('XX', xen_class)

    fsrc.write(code)
    fsrc.write(method_code)
    fhdr.write(hdr)
    fsrc.close()
    fhdr.close()

