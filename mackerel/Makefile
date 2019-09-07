#
# Copyright (C) 2017 - 2019 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mackerel-agent
PKG_VERSION:=0.62.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/mackerelio/mackerel-agent.git
PKG_SOURCE_VERSION:=c9db8018714023fef9c8ba7eac20471e8a1b297a

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE

PLUGIN_VER:=0.57.0
PLUGIN_TAR:=v$(PLUGIN_VER).tar.gz

include $(INCLUDE_DIR)/package.mk

#
# Environment - referred to lang/golang in openwrt/packages
#
GO_PKG_WORK_DIR:=$(PKG_BUILD_DIR)/.go_work
GO_PKG_GOPATH_DIR:=$(GO_PKG_WORK_DIR)/gopath
GO_PKG_CACHE_DIR:=$(GO_PKG_WORK_DIR)/cache
GO_PKG_TMP_DIR:=$(GO_PKG_WORK_DIR)/tmp

define GoPackage/BuildEnvironment
	GOPATH=$(GO_PKG_GOPATH_DIR) \
	GOCACHE=$(GO_PKG_CACHE_DIR) \
	GOTMPDIR=$(GO_PKG_TMP_DIR)
endef

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

define Package/mackerel-agent/Default
  SECTION:=admin
  CATEGORY:=Administration
  URL:=https://mackerel.io/
  MAINTAINER:=musashino205
endef

define Package/mackerel-agent
  $(Package/mackerel-agent/Default)
  TITLE:=agent program to post your host's metrics to mackerel.io
  DEPENDS:= +ca-certificates
endef

define Package/mackerel-agent/description
mackerel-agent is a piece of software which is installed
on your hosts to collect metrics and events and send them
to Mackerel where they can be visualized and monitored.
endef

define Package/mackerel-agent/config
	config PACKAGE_MACKEREL_MAX_COMPRESSION
	bool "Enable maximum compression by upx for small flash (<= 8 MiB)"
	default n
	depends on PACKAGE_mackerel-agent
endef

# mackerel-agent configuration files
define Package/mackerel-agent/conffiles
/etc/config/$(PKG_NAME).conf
/etc/mackerel-agent/id
endef

define Build/Prepare
	$(Build/Prepare/Default)
	mkdir -p $(GO_PKG_GOPATH_DIR) \
		$(GO_PKG_CACHE_DIR) \
		$(GO_PKG_TMP_DIR)
	#( cd "$(PKG_BUILD_DIR)" && go get -u all )

	$(TAR) xvf $(DL_DIR)/$(PLUGIN_TAR) -C $(PKG_BUILD_DIR)

	if [ ! -d $(PKG_BUILD_DIR)/plugins ]; then \
		mkdir $(PKG_BUILD_DIR)/plugins ; \
	fi
endef

ifneq ($(CONFIG_PACKAGE_MACKEREL_MAX_COMPRESSION),)
COMPRESSION_PARAM:=--brute
else
COMPRESSION_PARAM:=-7
endif

define Build/Compile
	$(call GoPackage/BuildEnvironment) $(call GoPackage/Environment) \
		go env
	$(call GoPackage/BuildEnvironment) $(call GoPackage/Environment) \
		make CC="$(HOSTCC)" -C $(PKG_BUILD_DIR) build
	upx $(COMPRESSION_PARAM) $(PKG_BUILD_DIR)/build/mackerel-agent
	$(call GoPackage/BuildEnvironment) $(call GoPackage/Environment) \
		go clean -modcache
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

define Package/mackerel-plugins/compile
	( cd $(PKG_BUILD_DIR)/mackerel-agent-plugins-$(PLUGIN_VER)/mackerel-plugin-$(1) \
		&& $(call GoPackage/BuildEnvironment) $(call GoPackage/Environment) \
			go build -o $(PKG_BUILD_DIR)/plugins/mackerel-plugin-$(1) )
	upx --best $(PKG_BUILD_DIR)/plugins/mackerel-plugin-$(1)
	$(call GoPackage/BuildEnvironment) $(call GoPackage/Environment) \
		go clean -modcache
endef

define Package/mackerel-plugins/install
	$(if $(filter $(1),$(SCRIPT_PLUGINS)),,$(call Package/mackerel-plugins/compile,$(1)))
	$(INSTALL_DIR) $(2)/usr/sbin
	$(INSTALL_BIN) \
		$(if $(filter $(1),$(SCRIPT_PLUGINS)),./plugins/mackerel-plugin-$(1).sh,$(PKG_BUILD_DIR)/plugins/mackerel-plugin-$(1)) \
			$(2)/usr/sbin/
endef

# Main package
$(eval $(call BuildPackage,mackerel-agent))

include go_pl.mk
include script_pl.mk
