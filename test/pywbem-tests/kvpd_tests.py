#!/usr/bin/env python

import pywbem
from xen_cim_operations import *
from TestSetUp import *
import XenAPI
import random
import string
import time

MAX_KEY = 256
MAX_VALUE = 40000

def copy_from(rec, new_rec, fields):
    """Utility function to copy between records
    particular fields"""

    for field in fields:
        if field not in rec.keys():
            raise Exception("Error: field '%s' does not exist in record '%s'" % (field, rec))
        new_rec[field] = rec[field]

class Test(TestSetUp):
    classname = "Xen_KVP"
    key_set = ['Key','Value']
    class_key = "DeviceID"
    test_vm = None

    def __init__(self, Ip, userName, password, vm_uuid):
        TestSetUp.__init__(self, Ip, userName, password, False, False)
        
        self.session = XenAPI.Session("http://%s" % Ip)
        self.session.xenapi.login_with_password(userName, password)
        self.vm_uuid = vm_uuid

        self.vsms = self.conn.EnumerateInstanceNames('Xen_VirtualSystemManagementService')

        vssd_refs = self.conn.EnumerateInstanceNames("Xen_ComputerSystem")
        for ref in vssd_refs:
            if ref['Name'] == vm_uuid:
                self.test_vm = ref
                
        if not self.test_vm:
            raise Exception("Error: cannot find VM with UUID %s" % vm_uuid)

        vm_ref = self.session.xenapi.VM.get_by_uuid(vm_uuid)
        if not 'kvp_enabled' in self.session.xenapi.VM.get_other_config(vm_ref).keys():
            # Initiate CIM call that setups up the specified VM for using
            # the KVP communication channel.
            # This is a requirement for any other KVP operations.
            print "Setup KVP Communication"
            SetupKVPCommunication(self.conn, self.test_vm)
            print "Init complete"

    def reboot_vm(self, vm_uuid=None):
        """Reboot the specified VM and wait for it to boot back again"""
        if not vm_uuid:
            vm_uuid = self.vm_uuid

        print "Rebooting VM: %s" % vm_uuid
        vm_ref = self.session.xenapi.VM.get_by_uuid(vm_uuid)
        self.session.xenapi.VM.clean_reboot(vm_ref)
        time.sleep(60)
    
    def _start(self, label):
        print "============= Starting %s... ==============" % label
        
    def create(self, key, value):
        print "create operation for Xen_KVP"
        
        # Create CIM Xen_KVP instance
        kvp_rasd = CIMInstance(self.classname)
        # 40000 is vendor reserved, and used to identify
        # the Xen_KVP resource.
        kvp_rasd['ResourceType'] = pywbem.Uint16(40000)
        kvp_rasd['Key'] = key
        kvp_rasd['Value'] = value

        in_params = {'ResourceSetting': kvp_rasd, 
                     'AffectedSystem': self.test_vm}

        rc = AddVMResource(self.conn, self.vsms[0], in_params)
        if rc == 0:
            raise Exception("Error: exception raised. Fail test")
        return kvp_rasd

    def enumerate_instances(self):
        """Enumerate all the Xen_KVP objects"""

        instances = self.conn.EnumerateInstances(self.classname)
        return instances

    def enumerate_instance_refs(self):
        """Enumerate all the Xen_KVP references"""
        names = self.conn.EnumerateInstanceNames(self.classname)
        return names        

    def get_instance_ref(self, instance):
        refs = self.enumerate_instance_refs()
        for ref in refs:
            if ref[self.class_key] == instance[self.class_key]:
                return ref

        return None

    def get_instance(self, instance_ref):
        """Return a CIM instance object, given it's reference"""
        return self.conn.GetInstance(instance_ref)

    def delete(self, ref):
        new_ref = CIMInstance("Xen_KVP")
        copy_from(ref, new_ref, ['DeviceID', 'Key'])
        #new_ref['ResourceType'] = pywbem.Uint16(40000)
           
        print "Ref keys: %s" % ref.keys()
        rasds_to_delete = [new_ref]
        in_params = {'ResourceSettings': rasds_to_delete}
        
        print in_params
        rc = DeleteVMResources(self.conn, self.vsms[0], in_params)
        if rc == 0:
            raise Exception("Error: failed to return OK from DeleteVMResources call")
        return rc

    def equiv_rasd(self, reca, recb):
        for key in self.key_set:
            #print "Testing for Key: '%s'" % key
            if reca[key] != recb[key]:
                #print "Record key '%s' doesn't match (%s != %s)" % (key, reca[key], recb[key])
                return False
        return True


    def _match_records(self, reca, recb):
        """Helper function for working out if two records match"""
        
        if reca.keys().sort() != recb.keys().sort():
            # Check the key list is identical, else return false
            return False
        
        for k,v in reca.iteritems():

            if k not in recb.keys():
                # If a key doesn't even exist, return false
                return False

            if v != recb[k]:
                # If a value doesn't match, then the records are not eqivalent.
                return False

        # If we have gotten this far, then the records match, we have compared
        # each of the key's, and checked their value are equal. We have also
        # ensured the key list is the same length (hence no subset matching).
        return True


    def assert_can_find(self, record, rec_list):
        res = []
        for rec in rec_list:
            if self._match_records(rec, record):
                res.append(rec)

        if not res:
            raise Exception("Error: could not find item '%s' in amongst list '%s'" \
                                % (record, rec_list))
        if len(res) > 1:
            for item in res:
                print item['DeviceID']

            raise Exception("Error: could find %d matching objects for '%s' in list '%s'" \
                                % (len(res), record, rec_list))

        return res[0]

            
    
    def assert_record_lists_match(self, lista, listb):
        """Compare two CIM instance lists and ensure they match."""

        if len(lista) != len(listb):
            raise Exception("Error: lists have a different number of objects. (A:%d, B:%d)" \
                                % (len(lista), len(listb)))

        for item in lista:
            self.assert_can_find(item, listb)
            print "Found item %s" % item

            

    def find_kvp_instance(self, kvp_rasd, instances=None):
        """Search for a given instance on a server"""

        if not instances:
            instances = self.enumerate_instances()

        print "Returned Instances: %s" % instances

        matches = []
        for instance in instances:
            #print "%s = %s" % (instance['Key'], instance['Value'])
            if self.equiv_rasd(instance, kvp_rasd):
                print "Found KVP: %s" % instance[self.class_key]
                matches.append(instance)

        if not matches:
            return None

        if len(matches) > 1:
            raise Exception("Error: found more than one matching object! %s" % matches)
        
        return matches[0]

    def find_kvp_instance_ref(self, device_id):
        instance_refs = self.enumerate_instance_refs()

        print "Returned Instance Refs: %s" % instance_refs
        for instance in instance_refs:
            if instance['DeviceID'] == device_id:
                print "Found match!"
                return instance
         
        raise Exception("Error: could not find a instance reference for DeviceID '%s'" % device_id)

    def clear_all_keys(self):
        instances = self.enumerate_instances()

        for instance in instances:
            self.delete(instance)

        assert not self.enumerate_instances()
            
    def _test_create_list_delete(self, key, value):
        """Test case for creating a pair, listing it, and then deleting it"""
        
        original_instances = self.enumerate_instances()

        print "Create KVP key (%s, %s)" % (key, value)
        kvp_rasd = self.create(key, value)

        print "Key created, finding instance..."
        instance = self.find_kvp_instance(kvp_rasd)

        if instance['Key'] != key:
            raise Exception("Error: Key '%s' != '%s'" % (instance['Key'], key))
        if instance['Value'] != value:
            raise Exception("Error: Value '%s' != '%s'" % (instance['Value'], value))
        
        if not instance:
            raise Exception("Error: KVP rasd was not created properly. Cannot retrieve it")

        ref = self.get_instance_ref(instance)
        print "Key retrieved. Attempting delete..."
        # Try to delete the Instance
        self.delete(instance)

        new_instances = self.enumerate_instances()
            
        self.assert_record_lists_match(original_instances, new_instances)
        
        print "Key deleted!"

    def _test_get_from_id(self, key, value):
        
        print "Create KVP key (%s, %s)" % (key, value)
        kvp_rasd = self.create(key, value)

        print "Key created, finding instance..."
        instance = self.find_kvp_instance(kvp_rasd)


        #inst_ref = CIMInstanceName(classname=self.classname, keybindings={"DeviceID":instance['DeviceID']})

        inst_ref = self.find_kvp_instance_ref(instance['DeviceID'])

        print "Retrieving KVP by ID %s" % inst_ref['DeviceID']
        new_instance = self.get_instance(inst_ref)

        if not self.equiv_rasd(instance, new_instance):
            raise Exception("Error: RASDs do not match! Failing to get from ID")
        

    def _random_string_generator(self, length, chars = string.ascii_lowercase + string.ascii_uppercase + string.digits):
        """Create a random string of a given length"""
        return ''.join(random.choice(chars) for x in range(length))

    def test_max_key(self):
        """ Test that the system copes with maximum 256 character key"""
        self._start("test_max_key")
        key = self._random_string_generator(MAX_KEY)
        value = "testvalueofsmalllength"
        self._test_create_list_delete(key, value)

    def test_max_value(self):
        """ Test that the system copes with the maximum 40000 characters"""
        self._start("test_max_value")
        self._test_create_list_delete("testkeyofsmalllength", 
                                      self._random_string_generator(MAX_VALUE))

    def test_beyond_max_key(self):
        """Test case that ensures an error is returned when attempting to 
        create a key that is too long."""
        self._start("test_beyond_max_key")
        try:
            self._test_create_list_delete(self._random_string_generator(MAX_VALUE + 1),
                                          self._random_string_generator(50))
            raise Exception("Excepted failure did not occur!")
        except Exception, e:
            print "Caught excpetion '%s'. Test Pass" % (str(e))

    def test_beyond_max_value(self):
        """Test case that ensures an error is returned when attempting to 
        create a value that is too long."""
        self._start("test_beyond_max_value")
        try:
            self._test_create_list_delete(self._random_string_generator(50),
                                          self._random_string_generator(MAX_VALUE + 1))
            raise Exception("Excepted failure did not occur!")
        except Exception, e:
            print "Caught excpetion '%s'. Test Pass" % (str(e))

    def test_zero_value(self):
        """Test case that ensures an error is returned when attempting to 
        create a value that is too short."""
        self._start("test_zero_value")
        try:
            self._test_create_list_delete(self._random_string_generator(50),
                                          "")
            raise Exception("Excepted failure did not occur!")
        except Exception, e:
            print "Caught excpetion '%s'. Test Pass" % (str(e))

    def test_zero_key(self):
        """Test case that ensures an error is returned when attempting to 
        create a value that is too short."""
        self._start("test_zero_value")
        try:
            self._test_create_list_delete("", self._random_string_generator(50))
            raise Exception("Excepted failure did not occur!")
        except Exception, e:
            print "Caught excpetion '%s'. Test Pass" % (str(e))


    def test_max_key_and_value(self):
        """ Test that the system copes with both the maximum key and value"""
        self._start("test_max_key_and_value")
        self._test_create_list_delete(self._random_string_generator(MAX_KEY),
                                      self._random_string_generator(MAX_VALUE))

    def test_get_from_id(self):
        self._start("test_get_from_id")
        self._test_get_from_id(self._random_string_generator(30),
                               self._random_string_generator(2048))

    def _test_multiple_key_create_delete(self, key_func, value_func, number):
        self.clear_all_keys()

        rasds = []

        start = time.time()
        for i in range (number):
            key = key_func()
            value = value_func()
            print "%d: Create KVP %s: %s" % (i, key, value)
            inst = self.create(key, value)
            if inst['Key'] != key:
                raise Exception("Error: Key '%s' != '%s'" % (inst['Key'], key))
            if inst['Value'] != value:
                raise Exception("Error: Value '%s' != '%s'" % (inst['Value'], value))
            rasds.append(inst)

        time_to_create = time.time() - start

        print "%d Keys Created in %d seconds (avg ~%ds kvp)" % (number, time_to_create, time_to_create / number)
        
        new_instances = self.enumerate_instances()

        for rasd in rasds:
            if not self.find_kvp_instance(rasd, new_instances):
                raise Exception("Error: cannot find instance for rasd %s" % rasd)
            
        self.clear_all_keys()

    def test_50_max_keys(self):
        self._start("test_50_max_keys")
        self._test_multiple_key_create_delete(lambda: self._random_string_generator(MAX_KEY),
                                              lambda: self._random_string_generator(MAX_VALUE),
                                              50)

    def test_keys_over_reboot(self, num_keys=10):
        original_instances = self.enumerate_instances()

        # Create a bulk of keys
        for i in range(num_keys):
            self.create(self._random_string_generator(MAX_KEY),
                        self._random_string_generator(MAX_VALUE))

        pre_reboot_instances = self.enumerate_instances()
        
        if len(pre_reboot_instances) != len(original_instances) + num_keys:
            raise Exception("Error: of the %d keys, only %d were created." % \
                                (num_keys, len(pre_reboot_instances) - len(original_instances)))
        
        # Reboot the VM, and wait for it to boot back up again.
        self.reboot_vm()

        new_instances = self.enumerate_instances()
        print "Got instances: %s" % new_instances

        # Execute a few re-try's incase VM boot time vary's
        count = 0
        while len(new_instances) == 0 and count < 4:
            print "Got 0 results, perhaps VM hasn't properly booted."
            count = count + 1
            print "Sleeping 20..."
            time.sleep(20)
            new_instances = self.enumerate_instances()

        if count == 4:
            raise Exception("Error: timed-out waiting for VM to boot - no results returned")

        self.assert_record_lists_match(pre_reboot_instances, new_instances)

        print "Record lists do match"

        for instance in new_instances:
            print "Delete %s" % instance[self.class_key]
            self.delete(instance)

        print "Test complete"
        
        
if __name__ == '__main__':
    count = len(sys.argv[1:])
    if (count != 4):
            print "Wrong arg count: Must pass Ip, username, password and vm_uuid as arguments"
            print "Count is "+str(count)        
            sys.exit(0)
    tst  = Test(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
    
    start_time = time.time()
    try:
        tst.test_get_from_id()
        tst.test_max_key()
        tst.test_max_value()
        tst.test_50_max_keys()
        tst.test_max_key_and_value()
        tst.test_zero_key()
        tst.test_zero_value()
        tst.test_beyond_max_key()
        tst.test_beyond_max_value()
        tst.test_keys_over_reboot()
    except Exception, e:
        print "Error: %s" % str(e)
        print "Not all the test cases passed..."

    end_time = time.time()

    duration = end_time - start_time
    mins = duration / 60
    secs = duration % 60

    print "Total time to run tests: %dm %ds" % (mins, secs)
