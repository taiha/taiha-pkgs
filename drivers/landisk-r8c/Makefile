# SPDX-License-Identifier: GPL-2.0-or-later OR MIT

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=landisk-r8c
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/landisk-r8c
  SUBMENU:=Other modules
  TITLE:=R8C MCU driver for I-O DATA LAN DISK series NAS
  DEPENDS:=+kmod-mfd-core
  FILES:=$(PKG_BUILD_DIR)/landisk-r8c.ko
  AUTOLOAD:=$(call AutoProbe,landisk-r8c,1)
endef

define KernelPackage/landisk-r8c/description
 test
endef

define KernelPackage/hwmon-landisk-r8c
  SUBMENU:=Hardware Monitoring Support
  TITLE:=R8C HW Monitoring driver for I-O DATA LAN DISK series NAS
  DEPENDS:=kmod-landisk-r8c
  FILES:=$(PKG_BUILD_DIR)/landisk-r8c-hwmon.ko
  AUTOLOAD:=$(call AutoProbe,landisk-r8c-hwmon,1)
endef

define KernelPackage/hwmon-landisk-r8c/description
 test
endef

define KernelPackage/input-landisk-r8c-beeper
  SUBMENU:=Input modules
  TITLE:=R8C beeper driver for I-O DATA LAN DISK series NAS
  DEPENDS:=kmod-landisk-r8c +kmod-input-core
  FILES:=$(PKG_BUILD_DIR)/landisk-r8c-beeper.ko
  AUTOLOAD:=$(call AutoProbe,landisk-r8c-beeper,)
endef

define KernelPackage/input-landisk-r8c-beeper/description
 test
endef

define KernelPackage/input-landisk-r8c-keys
  SUBMENU:=Input modules
  TITLE:=R8C keys driver for I-O DATA LAN DISK series NAS
  DEPENDS:=kmod-landisk-r8c +kmod-button-hotplug
  FILES:=$(PKG_BUILD_DIR)/landisk-r8c-keys.ko
  AUTOLOAD:=$(call AutoProbe,landisk-r8c-keys,1)
endef

define KernelPackage/input-landisk-r8c-keys/description
 test
endef

define KernelPackage/leds-landisk-r8c
  SUBMENU:=LED modules
  TITLE:=R8C LEDs driver for I-O DATA LAN DISK series NAS
  DEPENDS:=kmod-landisk-r8c
  FILES:=$(PKG_BUILD_DIR)/leds-landisk-r8c.ko
  AUTOLOAD:=$(call AutoProbe,leds-landisk-r8c,1)
endef

define KernelPackage/leds-landisk-r8c/description
 test
endef

define KernelPackage/reset-landisk-r8c
  SUBMENU:=Other modules
  TITLE:=R8C restart driver for I-O DATA LAN DISK series NAS
  DEPENDS:=kmod-landisk-r8c
  FILES:=$(PKG_BUILD_DIR)/landisk-r8c-reset.ko
  AUTOLOAD:=$(call AutoProbe,landisk-r8c-reset,1)
endef

define KernelPackage/reset-landisk-r8c/description
 test
endef

define Build/Prepare
	$(CP) ./src/. $(PKG_BUILD_DIR)
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,landisk-r8c))
$(eval $(call KernelPackage,hwmon-landisk-r8c))
$(eval $(call KernelPackage,input-landisk-r8c-beeper))
$(eval $(call KernelPackage,input-landisk-r8c-keys))
$(eval $(call KernelPackage,leds-landisk-r8c))
$(eval $(call KernelPackage,reset-landisk-r8c))
