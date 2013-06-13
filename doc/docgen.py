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
# Generates class documentation by making a GetClass() CIM request to the
# CIMOM running locally (can be changed to point anywhere) and
# inspecting the CIM class returned back (descripiton/values of 
# properties and methods)
# 
# For this to work, the MOF file corresponding to
# the class must already have been compiled into the CIMOM's repository.
# 
def generate_class_documentation (class_name, server, user, passwd, name_space="root/cimv2"):
    conn = pywbem.WBEMConnection('http://'+server+':5988', (user, passwd))
    params = {'LocalOnly':False, 'IncludeQualifiers':True, 'IncludeClassOrigin':True}
    class_obj = conn.GetClass(class_name, **params)
    doc = __generate_class_documentation(conn, class_obj)
    doc = doc.replace('<NAMESPACE>', name_space)
    return doc

def __usage ():
    print 'usage: python docgen.py <classname> <server> <username> <password> <<optional:namspace>>'
    print 'ex: python docgen.py Xen_VirtualSystemManagementService 192.168.2.78 root password'
    print 'Note: For docgen to work, the MOF file needs to have been registered with the CIMOM.'
    print '      For new classes, create a MOF file, add it to the schema directory,'
    print '      add the mof path to Makefile.am and run "make postinstall" to add it to the'
    print '      CIMOM\'s repository. Restart the CIMOM and you are good go with docgen.'

def __generate_class_documentation(conn, class_obj):
    doc = __get_class_doc_template()
    doc = doc.replace('<CLASSNAME>', class_obj.classname)
    doc = doc.replace('<CLASSDESCRIPTION>', class_obj.qualifiers['description'].value)
    doc = doc.replace('<CLASSLAYOUT>', __generate_class_definition(class_obj))

    association_class = 'false'
    if class_obj.qualifiers.has_key('Association'):
        association_class = 'true'

    # property list and descriptions
    keyProps = [p for p in class_obj.properties.values() \
                    if 'key' in p.qualifiers]
    props = class_obj.properties.values()
    props.sort()
    props_doc = ''
    for prop in props:
        prop_desc = ''
        props_doc += '<li>'
        if prop.qualifiers.has_key('description'):
            prop_desc = prop.qualifiers['description'].value
            prop_desc = prop_desc.replace("\\n", "")
        if prop in keyProps:
            props_doc += '<h3 class="propertyname">%s %s <FONT color="FF0000">(Key property)</FONT></h3><p class="propertydesc">%s</p>' % (__get_data_type(prop), prop.name, prop_desc)
        else:
            props_doc += '<h3 class="propertyname">%s %s</h3><p class="propertydesc">%s</p>' % (__get_data_type(prop), prop.name, prop_desc)
        # if parameter has a list of possible values it can return, document that
        if prop.qualifiers.has_key('valuemap'):
            props_doc += '<p class="propertyvalues"><b>Possible Values:</b><ul class="possiblevalues">%s</ul></p>' % (__get_values(prop, '<li>', '</li>'))
        props_doc += '</li>'
            
    props_doc = '<ul>%s</ul>' % props_doc
    doc = doc.replace('<PROPERTIES>', props_doc)

    # method list and descriptions
    methods = class_obj.methods.values()

    # to figure out if this method is supported or not generate a request to invoke the method and 
    # see if NOT_SUPPORTED is returned back. THIS IS NOT WORKING CURRENTLY
#    class_instances = conn.EnumerateInstanceNames(class_obj.classname)

    methods.sort()
    methods_doc = ''
    for method in methods:
        method_desc = method.qualifiers['description'].value
        method_desc = method_desc.replace("\\n", "")
        method_sig = __generate_method_signature(method)

#        if class_instances != None:
#            in_params = {}
#            try:
#                print 'invoking %s' % method.name
#                conn.InvokeMethod(method.name, class_instances[0], **in_params)
#            except pywbem.CIMError, c:
#                print 'method threw exception %s' % c

        methods_doc += '<li><h3 class="methodsig">%s</h3><p class"methoddesc">%s</p>' % (method_sig, method_desc)
        # for all parameters generate description in a table
        methods_doc += '<table class="paramstable">'
        methods_doc += '<tr class="table_header"><th>ParameterName</th><th>Description</th><th>Possible Values</th></tr>'
        for param in method.parameters.values():
            methods_doc += '<tr class="table_data"><td>%s</td><td>%s</td>' % (param.name, param.qualifiers['description'].value)
            method_arg_possible_values = __get_values(param, '', ', ')
            methods_doc += '<td>%s</td>' % (method_arg_possible_values)
            methods_doc += '</tr>'
        methods_doc += '</table>'
        # generate some description for the return value
        if method.qualifiers.has_key('Values'):
            methods_doc += '<p><b>Possible return values (%s):</b><ul class="possiblevalues">%s</ul></p>' % (method.return_type, __get_values(method,'<li>','</li>'))
        methods_doc += '</li>'
    methods_doc = '<ul>%s</ul>' % methods_doc
    doc = doc.replace('<METHODS>', methods_doc)

    return doc

def __generate_method_signature (method):
#    inParams = [ p for p in method.parameters.values() if \
#            'in' in p.qualifiers and p.qualifiers['in'].value ]
#    outParams = [ p for p in method.parameters.values() if \
#            'out' in p.qualifiers and p.qualifiers['out'].value ]
    in_params = ''
    out_params = ''
    conn = pywbem.WBEMConnection('http://localhost', ('', ''))

    for p in method.parameters.values():
        if 'out' in p.qualifiers and p.qualifiers['out'].value != False:
            if 'in' in p.qualifiers and p.qualifiers['in'].value != False:
                out_params += 'in out %s %s, ' % (__get_data_type(p), p.name)
            else:
                out_params += 'out %s %s, ' % (__get_data_type(p), p.name)
        else:
            in_params += 'in %s %s, ' % (__get_data_type(p), p.name)
    params = in_params + out_params
    params = params.rstrip(', ')
    sig = '%s %s ( %s )' %(method.return_type, method.name, params)
    return sig

def __get_values (param, ldecorator, rdecorator):
    possible_values = ''
    valcount = 0
    if param.qualifiers.has_key('valueMap'):
        for valuemap in param.qualifiers['valueMap'].value:
            if param.qualifiers.has_key('values'):
                possible_values += '%s%s ("%s")%s' % (ldecorator, valuemap, param.qualifiers['Values'].value[valcount], rdecorator)
            else:
                possible_values += '%s"%s"%s' % (ldecorator, valuemap, rdecorator)
            valcount = valcount + 1
    return possible_values
 
def __get_data_type (data):
    param_type = data.type
    if param_type == 'reference':
        param_type = 'ref %s' % (data.reference_class)
    if data.is_array:
        param_type += '[]'
    return param_type

def __generate_class_definition (class_obj):
    template = '''
    <pre><CLASSNAME> : <BASECLASS> {
Properties:
<PROPERTIES>
Methods:
<METHODS>}</pre>'''
    properties = class_obj.properties.values()
    properties.sort()
    method_objs = class_obj.methods.values()
    method_objs.sort()
    keyProps = [p for p in properties\
                    if 'key' in p.qualifiers]
    template = template.replace('<CLASSNAME>', class_obj.classname)
    template = template.replace('<BASECLASS>', class_obj.superclass)
    props = ''
    methods = ''
    for prop in properties:
        if prop in keyProps:
            # mark up key properties
            props += '    %s <b><FONT color="FF0000">%s</FONT></b>;\n' % (__get_data_type(prop), prop.name)
        else:
            props += '    %s %s;\n' % (__get_data_type(prop), prop.name)
    template = template.replace('<PROPERTIES>', props)
    for method in method_objs:
        methods += '    %s;\n' % __generate_method_signature(method)
    template = template.replace('<METHODS>', methods)
    return template

def __get_class_doc_template ():
    return '''
<html>
<head>
<link rel="stylesheet" type="text/css" href="classdoc.css" />
<title><CLASSNAME></title>
</head>
<body>
<h1>class <CLASSNAME></h1>
<p id="id_classdesc"><CLASSDESCRIPTION></p>
<p id="id_namespace">Defined in namespace: <NAMESPACE></p>
<p id="id_classlayout"><CLASSLAYOUT></p>
<h2>Properties</h2>
<PROPERTIES>
<h2>Methods</h2>
<p>It is possible that some of these methods are not supported and will return CIM_ERR_NOT_SUPPORTED (error code 7) back.</p>
<METHODS>
</body>
</html>
'''

if __name__ == '__main__':
    if len(sys.argv) < 5:
        __usage()
        sys.exit()
    classname = sys.argv[1]
    server = sys.argv[2]
    user = sys.argv[3]
    passwd = sys.argv[4]
    namespace = 'root/cimv2'
    if len(sys.argv) >= 5:
        namespace = sys.argv[5]
    doc = generate_class_documentation(classname, server, user, passwd, namespace)
    print doc

