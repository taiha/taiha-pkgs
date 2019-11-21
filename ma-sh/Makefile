#
# Copyright (C) 2018 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ma-sh
PKG_VERSION:=0.0.1
PKG_RELEASE:=1

PKG_LICENSE:=
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/ma-sh
  SECTION:=admin
  CATEGORY:=Administration
  TITLE:=a light-weight agent for Mackerel.io
  DEPENDS:= +libubus +libblobmsg-json
  MAINTAINER:=musashino205
endef

define Package/ma-sh/description
  A light-weight agent written in sh/C for Mackerel.io.
endef

define Package/ma-sh/agent_info_h
#ifndef AGENT_INFO_H\n\
#define AGENT_INFO_H\n\
#define AGENT_NAME \"$(PKG_NAME)\"\n\
#define AGENT_VER  \"$(PKG_VERSION)\"\n\
#endif
endef

define Package/ma-sh/agent_info_sh
#!/bin/sh\n\
\n\
# mackerel info/settings\n\
MA_AGENT_NAME=\"$(PKG_NAME)\"\n\
MA_AGENT_VER=\"$(PKG_VERSION)\"\n
endef

define Build/Compile
	echo -e "$(call Package/ma-sh/agent_info_sh)" |\
			cat - ./files/ma-sh.sh > $(PKG_BUILD_DIR)/ma-sh
	echo -e "$(call Package/ma-sh/agent_info_h)" \
			> $(PKG_BUILD_DIR)/agent_info.h
	$(call Build/Compile/Default)
endef

define Package/ma-sh/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/config

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ma-sh $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ma-tools $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ma-sh.init $(1)/etc/init.d/ma-sh
	$(INSTALL_CONF) ./files/ma-sh.conf $(1)/etc/config/ma-sh
endef

$(eval $(call BuildPackage,ma-sh))
