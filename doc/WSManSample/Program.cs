using System;
using System.Xml;
using System.Xml.Serialization;
using System.Text;
using System.IO;
using System.Collections.Specialized;
using System.Collections.Generic;
using WSManAutomation; // add reference to WSMAuto.dll which contains the COM interface to drive the Windows WS-Management interface
                                           // please see http://msdn.microsoft.com/en-us/library/aa384538(VS.85).aspx for more details

namespace WSManSample
{
    public class Program
    {
        #region Serialization Classes
        /////////////////////////////////////////////////////
        // Some helpful base serialization classes to deserialize WS-Management XML protocol responses
        //
        [XmlRoot(ElementName = "SelectorSet", 
            Namespace = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd")]
        public class CTX_SelectorSet_Type
        {
            [XmlElement(ElementName = "Selector", 
                Namespace = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd")]
            public CTX_Selector_Type[] Selector;
        }
        [XmlRoot(ElementName = "Selector", Namespace = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd")]
        public class CTX_Selector_Type
        {
            [XmlAttribute]
            public string Name;
            [XmlText]
            public string Value;
        }
        public class CTX_ReferenceParameters_Type
        {
            [XmlElement(ElementName = "ResourceURI", Namespace = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd")]
            public string ResourceURI;
            [XmlElement(ElementName = "SelectorSet", Namespace = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd")]
            public CTX_SelectorSet_Type SelectorSet;
        }
        public class CTX_Reference_Type
        {
            [XmlElement(ElementName = "Address", Namespace = "http://schemas.xmlsoap.org/ws/2004/08/addressing")]
            public string Address;
            [XmlElement(ElementName = "ReferenceParameters", Namespace = "http://schemas.xmlsoap.org/ws/2004/08/addressing")]
            public CTX_ReferenceParameters_Type ReferenceParameters;
        }
        [XmlRoot(ElementName = "DefinedSystem", 
            Namespace = "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService")]
        public class Xen_VirtualMachine_Reference_Type  : CTX_Reference_Type
        {
        }
        [XmlRoot(ElementName = "Job")]
        public class Xen_Job_Reference_Type : CTX_Reference_Type
        {
        }
        public class Xen_Job_Type
        {
            [XmlElement(ElementName = "JobState")]
            public int JobState;
        }
        [XmlRoot(ElementName = "Xen_SystemStateChangeJob", 
            Namespace = "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_SystemStateChangeJob")]
        public class Xen_SystemStateChangeJob_Type : Xen_Job_Type
        {
        }
        [XmlRoot(ElementName = "Xen_VirtualSystemModifyResourcesJob", 
            Namespace = "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemModifyResourcesJob")]
        public class Xen_VirtualSystemModifyResourcesJob_Type : Xen_Job_Type
        {
        }
        [XmlRoot(ElementName = "DefineSystem_OUTPUT", 
            Namespace = "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService")]
        public class Xen_DefineSystem_OUTPUT_Type
        {
            [XmlElement(ElementName = "ResultingSystem")]
            public Xen_VirtualMachine_Reference_Type  ResultingSystem;
            
            [XmlElement(ElementName = "ReturnValue")]
            public int ReturnValue;
            
            [XmlElement(ElementName = "Job")]
            public Xen_Job_Reference_Type Job;
        }
        [XmlRoot(ElementName = "AddResourceSetting_OUTPUT", 
            Namespace = "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService")]
        public class Xen_AddResourceSetting_OUTPUT_Type
        {
            [XmlElement(ElementName = "ReturnValue")]
            public int ReturnValue;
        }
        [XmlRoot(ElementName = "RequestStateChange_OUTPUT", 
            Namespace = "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem")]
        public class Xen_RequestStateChange_OUTPUT_Type
        {
            [XmlElement(ElementName = "ReturnValue")]
            public int ReturnValue;

            [XmlElement(ElementName = "Job")]
            public Xen_Job_Reference_Type Job;
        }
        #endregion

        /// <summary>
        /// Helper class to initiate WS-Management session with the remote XenServer and invoke methods
        /// </summary>
        public class MyWSManSession
        {
            #region private members
            private IWSManSession m_wsmanSession;
            private WSManClass m_wsman;
            private String m_cim_resourceURIBase;
            private IWSManResourceLocator m_vsmsServiceObj;
            #endregion

            #region CIM Helper methods
            /// <summary>
            /// Sets up a CIM connection with the remote XenServer
            /// </summary>
            public void SetupCIMConnection(string server, string user, string pass)
            {
                m_cim_resourceURIBase = "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2";
                m_wsman = new WSManClass();

                IWSManConnectionOptions cOptions = (IWSManConnectionOptions)m_wsman.CreateConnectionOptions();
                cOptions.UserName = user;
                cOptions.Password = pass;
                int iFlags = m_wsman.SessionFlagUTF8()                  |
                             m_wsman.SessionFlagUseBasic()              |
                             m_wsman.SessionFlagCredUsernamePassword()  |
                             m_wsman.SessionFlagNoEncryption();

                string hostUri = string.Format("http://{0}:5988", server);
                m_wsmanSession = (IWSManSession)m_wsman.CreateSession(hostUri, iFlags, cOptions);
                string identifyResponse = m_wsmanSession.Identify(0);

                if (!identifyResponse.Contains("Citrix"))
                    throw new Exception("Unknown WS-Management Server" + identifyResponse);

            }
            /// <summary>
            /// Enumerates all instances of a XenServer CIM class
            /// </summary>
            public List<string> Query(string XenClass)
            {
                List<string> xml_responses = new List<string>();
                string resourceURI = "http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/";
                IWSManResourceLocator resourceLocator = (IWSManResourceLocator)m_wsman.CreateResourceLocator(string.Format("{0}{1}", resourceURI, XenClass));
                try
                {
                    IWSManEnumerator vm_enum = (IWSManEnumerator)m_wsmanSession.Enumerate(resourceLocator, "", "", 0);
                    while (!vm_enum.AtEndOfStream)
                    {
                        string ss = vm_enum.ReadItem();
                        xml_responses.Add(ss);
                    }
                }
                catch (Exception ex)
                {
                    throw new Exception("Failed: Query()", ex);
                }
                return xml_responses;
            }
            /// <summary>
            /// Enumerates all instances of a CIM class using a CIM query language (WQL supported only)
            /// </summary>
            public List<string> Query(string CIMClass, string filter, string dialect)
            {
                List<string> xml_responses = new List<string>();
                string resource = "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/";
                IWSManResourceLocator resourceLocator = (IWSManResourceLocator)m_wsman.CreateResourceLocator(string.Format("{0}{1}", resource, CIMClass));
                try
                {
                    IWSManEnumerator vm_enum = (IWSManEnumerator)m_wsmanSession.Enumerate(resourceLocator, filter, dialect, 0);
                    while (!vm_enum.AtEndOfStream)
                    {
                        string ss = vm_enum.ReadItem();
                        xml_responses.Add(ss);
                    }
                }
                catch (Exception ex)
                {
                    throw new Exception("Failed: Query()", ex);
                }
                return xml_responses;
            }
            #endregion
            
            #region CIM Method SOAP XML generators
            /// <summary>
            /// Create the input XML for the Xen_VirtualSystemManagementService::DefineSystem method
            /// This allows for the creation of a VM with all the right metadata and also specify resources to be added 
            /// during VM creation.
            /// </summary>
            string GenerateDefineSystemInputXML(string vmName, string description, int procCount, int memMB)
            {
                return "<DefineSystem_INPUT" +
                            " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
                            " xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"" +
                            " xmlns=\"http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService\"" +
                            " xmlns:cssd=\"http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ComputerSystemSettingData\">" +
                            " <SystemSettings>" +
                                "<cssd:Xen_ComputerSystemSettingData xsi:type=\"Xen_ComputerSystemSettingData_Type\">" +
                                    "<cssd:HVM_Boot_Policy>BIOS order</cssd:HVM_Boot_Policy>" +
                                    "<cssd:HVM_Boot_Params>order=dc</cssd:HVM_Boot_Params>" +
                                    "<cssd:Platform>apic=true</cssd:Platform>" +
                                    "<cssd:Platform>acpi=true</cssd:Platform>" +
                                    "<cssd:Platform>pae=true</cssd:Platform>" +
                                    "<cssd:AutomaticShutdownAction>0</cssd:AutomaticShutdownAction>" +
                                    "<cssd:AutomaticStartupAction>1</cssd:AutomaticStartupAction>" +
                                    "<cssd:AutomaticRecoveryAction>2</cssd:AutomaticRecoveryAction>" +
                                    "<cssd:VirtualSystemType>DMTF:xen:HVM</cssd:VirtualSystemType>" +
                                    "<cssd:Description>" + description + "</cssd:Description>" +
                                    "<cssd:ElementName>" + vmName + "</cssd:ElementName>" +
                                "</cssd:Xen_ComputerSystemSettingData>" +
                            " </SystemSettings>" +
                            " <ResourceSettings>" +
                                "<rasd:Xen_ProcessorSettingData " +
                                "xmlns:rasd=\"http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_ProcessorSettingData\" " +
                                 "xsi:type=\"Xen_ProcessorSettingData_Type\">" +
                                    "<rasd:ResourceType>3</rasd:ResourceType>" +
                                    "<rasd:VirtualQuantity>" + procCount + "</rasd:VirtualQuantity>" +
                                    "<rasd:AllocationUnits>count</rasd:AllocationUnits>" +
                                    "<rasd:Limit>100</rasd:Limit>" +
                                    "<rasd:Weight>512</rasd:Weight>" +
                                "</rasd:Xen_ProcessorSettingData>" +
                            " </ResourceSettings>" +
                            " <ResourceSettings>" +
                                "<rasd:Xen_MemorySettingData " +
                                "xmlns:rasd=\"http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_MemorySettingData\" " +
                                "xsi:type=\"Xen_MemorySettingData_Type\">" +
                                    "<rasd:ResourceType>4</rasd:ResourceType>" +
                                    "<rasd:VirtualQuantity>"+ memMB + "</rasd:VirtualQuantity>" +
                                    "<rasd:AllocationUnits>byte*2^20</rasd:AllocationUnits>" +
                                "</rasd:Xen_MemorySettingData>" +
                            " </ResourceSettings>" +
                        "</DefineSystem_INPUT>";
            }
            /// <summary>
            /// Create the input XML for the Xen_VirtualSystemManagementService::DestroySystem method
            /// The method destroys the specified VM
            /// </summary>
            String GenerateDeleteVMInputXML(string vmuuid)
            {
                String output =
                    "<DestroySystem_INPUT " +
                     "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
                     "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" " +
                     "xmlns=\"http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService\">" +
                     "<AffectedSystem>" +
                       "<a:Address xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>" +
                         "<a:ReferenceParameters xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:w=\"http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd\">" +
                           "<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>" +
                           "<w:SelectorSet>" +
                             "<w:Selector Name=\"Name\">" + vmuuid + "</w:Selector>" +
                             "<w:Selector Name=\"CreationClassName\">Xen_ComputerSystem</w:Selector>" +
                           "</w:SelectorSet>" +
                         "</a:ReferenceParameters>" +
                       "</AffectedSystem>" +
                     "</DestroySystem_INPUT>";
                return output;
            }
            /// <summary>
            /// Create the input XML for the Xen_VirtualSystemManagementService::AddResourceSetting method
            /// The method adds an additional resource to the specified VM
            /// </summary>
            String GenerateAddResourceInputXML(String vmuuid, String rasd)
            {
                String output = "<AddResourceSetting_INPUT " +
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
                    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" " +
                    "xmlns=\"http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_VirtualSystemManagementService\">" +
                    "<AffectedSystem>" +
                      "<a:Address xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>" +
                      "<a:ReferenceParameters xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:w=\"http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd\">" +
                        "<w:ResourceURI>http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/Xen_ComputerSystem</w:ResourceURI>" +
                        "<w:SelectorSet>" +
                          "<w:Selector Name=\"Name\">" + vmuuid + "</w:Selector>" +
                          "<w:Selector Name=\"CreationClassName\">Xen_ComputerSystem</w:Selector>" +
                        "</w:SelectorSet>" +
                      "</a:ReferenceParameters>" +
                    "</AffectedSystem>" +
                    "<ResourceSetting>" + rasd + "</ResourceSetting>" +
                    "</AddResourceSetting_INPUT>";
                return output;
            }
            /// <summary>
            /// Specify resources to be added to a VM in XML
            /// </summary>
            StringCollection GetRasdsToBeAdded(string diskPoolId, string netPoolId)
            {
                //disks
                String disk_rasd = "<rasd:Xen_DiskSettingData " +
                                     "xmlns:rasd=\"http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_DiskSettingData\" " +
                                     "xsi:type=\"Xen_DiskSettingData_Type\">" +
                                        "<rasd:ResourceType>19</rasd:ResourceType>" +
                                        "<rasd:ResourceSubType>Disk</rasd:ResourceSubType>" +
                                        "<rasd:ElementName>my vm disk</rasd:ElementName>" +
                                        "<rasd:VirtualQuantity>2048</rasd:VirtualQuantity>" +
                                        "<rasd:AllocationUnits>byte*2^20</rasd:AllocationUnits>" +
                                        "<rasd:AddressOnParent>0</rasd:AddressOnParent>" +
                                        "<rasd:Access>3</rasd:Access>" +
                                        "<rasd:Bootable>false</rasd:Bootable>" +
                                        // if one had a physical disk image file already available (a VDI under XenServer),
                                        // you would reference it here
                                        // This can be done by inserting the reference to the Xen_DiskImage instance here
                                        //"<rasd:HostResource>root/cimv2:Xen_DiskImage.Device=\"device-id-here\"</rasd:HostResource>" +
                                        "<rasd:PoolId>" + diskPoolId + "</rasd:PoolId>" +
                                    "</rasd:Xen_DiskSettingData>";
                //nic
                String nic_rasd = "<rasd:Xen_NetworkPortSettingData " +
                                     "xmlns:rasd=\"http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/Xen_NetworkPortSettingData\" " +
                                     "xsi:type=\"Xen_NetworkPortSettingData_Type\">" +
                                        "<rasd:ResourceType>33</rasd:ResourceType>" +
                                        "<rasd:ElementName>0</rasd:ElementName>" +
                                        "<rasd:PoolId>" + netPoolId + "</rasd:PoolId>" +
                                        // if one wanted to specify a MAC address
                                        // "<rasd:Address>macaddrress</rasd:Address>" +
                                    "</rasd:Xen_NetworkPortSettingData>";
                StringCollection output = new StringCollection();
                output.Add(disk_rasd);
                output.Add(nic_rasd);
                return output;
            }
            /// <summary>
            /// Create the input XML for the Xen_ComputerSystem::RequestStateChange method
            /// This method changes the 'state' of a VM ('start'=2, 'shutdown'=4 and so on)
            /// </summary>
            String GenerateStateChangeInputXML(UInt16 state)
            {
                String output =
                    "<RequestStateChange_INPUT " +
                        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
                        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" " +
                        "xmlns=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ComputerSystem\">" +
                        "<RequestedState>" + state + "</RequestedState>" +
                    "</RequestStateChange_INPUT>";
                return output;

            }
            #endregion // SOAP XML generators

            #region Main Methods

            /// <summary>
            /// Retrieve a Storage Pool to allocate disks out of
            /// </summary>
            /// <returns></returns>
            private string GetStoragePool()
            {
                List<string> response = new List<string>();
                response = Query("Xen_StoragePool");
                foreach (string pool in response)
                {
                    XmlDocument doc = new XmlDocument();
                    doc.LoadXml(pool);
                    XmlNodeList list = doc.GetElementsByTagName("p:Name");
                    for (int i = 0; i < list.Count; i++)
                    {
                        if (list[i].InnerText.Contains("Local storage"))
                        {
                            XmlNodeList poolId = doc.GetElementsByTagName("p:PoolID");
                            return poolId[0].InnerText;
                        }
                    }
                }
                return null;
            }

            /// <summary>
            /// retrieves the network connection pool to allocate network connections out of
            /// </summary>
            /// <returns></returns>
            private string GetNetworkPool()
            {
                List<string> response = new List<string>();
                response = Query("Xen_NetworkConnectionPool");
                foreach (string pool in response)
                {
                    XmlDocument doc = new XmlDocument();
                    doc.LoadXml(pool);
                    XmlNodeList list = doc.GetElementsByTagName("p:Name");
                    for (int i = 0; i < list.Count; i++)
                    {
                        //search for network connection pool connected to eth0
                        if (list[i].InnerText.Contains("eth0"))
                        {
                            XmlNodeList poolId = doc.GetElementsByTagName("p:PoolID");
                            return poolId[0].InnerText;
                        }
                    }
                }
                return null;
            }
            /// <summary>
            /// Waits for async jobs returned by one of the CIM methods to complete
            /// </summary>
            private bool WaitForJobCompletion(Xen_Job_Reference_Type jobRef, string jobClassName, Type jobType)
            {
                bool jobComplete = false;
                string resourceURI = string.Format("{0}/{1}", m_cim_resourceURIBase, jobClassName);//"Xen_SystemStateChangeJob");
                IWSManResourceLocator resLoc = ConvertToResourceLocator(resourceURI, jobRef);
                while (true)
                {
                    string jobinststr = m_wsmanSession.Get(resLoc, 0);
                    using (StringReader stream = new StringReader(jobinststr))
                    {
                        XmlSerializer serializer = new XmlSerializer(jobType);
                        Xen_Job_Type job = (Xen_Job_Type)serializer.Deserialize(stream);
                        if ((job.JobState == 7) || (job.JobState == 10)) // 7 = complete, 10 = exception
                        {
                            if (job.JobState == 7)
                                jobComplete = true;
                            break;
                        }

                    }
                }
                return jobComplete;
            }
            /// <summary>
            /// Get the singleton instance of the Xen_VirtualSystemManagementService class
            /// This instance is a factory object that allows creation and lifecycle management of VMs
            /// </summary>
            private IWSManResourceLocator GetVirtualSystemManagementService()
            {
                if (m_vsmsServiceObj == null)
                {
                    string resourceURI = string.Format("{0}/{1}", m_cim_resourceURIBase, "Xen_VirtualSystemManagementService");
                    m_vsmsServiceObj = (IWSManResourceLocator)m_wsman.CreateResourceLocator(resourceURI);
                    m_vsmsServiceObj.AddSelector("Name", "Xen Hypervisor");
                    m_vsmsServiceObj.AddSelector("SystemCreationClassName", "Xen_HostComputerSystem");
                    m_vsmsServiceObj.AddSelector("SystemName", "myxenservername");
                    m_vsmsServiceObj.AddSelector("CreationClassName", "Xen_VirtualSystemManagementService");
                }
                return m_vsmsServiceObj;
            }
            /// <summary>
            /// Convert a resource URI to a WS-Management ResourceLocator
            /// A ResourceLocator is a reference to a specific CIM instance
            /// </summary>
            private IWSManResourceLocator ConvertToResourceLocator(string resourceURI, CTX_Reference_Type refType)
            {
                IWSManResourceLocator resLoc = (IWSManResourceLocator)m_wsman.CreateResourceLocator(resourceURI);
                foreach (CTX_Selector_Type selector in refType.ReferenceParameters.SelectorSet.Selector)
                {
                    resLoc.AddSelector(selector.Name, selector.Value);
                }
                return resLoc;
            }
            /// <summary>
            /// Get the Virtual Machine ID from a given Virtual Machine CIM reference
            /// </summary>
            private string FindVirtualMachineID(Xen_VirtualMachine_Reference_Type vmRef)
            {
                string vmuuid = null;
                foreach (CTX_Selector_Type selector in vmRef.ReferenceParameters.SelectorSet.Selector)
                {
                    if (selector.Name.Equals("Name"))
                    {
                        vmuuid = selector.Value;
                        break;
                    }
                }
                return vmuuid;
            }

            /// <summary>
            /// Create a Basic HVM (Hardware Virtualized) virtual machine with the 
            /// specified name, description, number of processors and memory.
            /// </summary>
            public Xen_VirtualMachine_Reference_Type  CreateVirtualMachine(
                string vmName, string vmDescription, int numProcs, int memMB
                )
            {
                IWSManResourceLocator vsmsService = GetVirtualSystemManagementService();
                // Create an empty VM with default processor and memory settings and no NIC or virtual disk
                string vmParams = GenerateDefineSystemInputXML(vmName, vmDescription, numProcs, memMB);
                string responseDefineSystem = m_wsmanSession.Invoke("DefineSystem", vsmsService, vmParams, 0);
                Xen_VirtualMachine_Reference_Type definedSystem = null;
                
                // The response contains a reference to the resulting VM. Deserialize it into a class.
                using (StringReader stream = new StringReader(responseDefineSystem))
                {
                    XmlSerializer serializer = new XmlSerializer(typeof(Xen_DefineSystem_OUTPUT_Type));
                    Xen_DefineSystem_OUTPUT_Type responseDS = (Xen_DefineSystem_OUTPUT_Type)serializer.Deserialize(stream);
                    definedSystem = responseDS.ResultingSystem;
                    if (responseDS.ReturnValue == 4096) // a job will be returned
                        WaitForJobCompletion(responseDS.Job, 
                                             "Xen_VirtualSystemModifyResourcesJob", 
                                             typeof(Xen_VirtualSystemModifyResourcesJob_Type));
                }
                return definedSystem;
            }
            /// <summary>
            /// Delete the Virtual Machine specified by the 
            /// Virtual Machine reference (ResourceLocator instance)
            /// </summary>
            public string DeleteVirtualMachine(Xen_VirtualMachine_Reference_Type  affectedVM)
            {
                string responseDestroy = null;
                string vmid = null;
                try
                {
                    vmid = FindVirtualMachineID(affectedVM);
                    IWSManResourceLocator vsmsService = GetVirtualSystemManagementService();
                    if (vmid != null) {
                        string vmParams = GenerateDeleteVMInputXML(vmid);
                        responseDestroy = m_wsmanSession.Invoke("DestroySystem", vsmsService, vmParams, 0);
                    }
                }
                catch (Exception ex)
                {
                    throw ex;
                }
                return responseDestroy;
            }
            /// <summary>
            /// Add a virtual Disk and a virtual NIC to the virtual machine
            /// specified by the virtual machine reference (resource locator)
            /// </summary>
            public void AddDiskAndNicToVirtualMachine(Xen_VirtualMachine_Reference_Type definedSystem)
            {
                IWSManResourceLocator vsmsService = GetVirtualSystemManagementService();
                string responseAddResourceSetting = null;
                string diskPoolId = GetStoragePool();
                string netPoolId = GetNetworkPool();
                StringCollection rasds = GetRasdsToBeAdded(diskPoolId, netPoolId);
                if (rasds != null)
                {
                    string vmuuid = null;
                    // add the RASDs one by one
                    foreach (String rasd in rasds)
                    {
                        vmuuid = FindVirtualMachineID(definedSystem);
                        String vmParams = GenerateAddResourceInputXML(vmuuid, rasd);
                        if (vmParams != null)
                        {
                            responseAddResourceSetting = m_wsmanSession.Invoke("AddResourceSetting", vsmsService, vmParams, 0);
                        }
                    }
                }
                else
                {
                    throw new Exception("No RASDs defined");
                }
            }
            /// <summary>
            /// Change the state of the virtual machine specified by the 
            /// virtual machine reference (ResourceLocator) and the specified state (start/stop/etc.)
            /// </summary>
            public string ChangeVirtualMachineState(Xen_VirtualMachine_Reference_Type  virtualSystem, UInt16 state)
            {
                string resourceURIInUse = null;
                string response = null;
                resourceURIInUse = string.Format("{0}/{1}", m_cim_resourceURIBase, "Xen_ComputerSystem");
                IWSManResourceLocator virtualMachineLoc = 
                    (IWSManResourceLocator)ConvertToResourceLocator(resourceURIInUse, virtualSystem);
                try
                {
                    string vmParams = GenerateStateChangeInputXML(state);
                    response = m_wsmanSession.Invoke("RequestStateChange", virtualMachineLoc, vmParams, 0);
                    // The response contains reference to the resulting VM. Deserialize it into a class.
                    using (StringReader stream = new StringReader(response))
                    {
                        XmlSerializer serializer = new XmlSerializer(typeof(Xen_RequestStateChange_OUTPUT_Type));
                        Xen_RequestStateChange_OUTPUT_Type responseSer = (Xen_RequestStateChange_OUTPUT_Type)serializer.Deserialize(stream);
                        WaitForJobCompletion(responseSer.Job, 
                                             "Xen_SystemStateChangeJob", 
                                             typeof(Xen_SystemStateChangeJob_Type)); // wait until the state change job is complete
                    }
                }
                catch (Exception ex)
                {
                    throw ex;
                }
                return response;
            }
            #endregion
        }
        /// <summary>
        /// Main entry point
        /// </summary>
        static void Main(string[] args)
        {
            MyWSManSession mysession = new MyWSManSession();
            mysession.SetupCIMConnection("192.168.1.100", "root", "mypass");       // setup the CIM connection to the server
            Xen_VirtualMachine_Reference_Type newSystem = 
                mysession.CreateVirtualMachine("WSManTestVM", "My VM Description", 2, 1024);  // create a new VM
            mysession.AddDiskAndNicToVirtualMachine(newSystem);                      //Add a few more resources to the VM
            mysession.ChangeVirtualMachineState(newSystem, 2);                      //start the VM
            mysession.ChangeVirtualMachineState(newSystem, 32768);                  //Force Shutdown of the VM
            mysession.DeleteVirtualMachine(newSystem);                              //Delete the VM
        }
    }
}
