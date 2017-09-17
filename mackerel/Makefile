#
# Copyright (C) 2017 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mackerel-agent
PKG_VERSION:=0.44.2
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/taiha/mackerel-agent.git
PKG_SOURCE_VERSION:=641dcec3f41507904fd22620a4244620bc901746

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/mackerel-agent/Default
  SECTION:=admin
  CATEGORY:=Administration
  TITLE:=The server monitoring platform
  URL:=https://mackerel.io/
  MAINTAINER:=musashino205
endef

define Package/mackerel-agent
  $(call Package/mackerel-agent/Default)
  TITLE:=The server monitoring platform
  MENU:=1
endef

define Package/mackerel-agent/description
  "The server monitoring platform we always wanted."
  Precisely tuned graphs, role-based architecture, fully customizable notifications,
  congruency with autoscale, and other innovative features.
endef

# mackerel-agent configuration files
define Package/mackerel-agent/conffiles
/etc/config/$(PKG_NAME).conf
/etc/mackerel-agent/id
endef

# golang cross-compile architecture (GOARCH)
PKG_GOARCH:=
# golang soft-float support (GOARM/GOMIPS)
GO_SF:=
# set GOARCH and GOARM/GOMIPS by $(ARCH)
ifeq ($(ARCH),mips)		# for MIPS big-endianess SoC
	PKG_GOARCH:=mips
	GO_SF:=GOMIPS=softfloat
endif
ifeq ($(ARCH),mipsel)		# for MIPS little-endianess SoC
	PKG_GOARCH:=mipsle
	GO_SF:=GOMIPS=softfloat
endif
ifeq ($(ARCH),mips64)		# for MIPS64 big-endianess SoC
	PKG_GOARCH:=mips64
	GO_SF:=GOMIPS=softfloat
endif
ifeq ($(ARCH),arm)		# for ARM SoC
	PKG_GOARCH:=arm
	GO_SF:=GOARM=5
endif
ifeq ($(ARCH),i386)		# for x86 (i386) SoC/CPU
	PKG_GOARCH:=386
endif
ifeq ($(ARCH),x86_64)		# for x86_64 (amd64) SoC/CPU
	PKG_GOARCH:=amd64
endif
GO_PARAMS:=GOOS=linux GOARCH=$(PKG_GOARCH) $(GO_SF)

define Build/Configure
	tar xvf $(DL_DIR)/$(PKG_NAME)-$(PKG_VERSION).tar.xz -C $(BUILD_DIR)
	make -C $(PKG_BUILD_DIR) generate
endef

define Build/Compile
	$(GO_PARAMS) make CC=gcc -C $(PKG_BUILD_DIR) build
	upx $(PKG_BUILD_DIR)/build/mackerel-agent
endef

define Package/mackerel-agent/install
	# Install binary file
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build/$(PKG_NAME) $(1)/usr/bin/

	# Install conf file
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/$(PKG_NAME).conf $(1)/etc/config/

	# Install id file
	$(INSTALL_DIR) $(1)/etc/$(PKG_NAME)
	$(INSTALL_CONF) ./files/id $(1)/etc/$(PKG_NAME)/

	# Install init script
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/mackerel.init $(1)/etc/init.d/mackerel
endef

# Build/Install plugins
define BuildPlugin

  define Package/mackerel-plugin-$(1)
  $$(call Package/mackerel-agent/Default)
    TITLE:=$(2) plugin
    DEPENDS:= mackerel-agent $(3)
  endef

  define Package/mackerel-plugin-$(1)/compile
	if [ ! -d $(PKG_BUILD_DIR)/plugins ]; then \
		mkdir $(PKG_BUILD_DIR)/plugins ; \
	fi

	go get github.com/mackerelio/mackerel-agent-plugins
	$(GO_PARAMS) \
		go build -o $(PKG_BUILD_DIR)/plugins/mackerel-plugin-$(1) \
			$(GOPATH)/src/github.com/mackerelio/mackerel-agent-plugins/mackerel-plugin-$(1)/main.go
  endef

  define Package/mackerel-plugin-$(1)/install
	$$(call Package/mackerel-plugin-$(1)/compile)
	$(INSTALL_DIR) $$(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/plugins/mackerel-plugin-$(1) \
		$$(1)/usr/sbin/
  endef

  $$(eval $$(call BuildPackage,mackerel-plugin-$(1)))
endef

# Main package
$(eval $(call BuildPackage,mackerel-agent))

# Plugin packages
#$(eval $(call BuildPlugin,NAME,DESCRIPTION,DEPENDENCIES))
$(eval $(call BuildPlugin,conntrack,nf_conntrack custom metrics,))
$(eval $(call BuildPlugin,linux,Get linux process metrics,+ss +coreutils +coreutils-who))
$(eval $(call BuildPlugin,multicore,Get multicore CPU metrics,))
$(eval $(call BuildPlugin,uptime,uptime custom metrics,))
