#
# Copyright (C) 2017 - 2019 musashino205
#
# This is free software, licensed under the GNU General Public License v2.
#

PLUGIN_VER:=0.56.0
PLUGIN_TAR:=v$(PLUGIN_VER).tar.gz

define Package/mackerel-plugin-conntrack
	$(call Package/mackerel-agent/Default)
	TITLE:=conntrack plugin for mackerel-agent
	DEPENDS:=mackerel-agent
endef

define Package/mackerel-plugin-conntrack/description
	nf_conntrack custom metrics
endef

define Package/mackerel-plugin-linux
	$(call Package/mackerel-agent/Default)
	TITLE:=linux plugin for mackerel-agent
	DEPENDS:=mackerel-agent +ss +coreutils +coreutils-who
endef

define Package/mackerel-plugin-linux
	Get linux process metrics
endef

define Package/mackerel-plugin-multicore
	$(call Package/mackerel-agent/Default)
	TITLE:=multicore plugin for mackerel-agent
	DEPENDS:=mackerel-agent
endef

define Package/mackerel-plugin-multicore/description
	Get multicore CPU metrics
endef

define Package/mackerel-plugin-uptime
	$(call Package/mackerel-agent/Default)
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

define Download/mackerel-plugin-conntrack
	$(call Download/mackerel-official-plugins)
endef
$(eval $(call Download,mackerel-plugin-conntrack))

define Download/mackerel-plugin-linux
	$(call Download/mackerel-official-plugins)
endef
$(eval $(call Download,mackerel-plugin-linux))

define Download/mackerel-plugin-multicore
	$(call Download/mackerel-official-plugins)
endef
$(eval $(call Download,mackerel-plugin-multicore))

define Download/mackerel-plugin-uptime
	$(call Download/mackerel-official-plugins)
endef
$(eval $(call Download,mackerel-plugin-uptime))

# plugin installations
define Package/mackerel-plugin-conntrack/install
	$(call mackerel-plugins/install,conntrack)
endef

define Package/mackerel-plugin-linux/install
	$(call mackerel-plugins/install,linux)
endef

define Package/mackerel-plugin-multicore/install
	$(call mackerel-plugins/install,linux)
endef

define Package/mackerel-plugin-uptime/install
	$(call mackerel-plugins/install,uptime)
endef

# Plugin packages
$(eval $(call BuildPackage,mackerel-plugin-conntrack))
$(eval $(call BuildPackage,mackerel-plugin-linux))
$(eval $(call BuildPackage,mackerel-plugin-multicore))
$(eval $(call BuildPackage,mackerel-plugin-uptime))
