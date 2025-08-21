include $(TOPDIR)/rules.mk

PKG_NAME:=api_c
PKG_VERSION:=1.1
PKG_RELEASE:=3

include $(INCLUDE_DIR)/package.mk

define Package/api_c
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=Modular OpenWrt API Server
	DEPENDS:=+libc +libsqlite3
endef

define Package/api_c/description
	Modular API server for OpenWrt with extensible endpoint system.
	Provides REST API endpoints for system information, network status,
	wireless configuration, and more.
endef

define Build/Configure
endef

# Source files
SOURCES := \
	$(PKG_BUILD_DIR)/main.c \
	$(PKG_BUILD_DIR)/mongoose/mongoose.c \
	$(PKG_BUILD_DIR)/api/api_manager.c \
	$(PKG_BUILD_DIR)/api/helpers/response.c \
	$(PKG_BUILD_DIR)/api/helpers/system_info.c \
	$(PKG_BUILD_DIR)/api/helpers/database.c \
	$(PKG_BUILD_DIR)/api/endpoints/status.c \
	$(PKG_BUILD_DIR)/api/endpoints/system.c \
	$(PKG_BUILD_DIR)/api/endpoints/network.c \
	$(PKG_BUILD_DIR)/api/endpoints/wireless.c \
	$(PKG_BUILD_DIR)/api/endpoints/monitoring.c \
	$(PKG_BUILD_DIR)/api/endpoints/database.c

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) \
		-I$(PKG_BUILD_DIR) \
		-I$(PKG_BUILD_DIR)/mongoose \
		-I$(PKG_BUILD_DIR)/api \
		-o $(PKG_BUILD_DIR)/api_c \
		$(SOURCES) \
		$(TARGET_LDFLAGS) -lsqlite3
endef

define Package/api_c/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/api_c $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/api_c.init $(1)/etc/init.d/api_c
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/api_c.config $(1)/etc/config/api_c
	
	$(INSTALL_DIR) $(1)/usr/share/api_c
	$(INSTALL_DATA) ./files/README.md $(1)/usr/share/api_c/ || true
	$(INSTALL_BIN) ./files/auto_snapshot.sh $(1)/usr/share/api_c/
	
	$(INSTALL_DIR) $(1)/var/log
endef

define Package/api_c/postinst
#!/bin/sh
if [ -z "${IPKG_INSTROOT}" ]; then
	echo "Enabling api_c service..."
	/etc/init.d/api_c enable
	echo "Starting api_c service..."
	/etc/init.d/api_c start
	echo ""
	echo "OpenWrt API Server with Database is now running!"
	echo "Visit http://your-router-ip:9000/api for documentation"
	echo ""
	echo "Database features:"
	echo "  - Save snapshots: curl -X POST http://your-router-ip:9000/api/database/save/snapshot"
	echo "  - View snapshots: curl http://your-router-ip:9000/api/database/snapshots"
	echo "  - Auto snapshots: /usr/share/api_c/auto_snapshot.sh setup-cron 15"
	echo "  - Database stats: /usr/share/api_c/auto_snapshot.sh stats"
fi
exit 0
endef

define Package/api_c/prerm
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	echo "Stopping api_c service..."
	/etc/init.d/api_c stop
	/etc/init.d/api_c disable
fi
exit 0
endef

$(eval $(call BuildPackage,api_c))
