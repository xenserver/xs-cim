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
import os
import sys
from docgen import *
from subprocess import *

#
# This module enumerates all the CIM classes that are listed in the CIMOM
# registration file and generates documentation on each class using the docgen
# python module.
# 
def usage ():
    return '''
python MakeDoc.py <path_to_reg_file> <output_directory> <CIM servername> <user> <password>
    <path_to_reg_file> : this is the path to the provider registration file.
    <output_directory> : this is the directory where the generated HTML docs will be written.
    <CIM servername>   : CIM server with upto-date schema where getclass requests can be directed to
    <user>             : CIM server username
    <password>         : CIM Server password
    '''

def __generate_INDEX_html (prov_info_list):
    index_html = __get_INDEX_template()
    insts = []
    assocs = []
    events = []
    for prov_info in prov_info_list:
        (classname, namespace, prov_name, prov_module, prov_type) = prov_info
        #item = '<li><A HREF="%s.html">%s</A></li>' % (classname, classname)
        if 'association' in prov_type :
            assocs.append(classname)
        if 'indication' in prov_type:
            events.append(classname)
        if 'instance' in prov_type or 'method' in prov_type:
            insts.append(classname)
    insts.sort()
    assocs.sort()
    events.sort()
    insts_str = ''
    assoc_str = ''
    event_str = ''
    for classname in insts:
         insts_str += '<li><A HREF="%s.html">%s</A></li>' % (classname, classname)
    for classname in assocs:
         assoc_str += '<li><A HREF="%s.html">%s</A></li>' % (classname, classname)
    for classname in events:
         event_str += '<li><A HREF="%s.html">%s</A></li>' % (classname, classname)
    insts_str = '<ul>' + insts_str + '</ul>'
    assoc_str = '<ul>' + assoc_str + '</ul>'
    event_str = '<ul>' + event_str + '</ul>'
    index_html = index_html.replace('<CLASSLIST>', insts_str)
    index_html = index_html.replace('<ASSOCIATIONLIST>', assoc_str)
    index_html = index_html.replace('<EVENTLIST>', event_str)
    return index_html

def __read_provider_info (reg_file):
    '''
    returns the provider list in the form of the tuple 
    (classname, namespace(s), providername, providermodule, providertype)
    '''
    f = open(reg_file)
    prov_info_list = f.readlines()
    f.close()
    out_prov_info_list = []
    for prov_info in prov_info_list:
        prov_info.lstrip(' ')
        if prov_info.startswith('#'): # ignore comments
            continue
        out_prov_info = prov_info.split(' ', 4)
        out_prov_info_list.append(out_prov_info)
    return out_prov_info_list

def __get_INDEX_template ():
    return '''
<html>
<head>
<link rel="stylesheet" type="text/css" href="classdoc.css" />
<title>XenServer CIM Documentation README</title>
<h1>XenServer CIM</h1>
<p>The XenServer CIM interface allows the management of a pool of XenServer hosts 
for the purpose of creating and managing the lifecycle of virtual machines. 
The management interface is based on the 
<a href="http://www.dmtf.org/standards/cim/">DMTF CIM standard</a> and implements 
the <a href="http://www.dmtf.org/standards/published_documents/DSP2013_1.0.0.pdf">CIM management 
profiles relating to virtualization</a>. For more information on CIM or the SVPC management profiles,
please refer to <a href="http://dmtf.org">DMTF</a>.
</p>
</head>

<body>

<h2>Sample Source code</h2>
    <p>The following code samples illustrate the <b>general pattern</b> 
    for the use of the XenServer CIM classes. You are free to use the 
    code samples at your own risk. <b>For extensive usage examples, 
    please refer to the test directory in the source distribution, which contain
    extensive samples in python, powershell wsman scripts and winrm batch files.</b> 
    For more information on the CIM classes (description of the 
    properties and methods, etc.), please refer to the class documentation 
    below
    </p>
<li><A HREF="../Python-Sample.html">Usage of XenServer-CIM with python</A></li>
<li><A HREF="../WSMan-Sample.html">Usage of XenServer-CIM with Windows WS-Management</A></li>

<h2>Class Documentation</h2>
<p>The following XenServer CIM Classes are defined for managing XenServer hosts, 
    virtual machines and their devices etc. These are implemented as providers 
    and are hosted on the <a href="http://openpegasus.org">Open Pegasus CIMOM</a>. 
    The implementation supports the CIM-XML and WS-Management transport protocols. 
    Any standard CIM-XML or WS-Management client, such as wbemcli or WinRM 
    (Windows WS-Management client) can be used to access these CIM classes as the 
    source code examples (listed above). All classes are implemented in the 'root/cimv2'
    CIM namespace</p>
<p>The interface supports the following standard CIM-XML methods</p> 
    <li>EnumerateInstances - enumerates all CIM object instances (and all their properties) of a class</li>
    <li>EnumerateInstanceNames -enumerates all CIM object references (just the key properties) of a class</li>
    <li>GetInstance - gets a particular CIM instance give a CIM object reference </li>
    <li>DeleteInstance</li>
    <li>ModifyInstance</li>
    <li>InvokeMethod - Invokes an extrinsic method defined for an object.</li>
    <li>Associators</li>
    <li>AssociatorNames</li>
    <li>References</li>
    <li>ReferenceNames</li> 
    Indications are also supported for the event classes listed below.</p>

<p>The interface also supports the following WS-Management methods</p>
    <li>Identify - queries the server to determine if it supports the WS-Management protocol</li>
    <li>Enumerate - Enumerates all CIM object instances of a certain class</li>
    <li>Get - Gets a specific instance of a CIM class</li>
    <li>Delete - Deletes a specific instance of a CIM class</li>
    <li>Modify - Modifies a specific instance of a CIM class</li>
    <li>Invoke - Invokes an extrinsic method defined for a CIM instance</li>
<p>Note: Certain methods such as CreateInstance or Put may not be supported and may require the use of specific
service objects to create or delete the underlying objects. For Example: A CreateInstance call will not work 
to be able to create a new Virtual System. Instead, one would have to use the
Xen_VirtualSystemManagementService factory class to create/modify/delete VMs.</p>
<h2>Instance/Method Providers</h2>
<p>Following is a list of classes representing managed entities on XenServer, 
capabilities of those entities, or service objects for managing them. Some of them
implement methods that can be invoked using the InvokeMethod CIM intrinsic API.</p>
<CLASSLIST>
<h2>Association Providers</h2>
<p>Following is a list of association classes representing relationships between
managed entities on XenServer such as the relationship between a VM and its disk. Each
instance of an association class (eg: Xen_ComputerSystemDisk) represents a unique instance
of a XenServer object (a Xen_ComputerSystem) and a unique instance of its associated object 
(a Xen_Disk).</p>
<ASSOCIATIONLIST>
<h2>Events</h2>
<p>The following CIM indications are defined for the XenServer CIM implementation. 
They can be subscribed to using the 
<a href="http://www.openpegasus.org/pp/uploads/40/3162/SNMPMapperBackgrounder.pdf">
CIM indication subscription</a> mechanism.</p>
<EVENTLIST>
</body>
</html>'''

if __name__ =='__main__':
    # get list of classes and their provider types
    if len(sys.argv) != 6:
        print usage()
        sys.exit(1)

    # read the class list
    prov_list = __read_provider_info(sys.argv[1])

    # write the index.html that lins to the other class documentation
    index_html_str = __generate_INDEX_html(prov_list)
    f = open('%s/index.html' % (sys.argv[2]),'w')
    f.write(index_html_str)
    f.close()


    # write all the individual class documentation
    for prov_info in prov_list:
        (classname, namespace, prov_name, prov_module, prov_type) = prov_info
        try:
            doc = generate_class_documentation(classname, sys.argv[3], sys.argv[4], sys.argv[5], namespace)
            f = open('%s/%s.html' % (sys.argv[2], classname), 'w')
            f.write(doc)
            f.close()
        except Exception, e:
            print 'Could not generate documentation for %s (namespace %s): Error %s' % (classname, namespace, str(e))
            continue
        
