#
# Copyright (C) 2017 - 2019 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

define Package/mackerel-plugin-conntrack
$(Package/mackerel-agent/Default)
  TITLE:=conntrack plugin for mackerel-agent
  DEPENDS:=mackerel-agent
endef

define Package/mackerel-plugin-conntrack/description
nf_conntrack custom metrics
endef

define Package/mackerel-plugin-linux
$(Package/mackerel-agent/Default)
  TITLE:=linux plugin for mackerel-agent
  DEPENDS:=mackerel-agent +ss +coreutils +coreutils-who
endef

define Package/mackerel-plugin-linux/description
Get linux process metrics
endef

define Package/mackerel-plugin-multicore
$(Package/mackerel-agent/Default)
  TITLE:=multicore plugin for mackerel-agent
  DEPENDS:=mackerel-agent
endef

define Package/mackerel-plugin-multicore/description
Get multicore CPU metrics
endef

define Package/mackerel-plugin-uptime
$(Package/mackerel-agent/Default)
  TITLE:=uptime plugin for mackerel-agent
  DEPENDS:=mackerel-agent
endef

define Package/mackerle-plugin-uptime/description
uptime custom metrics
endef

define Download/mackerel-official-plugins
  URL:=https://github.com/mackerelio/mackerel-agent-plugins/archive/
  FILE:=$(PLUGIN_TAR)
  HASH:=2deb9892038a2739539ddaf9854bd6016c1c7c65e915b04941be4820a35fd7f8
endef
$(eval $(call Download,mackerel-official-plugins))

# plugin installations
define Package/mackerel-plugin-conntrack/install
$(call Package/mackerel-plugins/install,conntrack,$(1))
endef

define Package/mackerel-plugin-linux/install
$(call Package/mackerel-plugins/install,linux,$(1))
endef

define Package/mackerel-plugin-multicore/install
$(call Package/mackerel-plugins/install,multicore,$(1))
endef

define Package/mackerel-plugin-uptime/install
$(call Package/mackerel-plugins/install,uptime,$(1))
endef

# Plugin packages
$(eval $(call BuildPackage,mackerel-plugin-conntrack))
$(eval $(call BuildPackage,mackerel-plugin-linux))
$(eval $(call BuildPackage,mackerel-plugin-multicore))
$(eval $(call BuildPackage,mackerel-plugin-uptime))
