#
# Copyright (C) 2017 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mackerel-agent
PKG_VERSION:=0.44.2
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/taiha/mackerel-agent.git
PKG_SOURCE_VERSION:=520a1ba9e76750bf64485e6f0c8075c317d4ce32

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/mackerel-agent
  SECTION:=admin
  CATEGORY:=Administration
  TITLE:=The server monitoring platform
  URL:=https://mackerel.io/
  MAINTAINER:=musashino205
endef

define Package/mackerel-agent/description
  "The server monitoring platform we always wanted."
  Precisely tuned graphs, role-based architecture, fully customizable notifications,
  congruency with autoscale, and other innovative features.
endef

# mackerel-agent configuration files
define Package/mackerel-agent/conffiles
/etc/config/$(PKG_NAME).conf
#/etc/mackerel-agent/id
endef

# golang cross-compile architecture (GOARCH)
PKG_GOARCH:=
# golang soft-float (GOARM/GOMIPS)
GO_SF:=
# set GOARCH and GOARM/GOMIPS by targets
ifdef CONFIG_TARGET_ar71xx		# Qualcomm Atheros MIPS
	PKG_GOARCH:=mips
	GO_SF:=GOMIPS=softfloat
endif
ifdef CONFIG_TARGET_bcm53xx		# Broadcom ARM
	PKG_GOARCH:=arm
	GO_SF:=GOARM=5
endif
ifdef CONFIG_TARGET_brcm2708		# Broadcom ARM (RPi)
	PKG_GOARCH:=arm
endif
ifdef CONFIG_TARGET_brcm47xx		# Broadcom MIPS
	PKG_GOARCH:=mipsle
	GO_SF:=GOMIPS=softfloat
endif
ifdef CONFIG_TARGET_brcm63xx
	PKG_GOARCH:=mips
	GO_SF:=GOMIPS=softfloat
endif
ifdef CONFIG_TARGET_kirkwood
	PKG_GOARCH:=arm
endif
ifdef CONFIG_TARGET_lantiq
# big/little endian mixed
	PKG_GOARCH:=mipsle
	GO_SF:=GOMIPS=softfloat
endif
ifdef CONFIG_TARGET_mvebu
	PKG_GOARCH:=arm
endif
ifdef CONFIG_TARGET_ramips		# Ralink/MediaTek MIPS
	PKG_GOARCH:=mipsle
	GO_SF:=GOMIPS=softfloat
endif
ifdef CONFIG_TARGET_sunxi
	PKG_GOARCH:=arm
endif

define Build/Configure
	tar xvf $(DL_DIR)/$(PKG_NAME)-$(PKG_VERSION).tar.xz -C $(BUILD_DIR)
	make -C $(PKG_BUILD_DIR) generate
endef

define Build/Compile
	GOOS=linux GOARCH=$(PKG_GOARCH) $(GO_SF) make CC=gcc -C $(PKG_BUILD_DIR) build
endef

define Package/mackerel-agent/install
	# Install binary file
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build/mackerel-agent $(1)/usr/bin/

	# Install conf file
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/mackerel-agent.conf $(1)/etc/config/

	# Install init script
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/mackerel.init $(1)/etc/init.d/mackerel
endef

$(eval $(call BuildPackage,mackerel-agent))
