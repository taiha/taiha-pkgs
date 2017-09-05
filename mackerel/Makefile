#
# Copyright (C) 2009-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mackerel
PKG_VERSION:=0.44.1
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/mackerelio/mackerel-agent.git
PKG_SOURCE_VERSION:=1e6fd19544f942a8e4cb2d29b665a2d4cc0f58fb
PKG_MIRROR_HASH:=
PKG_MAINTAINER:=musashino205

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/mackerel
  SECTION:=admin
  CATEGORY:=Administration
  TITLE:=mackerel
  URL:=https://mackerel.io/
endef

define Package/mackerel/description
  "The server monitoring platform we always wanted."
  Precisely tuned graphs, role-based architecture, \
  fully customizable notifications, congruency with autoscale, and other innovative features.
endef

define Build/Configure
endef

define Build/Prepare
endef

LDFLAGS:=-s -w
define Build/Compile
	tar xvf $(DL_DIR)/$(PKG_NAME)-$(PKG_VERSION).tar.xz -C $(BUILD_DIR)
	make generate
	$(CP) $GOPATH/src/github.com/shirou/gopsutil/host_linux_arm.go \
		$GOPATH/src/github.com/shirou/gopsutil/host/host_linux_mips.go
	GOOS=linux GOARCH=mips GOMIPS=softfloat make build $(LDFLAGS)
endef

define Package/sl/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sl $(1)/usr/bin/
	$(CP) $(PKG_BUILD_DIR)/mackerel-agent.sample.conf $(1)/etc/config/mackerel-agent.conf
endef

$(eval $(call BuildPackage,mackerel))
