#
# Copyright (C) 2018 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mkr
PKG_VERSION:=0.32.1
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/mackerelio/mkr.git
PKG_SOURCE_VERSION:=ad13446c7e8e64527afcc05315743f7dc91cd8d1

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/mkr
  SECTION:=admin
  CATEGORY:=Administration
  TITLE:=Command Line Tool for Mackerel
  URL:=https://mackerel.io/
  DEPENDS:= +ca-certificates
  MAINTAINER:=musashino205
endef

define Package/mkr/description
  mkr is a command-line interface tool for the Mackerel
  API written in Go. mkr helps to automate tedious daily
  server operations to best leverage Mackerel's and
  Unix's tools.
  mkr output format is JSON, so it can be filtered with
  a JSON processor such as jq.
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

define Build/Compile
	$(GO_PARAMS) make CC=gcc -C $(PKG_BUILD_DIR) build
	upx $(PKG_BUILD_DIR)/$(PKG_NAME)
endef

define Package/mkr/install
	# Install binary file
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(PKG_NAME) $(1)/usr/bin/
endef

$(eval $(call BuildPackage,mkr))
