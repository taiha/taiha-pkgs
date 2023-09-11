# SPDX-License-Identifier: GPL-2.0-or-later OR MIT

include $(TOPDIR)/rules.mk

PKG_NAME:=base-files-landisk
PKG_FLAGS:=nonshared
PKG_RELEASE:=$(COMMITCOUNT)

PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/base-files-landisk
  SECTION:=base
  CATEGORY:=Base system
  DEPENDS:=base-files kmod-leds-landisk-r8c
  TITLE:=base-files for I-O DATA LAN DISK
  URL:=
  VERSION:=0.0.1
endef

define Package/base-files-landisk/description
  This package provides base-files for I-O DATA LAN DISK series NAS.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/base-files-landisk/install
	install -d -m0775 $(1)/etc/init.d/
	install -m0775 ./files/etc/init.d/done-landisk $(1)/etc/init.d/

	install -d -m0775 $(1)/lib/preinit/
	install -m0664 ./files/lib/preinit/* $(1)/lib/preinit/
endef

$(eval $(call BuildPackage,base-files-landisk))
