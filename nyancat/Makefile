#
# Copyright (C) 2017 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=nyancat
PKG_VERSION:=1.5.2+
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/klange/nyancat.git
PKG_SOURCE_VERSION:=5ffb6c5c03d0e9156db8f360599d4f0449bb16b9

PKG_LICENSE:=NCSA
PKG_LICENSE_FILES:=

include $(INCLUDE_DIR)/package.mk

define Package/nyancat
  SECTION:=utils
  CATEGORY:=Utilities
  URL:=https://github.com/klange/nyancat
  TITLE:=Nyancat rendered in your terminal.
  MAINTAINER:=musashino205
endef

define Package/nyancat/description
  Nyancat in your terminal, rendered through ANSI escape sequences.
  This is the source for the Debian package `nyancat`.
  http://nyancat.dakko.us/
endef

define Package/nyancat/install
	# Install binary file
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/nyancat $(1)/usr/bin/
endef

$(eval $(call BuildPackage,nyancat))
