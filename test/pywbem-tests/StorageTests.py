#!/usr/bin/env python

'''Copyright (C) 2008 Citrix Systems Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
=========================================================================
'''

import sys
import pywbem
from xen_cim_operations import *
from TestSetUp import *
from xml.dom import minidom

'''
Exercise the methods in the Xen_StoragePoolManagementService class.
Allows caller to create/destroy SRs, create/destroy VDIs, connect to/disconnect from VDI using BITS, ISCSI protocols
'''
class StorageTests(TestSetUp):
    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, False, False)
        self.spms = self.conn.EnumerateInstanceNames("Xen_StoragePoolManagementService")
        self.nfs_connection_string = ['server=192.168.5.10','serverpath=/vol/ShashiNFS']
        self.transfer_vm_network_config = ["172.16.2.100", "255.255.255.0", "10.60.1.25"] # fake static ip address for transfer vm, to check if it gets it
        #self.iscsi_connection_string = ['target=192.168.5.10','port=3260','SCSIid=360a9800050335538425a5177612f4150','targetIQN=iqn.1992-08.com.netapp:sn.135049502']
        self.iscsi_server = "192.168.5.10"
        self.iscsi_port = "3260"
        self.iscsi_connection_string = None # discover the SCSIid and targetIQN 
        #self.nfsiso_connection_string  = 'location=//10.60.1.15/users,iso_path=/reduser/testisos,username=eng/reduser,cifspassword=Citrix$2'
        self.nfsiso_connection_string  = ['location=//10.60.2.116/public','iso_path=/ISOs','username=ENG/reduser','cifspassword=Citrix$2']
        self.bad_nfs_connection_string = ['server=192.168.5.10'] # missing serverpath
        self.bad_iscsi_connection_string = ['target=192.168.5.10','port=3260','SCSIid=360a9800050335538425a5177612f4150'] # missing targetIQN
        self.disk_images = self.conn.EnumerateInstanceNames('Xen_DiskImage')
        self.good_disk_ref = self.disk_images[0]
        self.bad_disk_ref = self.disk_images[0].copy()
        self.bad_disk_ref['DeviceID'] = 'Xen:bad-vm-ref/bad-reference'

    def discover_elements_that_conform_to_storage_virtualization_profile(self):
        self.TestBegin()
        rc = 0
        # get the system virtualization profile
        profiles = []
        for profile in self.conn.EnumerateInstances('CIM_RegisteredProfile', 'root/interop'):
            if ((profile['RegisteredName'] == 'System Virtualization') and 
                (profile['RegisteredVersion'] >= '1.0.0') and
                (profile['RegisteredOrganization'] == 2)):
                profiles.append(profile)
        for profile in profiles:
            print 'Found System Virtualization Profile:'
            print '    ' + str(profile.items())
            profile_ref =  CIMInstanceName(classname=profile.classname, namespace='root/interop', 
                                           keybindings={"InstanceID":profile["InstanceID"]})
            # get the associated Storage Virtualization Profile
            association_class = 'CIM_ReferencedProfile' 
            result_class      = 'Xen_RegisteredStorageVirtualizationProfile'
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            ref_profiles = self.conn.AssociatorNames(profile_ref, **in_params)
            for profile_ref in ref_profiles:
                profile = self.conn.GetInstance(profile_ref)
                print 'Found Storage Resource Virtualization Profile:'
                print '    ' + str(profile.items())
                # get the ComputerSystem Element that conforms to the Storage Virtualization Profile
                association_class = 'CIM_ElementConformsToProfile' 
                result_class      = 'CIM_ComputerSystem'
                in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
                elements = self.conn.AssociatorNames(profile_ref, **in_params)
                for element in elements:
                    print 'Found CIM_ComputerSystem that conforms to Storage Virtualization Profile'
                    print '    ' + str(element.items())
                hosts = self.conn.EnumerateInstanceNames('Xen_HostComputerSystem')
                if len(hosts) == len(elements):
                    rc = 1 # found computer system
        self.TestEnd(rc)

    def _get_pool_capabilities (self, pool_ref):
        rc = 1
        association_class = 'CIM_ElementCapabilities' # association to traverse via Xen_ComputerSystem
        result_class      = 'CIM_AllocationCapabilities' # class we are looking for
        in_params 	      = {'ResultClass': result_class, 'AssocClass': association_class }
        caps = self.conn.AssociatorNames(pool_ref, **in_params)
        for cap_ref in caps:
            cap = self.conn.GetInstance(cap_ref)
            print 'Pool %s capabilities: %s' % (pool_ref['InstanceID'], str(cap))
        if len(caps) == 1:
            rc = 0
        return rc

    def _destroy_sr (self, pool_ref, forget_sr=False):
        isSuccess = 0
        method_name = 'DeleteResourcePool'
        in_params = {'Pool':pool_ref}
        if forget_sr == True:
            method_name = 'DetachStoragePool'
        [rval, out_params] = self.conn.InvokeMethod(method_name, self.spms[0], **in_params)
        return rval

    def _create_disk_image (self, sr_to_use):
        print sr_to_use.items()
        [system_id, sep, pool_id] = sr_to_use['InstanceID'].partition('/')
        print 'PoolID - %s' % pool_id
        new_disk_sasd = CIMInstance('Xen_DiskSettingData')
        new_disk_sasd['ElementName'] = sys._getframe(0).f_code.co_name
        new_disk_sasd['ResourceType'] = pywbem.Uint16(19)
        new_disk_sasd['ResourceSubType'] = "Disk" 
        new_disk_sasd['VirtualQuantity'] = pywbem.Uint64(2048)
        new_disk_sasd['AllocationUnits'] = "MegaBytes"
        new_disk_sasd['Bootable'] = False
        new_disk_sasd['Access'] = pywbem.Uint8(3)
        new_disk_sasd['PoolID'] = pool_id
        in_params = {"ResourceSetting": new_disk_sasd}
        [rc, out_params] = self.conn.InvokeMethod("CreateDiskImage", self.spms[0], **in_params)
        new_disk = out_params["ResultingDiskImage"]
        print "Created Disk"
        print new_disk.items()
        return [rc, new_disk]

    def _delete_disk_image (self, disk):
        in_params = {"DiskImage": disk}
        [rc, out_params] = self.conn.InvokeMethod("DeleteDiskImage", self.spms[0], **in_params)
        return rc

    def _connect_to_disk (self, disk_image, protocol, network_config=None, use_ssl=False, use_management_net=False):
        rc = 0
        connect_handle = None
        print 'connecting to %s' % str(disk_image)
        [system_id, sep, disk_id] = disk_image['DeviceID'].partition('/')
        network_to_use = None
        if use_management_net == False:
            network_refs = self.conn.EnumerateInstanceNames("Xen_VirtualSwitch")
            for network_ref in network_refs:
                network = self.conn.GetInstance(network_ref)
                if network['ElementName'] == 'Pool-wide network associated with eth0':
                    network_to_use = network_ref
        print 'Connecting to disk %s' % disk_id
        #if protocol == 'bits':
        #    in_params = {"DiskImage": disk_id, "VirtualSwitch": network_to_use, "Protocol":protocol, "UseSSL": True}
        #else:
        in_params = {"DiskImage": disk_id, "Protocol":protocol}
        if network_to_use != None:
            in_params["VirtualSwitch"] = network_to_use
        if use_ssl == True:
            in_params["UseSSL"] = use_ssl
        if network_config != None:
            in_params["NetworkConfiguration"] = network_config
        [rval, out_params] = self.conn.InvokeMethod("ConnectToDiskImage", self.spms[0], **in_params)
        job = None
        if rval == 4096:
            rc = 1
            job_ref = out_params['Job']
            job = WaitForJobCompletion(self.conn, job_ref)
            disk_uri = job["TargetURI"]
            connect_handle = job['ConnectionHandle']
            if protocol == 'iscsi':
                chapuser = job['Username']
                chappass = job['Password']
                print 'CHAP creds (user:%s, pass:%s)' % (chapuser, chappass)
            if disk_uri == '':
                rc = 0
            print 'Connected Disk URI: "%s"' % disk_uri
            if use_ssl == True:
                ssl_cert = job["SSLCertificate"]
                print 'SSL cert: "%s"' % ssl_cert
                if ssl_cert == '':
                    rc = 0 # found cert, success
            if network_config != None:
                if disk_uri.find(self.transfer_vm_network_config[0]) == -1:
                    rc = 0 # if we cannot find the static ip in the disk uri, then return error
            self.conn.DeleteInstance(job_ref)
        if rc == 1:
            connect_handle = job['ConnectionHandle']
        return connect_handle

    def _disconnect_from_disk (self, connect_handle):
        print 'Disconnecting from disk connection: %s' % connect_handle
        in_params = {"ConnectionHandle": connect_handle}
        [rc, out_params] = self.conn.InvokeMethod("DisconnectFromDiskImage", self.spms[0], **in_params)
        if rc != 4096:
            print 'DisconnectFromDiskImages returned error %d' % rc
        else:
            job_ref = out_params['Job']
            job = WaitForJobCompletion(self.conn, job_ref)
            if job['JobState'] != 7:
                rc = 1
            else:
                rc = 0
            self.conn.DeleteInstance(job_ref)
        return rc

    def _create_nfs_sr(self):
        isSuccess = 0
        setting = CIMInstance('CIM_ResourceAllocationSettingData')
        setting['Connection'] = self.nfs_connection_string
        setting['ResourceType'] = pywbem.Uint16(19)
        setting['ResourceSubTYpe'] = "nfs"
        pool_ref = None
        in_params = {'ElementName':'Test-NFS-SR', 'ResourceType': '19', 'Settings':setting}
        [rval, out_params] = self.conn.InvokeMethod("CreateStoragePool", self.spms[0], **in_params)
        if rval == 0:
            isSuccess = 1
            pool_ref = out_params['Pool']
            print pool_ref.items()
        return pool_ref

    def create_iscsi_sr(self):
        self.TestBegin()
        isSuccess = 0
        setting = CIMInstance('CIM_ResourceAllocationSettingData')
        setting['Connection'] = self.iscsi_connection_string
        setting['ResourceType'] = pywbem.Uint16(19)
        setting['ResourceSubTYpe'] = "lvmoiscsi"
        pool_ref = None
        in_params = {'ElementName':'Test-ISCSI-SR', 'ResourceType': '19', 'Settings':setting}
        [rval, out_params] = self.conn.InvokeMethod("CreateStoragePool", self.spms[0], **in_params)
        if rval == 0:
            isSuccess = 1
            pool_ref = out_params['Pool']
            print pool_ref.items()
        self.TestEnd(isSuccess)
        return pool_ref

    def create_nfsiso_sr(self):
        self.TestBegin()
        isSuccess = 0
        setting = CIMInstance('CIM_ResourceAllocationSettingData')
        setting['Connection'] = self.nfsiso_connection_string
        setting['ResourceType'] = pywbem.Uint16(19)
        setting['ResourceSubTYpe'] = "iso"
        pool_ref = None
        in_params = {'ElementName':'Test-NFSISO-SR', 'ResourceType': '16', 'Settings':setting}
        [rval, out_params] = self.conn.InvokeMethod("CreateStoragePool", self.spms[0], **in_params)
        if rval == 0:
            isSuccess = 1
            pool_ref = out_params['Pool']
            print pool_ref.items()
        self.TestEnd(isSuccess)
        return pool_ref

    def discover_iscsi_target (self):
        self.TestBegin()
        in_params = {'TargetHost':self.iscsi_server, 'Port':self.iscsi_port}
        print '\nDiscovering IQNs on server %s:%s' % (self.iscsi_server, self.iscsi_port)
        [rval, out_params] = self.conn.InvokeMethod("DiscoveriSCSITargetInfo", self.spms[0], **in_params)
        xmldom = minidom.parseString(out_params['TargetInfo']).documentElement
        iqns = xmldom.getElementsByTagName('TGT')
        target_iqn = None
        for tgt in iqns:
            ip_address = tgt.getElementsByTagName('IPAddress')[0].childNodes[0].nodeValue.lstrip().rstrip()
            if ip_address == '192.168.5.10':
                target_iqn = tgt.getElementsByTagName('TargetIQN')[0].childNodes[0].nodeValue.lstrip().rstrip()
                break
        xmldom.unlink()
        print '\nDiscovering LUNs on %s' % target_iqn
        in_params['TargetIQN'] = target_iqn # add the target iqn and rediscover and get all the luns 
        [rval, out_params] = self.conn.InvokeMethod("DiscoveriSCSITargetInfo", self.spms[0], **in_params)
        print "%s" % out_params["TargetInfo"]
        xmldom2 = minidom.parseString(out_params['TargetInfo']).documentElement
        scsiid = xmldom2.getElementsByTagName('LUN')[0].getElementsByTagName('SCSIid')[0].childNodes[0].nodeValue.lstrip().rstrip()
        print 'Found SCSIid %s' % scsiid
        success = 1
        self.iscsi_connection_string = ['target='+self.iscsi_server,'port='+self.iscsi_port,'targetIQN='+target_iqn, 'SCSIid='+scsiid,]
        self.TestEnd(success)
        return self.iscsi_connection_string

    # Silk requires test names to be unique !! hence the duplication here
    def get_nfs_pool_capabilities (self, pool_ref):
        self.TestBegin()
        rc = self._get_pool_capabilities(pool_ref)
        self.TestEnd2(rc)
    def get_iscsi_pool_capabilities (self, pool_ref):
        self.TestBegin()
        rc = self._get_pool_capabilities(pool_ref)
        self.TestEnd2(rc)
    def create_nfs_disk_image (self, sr):
        self.TestBegin()
        [rc, disk_image] = self._create_disk_image(sr)
        self.TestEnd2(rc)
        return disk_image
    def delete_nfs_disk_image (self, disk):
        self.TestBegin()
        rc = self._delete_disk_image(disk)
        self.TestEnd2(rc)
    def create_iscsi_disk_image (self, sr):
        self.TestBegin()
        [rc, disk_image] = self._create_disk_image(sr)
        self.TestEnd2(rc)
        return disk_image
    def delete_iscsi_disk_image (self, disk):
        self.TestBegin()
        rc = self._delete_disk_image(disk)
        self.TestEnd2(rc)
    def connect_to_nfs_disk_image_using_iscsi (self, disk_images):
        self.TestBegin()
        connect_handle = self._connect_to_disk(disk_images, "iscsi")
        self.TestEnd(connect_handle != None)
        return connect_handle
    def connect_to_nfs_disk_image_using_bits (self, disk_images):
        self.TestBegin()
        connect_handle = self._connect_to_disk(disk_images, "bits", self.transfer_vm_network_config, True, False)
        self.TestEnd(connect_handle != None)
        return connect_handle
    def disconnect_from_nfs_disk_image_using_iscsi (self, connect_handle):
        self.TestBegin()
        rc = self._disconnect_from_disk(connect_handle)
        self.TestEnd2(rc)
    def disconnect_from_nfs_disk_image_using_bits (self, connect_handle):
        self.TestBegin()
        rc = self._disconnect_from_disk(connect_handle)
        self.TestEnd2(rc)
    def connect_to_iscsi_disk_image_using_iscsi (self, disk_images):
        self.TestBegin()
        connect_handle = self._connect_to_disk(disk_images, "iscsi")
        self.TestEnd(connect_handle != None)
        return connect_handle
    def connect_to_iscsi_disk_image_using_bits (self, disk_images):
        self.TestBegin()
        connect_handle = self._connect_to_disk(disk_images, "bits", self.transfer_vm_network_config, True, True)
        self.TestEnd(connect_handle != None)
        return connect_handle
    def disconnect_from_iscsi_disk_image_using_iscsi (self, connect_handle):
        self.TestBegin()
        rc = self._disconnect_from_disk(connect_handle)
        self.TestEnd2(rc)
    def disconnect_from_iscsi_disk_image_using_bits (self, connect_handle):
        self.TestBegin()
        rc = self._disconnect_from_disk(connect_handle)
        self.TestEnd2(rc)
    def create_nfs_sr (self):
        self.TestBegin()
        sr = self._create_nfs_sr()
        self.TestEnd((sr != None))
        return sr
    def attach_nfs_sr (self):
        self.TestBegin()
        sr = self._create_nfs_sr()
        self.TestEnd((sr != None))
        return sr
    def detach_nfs_sr (self, sr):
        self.TestBegin()
        rc = self._destroy_sr(sr, True)
        self.TestEnd2(rc)
    def destroy_nfs_sr (self, sr):
        self.TestBegin()
        rc = self._destroy_sr(sr)
        self.TestEnd2(rc)
    def destroy_iscsi_sr (self, sr):
        self.TestBegin()
        rc = self._destroy_sr(sr)
        self.TestEnd2(rc)
    def detach_iscsi_sr (self, sr):
        self.TestBegin()
        rc = self._destroy_sr(sr, True)
        self.TestEnd2(rc)
    def destroy_nfsiso_sr (self, sr):
        self.TestBegin()
        rc = self._destroy_sr(sr, True)
        self.TestEnd2(rc)

    #
    # Error Tests
    # 
    def create_sr_error_tests (self):
        self.TestBegin()
        result = 1
        print 'Creating SR with missing connection string in RASD'
        try:
            setting = CIMInstance('CIM_ResourceAllocationSettingData')
            setting['Connection'] = [self.bad_nfs_connection_string]
            setting['ResourceType'] = pywbem.Uint16(19)
            setting['ResourceSubType'] = "nfs"
            in_params = {'ElementName':'Test-NFS-SR', 'ResourceType': '19', 'Settings':setting}
            [rval, out_params] = self.conn.InvokeMethod("CreateStoragePool", self.spms[0], **in_params)
            if rval == 0:
                print 'SR created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Creating SR with wrong ResourceSubType'
        try:
            setting = CIMInstance('CIM_ResourceAllocationSettingData')
            setting['Connection'] = [self.nfs_connection_string]
            setting['ResourceType'] = pywbem.Uint16(19)
            setting['ResourceSubType'] = "iscsinfsiscsi"
            in_params = {'ElementName':'Test-NFS-SR', 'ResourceType': '19', 'Settings':setting}
            [rval, out_params] = self.conn.InvokeMethod("CreateStoragePool", self.spms[0], **in_params)
            if rval == 0:
                print 'SR created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Creating SR with missing setting info'
        try:
            in_params = {'ElementName':'Test-NFS-SR', 'ResourceType': '19'}
            [rval, out_params] = self.conn.InvokeMethod("CreateStoragePool", self.spms[0], **in_params)
            if rval == 0:
                print 'SR created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Creating SR with missing element name'
        try:
            setting = CIMInstance('CIM_ResourceAllocationSettingData')
            setting['Connection'] = [self.nfs_connection_string]
            setting['ResourceType'] = pywbem.Uint16(19)
            setting['ResourceSubType'] = "nfs"
            in_params = {'ResourceType': '19', 'Settings':setting}
            [rval, out_params] = self.conn.InvokeMethod("CreateStoragePool", self.spms[0], **in_params)
            if rval == 0:
                print 'SR created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Creating SR with no params'
        try:
            in_params = {}
            [rval, out_params] = self.conn.InvokeMethod("CreateStoragePool", self.spms[0], **in_params)
            if rval == 0:
                print 'SR created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        self.TestEnd(result)
    def create_disk_image_error_tests(self):
        self.TestBegin()
        result = 1
        print 'Creating a disk image with bad disk RASD'
        try:
            [system_id, sep, pool_id] = self.sr_to_use['InstanceID'].partition('/')
            new_disk_sasd = CIMInstance('Xen_DiskSettingData')
            new_disk_rasd['PoolID'] = pool_id
            new_disk_rasd['ResourceType']  = "19"
            new_disk_rasd['ResourceSubType']  = pywbem.Uint8(123)
            new_disk_rasd['VirtualQuantity'] = "2048"
            new_disk_rasd['AllocationUnits'] = pywbem.Uint16(1234)
            new_disk_rasd['Bootable'] = "False"
            new_disk_rasd['Access'] = '3'
            in_params = {"ResourceSetting": new_disk_sasd}
            [rc, out_params] = self.conn.InvokeMethod("CreateDiskImage", self.spms[0], **in_params)
            if rc == 0:
                print 'Disk was created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Creating a disk image with disk RASD containing wrong poolid'
        try:
            new_disk_sasd = CIMInstance('Xen_DiskSettingData')
            new_disk_rasd['PoolID'] = "bad_reference"
            new_disk_rasd['ResourceType']  = pywbem.Uint16(19)
            new_disk_rasd['ResourceSubType']  = "Disk"
            new_disk_rasd['VirtualQuantity'] = pywbem.Uint64(2048)
            new_disk_rasd['AllocationUnits'] = "MegaBytes"
            new_disk_rasd['Bootable'] = False
            new_disk_rasd['Access'] = pywbem.Uint8(3)
            in_params = {"ResourceSetting": new_disk_sasd}
            [rc, out_params] = self.conn.InvokeMethod("CreateDiskImage", self.spms[0], **in_params)
            if rc == 0:
                print 'Disk was created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Creating a disk image with empty disk RASD'
        try:
            new_disk_sasd = ''
            in_params = {"ResourceSetting": new_disk_sasd}
            [rc, out_params] = self.conn.InvokeMethod("CreateDiskImage", self.spms[0], **in_params)
            if rc == 0:
                print 'Disk was created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Creating a disk image with no params'
        try:
            in_params = {}
            [rc, out_params] = self.conn.InvokeMethod("CreateDiskImage", self.spms[0], **in_params)
            if rc == 0:
                print 'Disk was created when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        self.TestEnd(result)
    def delete_disk_image_error_tests(self):
        self.TestBegin()
        result = 1
        print 'Deleteing a disk image with bad reference'
        try:
            in_params = {'DiskImage': self.bad_disk_ref}
            [rc, out_params] = self.conn.InvokeMethod("DeleteDiskImage", self.spms[0], **in_params)
            if rc == 0:
                print 'Disk Delete succeeded when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Deleteing a disk image with no params'
        try:
            in_params = {}
            [rc, out_params] = self.conn.InvokeMethod("DeleteDiskImage", self.spms[0], **in_params)
            if rc == 0:
                print 'Disk Delete succeeded when it shouldnt have been'
                result = 0
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        self.TestEnd(result)
    def connect_disk_image_error_tests(self):
        self.TestBegin()
        result = 1
        print 'Connect to bad disk reference'
        try:
            result = self._connect_to_disk(self.bad_disk_ref, 'bits')
            if result != None:
                print 'connect succeeded when it shouldnt have'
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Connect using bad protocol'
        try:
            result = self._connect_to_disk(self.good_disk_ref, 'bits123')
            if result != None:
                print 'connect succeeded when it shouldnt have'
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Connect using no protocol'
        try:
            result = self._connect_to_disk(self.good_disk_ref, '')
            if result != None:
                print 'connect succeeded when it shouldnt have'
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Connect using Null protocol'
        try:
            result = self._connect_to_disk(self.good_disk_ref, None)
            if result != None:
                print 'connect succeeded when it shouldnt have'
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        print 'Connect using Null protocol'
        try:
            result = self._connect_to_disk(self.good_disk_ref, None)
            if result != None:
                print 'connect succeeded when it shouldnt have'
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        self.TestEnd(result)
    def disconnect_disk_image_error_tests(self):
        self.TestBegin()
        result = 1
        print 'Pass bad disk reference to Disconnect'
        try:
            result = self._disconnect_from_disk(self.bad_disk_ref)
            if result == 0:
                print 'Disconnect succeeded when it shouldnt have'
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        self.TestEnd(result)
    def destroy_sr_error_tests(self):
        self.TestBegin()
        result = 1
        srs = self.conn.EnumerateInstanceNames('Xen_StoragePool')
        bad_sr_ref = srs[0].copy()
        bad_sr_ref['InstanceID'] = 'Xen:badreference'
        try:
            result = self._destroy_sr(bad_sr_ref)
            if result == 0:
                print 'Destroy SR succeeded when it shouldnt have'
        except Exception, e:
            print 'Exception thrown %s' % str(e)
        self.TestEnd(result)

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username, and password as arguments"
            print "Count is "+str(count)        
            sys.exit(0)

    st = StorageTests(sys.argv[1], sys.argv[2], sys.argv[3])
    try:
        # Discovery 
        #st.discover_elements_that_conform_to_storage_virtualization_profile()

        # NFS-ISO SR
        nfsiso_sr = st.create_nfsiso_sr()                   # create an NFS ISO SR
        st.destroy_nfsiso_sr(nfsiso_sr)                     # Destroy the SR using the SR CIM reference

        # NFS SR tests
        nfs_sr = st.create_nfs_sr()                         # Create an NFS SR
        disk_image = st.create_nfs_disk_image(nfs_sr)       # Create a VDI (disk image) on the SR
        connect_handle = st.connect_to_nfs_disk_image_using_bits(disk_image)    # Connect to the VDI using the BITS protocol
        st.disconnect_from_nfs_disk_image_using_bits(connect_handle)            # Disconnect from the VDI
        connect_handle = st.connect_to_nfs_disk_image_using_iscsi(disk_image)   # Connect to the VDI using the iSCSI protocol
        st.disconnect_from_nfs_disk_image_using_iscsi(connect_handle)           # Disconnect from the VDI
        st.delete_nfs_disk_image(disk_image)                # Delete the VDI
        st.detach_nfs_sr(nfs_sr)                            # Detach the SR
        nfs_sr = st.attach_nfs_sr()                         # Reattach the same SR
        st.destroy_nfs_sr(nfs_sr['InstanceID'])             # Destroy the SR using the instanceID property of SR

        # iSCSI SR tests
        st.discover_iscsi_target ()                          # discover the iSCSI target IQNs, LUNs and create the proper iscsi_connection_string
        iscsi_sr = st.create_iscsi_sr()                      # create an iSCSI SR
        disk_image = st.create_iscsi_disk_image(iscsi_sr)    # Create a VDI on it
        connect_handle = st.connect_to_iscsi_disk_image_using_bits(disk_image)  # Connect to the VDI using the the BITS protocol
        st.disconnect_from_iscsi_disk_image_using_bits(connect_handle)          # Disconnect from the VDI
        connect_handle= st.connect_to_iscsi_disk_image_using_iscsi(disk_image)  # Connect to the VDI using the iscsi protocol
        st.disconnect_from_iscsi_disk_image_using_iscsi(connect_handle)         # Disconnect from the VDI
        st.delete_iscsi_disk_image(disk_image)                                  # Delete the disk VDI
        (hostuuid, separator, sruuid) = iscsi_sr['InstanceID'].partition('/')   # partition the UUID string into "Xen:host" part and the "sr uuid" part
        st.destroy_iscsi_sr(sruuid)                          # Destroy the SR using just the UUID

        #Error tests
        st.create_sr_error_tests()                           # Pass bad parameters for the CreateStoragePool method and expect errors
        st.create_disk_image_error_tests()                   # Pass bad parameters for the CreateDiskImage method and expect errors
        st.delete_disk_image_error_tests()                   # Pass bad parameters for the DeleteDiskImage method and expect errors
        st.connect_disk_image_error_tests()                  # Pass bad parameters for the ConnectToDiskImage method and expect errors
        st.disconnect_disk_image_error_tests()               # Pass bad parameters for the DisconnectFromDiskImage method and expect errors
        st.destroy_sr_error_tests()                          # Pass bad parameters for the DeleteResourcePool method and expect errors
    finally:
        st.TestCleanup()
    
    sys.exit(0)

