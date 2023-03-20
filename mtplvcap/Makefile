# SPDX-License-Identifier: GPL-2.0-or-later OR MIT
#
# Copyright (C) 2023 musashino205
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mtplvcap
PKG_VERSION:=1.6.1
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/puhitaku/mtplvcap/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=7cfec816480bda3ec65c601167043d9dcf1f07b43dcff57a67b10472ec91e368

PKG_LICENSE:=BSD-3-Clause
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=musashino205

PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0

GO_PKG:=github.com/puhitaku/mtplvcap
GO_PKG_LDFLAGS:=-s -w

include $(INCLUDE_DIR)/package.mk
include ../../packages/lang/golang/golang-package.mk

define Package/mtplvcap
  SECTION:=multimedia
  CATEGORY:=Multimedia
  TITLE:=mtplvcap
  URL:=https://github.com/puhitaku/mtplvcap/
  DEPENDS:=$(GO_ARCH_DEPENDS) +libusb-1.0
endef

define Package/mtplvcap/description
mtplvcap is a multi-platform (Windows/Mac/Linux) software that relays the Live
View of Nikon cameras via WebSocket.

mtplvcap + OBS turn your cameras into web cameras without HDMI capture device.
Enjoy video chatting on Google Hangouts/Meet/Zoom etc. with your favorite cameras!
endef

define Package/mackerel-agent/install
	$(call GoPackage/Package/Install/Bin,$(1))
endef

$(eval $(call GoBinPackage,mtplvcap))
$(eval $(call BuildPackage,mtplvcap))
