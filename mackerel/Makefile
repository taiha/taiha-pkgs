#
# Copyright (C) 2009-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mackerel-agent
PKG_VERSION:=0.44.1
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/taiha/mackerel-agent.git
PKG_SOURCE_VERSION:=3c0cf2f3d2b37bd586f9eaea69435bea64184cc0
PKG_MIRROR_HASH:=
PKG_MAINTAINER:=musashino205

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/mackerel-agent
  SECTION:=admin
  CATEGORY:=Administration
  TITLE:=The server monitoring platform
  URL:=https://mackerel.io/
endef

define Package/mackerel-agent/description
  "The server monitoring platform we always wanted."
  Precisely tuned graphs, role-based architecture, fully customizable notifications,
  congruency with autoscale, and other innovative features.
endef

define Package/mackerel-agent/conffiles
/etc/config/$(PKG_NAME).conf
endef

define Build/Configure
	tar xvf $(DL_DIR)/$(PKG_NAME)-$(PKG_VERSION).tar.xz -C $(BUILD_DIR)
	make -C $(PKG_BUILD_DIR) generate
endef

define Build/Compile
	GOOS=linux GOARCH=mips GOMIPS=softfloat make -C $(PKG_BUILD_DIR) build
endef

define Package/mackerel-agent/install
	# Install binary file
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build/mackerel-agent $(1)/usr/bin/

	# Install conf file
	$(INSTALL_DIR) $(1)/etc/config
	$(CP) $(PKG_BUILD_DIR)/mackerel-agent.sample.conf $(PKG_BUILD_DIR)/mackerel-agent.conf
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/mackerel-agent.conf $(1)/etc/config/
endef

$(eval $(call BuildPackage,mackerel-agent))
