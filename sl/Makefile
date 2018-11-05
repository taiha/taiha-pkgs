#
# Copyright (C) 2009-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=sl
PKG_VERSION:=5.02
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/mtoyoda/sl.git
PKG_SOURCE_VERSION:=923e7d7ebc5c1f009755bdeb789ac25658ccce03
PKG_MIRROR_HASH:=
PKG_MAINTAINER:=musashino205

PKG_LICENSE:=mtoyoda
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/sl
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=sl
  DEPENDS:=+libncurses
  URL:=https://github.com/mtoyoda/sl.git
endef

define Package/sl/description
  sl command
endef

define Build/Configure
endef

define Build/Prepare
endef

define Build/Compile
	tar xvf $(BUILD_DIR)/../../dl/$(PKG_NAME)-$(PKG_VERSION).tar.xz -C $(BUILD_DIR)
	$(TARGET_CC) \
			-Wall -g \
			-o $(PKG_BUILD_DIR)/sl $(PKG_BUILD_DIR)/sl.c -lncurses
endef

define Package/sl/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sl $(1)/usr/bin/
endef

$(eval $(call BuildPackage,sl))
