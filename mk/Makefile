USE_BRANDING := yes
IMPORT_BRANDING := yes
include $(B_BASE)/common.mk
include $(B_BASE)/rpmbuild.mk

REPONAME := xs-cim
REPO := $(call git_loc,$(REPONAME))

LIBXENSERVER_BIN := $(PROJECT_OUTPUTDIR)/xen-api-sdk/libxenserver-bin.tar.bz2
LIBXENSERVER_SRC := $(PROJECT_OUTPUTDIR)/xen-api-sdk/libxenserver-src.tar.bz2

LIBXENSERVER_DIR := $(MY_OBJ_DIR)/libxenserver
LIBXENSERVER_H := $(LIBXENSERVER_DIR)/include/xen/api/xen_all.h
LIBXENSERVER_SO := $(LIBXENSERVER_DIR)/libxenserver.so

CMPI_TAR := $(PROJECT_OUTPUTDIR)/openpegasus/cmpi-headers.tar.bz2

PEGASUS_ROOT := $(MY_OBJ_DIR)/pegasus
CMPI_H := $(PEGASUS_ROOT)/src/Pegasus/Provider/CMPI/CmpiInstance.h

WIN_SERVICE_VER := 1.0.42
WIN_SERVICE_DIR := $(CARBON_DISTFILES)/xscim/winkvpservice/$(WIN_SERVICE_VER)
WWW_KVP := winkvp
WWW_ROOT := /opt/xensource/www

WIN_SERVICE_MSI := CitrixXenServerIntegrationService-$(WIN_SERVICE_VER).msi
WIN_SERVICE_MSI_SYM := CitrixXenServerIntegrationService.msi

WIN_SERVICE_MSI_DEBUG := CitrixXenServerIntegrationService-$(WIN_SERVICE_VER)-debug.msi
WIN_SERVICE_MSI_SYM_DEBUG := CitrixXenServerIntegrationService-debug.msi

WIN_SERVICE_EXE := setup-$(WIN_SERVICE_VER).exe
WIN_SERVICE_EXE_SYM := setup.exe

XAPI_PLUGINS := $(REPO)/plugins/xscim
XAPI_PLUGINS_PATH := /etc/xapi.d/plugins

XS_UNINSTALL := $(REPO)/uninstall.sh
XS_CIM_DIR_PATH := /opt/xensource/packages/files/xs-cim

INIT_D_PATH := /etc/init.d

XS_CIM_RPM := $(MY_OUTPUT_DIR)/RPMS/i386/xs-cim-cmpi-$(PRODUCT_VERSION)-$(BUILD_NUMBER).i386.rpm
XS_CIM_RPM_LINK := $(MY_OUTPUT_DIR)/xs-cim-cmpi.i386.rpm

XS_CIM_HOME := /opt/xs-cim-cmpi
XS_CIM_SPEC := $(MY_OBJ_DIR)/xs-cim-cmpi.spec
XS_CIM_RPM_TMP_DIR := $(MY_OBJ_DIR)/RPM_BUILD_DIRECTORY/tmp/xs-cim-cmpi

KVPD_DIR := $(REPO)/kvpservice

# Keep in sync with ../Makefile.am.
XEN_MOF_NAMES = \
	Xen_ComputerSystem.mof \
	Xen_HostComputerSystem.mof \
	Xen_HostPool.mof \
	Xen_Job.mof \
	Xen_VirtualSystemManagementService.mof \
	Xen_VirtualSystemSnapshotService.mof \
	Xen_VirtualSystemMigrationService.mof \
	Xen_StoragePoolManagementService.mof \
	Xen_VirtualSwitch.mof \
	Xen_HostDevices.mof \
	Xen_VirtualDevices.mof \
	Xen_ResourcePool.mof \
	Xen_ComputerSystemIndication.mof \
	Xen_VirtualizationCapabilities.mof \
	Xen_AllocationCapabilities.mof \
	Xen_ComputerSystemSettingData.mof \
	Xen_ResourceAllocationSettingData.mof \
	Xen_ResourceCapabilitiesSettingData.mof \
	Xen_ComputerSystemCapabilities.mof \
	Xen_Metrics.mof \
	Xen_Associations.mof
XEN_MOFS_SPEC := $(addprefix %{_datadir}/%{name}/, $(XEN_MOF_NAMES))
XEN_MOFS_SH := $(addprefix $$sharedir/, $(XEN_MOF_NAMES))

OUTPUT := $(XS_CIM_RPM) \
	  $(XS_CIM_RPM_LINK) \
	  $(MY_SOURCES)/xs-cim-src.tar.bz2 \
	  $(MY_SOURCES)/MANIFEST

.PHONY: build
build: clean $(OUTPUT)
	@:

$(XS_CIM_RPM_LINK): $(XS_CIM_RPM)
	ln -s $(patsubst $(@D)/%,%,$^) $@

$(XS_CIM_RPM): $(XS_CIM_SPEC) xs-cim-cmpi
	mkdir -p $(@D)
	$(RPMBUILD) -bb $<

$(XS_CIM_SPEC): $(REPO)/xs-cim-cmpi.spec.in xs-cim-cmpi
	mkdir -p $(@D)
	sed -e 's/@PRODUCT_VERSION@/$(PRODUCT_VERSION)/g' \
	    -e 's/@BUILD_NUMBER@/$(BUILD_NUMBER)/g' \
	    -e 's,@XEN_MOFS@,$(XEN_MOFS_SPEC),g' \
	    -e 's,@WWW_KVP@,$(WWW_KVP),g' \
	    -e 's,@WIN_SERVICE_EXE@,$(WIN_SERVICE_EXE),g' \
	    -e 's,@WIN_SERVICE_EXE_SYM@,$(WIN_SERVICE_EXE_SYM),g' \
	    -e 's,@WIN_SERVICE_MSI@,$(WIN_SERVICE_MSI),g' \
	    -e 's,@WIN_SERVICE_MSI_SYM@,$(WIN_SERVICE_MSI_SYM),g' \
	    -e 's,@WIN_SERVICE_MSI_DEBUG@,$(WIN_SERVICE_MSI_DEBUG),g' \
	    -e 's,@WIN_SERVICE_MSI_SYM_DEBUG@,$(WIN_SERVICE_MSI_SYM_DEBUG),g' \
	    $< >$@

.PHONY: xs-cim-cmpi
xs-cim-cmpi: $(LIBXENSERVER_SO) $(LIBXENSERVER_H) $(CMPI_H) $(REPO)/Makefile
	$(call mkdir_clean,$(XS_CIM_RPM_TMP_DIR))
	make -C $(REPO)
	make -C $(REPO) DESTDIR=$(XS_CIM_RPM_TMP_DIR) install
	rsync -av --exclude \*~ $(REPO)/overlay/ $(XS_CIM_RPM_TMP_DIR)/
	sed -i -e 's,@XEN_MOFS@,$(XEN_MOFS_SH),g' $(XS_CIM_RPM_TMP_DIR)/$(XS_CIM_HOME)/share/install-xs-cim-cmpi.sh
	rm -f $(XS_CIM_RPM_TMP_DIR)$(XS_CIM_HOME)/lib/cmpi/*.{a,la}
	mv $(XS_CIM_RPM_TMP_DIR)$(XS_CIM_HOME)/etc/* $(XS_CIM_RPM_TMP_DIR)/etc
	mkdir -p $(XS_CIM_RPM_TMP_DIR)$(XS_CIM_HOME)/lib
	cp -a $(LIBXENSERVER_DIR)/libxenserver.so* $(XS_CIM_RPM_TMP_DIR)$(XS_CIM_HOME)/lib
	cp -a $(KVPD_DIR)/*.py $(XS_CIM_RPM_TMP_DIR)$(XS_CIM_HOME)
	mkdir -p $(XS_CIM_RPM_TMP_DIR)$(XAPI_PLUGINS_PATH)
	cp -a $(XAPI_PLUGINS) $(XS_CIM_RPM_TMP_DIR)/$(XAPI_PLUGINS_PATH)/
	mkdir -p $(XS_CIM_RPM_TMP_DIR)$(XS_CIM_DIR_PATH)
	cp -a $(XS_UNINSTALL) $(XS_CIM_RPM_TMP_DIR)$(XS_CIM_DIR_PATH)/
	mkdir -p $(XS_CIM_RPM_TMP_DIR)$(INIT_D_PATH)
	cp $(KVPD_DIR)/kvpd $(XS_CIM_RPM_TMP_DIR)/$(INIT_D_PATH)
	mkdir -p $(XS_CIM_RPM_TMP_DIR)$(WWW_ROOT)/$(WWW_KVP)/
	cp $(WIN_SERVICE_DIR)/Release/$(WIN_SERVICE_MSI_SYM) \
		$(XS_CIM_RPM_TMP_DIR)$(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_MSI)
	cp $(WIN_SERVICE_DIR)/Debug/$(WIN_SERVICE_MSI_SYM) \
		$(XS_CIM_RPM_TMP_DIR)$(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_MSI_DEBUG)
	cp $(WIN_SERVICE_DIR)/Release/$(WIN_SERVICE_EXE_SYM) \
		$(XS_CIM_RPM_TMP_DIR)$(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_EXE)
	ln -s $(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_MSI) \
		$(XS_CIM_RPM_TMP_DIR)/$(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_MSI_SYM)
	ln -s $(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_EXE) \
		$(XS_CIM_RPM_TMP_DIR)/$(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_EXE_SYM)
	ln -s $(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_MSI_DEBUG) \
		$(XS_CIM_RPM_TMP_DIR)/$(WWW_ROOT)/$(WWW_KVP)/$(WIN_SERVICE_MSI_SYM_DEBUG)

$(REPO)/Makefile: $(REPO)/configure
	cd $(REPO) && libtoolize --force
	cd $(REPO) && aclocal
	cd $(REPO) && autoheader
	cd $(REPO) && automake --add-missing
	cd $(REPO) && ./configure --prefix=$(XS_CIM_HOME) \
	                          CIMSERVER=pegasus \
				  PROVIDERDIR=$(XS_CIM_HOME)/lib/cmpi \
	                          PEGASUS_ROOT=$(PEGASUS_ROOT) \
	                          LDFLAGS=-L$(LIBXENSERVER_DIR) \
	                          --with-libxenserver=$(LIBXENSERVER_DIR)

$(LIBXENSERVER_SO): $(LIBXENSERVER_BIN)
	tar -C $(MY_OBJ_DIR) -xjf $^

$(LIBXENSERVER_H): $(LIBXENSERVER_SRC)
	tar -C $(MY_OBJ_DIR) -xjf $^

$(CMPI_H): $(CMPI_TAR)
	$(call mkdir_clean,$(dir $(@D)))
	tar -C $(dir $(@D)) -xjf $^

$(MY_SOURCES)/MANIFEST:
	mkdir -p $(@D)
	echo "xs-cim lgpl local xs-cim-src.tar.bz2" >$@

$(MY_SOURCES)/xs-cim-src.tar.bz2:
	mkdir -p $(@D)
	tar -C $(REPO) -cjf $@ . --exclude=.git \
                                 --exclude=.gitignore \
                                 --exclude-from=$(REPO)/.gitignore

clean:
	( [ -f $(REPO)/Makefile ] && make -C $(REPO) distclean ) || true
	rm -rf $(REPO)/autom4te.cache
	for f in `cat $(REPO)/.hgignore` ; do ( [ -f $(REPO)/$$f ] && rm $(REPO)/$$f ) || true ; done
	rm -rf $(MY_OBJ_DIR)/*
	rm -rf $(OUTPUT)
