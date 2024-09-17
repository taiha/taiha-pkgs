#
# Copyright (C) 2009-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=fortigate-fwinfo
PKG_VERSION:=0.01
PKG_RELEASE:=1

PKG_MAINTAINER:=musashino205

PKG_LICENSE:=GPL 2.0
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/fortigate-fwinfo
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=firmware-info utility for Fortinet FortiGate/FortiWiFi devices
  URL:=
endef

define Package/fortigate-fwinfo/description
  A utility for updating firmware informations on Fortinet FortiGate/FortiWiFi
  30E/5xE devices.
endef

define Package/fortigate-fwinfo/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/fortigate-fwinfo $(1)/usr/bin/
endef

$(eval $(call BuildPackage,fortigate-fwinfo))
