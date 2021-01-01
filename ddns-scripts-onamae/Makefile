#
# Copyright (C) 2020 musashino_205
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ddns-scripts-onamae
PKG_VERSION:=0.0.1
PKG_RELEASE:=1

PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk


define Package/ddns-scripts-onamae
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=IP Addresses and Names
  PKGARCH:=all
  TITLE:=Extension for onamae.com
  DEPENDS:=ddns-scripts +openssl-util
endef

define Package/ddns-scripts-onamae/description
  Dynamic DNS Client scripts extension for onamae.com
endef


define Build/Configure
endef

define Build/Compile
endef


define Package/ddns-scripts-onamae/install
	$(INSTALL_DIR) $(1)/usr/lib/ddns
	$(INSTALL_BIN) ./files/usr/lib/ddns/update_onamae_com.sh \
		$(1)/usr/lib/ddns

	$(INSTALL_DIR) $(1)/usr/share/ddns/default
	$(INSTALL_DATA) ./files/usr/share/ddns/default/onamae.com.json \
		$(1)/usr/share/ddns/default/
endef

define Package/ddns-scripts-onamae/prerm
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	/etc/init.d/ddns stop
fi
exit 0
endef


$(eval $(call BuildPackage,ddns-scripts-onamae))
