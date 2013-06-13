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
#		Luke Szymanski <Lukasz.Szymanski@unisys.com>
#		Shashi Uli <shashi.uli@citrix.com>
# Description:
#    Automake input file for the CMPI Xen CIM providers.
# =============================================================================

AUTOMAKE_OPTIONS=dist-bzip2

# Start with an empty extra distribution file list
EXTRA_DIST=

# Add the RPM spec file to the distribution file list
EXTRA_DIST+=$(PACKAGE).spec

# Remove hg directories when building a distribution package
dist-hook:
	test -d "$(distdir)" && (find $(distdir) -type d -name .hg | xargs -i rm -rf {})

# -----------------------------------------------------------------------------
# Automake instructions for documentation
# -----------------------------------------------------------------------------

# MAKE CHANGES HERE TO INSTALL THE DOCUMENTATION FILES FOR YOUR PROVIDER
# docdir specifies where the documentation will be installed
docdir=$(datadir)/doc/packages/$(PACKAGE)

# Add standard package doc files
doc_DATA=README AUTHORS COPYING

# ADD ANY EXTRA DOCUMENTATION FILES HERE
#doc_DATA+=README.RPM README.DEBUG
#EXTRA_DIST+=doc README.RPM README.DEBUG


# -----------------------------------------------------------------------------
# Automake instructions for source 
# -----------------------------------------------------------------------------
# MAKE CHANGES HERE TO COMPILE ALL THE SOURCE FILES FOR YOUR PROVIDER

# providerdir specifies where provider libraries will be installed.
# @PROVIDERDIR@ is set by the configure script.
providerdir=@PROVIDERDIR@

SUBDIRS = src

# -----------------------------------------------------------------------------
# Automake instructions for schema
# -----------------------------------------------------------------------------

# LIST ALL THE CLASS DEFINITION FILES (MOFS) HERE
XEN_MOFS = \
	schema/Xen_ComputerSystem.mof \
	schema/Xen_HostComputerSystem.mof \
	schema/Xen_HostPool.mof \
	schema/Xen_Job.mof \
	schema/Xen_VirtualSystemManagementService.mof \
	schema/Xen_VirtualSystemSnapshotService.mof \
	schema/Xen_VirtualSystemMigrationService.mof \
	schema/Xen_StoragePoolManagementService.mof \
        schema/Xen_VirtualSwitch.mof \
	schema/Xen_HostDevices.mof \
	schema/Xen_VirtualDevices.mof \
	schema/Xen_ResourcePool.mof \
	schema/Xen_ComputerSystemIndication.mof \
	schema/Xen_VirtualizationCapabilities.mof \
	schema/Xen_AllocationCapabilities.mof \
	schema/Xen_ComputerSystemSettingData.mof \
        schema/Xen_ResourceAllocationSettingData.mof \
        schema/Xen_ResourceCapabilitiesSettingData.mof \
	schema/Xen_ComputerSystemCapabilities.mof \
	schema/Xen_Metrics.mof \
	schema/Xen_Associations.mof


# LIST ALL THE CLASS CIMOM REGISTRATION FILES HERE
XEN_REGS = \
	schema/Xen_DefaultNamespace.regs


# Some classes must reside in multiple namespaces.  We list these here.
INTEROP_MOFS = \
	schema/Xen_RegisteredProfiles.mof

INTEROP_REGS = \
        schema/Xen_InteropNamespace.regs

# No need to include SVPC MOFs here, 
# They are already in the experimental MOFS directory on the DMTF site.
# CIMOM repository should be build with the latest MOFS.

MOFS = ${XEN_MOFS}
REGS = $(XEN_REGS)

# Un/Register the providers and class definitions from/to the current CIMOM.
# @CIMSERVER@ is set by the configure script
if CIM_IS_OPENPEGASUS
postinstall:
	sh provider-register.sh -t @CIMSERVER@ -n root/cimv2 -m $(MOFS) -r $(XEN_REGS)
	sh provider-register.sh -t @CIMSERVER@ -n root/PG_InterOp -m $(INTEROP_MOFS) -r $(INTEROP_REGS)
else
postinstall:
	sh provider-register.sh -t @CIMSERVER@ -n root/cimv2 -m $(MOFS) -r $(XEN_REGS)
	sh provider-register.sh -t @CIMSERVER@ -n root/interop -m $(INTEROP_MOFS) -r $(INTEROP_REGS)
endif
preuninstall:
	sh provider-register.sh -d -t @CIMSERVER@ -r $(REGS) -m $(MOFS)
	sh provider-register.sh -d -t @CIMSERVER@ -r $(INTEROP_REGS) -m $(INTEROP_MOFS)

# -----------------------------------------------------------------------------
# Files to be installed that the original packages dont install
# Needed for integrating with SFCB/openwsman or openpegasus
# -----------------------------------------------------------------------------
misc_INSTALL_SRC=misc-install
if CIM_IS_OPENPEGASUS
pkgdata_SCRIPTS=\
	$(misc_INSTALL_SRC)/scripts/cimiptables \
	$(misc_INSTALL_SRC)/scripts/cimrestart \
	$(misc_INSTALL_SRC)/scripts/cimstart \
	$(misc_INSTALL_SRC)/scripts/cimstop \
	$(misc_INSTALL_SRC)/scripts/cimver \
	$(misc_INSTALL_SRC)/scripts/xen-cim
pkgdata_PAM=
else
pkgdata_SCRIPTS=\
	$(misc_INSTALL_SRC)/scripts/cimiptables \
	$(misc_INSTALL_SRC)/scripts/cimrestart \
	$(misc_INSTALL_SRC)/scripts/cimstart \
	$(misc_INSTALL_SRC)/scripts/cimstop \
	$(misc_INSTALL_SRC)/scripts/cimver \
	$(misc_INSTALL_SRC)/scripts/xen-cim
pkgdata_PAM=\
	$(misc_INSTALL_SRC)/conf/openwsman
endif
pkgdata_WSMANCONF=$(misc_INSTALL_SRC)/conf/identify.xml
pamdir=$(sysconfdir)/pam.d
initdir=$(sysconfdir)/init.d
openwsmandir=$(sysconfdir)/openwsman
installpamconf:
	test -z "$(pamdir)" || $(mkdir_p) "$(DESTDIR)$(pamdir)"
	@list='$(pkgdata_PAM)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f=$(am__strip_dir) \
	  echo "$(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(pamdir)/$$f'"; \
	  $(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(pamdir)/$$f"; \
	done
uninstallpamconf:
	@$(NORMAL_UNINSTALL)
	@list='$(pkgdata_PAM)'; for p in $$list; do \
	  f=$(am__strip_dir) \
	  echo " rm -f '$(DESTDIR)$(pamdir)/$$f'"; \
	  rm -f "$(DESTDIR)$(pamdir)/$$f"; \
	done
installwsmanconf:
	test -z "$(openwsmandir)" || $(mkdir_p) "$(DESTDIR)$(openwsmandir)"
	@list='$(pkgdata_WSMANCONF)'; for p in $$list; do \
	if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	f=$(am__strip_dir) \
	echo "$(INSTALL_SCRIPT) '$$d$$p' '$(DESTDIR)$(openwsmandir)/$$f'"; \
	$(INSTALL_SCRIPT) "$$d$$p" "$(DESTDIR)$(openwsmandir)/$$f"; \
	done
uninstallwsmanconf:
	@$(NORMAL_UNINSTALL)
	@list='$(pkgdata_WSMANCONF)'; for p in $$list; do \
	f=$(am__strip_dir) \
	echo " rm -f '$(DESTDIR)$(openwsmandir)/$$f'"; \
	rm -f "$(DESTDIR)$(openwsmandir)/$$f"; \
	done
if CIM_IS_OPENPEGASUS
install-data-local: installpamconf
uninstall-local: uninstallpamconf
else
install-data-local: installpamconf installwsmanconf
uninstall-local: uninstallpamconf uninstallwsmanconf
endif
	
runtest:
	cd test && ./setup-test.sh && ./run-test.sh @XMTESTDIR@

# Add the schema files, cimom conf file and scripts to the distribution file list
pkgdata_DATA=$(MOFS) $(REGS) $(INTEROP_MOFS) $(INTEROP_REGS)
EXTRA_DIST+=schema $(pkgdata_WSMANCONF) $(pkgdata_SFCBCONF) $(pkgdata_PAM) $(pkgdata_DATA) $(pkgdata_SCRIPTS)

# -----------------------------------------------------------------------------
# Automake instructions to generate source tarball for cmpi-xen
# -----------------------------------------------------------------------------

#dist:
#	sh make-srcpkg.sh
