# SPDX-License-Identifier: GPL-2.0-or-later OR MIT

include $(TOPDIR)/rules.mk

PKG_NAME:=base-files-terastation
PKG_FLAGS:=nonshared
PKG_RELEASE:=$(COMMITCOUNT)

PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/base-files-terastation
  SECTION:=base
  CATEGORY:=Base system
  DEPENDS:=base-files kmod-ts-miconv2
  TITLE:=base-files for Buffalo TeraStation
  URL:=
  VERSION:=0.0.1
endef

define Package/base-files-terastation/description
  This package provides base-files for Buffalo TeraStation series NAS.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/base-files-terastation/install
	install -d -m0775 $(1)/etc/init.d/
	install -m0775 ./files/etc/init.d/done-terastation $(1)/etc/init.d/

	install -d -m0775 $(1)/etc/board.d/
	install -m0664 ./files/etc/board.d/* $(1)/etc/board.d/

	install -d -m0775 $(1)/lib/preinit/
	install -m0664 ./files/lib/preinit/* $(1)/lib/preinit/

	$(INSTALL_DIR) $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/* $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,base-files-terastation))
