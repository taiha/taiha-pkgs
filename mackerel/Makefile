#
# Copyright (C) 2017 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mackerel-agent
PKG_VERSION:=0.58.2
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/mackerelio/mackerel-agent.git
PKG_SOURCE_VERSION:=b5bee6b22f30a00d9b66c96dec7c3b9ee3827cd0

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE

SCRIPT_PLUGINS:= temp portlink appproto mastodon

include $(INCLUDE_DIR)/package.mk

define Package/mackerel-agent/Default
  SECTION:=admin
  CATEGORY:=Administration
  URL:=https://mackerel.io/
  MAINTAINER:=musashino205
endef

define Package/mackerel-agent
  $(call Package/mackerel-agent/Default)
  TITLE:=agent program to post your host's metrics to mackerel.io
  DEPENDS:= +ca-certificates
  MENU:=1
endef

define Package/mackerel-agent/description
  mackerel-agent is a piece of software which is installed
  on your hosts to collect metrics and events and send them
  to Mackerel where they can be visualized and monitored.
endef

# mackerel-agent configuration files
define Package/mackerel-agent/conffiles
/etc/config/$(PKG_NAME).conf
/etc/mackerel-agent/id
endef

#
# Environment - referred to lang/golang in openwrt/packages
#
go_arch=$(subst \
  aarch64,arm64,$(subst \
  i386,386,$(subst \
  mipsel,mipsle,$(subst \
  mips64el,mips64le,$(subst \
  powerpc64,ppc64,$(subst \
  x86_64,amd64,$(1)))))))

GO_PKG_GOOS:=linux
GO_PKG_ARCH:=$(call go_arch,$(ARCH))
GO_PKG_386:=387
GO_PKG_ARM:=$(if $(findstring $(GO_PKG_ARCH),arm),$(if $(CONFIG_HAS_FPU),$(if $(CONFIG_arm_v7),7,$(if $(CONFIG_arm_v6),6,5)),5),)
GO_PKG_MIPS:=$(if $(filter $(GO_PKG_ARCH),mips mipsle),$(if $(CONFIG_HAS_FPU),hardfloat,softfloat),)

define GoPackage/Environment
	GOOS=$(GO_PKG_GOOS) \
	GOARCH=$(GO_PKG_ARCH) \
	GO386=$(GO_PKG_386) \
	GOARM=$(GO_PKG_ARM) \
	GOMIPS=$(GO_PKG_MIPS)
endef

define Build/Compile
	# required in cmdutil/cmdutil.go (exitStatus.Signaled)
	go get -u github.com/Songmu/timeout

	$(call GoPackage/Environment) make CC=gcc -C $(PKG_BUILD_DIR) build
	upx --best $(PKG_BUILD_DIR)/build/mackerel-agent
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
    TITLE:=$(4) plugin
    DEPENDS:= mackerel-agent $(5)
  endef

  define Package/mackerel-plugin-$(1)/compile
	if [ ! -d $(PKG_BUILD_DIR)/plugins ]; then \
		mkdir $(PKG_BUILD_DIR)/plugins ; \
	fi
	go get -u github.com/$(2)
	$(call GoPackage/Environment) \
		go build -o $(PKG_BUILD_DIR)/plugins/mackerel-plugin-$(1) \
			$(GOPATH)/src/github.com/$(2)$(3)/main.go
  endef

  define Package/mackerel-plugin-$(1)/install
	$(INSTALL_DIR) $$(1)/usr/sbin
	# $(if [var],[not empty or true],[empty or false])
	$(if $(filter $(1),$(SCRIPT_PLUGINS)),
		$(INSTALL_BIN) ./plugins/mackerel-plugin-$(1).sh \
			$$(1)/usr/sbin/ ,
		$$(call Package/mackerel-plugin-$(1)/compile)
		$(INSTALL_BIN) $(PKG_BUILD_DIR)/plugins/mackerel-plugin-$(1) \
			$$(1)/usr/sbin/
	)
  endef

  $$(eval $$(call BuildPackage,mackerel-plugin-$(1)))
endef

# Main package
$(eval $(call BuildPackage,mackerel-agent))

OFFICIAL_REPO:=mackerelio/mackerel-agent-plugins
PREFIX:=mackerel-plugin
# Plugin packages
#$(eval $(call BuildPlugin,NAME(1),REPO(2),DIR(3),DESCRIPTION(4),DEPENDENCIES(5)))
# Official plugins
$(eval $(call BuildPlugin,conntrack,$(OFFICIAL_REPO),/$(PREFIX)-conntrack,nf_conntrack custom metrics,))
$(eval $(call BuildPlugin,linux,$(OFFICIAL_REPO),/$(PREFIX)-linux,Get linux process metrics,+ss +coreutils +coreutils-who))
$(eval $(call BuildPlugin,multicore,$(OFFICIAL_REPO),/$(PREFIX)-multicore,Get multicore CPU metrics,))
$(eval $(call BuildPlugin,uptime,$(OFFICIAL_REPO),/$(PREFIX)-uptime,uptime custom metrics,))
# Script plugins
$(eval $(call BuildPlugin,temp,,,Get CPU/SoC temperature,))
$(eval $(call BuildPlugin,portlink,,,Get link speed of ethernet ports,))
$(eval $(call BuildPlugin,appproto,,,Get Bytes/Packets of application protocols,+nlbwmon))
$(eval $(call BuildPlugin,mastodon,,,Get statisics of Mastodon instances,+curl))
# Third party plugins
