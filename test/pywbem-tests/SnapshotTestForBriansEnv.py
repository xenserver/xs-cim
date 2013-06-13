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
import time
import getpass
import os
from xen_cim_operations import *
from TestSetUp import *
import bits

'''
Exercises the methods in the Xen_SnapshotManagementService class
Allows caller to create/destroy snapshots
'''
class SnapshotTest(TestSetUp):

    def __init__(self, Ip, userName, password):
        TestSetUp.__init__(self, Ip, userName, password, False, False)
        self.snapshot_settings = CIMInstance('Xen_ComputerSystemSnapshot')
        self.snapshot_settings['ElementName'] = self.__class__.__name__ + '_Snapshot'
        self.orig_vssd = None
        self.snapshot_ref = None
        self.imported_vm = None
        self.snapshot_tree_dir = '/tmp/SnapshotTreeTest/'
        os.makedirs(self.snapshot_tree_dir)
        #self.vsss = None
        self.cimserver_ip = Ip
        self.vsss = self.conn.EnumerateInstanceNames("Xen_VirtualSystemSnapshotService")
        #self.test_vm = self._CreateSnapshotTestVM() # create a test vm to create snapshots out of
        self.test_vm = None
        #self.export_import_vm = self.test_vm # VM to test export/import snapshot tree with - expected to be created prior to running test - crate one with sufficient branches etc.
        vms = self.conn.EnumerateInstanceNames('Xen_ComputerSystem')
        for vm in vms:
           vminst = self.conn.GetInstance(vm)
           if vminst['ElementName'] == 'DemoLinuxVM' and vminst['Description'] == 'Source VM used with ExportImportSnapshotTree.ps1':
               self.export_import_vm = vm
               break


    def _CreateSnapshotTestVM(self):
        self.orig_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        self.orig_vssd['ElementName'] = "SnapshotTestVM"
        self.orig_vssd['Description'] = "VM to test snapshot actions"
        vm = CreateVMBasedOnTemplateName(self.conn, self.vsms[0], "XenServer Transfer VM", self.orig_vssd)
        # VM has 1 proc, 1 disk and 1 NIC
        return vm

    def _MakeVMChanges (self, vm, new_name, new_description):
        # Add an additional disk, processor and NIC to the test vm
        rasds = [self.proc1_rasd, self.nic1_rasd]
        keys = {'InstanceID': 'Xen:' + vm['Name']}
        affected_conf = CIMInstanceName('Xen_ComputerSystemSettingData', keys)
        in_params = {'ResourceSettings': rasds, 'AffectedConfiguration':affected_conf }
        AddResourcesToVM(self.conn, self.vsms[0], in_params)

        # Also modify the name of the VM
        new_vssd = CIMInstance('Xen_ComputerSystemSettingData')
        new_vssd['InstanceID'] = 'Xen:' +  vm['Name']
        new_vssd['ElementName'] = new_name
        new_vssd['Description'] = new_description
        in_params = {'SystemSettings': new_vssd} 
        ModifyVM(self.conn, self.vsms[0], in_params)

    def discover_capabilites (self):
        self.TestBegin()
        caps = self.conn.EnumerateInstanceNames("Xen_VirtualSystemSnapshotCapabilities")
        rc = 0
        if len(caps) != 1:
            print "Exception: Couldnt find SnapshotCapabilities"
        else:
            print 'Xen_VirtualSystemSnapshotCapabilities'
            print '-------------------------------------'
            for cap in caps:
                cap_inst = self.conn.GetInstance(cap)
                print cap_inst.items()
            caps = self.conn.EnumerateInstanceNames("Xen_VirtualSystemSnapshotServiceCapabilities")
            if len(caps) != 1:
                print "Exception: Couldnt find SnapshotServiceCapabilities"
            else:
                print 'Xen_VirtualSystemSnapshotServiceCapabilities'
                print '--------------------------------------------'
                for cap in caps:
                    cap_inst = self.conn.GetInstance(cap)
                    print cap_inst.items()
                rc = 1
        self.TestEnd(rc)

    def find_snapshotservice (self):
        self.TestBegin()
        rc = 1
        hosts = self.conn.EnumerateInstanceNames("Xen_HostComputerSystem")
        if len(hosts) == 0:
            print "Exception: Couldnt find a host"
            rc = 0
        else:
            association_class = 'Xen_HostedVirtualSystemSnapshotService' # association to traverse via Xen_ComputerSystem
            result_class      = 'Xen_VirtualSystemSnapshotService'  # result class we are looking for
            in_params         = {'ResultClass': result_class, 'AssocClass': association_class }
            vssss             = self.conn.AssociatorNames(hosts[0], **in_params)
            if len(vssss) != 1:
                print 'Error: Couldnt not find a Xen_VirtualSystemSnapshotService'
                rc = 0
            else:
                self.vsss = vssss
        self.TestEnd(rc)

    def create_snapshot_and_make_vm_changes (self, newname, newdesc):
        self.TestBegin()
        rc = 1
        in_params = {'AffectedSystem': self.test_vm, 
                     'SnapshotSettings': self.snapshot_settings}
        self.snapshot_ref = CreateVMSnapshot (self.conn, self.vsss[0], in_params)
        if self.snapshot_ref == None:
            rc = 0
            print 'ERROR: Snapshot was not NOT created'
        # Make some changes to the VM 
        print 'making vm changes'
        self._MakeVMChanges(self.test_vm, newname, newdesc)
        self.TestEnd(rc)      
        return self.snapshot_ref

    def _upload_disk_contents (self, spms, disk_image_ref, src, append_vhd=False):
        [handle, disk_uri, ssl_cert, user, passwd] = ConnectToDisk (self.conn, spms, disk_image_ref, "bits", None, None, False, True)
        protocol = disk_uri.split('/')[0].split(':')[0]
        (server, port) = disk_uri.split('/')[2].split('@')[1].split(':')
        (user, passwd) = disk_uri.split('/')[2].split('@')[0].split(':')
        vdi_path = '/' + disk_uri.split('/')[3]
        if append_vhd:
            vdi_path = vdi_path + '.vhd' # need to append .vhd to the end 
        disk_image = self.conn.GetInstance(disk_image_ref)
        print 'Uploading vhd %s (size %d) to disk %s (size %d) on server %s' % (
            src, os.stat(src).st_size, vdi_path, disk_image['NumberOfBlocks'], server)
        bits.upload(src, protocol, server, port, user, passwd, vdi_path)
        DisconnectFromDisk (self.conn, spms, handle)

    def _download_disk_contents (self, disk_url, dest):
        protocol = disk_url.split('/')[0].split(':')[0]
        if '@' in disk_url:
            serverip = disk_url.split('/')[2].split('@')[1]
            (username, password)  = disk_url.split('/')[2].split('@')[0].split(':')
        else:
            serverip = disk_url.split('/')[2]
            username = ''
            password = ''
        disk_urlpath = '/' + disk_url.split('/')[3]
        destloc = dest
        if os.path.isdir(dest):
            destloc = dest + disk_urlpath
        print 'Downloading disk %s on server %s to %s' % (disk_urlpath, serverip, destloc)
        bits.download(protocol, serverip, disk_urlpath, destloc, username, password)

    def export_snapshot_tree (self):
        self.TestBegin()
        rc = 1
        # Start the export process
        in_params = {'System': self.export_import_vm,
                     #'UseSSL': True,
                    }
        (rval, out_params) = self.conn.InvokeMethod('StartSnapshotForestExport', self.vsss[0], **in_params)
        print 'StartExport returned %d, job:%s' % (rval, out_params['Job'])
        if rval == 4096:
            job_ref = out_params['Job']
            job = WaitForJobCompletion(self.conn, job_ref)

            # Get the metadata (export.xva) URL and download it
            print 'PrepareExport returned %d, metadata url: %s' % (rval, job['MetadataURI'])
            self._download_disk_contents(job['MetadataURI'], self.snapshot_tree_dir+'metadata.raw')

            if 'SSLCertificates' in job.keys() and job['SSLCertificates'] != None:
                for cert in job['SSLCertificates']:
                    print 'Got SSL Certificate:%s' % cert

            for disk_url in job['DiskImageURIs']:
                print 'diskURL:%s' % (disk_url)
                #time.sleep(5) # wait till the transfer vm comes up
                self._download_disk_contents (disk_url, self.snapshot_tree_dir)

            # end the export process and tear down all transfer vms
            in_params = {'ExportConnectionHandle': job['ExportConnectionHandle']}
            (rval, out_params) = self.conn.InvokeMethod('EndSnapshotForestExport', self.vsss[0], **in_params)
            if rval == 4096:
                job_ref = out_params['Job']
                job = WaitForJobCompletion(self.conn, job_ref)
            else:
                print 'no job returned from EndSnapshotForestExport'
                rc = 0
        else:
            print 'no job returned from StartSnapshotForestExport'
            rc = 0
        self.TestEnd(rc)

    def __find_disk_file (self, srcdir, pattern):
        filenames = os.listdir(srcdir)
        for filename in filenames:
            print 'matching %s with %s' % (pattern, filename)
            if pattern in filename:
                return filename

    def import_snapshot_tree (self):
        self.TestBegin()
        # upload the metadata disk first
        rc = 1
        spms = self.conn.EnumerateInstanceNames('Xen_StoragePoolManagementService')
        disk_dir = self.snapshot_tree_dir
        #disk_dir = '/tmp/SnapshotTreeImportSample/'
        src = disk_dir + self.__find_disk_file(disk_dir, '.raw')
        [rval, metadata_disk] = CreateDiskImage(self.conn, spms[0], self.sr_to_use, os.stat(src).st_size*2, 'Metadata')
        self._upload_disk_contents(spms[0], metadata_disk, src)

        # now import the rest of the delta disks one by one
        in_params = {'MetadataDiskImage': metadata_disk}
        [rval, out_params] = self.conn.InvokeMethod("PrepareSnapshotForestImport", self.vsss[0], **in_params)
        import_context = out_params['ImportContext']
        disk_image_map = ''
        iter = 0
        while True:
            print '---------- ITERATION %d ----------------------' % iter
            print 'DiskMap output from previous iteration:\n%s\n' % disk_image_map
            print 'Import instructions:\n%s\n' % import_context
            # create the next disk in the delta disk chain sequence
            in_params = {'ImportContext':import_context, 
                         'StoragePool':self.sr_to_use,
                         'DiskImageMap':disk_image_map}
            [rval, out_params] = self.conn.InvokeMethod("CreateNextDiskInImportSequence", self.vsss[0], **in_params)
            if rval == 32768: # no more disks to be created
                break

            src = disk_dir +  self.__find_disk_file(disk_dir, out_params['OldDiskID'])
            self._upload_disk_contents(spms[0], out_params['NewDiskImage'], src, True)
            print 'OldDiskID: %s being mapped to %s' % (out_params['OldDiskID'], out_params['NewDiskImage']['DeviceID'])

            # get the updated disk image map and import instructions
            print '========== END ITERATION %d ====================' % iter
            disk_image_map = out_params['DiskImageMap']
            if 'ImportContext' in out_params.keys():
                import_context = out_params['ImportContext']
            else:
                print 'No more instructions available. Done creating disks'
                break
            iter = iter + 1
        in_params = {'MetadataDiskImage': metadata_disk,
                     'StoragePool': self.sr_to_use,
                     'DiskImageMap': disk_image_map
                      }
        (rval, out_params) = self.conn.InvokeMethod('FinalizeSnapshotForestImport', self.vsss[0], **in_params)
        self.imported_vm = out_params['VirtualSystem']
        print 'Created new VM after import:' + self.imported_vm['Name']
        self.TestEnd(rc)

    def revert_to_snapshot (self, snapshot_ref):
        self.TestBegin()
        in_params = {'Snapshot': snapshot_ref}
        print 'reverting to snapshot'
        rc = RevertVMToSnapshot (self.conn, self.vsss[0], in_params)
        self.TestEnd(rc)

    def destroy_snapshot (self, snapshot_ref):
        self.TestBegin()
        in_params = {'AffectedSnapshot': snapshot_ref}
        rc = DestroyVMSnapshot(self.conn, self.vsss[0], in_params)
        self.TestEnd(rc)

    def local_cleanup (self):
        if self.test_vm != None:
            DeleteVM(self.conn, self.vsms[0], self.test_vm)
        #DeleteVM(self.conn, self.vsms[0], self.imported_vm)
        # cleanup temp snapshot disks
        #filenames = os.listdir(self.snapshot_tree_dir)
        #for filename in filenames:
        #    os.remove(self.snapshot_tree_dir+'/'+filename)
        #os.removedirs(self.snapshot_tree_dir)

    def create_snapshot_error_tests (self):
        self.TestBegin()
        rc = 1
        print 'Try with a bad snapshot setting'
        bad_snapshot_setting = CIMInstance('Xen_ComputerSystemSnapshot')
        bad_snapshot_setting['ElementName'] = pywbem.Uint64(12345)
        in_params = {'AffectedSystem': self.test_vm, 
                     'SnapshotSettings': bad_snapshot_setting}

        snapshot_ref = CreateVMSnapshot (self.conn, self.vsss[0], in_params)
        if snapshot_ref != None:
            print 'Snapshot created unexpectedly'
            rc = 0
        print 'Try with a different bad snapshot setting'
        bad_snapshot_setting = CIMInstance('Xen_ComputerSystemSnapshot123')
        bad_snapshot_setting['ElementName'] = '12345'
        in_params = {'AffectedSystem': self.test_vm, 
                     'SnapshotSettings': bad_snapshot_setting}
        snapshot_ref = CreateVMSnapshot (self.conn, self.vsss[0], in_params)
        if snapshot_ref != None:
            print 'Snapshot created unexpectedly'
            rc = 0
        print 'Try with an emtpy snapshot setting'
        bad_snapshot_setting = '''
           '''
        in_params = {'AffectedSystem': self.test_vm, 
                     'SnapshotSettings': bad_snapshot_setting}
        snapshot_ref = CreateVMSnapshot (self.conn, self.vsss[0], in_params)
        if snapshot_ref != None:
            print 'Snapshot created unexpectedly'
            rc = 0
        print 'Try with a NULL snapshot setting'
        in_params = {'AffectedSystem': self.test_vm, 
                     'SnapshotSettings': None}
        snapshot_ref = CreateVMSnapshot (self.conn, self.vsss[0], in_params)
        if snapshot_ref != None:
            print 'Snapshot created unexpectedly'
            rc = 0
        print 'Try with a bad reference to an affected system'
        bad_test_vm = self.test_vm.copy()
        bad_test_vm['Name'] = 'bad-reference'
        in_params = {'AffectedSystem': bad_test_vm, 
                     'SnapshotSettings': self.snapshot_settings}
        snapshot_ref = CreateVMSnapshot (self.conn, self.vsss[0], in_params)
        if snapshot_ref != None:
            print 'Snapshot created unexpectedly'
            rc = 0
        print 'Try with a NULL affected system'
        in_params = {'AffectedSystem': None, 
                     'SnapshotSettings': self.snapshot_settings}
        snapshot_ref = CreateVMSnapshot (self.conn, self.vsss[0], in_params)
        if snapshot_ref != None:
            print 'Snapshot created unexpectedly'
            rc = 0
        print 'Try with no input parameters'
        in_params = {}
        snapshot_ref = CreateVMSnapshot (self.conn, self.vsss[0], in_params)
        if snapshot_ref != None:
            print 'Snapshot created unexpectedly'
            rc = 0

        self.TestEnd(rc)

    def destroy_snapshot_error_tests (self):
        self.TestBegin()
        result = 1
        print 'Try with bad snapshot reference'
        bad_snapshot_ref = self.snapshot_ref.copy()
        bad_snapshot_ref['InstanceID'] = 'bad-reference'
        in_params = {'AffectedSnapshot': bad_snapshot_ref}
        rc = DestroyVMSnapshot(self.conn, self.vsss[0], in_params)
        if rc == 1:
            print 'Destroy succeeded when it should have failed'
            result = 0
        print 'Try with NULL snapshot reference'
        in_params = {'AffectedSnapshot': None}
        rc = DestroyVMSnapshot(self.conn, self.vsss[0], in_params)
        if rc == 1:
            print 'Destroy succeeded when it should have failed'
            result = 0
        print 'Try with no input parameters'
        in_params = {}
        rc = DestroyVMSnapshot(self.conn, self.vsss[0], in_params)
        if rc == 1:
            print 'Destroy succeeded when it should have failed'
            result = 0
        self.TestEnd(result)

if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 3):
            print "Wrong arg count: Must pass Ip, username, and password as arguments"
            print "Count is "+str(count)        
            sys.exit(0)
    st = SnapshotTest(sys.argv[1], sys.argv[2], sys.argv[3])
    try:
        # Successful tests
        #st.discover_capabilites()   # Discover the capabilities of the Xen_SnaphotManagementService class - what kind of snapshots are supported
        #st.find_snapshotservice()   # Find a snapshot service
        #snap1 = st.create_snapshot_and_make_vm_changes('NewName', 'New description')    # create a snapshot and then make some changes to the vm
        #snap2 = st.create_snapshot_and_make_vm_changes('NewerName', 'Newer description')# create a 2nd snapshot and then make more changes to the vm
        #st.revert_to_snapshot(snap1)     # revert to the snapshot
        st.export_snapshot_tree()   # export the snapshot tree
        st.import_snapshot_tree()   # import the snapshot tree
        #st.destroy_snapshot(snap2)       # destroy 2nd snapshot
        #st.destroy_snapshot(snap1)       # destroy 1st snapshot

        # Error tests
        #st.create_snapshot_error_tests()    # Pass invalid parameters to the CreateSnapshot method and expect errors
        #st.destroy_snapshot_error_tests()   # Pass invalid parameters to the DestroySnapshot method and expect errors
    finally:
        st.local_cleanup()
        st.TestCleanup()
    
    sys.exit(0)

