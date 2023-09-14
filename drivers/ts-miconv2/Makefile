# SPDX-License-Identifier: GPL-2.0-or-later OR MIT

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=ts-miconv2
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/ts-miconv2
  SUBMENU:=Other modules
  TITLE:=MICON v2 MCU driver for Buffalo TeraStation series NAS
  DEPENDS:=+kmod-mfd-core
  FILES:=$(PKG_BUILD_DIR)/ts-miconv2.ko
  AUTOLOAD:=$(call AutoLoad,20,ts-miconv2,1)
endef

define KernelPackage/ts-miconv2/description
endef

define KernelPackage/hwmon-ts-miconv2
  SUBMENU:=Hardware Monitoring Support
  TITLE:=MICON v2 hwmon driver for Buffalo TeraStation
  DEPENDS:=kmod-ts-miconv2 +kmod-hwmon-core
  FILES:=$(PKG_BUILD_DIR)/ts-miconv2-hwmon.ko
  AUTOLOAD:=$(call AutoLoad,40,ts-miconv2-hwmon,1)
endef

define KernelPackage/hwmon-ts-miconv2/description
endef

define KernelPackage/input-ts-miconv2-keys
  SUBMENU:=Input modules
  TITLE:=MICON v2 Keys driver for Buffalo TeraStation
  DEPENDS:=kmod-ts-miconv2 +kmod-button-hotplug
  FILES:=$(PKG_BUILD_DIR)/ts-miconv2-keys.ko
  AUTOLOAD:=$(call AutoLoad,45,ts-miconv2-keys,1)
endef

define KernelPackage/input-ts-miconv2-keys/description
endef

define KernelPackage/leds-ts-miconv2
  SUBMENU:=LED modules
  TITLE:=MICON v2 LEDs driver for Buffalo TeraStation
  DEPENDS:=kmod-ts-miconv2
  FILES:=$(PKG_BUILD_DIR)/leds-ts-miconv2.ko
  AUTOLOAD:=$(call AutoLoad,45,leds-ts-miconv2,1)
endef

define KernelPackage/leds-ts-miconv2/description
endef

define KernelPackage/reset-ts-miconv2
  SUBMENU:=Other modules
  TITLE:=MICON v2 Restart driver for Buffalo TeraStation
  DEPENDS:=kmod-ts-miconv2
  FILES:=$(PKG_BUILD_DIR)/ts-miconv2-reset.ko
  AUTOLOAD:=$(call AutoLoad,40,ts-miconv2-reset,1)
endef

define KernelPackage/reset-ts-miconv2/description
endef

define KernelPackage/wdt-ts-miconv2
  SUBMENU:=Other modules
  TITLE:=MICON v2 WDT driver for Buffalo TeraStation
  DEPENDS:=kmod-ts-miconv2
  FILES:=$(PKG_BUILD_DIR)/ts-miconv2-wdt.ko
  AUTOLOAD:=$(call AutoLoad,40,ts-miconv2-wdt,1)
endef

define KernelPackage/wdt-ts-miconv2/description
endef

define Build/Prepare
	$(CP) ./src/. $(PKG_BUILD_DIR)
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,ts-miconv2))
$(eval $(call KernelPackage,hwmon-ts-miconv2))
$(eval $(call KernelPackage,input-ts-miconv2-keys))
$(eval $(call KernelPackage,leds-ts-miconv2))
$(eval $(call KernelPackage,reset-ts-miconv2))
$(eval $(call KernelPackage,wdt-ts-miconv2))
