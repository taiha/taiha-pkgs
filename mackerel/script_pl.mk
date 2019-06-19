#
# Copyright (C) 2017 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

SCRIPT_PLUGINS:= temp portlink appproto

define Package/mackerel-plugin-temp
$(call Package/mackerel-agent/Default)
  TITLE:=temp plugin for mackerel-agent
  DEPENDS:=mackerel-agent
endef

define Package/mackerel-plugin-temp/description
Get CPU/SoC temperature
endef

define Package/mackerel-plugin-portlink
$(call Package/mackerel-agent/Default)
  TITLE:=portlink plugin for mackerel-agent
  DEPENDS:=mackerel-agent
endef

define Package/mackerel-plugin-portlink/description
Get link speed of ethernet ports on the switch
endef

define Package/mackerel-plugin-appproto
$(call Package/mackerel-agent/Default)
  TITLE:=appproto plugin for mackerel-agent
  DEPENDS:=mackerel-agent +nlbwmon
endef

define Package/mackerel-plugin-appproto/description
Get Bytes/Packets of application protocols
endef

# package installations
define Package/mackerel-plugin-temp/install
$(call mackerel-plugins/install,temp)
endef

define Package/mackerel-plugin-portlink/install
$(call mackerel-plugins/install,portlink)
endef

define Package/mackerel-plugin-appproto/install
$(call mackerel-plugins/install,appproto)
endef

# Plugin packages
$(eval $(call BuildPackage,mackerel-plugin-temp))
$(eval $(call BuildPackage,mackerel-plugin-portlink))
$(eval $(call BuildPackage,mackerel-plugin-appproto))
