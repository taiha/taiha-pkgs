#
# Copyright (C) 2021 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=cmatrix
PKG_VERSION:=2.0+
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/abishekvashok/cmatrix
PKG_SOURCE_VERSION:=eb2fd2a5fed63da49848ff5e9b2d1b1d5e2ecd81

PKG_LICENSE:=GPLv3
PKG_LICENSE_FILES:=

PKG_FIXUP:=autoreconf

include $(INCLUDE_DIR)/package.mk

define Package/cmatrix
  SECTION:=utils
  CATEGORY:=Utilities
  URL:=https://github.com/abishekvashok/cmatrix
  TITLE:=this shows text flying in and out in a terminal
  DEPENDS:=+libncurses
  MAINTAINER:=musashino205
endef

define Package/cmatrix/description
  CMatrix is based on the screensaver from The Matrix website.
  It shows text flying in and out in a terminal like as seen in
  "The Matrix" movie.
  It can scroll lines all at the same rate or asynchronously and
  at a user-defined speed.
endef

CONFIGURE_ARGS += \
	--without-fonts

define Package/cmatrix/install
	# Install binary file
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cmatrix $(1)/usr/bin/
endef

$(eval $(call BuildPackage,cmatrix))
