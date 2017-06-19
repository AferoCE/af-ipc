#  Copyright (c) 2015 Afero, Inc. All rights reserved.

include $(TOPDIR)/rules.mk

PKG_NAME:=af-ipc
PKG_VERSION:=0.1
PKG_RELEASE:=1

USE_SOURCE_DIR:=$(CURDIR)/pkg

PKG_BUILD_PARALLEL:=1
PKG_FIXUP:=libtool autoreconf
PKG_INSTALL:=1
PKG_USE_MIPS16:=0

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/af-ipc
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Afero IPC Infrastructure Layer
  DEPENDS:=+libevent2 +libpthread +libevent2-pthreads +af-util
  URL:=http://www.kiban.io
endef

define Package/af-ipc/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib
#	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/libaf_ipc.a $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/libaf_ipc.so* $(1)/usr/lib/
#	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/test_server $(1)/usr/bin/
#	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/test_client $(1)/usr/bin/
	$(INSTALL_BIN) $(STAGING_DIR)/usr/lib/libevent-2.0.so.5 $(1)/usr/lib
	$(INSTALL_BIN) $(STAGING_DIR)/usr/lib/libevent_pthreads-2.0.so.5 $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/ipc_send $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/ipc_wait $(1)/usr/bin/

	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/include/af_ipc_*.h $(STAGING_DIR)/usr/include
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/include/af_rpc.h $(STAGING_DIR)/usr/include
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/libaf_ipc.so* $(STAGING_DIR)/usr/lib
endef

define Build/Clean
	$(RM) -rf $(CURDIR)/pkg/src/.deps/*
	$(RM) -rf $(CURDIR)/pkg/src/*.o $(CURDIR)/pkg/src/*.lo
	$(RM) -rf $(CURDIR)/pkg/src/linux/*.o $(CURDIR)/pkg/src/.libs/* 
	$(RM) -rf $(CURDIR)/pkg/src/profile/bento/*.o
	$(RM) -rf $(CURDIR)/pkg/autom4te.cache/*
	$(RM) -rf $(CURDIR)/pkg/ipkg-install/*
	$(RM) -rf $(CURDIR)/pkg/ipkg-ar71xx/$(PKG_NAME)/*
	$(RM) -rf $(CURDIR)/pkg/libtool $(CURDIR)/pkg/config.*
	$(RM) -rf $(CURDIR)/pkg/.quilt_checked  $(CURDIR)/pkg/.prepared $(CURDIR)/pkg/.configured_ $(CURDIR)/pkg/.built
	$(RM) -rf $(CURDIR)/pkg/COPYING $(CURDIR)/pkg/NEWS
	$(RM) -rf $(CURDIR)/pkg/src/Makefile $(CURDIR)/pkg/src/Makefile.in $(CURDIR)/pkg/Makefile $(CURDIR)/pkg/Makefile.in
	$(RM) -rf $(CURDIR)/pkg/aclocal.m4 $(CURDIR)/pkg/ChangeLog  $(CURDIR)/pkg/ABOUT-NLS $(CURDIR)/pkg/AUTHORS $(CURDIR)/pkg/configure
	$(RM) -rf $(CURDIR)/pkg/.source_dir $(CURDIR)/pkg/stamp-h1 $(CURDIR)/pkg/src/libaf_ipc.la
	$(RM) -rf $(CURDIR)/pkg/src/test_client $(CURDIR)/pkg/src/test_server $(CURDIR)/pkg/src/ipc_send $(CURDIR)/pkg/src/test_rpc

	$(RM) -rf $(STAGING_DIR)/pkginfo/libaf_ipc.* 
	$(RM) -rf $(STAGING_DIR)/usr/lib/libaf_ipc.so*
	$(RM) -rf $(STAGING_DIR)/usr/include/af_ipc_*.h
	$(RM) -rf $(STAGING_DIR)/usr/include/af_rpc.h
	$(RM) -rf $(1)/usr/lib/libaf_ipc* $(1)/usr/lib/libafsec.a $(1)/stamp/.libaf_ipc_installed
endef




$(eval $(call BuildPackage,af-ipc))





