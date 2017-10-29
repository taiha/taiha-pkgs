#
# Copyright (C) 2017 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=gowsay
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_LICENSE:=GPLv3
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/gowsay
  SECTION:=utils
  CATEGORY:=Utilities
  URL:=https://github.com/syohex/gowsay
  TITLE:=cowsay implementation in Golang
  MAINTAINER:=musashino205
endef

define Package/gowsay/description
  cowsay implementation in Golang.
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

define Build/Prepare
	go get -u github.com/syohex/gowsay
endef

define Build/Configure
endef

define Build/Compile
	$(GO_PARAMS) go build -ldflags="-s -w" \
		-o $(PKG_BUILD_DIR)/gowsay \
		$(GOPATH)/src/github.com/syohex/gowsay/*.go
	upx $(PKG_BUILD_DIR)/gowsay
endef

define Package/gowsay/install
	# Install binary file
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gowsay $(1)/usr/bin/
endef

$(eval $(call BuildPackage,gowsay))
