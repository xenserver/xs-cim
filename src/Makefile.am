# =============================================================================
# (C) Copyright IBM Corp. 2006
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
# =============================================================================
# Author:       Dr. Gareth S. Bestor <bestor@us.ibm.com>
# Contributors: Tokunbo Adeshiyan <tokunbo@us.ibm.com>
#               Jim Fehlig <jfehlig@novell.com>
#		Rajagopalan Subrahmanian <raj.subrahmanian@unisys.com>
# Description:
#    Automake input file for the CMPI Xen CIM provider.
# =============================================================================

AM_CFLAGS=-O2 -Wall -Werror -Wno-nonnull @LIBXEN_CFLAGS@ @LIBXML2_CFLAGS@

# Start with an empty extra distribution file list
EXTRA_DIST=

# Add lex and yacc files to distribution
EXTRA_DIST+=Xen_SettingDataLexer.l Xen_SettingDataParser.y Xen_SettingDataParser.h 

AM_CPPFLAGS = -I./include -DSBLIM_DEBUG
AM_LDFLAGS = -avoid-version -no-undefined
LIBS = libXen_Support

# -----------------------------------------------------------------------------
# Automake instructions for source 
# -----------------------------------------------------------------------------
# MAKE CHANGES HERE TO COMPILE ALL THE SOURCE FILES FOR YOUR PROVIDER

# providerdir specifies where provider libraries will be installed.
# @PROVIDERDIR@ is set by the configure script.
providerdir=@PROVIDERDIR@

# LIST HEADER FILES USED TO COMPILE THIS PROVIDER BUT NOT INSTALLED HERE
noinst_HEADERS = \
	include/providerinterface.h \
	include/cmpilify.h \
	include/cmpitrace.h \
	include/cmpiutil.h \
	include/dmtf.h \
	include/provider_common.h \
	include/xen_utils.h \
        include/RASDs.h \
	include/Xen_AllocationCapabilities.h \
        include/Xen_Capabilities.h \
	include/Xen_ComputerSystem.h \
	include/Xen_Console.h \
	include/Xen_KVP.h \
	include/Xen_Disk.h \
	include/Xen_HostPool.h \
	include/Xen_Job.h \
	include/Xen_MetricService.h \
	include/Xen_Processor.h \
	include/Xen_StoragePoolManagementService.h \
	include/Xen_VirtualSwitchManagementService.h \
	include/Xen_VirtualSystemManagementService.h \
	include/Xen_VirtualSystemMigrationService.h \
	include/Xen_VirtualSystemSettingData.h \
	include/Xen_VirtualSystemSnapshotService.h

# LIST ANY UTILITY LIBRARIES, THEIR SOURCE FILE(S), AND ANY LIBS REQUIRED FOR LINKING HERE
# libXen_utils contains Xen API and CMPI wrapper functions for the providers
#lib_LTLIBRARIES = libXen_ProviderCommon.la


# LIST EACH PROVIDER LIBRARY, ITS SOURCE FILE(S), AND ANY LIBS REQUIRED 
# FOR LINKING HERE
provider_LTLIBRARIES = \
	libXen_ProviderCommon.la \
	libXen_Support.la \
	libXen_RegisteredProfiles.la \
	libXen_ComputerSystem.la \
	libXen_HostPool.la \
	libXen_Job.la \
	libXen_VirtualSystemManagementService.la \
	libXen_StoragePoolManagementService.la \
	libXen_VirtualSystemSnapshotService.la \
	libXen_VirtualSystemMigrationService.la \
	libXen_VirtualSwitchManagementService.la \
	libXen_Processor.la \
	libXen_DiskImage.la \
	libXen_Disk.la \
	libXen_NetworkPort.la \
	libXen_VirtualSwitch.la \
	libXen_Console.la \
	libXen_KVP.la \
        libXen_ComputerSystemIndication.la \
	libXen_VirtualizationCapabilities.la \
	libXen_ElementSettingData.la \
	libXen_ProcessorCapabilitiesSettingData.la \
	libXen_MemoryCapabilitiesSettingData.la \
	libXen_StorageCapabilitiesSettingData.la \
	libXen_NetworkConnectionCapabilitiesSettingData.la \
	libXen_Services.la \
	libXen_VSMSElementCapabilities.la \
	libXen_associationProviderCommon.la \
	libXen_StoragePool.la \
        libXen_HostComputerSystem.la \
	libXen_MemoryState.la \
        libXen_HostProcessor.la \
	libXen_HostNetworkPort.la \
	libXen_MetricService.la

libXen_Support_la_SOURCES = cmpitrace.c cmpiutil.c xen_utils.c cmpilify.c Xen_SettingDataLexer.c Xen_SettingDataParser.c Xen_Job_Helper.c
libXen_Support_la_CFLAGS = $(AM_CFLAGS) -Wno-unused-function
libXen_Support_la_LIBADD = @LIBXEN_LIBS@ @LIBXML2_LIBS@ -lpthread -luuid 

libXen_ProviderCommon_la_SOURCES = ProxyProvider.c ProxyHelper.c
libXen_ProviderCommon_la_LIBADD = libXen_Support.la libXen_ComputerSystem.la libXen_Processor.la libXen_Disk.la libXen_Console.la libXen_KVP.la libXen_NetworkPort.la libXen_DiskImage.la libXen_MemoryState.la libXen_HostComputerSystem.la  libXen_VirtualSwitch.la libXen_StoragePool.la libXen_HostNetworkPort.la libXen_HostProcessor.la libXen_HostPool.la libXen_Services.la libXen_Job.la libXen_MetricService.la libXen_MemoryCapabilitiesSettingData.la libXen_NetworkConnectionCapabilitiesSettingData.la libXen_ProcessorCapabilitiesSettingData.la libXen_StorageCapabilitiesSettingData.la libXen_VirtualizationCapabilities.la libXen_VirtualSystemManagementService.la libXen_VirtualSystemMigrationService.la libXen_VirtualSystemSnapshotService.la libXen_VirtualSwitchManagementService.la libXen_StoragePoolManagementService.la
libXen_ProviderCommon_la_LDFLAGS = -module  -avoid-version -no-undefined

libXen_Services_la_SOURCES = Xen_Services.c

libXen_VirtualSystemManagementService_la_SOURCES = Xen_VirtualSystemManagementService.c
libXen_VirtualSystemManagementService_la_LIBADD = libXen_Support.la libXen_DiskImage.la libXen_ComputerSystem.la libXen_Disk.la libXen_NetworkPort.la libXen_Console.la libXen_KVP.la

libXen_StoragePoolManagementService_la_SOURCES = Xen_StoragePoolManagementService.c
libXen_StoragePoolManagementService_la_LIBADD = libXen_Support.la libXen_DiskImage.la libXen_Disk.la libXen_StoragePool.la

libXen_VirtualSystemSnapshotService_la_SOURCES = Xen_VirtualSystemSnapshotService.c
libXen_VirtualSystemSnapshotService_la_LIBADD = libXen_Support.la libXen_ComputerSystem.la

libXen_VirtualSystemMigrationService_la_SOURCES = Xen_VirtualSystemMigrationService.c

libXen_VirtualSwitchManagementService_la_SOURCES = Xen_VirtualSwitchManagementService.c
libXen_VirtualSwitchManagementService_la_LIBADD = libXen_Support.la libXen_HostNetworkPort.la libXen_VirtualSwitch.la

libXen_HostPool_la_SOURCES = Xen_HostPool.c

libXen_HostComputerSystem_la_SOURCES = Xen_HostComputerSystem.c

libXen_ComputerSystem_la_SOURCES = Xen_ComputerSystem.c Xen_VirtualSystemSettingData.c Xen_MemorySettingData.c Xen_ProcessorSettingData.c

libXen_VirtualSwitch_la_SOURCES = Xen_VirtualSwitch.c
libXen_VirtualSwitch_la_LIBADD = libXen_Support.la -luuid 

libXen_StoragePool_la_SOURCES = Xen_StoragePool.c

libXen_DiskImage_la_SOURCES = Xen_DiskImage.c

libXen_MemoryState_la_SOURCES = Xen_MemoryState.c

libXen_MetricService_la_SOURCES = Xen_MetricService.c

libXen_HostProcessor_la_SOURCES = Xen_HostProcessor.c

libXen_HostNetworkPort_la_SOURCES = Xen_HostNetworkPort.c

libXen_Processor_la_SOURCES = Xen_Processor.c

libXen_Disk_la_SOURCES = Xen_Disk.c Xen_DiskSettingData.c
libXen_Disk_la_LIBADD = libXen_Support.la libXen_DiskImage.la

libXen_NetworkPort_la_SOURCES = Xen_NetworkPort.c Xen_NetworkPortSettingData.c

libXen_Console_la_SOURCES = Xen_Console.c Xen_ConsoleSettingData.c

libXen_KVP_la_SOURCES = Xen_KVP.c Xen_KVPSettingData.c

libXen_Job_la_SOURCES = Xen_Job.c

libXen_ElementSettingData_la_SOURCES = Xen_ElementSettingData.c

libXen_VirtualizationCapabilities_la_SOURCES = Xen_VirtualizationCapabilities.c

libXen_ProcessorCapabilitiesSettingData_la_SOURCES = Xen_ProcessorCapabilitiesSettingData.c

libXen_MemoryCapabilitiesSettingData_la_SOURCES = Xen_MemoryCapabilitiesSettingData.c

libXen_StorageCapabilitiesSettingData_la_SOURCES = Xen_StorageCapabilitiesSettingData.c

libXen_NetworkConnectionCapabilitiesSettingData_la_SOURCES = Xen_NetworkConnectionCapabilitiesSettingData.c

libXen_ComputerSystemIndication_la_SOURCES = Xen_ComputerSystemIndication.c
libXen_ComputerSystemIndication_la_LDFLAGS = -module -avoid-version -no-undefined

libXen_RegisteredProfiles_la_SOURCES = Xen_RegisteredProfiles.c
libXen_RegisteredProfiles_la_LDFLAGS = -module -avoid-version -no-undefined

libXen_VSMSElementCapabilities_la_SOURCES = Xen_VSMSElementCapabilities.c
libXen_VSMSElementCapabilities_la_LDFLAGS = -module -avoid-version -no-undefined

libXen_associationProviderCommon_la_SOURCES = associationProviderCommon.c
libXen_associationProviderCommon_la_LDFLAGS = -module -avoid-version -no-undefined

BUILT_SOURCES=Xen_SettingDataLexer.c Xen_SettingDataParser.c
AM_YFLAGS=-d
Xen_SettingDataLexer.c: Xen_SettingDataLexer.l Xen_SettingDataParser.y
	$(LEX) -t Xen_SettingDataLexer.l | sed -e "s/yy/Xen_SettingDatayy/g" > Xen_SettingDataLexer.c

Xen_SettingDataParser.c: Xen_SettingDataLexer.l Xen_SettingDataParser.y
	$(YACC) -p Xen_SettingDatayy -d -v -o Xen_SettingDataParser.c Xen_SettingDataParser.y
