# OpenWrt Modular API Server

<!--toc:start-->

- [OpenWrt Modular API Server](#openwrt-modular-api-server)
  - [Features](#features)
  - [Quick Start](#quick-start)
  - [Available Endpoints](#available-endpoints)
    - [Core API](#core-api)
    - [Status & Health](#status-health)
    - [System Information](#system-information)
    - [Network Management](#network-management)
    - [Wireless Management](#wireless-management)
  - [Development Guide](#development-guide)
    - [Adding New Endpoints](#adding-new-endpoints)
    - [Creating Custom Handlers](#creating-custom-handlers)
    - [Helper Functions Available](#helper-functions-available)
      - [Response Helpers](#response-helpers)
      - [JSON Builders](#json-builders)
      - [System Information](#system-information)
  - [New Monitoring Endpoints](#new-monitoring-endpoints)
    - [1. Get All Processes (equivalent to your script without --top)](#1-get-all-processes-equivalent-to-your-script-without-top)
    - [2. Get Top N Processes (equivalent to your script --top N)](#2-get-top-n-processes-equivalent-to-your-script-top-n)
    - [3. Memory Summary (detailed memory stats)](#3-memory-summary-detailed-memory-stats)
    - [4. System Statistics (combined overview)](#4-system-statistics-combined-overview)
  - [Integration Benefits](#integration-benefits)
  - [Command Line Equivalents](#command-line-equivalents)
  - [Usage in Web Interfaces](#usage-in-web-interfaces)
    - [HTTP Methods](#http-methods)
    - [Error Handling](#error-handling)
  - [Configuration](#configuration)
    - [Service Management](#service-management)
    - [Custom Port](#custom-port)
  - [Security Considerations](#security-considerations)
  - [Troubleshooting](#troubleshooting)
    - [Common Issues](#common-issues)
    - [Debugging](#debugging)
  - [Contributing](#contributing)
  - [License](#license)
  <!--toc:end-->

A highly modular and extensible REST API server for OpenWrt routers, built with the Mongoose embedded web server library.

## Features

- **Modular Architecture**: Easy to add new endpoints and functionality
- **Comprehensive System Info**: Access system status, memory, CPU, and network information
- **Network Management**: Monitor WAN/LAN status, DHCP leases, and routing
- **Wireless Control**: View WiFi configuration, scan networks, and manage connections
- **Extensible Design**: Simple framework for adding custom API endpoints
- **JSON Responses**: All responses in standardized JSON format
- **Built-in Documentation**: Self-documenting API with endpoint listings

## Quick Start

After installation, the API server runs on port 9000. Access the documentation at:

```
http://your-router-ip:9000/api
```

## Available Endpoints

### Core API

- `GET /api` - API documentation and endpoint listing
- `GET /api/help` - Same as above

### Status & Health

- `GET /api/status` - API server status
- `GET /api/health` - System health check
- `GET /api/version` - Version information

### System Information

- `GET /api/system/info` - Comprehensive system information
- `GET /api/system/uptime` - System uptime
- `GET /api/system/memory` - Memory usage
- `GET /api/system/load` - System load average
- `GET /api/system/datetime` - Current date and time
- `POST /api/system/reboot` - Schedule system reboot

### Network Management

- `GET /api/network/interfaces` - Network interface list
- `GET /api/network/routes` - Routing table
- `GET /api/network/wan` - WAN interface status
- `GET /api/network/lan` - LAN interface status
- `GET /api/network/dhcp/leases` - DHCP lease information
- `GET /api/network/ping` - Connectivity test

### Wireless Management

- `GET /api/wireless/status` - Wireless interface status
- `GET /api/wireless/scan` - Scan for WiFi networks
- `GET /api/wireless/config` - Current WiFi configuration
- `GET /api/wireless/clients` - Connected client count
- `POST /api/wireless/restart` - Restart wireless interface

## Development Guide

### Adding New Endpoints

1. **Create Endpoint Module**:

   ```bash
   ./scripts/generate_endpoint.sh your_module_name
   ```

2. **Update Headers**:
   Add function declaration to `api/api_manager.h`:

   ```c
   void register_your_module_endpoints(api_manager_t *manager);
   ```

3. **Register in Main**:
   Add to `main.c` in `initialize_api_endpoints()`:

   ```c
   register_your_module_endpoints(&api_manager);
   ```

4. **Update Makefile**:
   Add source file to `SOURCES` list:

   ```makefile
   $(PKG_BUILD_DIR)/api/endpoints/your_module.c \
   ```

### Creating Custom Handlers

```c
#include "../api_manager.h"
#include "../helpers/response.h"

static void handle_custom_endpoint(struct mg_connection *c, struct mg_http_message *hm) {
    // Simple success response
    send_success_response(c, "Custom endpoint works", NULL);

    // JSON object response
    const char *kvp[] = {"key1", "value1", "key2", "value2"};
    char *json = build_json_object(kvp, 4);
    send_json_response(c, 200, json);
    free_json_string(json);

    // Error response
    send_error_response(c, 400, "Bad Request", "Invalid parameters");
}
```

### Helper Functions Available

#### Response Helpers

- `send_json_response(c, status, json_data)`
- `send_success_response(c, message, data)`
- `send_error_response(c, status, error, message)`
- `send_data_response(c, key, value)`

#### JSON Builders

- `build_json_object(key_value_pairs, count)`
- `build_json_array(items, count)`
- `free_json_string(json_str)`

#### System Information

- `get_system_uptime()`
- `get_system_load()`
- `get_memory_info()`
- `get_cpu_info()`
- `get_hostname()`
- `run_command(command)`
- `get_openwrt_version()`
- `get_uci_config(config_name)`

## New Monitoring Endpoints

### 1. Get All Processes (equivalent to your script without --top)

```bash
curl http://your-router-ip:9000/api/monitoring/processes
```

**Response Example:**

```json
{
  "success": true,
  "processes": [
    {
      "rank": 1,
      "pid": 1234,
      "name": "uhttpd",
      "rss_kb": 2048,
      "rss_mb": 2.0
    },
    {
      "rank": 2,
      "pid": 5678,
      "name": "kernel",
      "rss_kb": 1536,
      "rss_mb": 1.5
    }
  ],
  "summary": {
    "total_processes": 45,
    "total_ram_kb": 12345,
    "total_ram_mb": 12.05,
    "highest_process": "uhttpd",
    "highest_ram_kb": 2048,
    "highest_ram_mb": 2.0
  }
}
```

### 2. Get Top N Processes (equivalent to your script --top N)

```bash
# Get top 10 processes
curl http://your-router-ip:9000/api/monitoring/processes/top/10

# Get top 5 processes
curl http://your-router-ip:9000/api/monitoring/processes/top/5
```

**Response Example:**

```json
{
  "success": true,
  "limit": 10,
  "processes": [
    {
      "rank": 1,
      "pid": 1234,
      "name": "uhttpd",
      "rss_kb": 2048,
      "rss_mb": 2.0
    }
  ]
}
```

### 3. Memory Summary (detailed memory stats)

```bash
curl http://your-router-ip:9000/api/monitoring/memory/summary
```

**Response Example:**

```json
{
  "success": true,
  "memory": {
    "total_kb": 131072,
    "total_mb": 128.0,
    "free_kb": 45678,
    "free_mb": 44.61,
    "used_kb": 85394,
    "used_mb": 83.39,
    "available_kb": 50000,
    "available_mb": 48.83,
    "buffers_kb": 2048,
    "cached_kb": 8192,
    "usage_percent": 65.15
  }
}
```

### 4. System Statistics (combined overview)

```bash
curl http://your-router-ip:9000/api/monitoring/system/stats
```

**Response Example:**

```json
{
  "success": true,
  "system_stats": {
    "uptime_seconds": "12345.67",
    "load_average": "0.25 0.30 0.28",
    "total_processes": 45,
    "total_process_ram_kb": 12345,
    "total_process_ram_mb": 12.05,
    "top_process": {
      "name": "uhttpd",
      "pid": 1234,
      "ram_kb": 2048,
      "ram_mb": 2.0
    }
  }
}
```

## Usage in Web Interfaces

You can now easily create web dashboards that consume this data:

```javascript
// Fetch top 10 processes
fetch("/api/monitoring/processes/top/10")
  .then((response) => response.json())
  .then((data) => {
    console.log("Top process:", data.processes[0].name);
    console.log("RAM usage:", data.processes[0].rss_mb + " MB");
  });

// Fetch memory summary
fetch("/api/monitoring/memory/summary")
  .then((response) => response.json())
  .then((data) => {
    console.log("Memory usage:", data.memory.usage_percent + "%");
  });
```

The monitoring module maintains all the functionality of your original script while providing it through a clean REST API interface!

### HTTP Methods

The API manager supports:

- `METHOD_GET`
- `METHOD_POST`
- `METHOD_PUT`
- `METHOD_DELETE`
- `METHOD_PATCH`

### Error Handling

All endpoints should handle errors gracefully:

```c
static void handle_example(struct mg_connection *c, struct mg_http_message *hm) {
    if (!file_exists("/some/required/file")) {
        send_error_response(c, 404, "Not Found", "Required file not found");
        return;
    }

    char *data = read_file("/some/file");
    if (!data) {
        send_error_response(c, 500, "Internal Server Error", "Failed to read file");
        return;
    }

    send_data_response(c, "file_content", data);
}
```

## Configuration

### Service Management

```bash
# Start service
/etc/init.d/api_c start

# Stop service
/etc/init.d/api_c stop

# Restart service
/etc/init.d/api_c restart

# Check status
/etc/init.d/api_c status
```

### Custom Port

Edit `/etc/config/api_c` to change the port:

```
config api_c 'main'
    option port '9000'
    option enabled '1'
```

## Security Considerations

- The API currently runs without authentication
- Be careful with system modification endpoints (reboot, restart services)
- Consider implementing rate limiting for production use
- Network access should be restricted to trusted networks

## Troubleshooting

### Common Issues

1. **Service won't start**:

   ```bash
   logread | grep api_c
   ```

2. **Compilation errors**:
   - Check all source files are included in Makefile
   - Verify function declarations in headers match implementations

3. **404 errors**:
   - Check route registration in module functions
   - Verify endpoint module is registered in main.c

### Debugging

Enable verbose logging by modifying the service startup script or running manually:

```bash
/usr/bin/api_c 9000
```

## Contributing

When adding new features:

1. Follow the modular structure
2. Use the provided helper functions
3. Add comprehensive error handling
4. Update this documentation
5. Test all endpoints thoroughly

## License

This project is part of the OpenWrt ecosystem and follows OpenWrt licensing.
