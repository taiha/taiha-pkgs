# SPDX-License-Identifier: GPL-2.0-or-later OR MIT

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=linux-optdrivers
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/mfd-core
  SUBMENU:=Other modules
  TITLE:=MFD Core
  FILES:=$(PKG_BUILD_DIR)/mfd-core.ko
  AUTOLOAD:=$(call AutoProbe,mfd-core,1)
endef

define KernelPackage/mfd-core/description
endef

define Build/Prepare
	$(CP) ./src/. $(PKG_BUILD_DIR)
	$(LN) -s $(LINUX_DIR)/drivers/mfd/mfd-core.c $(PKG_BUILD_DIR)/mfd-core.c
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,mfd-core))
