#include "../api_manager.h"
#include "../helpers/response.h"
#include "../helpers/system_info.h"

// Handler for /api/wireless/status
static void handle_wireless_status(struct mg_connection *c,
                                   struct mg_http_message *hm) {
  char *status = run_command("wifi status");
  if (strlen(status) > 10) {
    send_data_response(c, "wireless_status", status);
  } else {
    send_error_response(c, 503, "Service Unavailable",
                        "Wireless interface not available");
  }
}

// Handler for /api/wireless/scan
static void handle_wireless_scan(struct mg_connection *c,
                                 struct mg_http_message *hm) {
  char *scan_result =
      run_command("iwlist scan 2>/dev/null | grep ESSID | head -10");
  send_data_response(c, "scan_results", scan_result);
}

// Handler for /api/wireless/config
static void handle_wireless_config(struct mg_connection *c,
                                   struct mg_http_message *hm) {
  char *ssid = run_command(
      "uci get wireless.@wifi-iface[0].ssid 2>/dev/null || echo 'unknown'");
  char *mode = run_command(
      "uci get wireless.@wifi-iface[0].mode 2>/dev/null || echo 'unknown'");
  char *channel =
      run_command("uci get wireless.radio0.channel 2>/dev/null || echo 'auto'");
  char *encryption = run_command(
      "uci get wireless.@wifi-iface[0].encryption 2>/dev/null || echo 'none'");

  char response[512];
  snprintf(response, sizeof(response),
           "{"
           "\"ssid\": \"%s\","
           "\"mode\": \"%s\","
           "\"channel\": \"%s\","
           "\"encryption\": \"%s\""
           "}",
           ssid, mode, channel, encryption);

  send_json_response(c, 200, response);
}

// Handler for /api/wireless/clients
static void handle_wireless_clients(struct mg_connection *c,
                                    struct mg_http_message *hm) {
  char *clients =
      run_command("iw dev wlan0 station dump | grep Station | wc -l");
  send_data_response(c, "connected_clients", clients);
}

// Handler for /api/wireless/restart (POST)
static void handle_wireless_restart(struct mg_connection *c,
                                    struct mg_http_message *hm) {
  send_success_response(c, "Wireless restart initiated", NULL);
  system("wifi down && wifi up &");
}

// Register all wireless endpoints
void register_wireless_endpoints(api_manager_t *manager) {
  api_register_route(manager, "/api/wireless/status", METHOD_GET,
                     handle_wireless_status, "Get wireless interface status");
  api_register_route(manager, "/api/wireless/scan", METHOD_GET,
                     handle_wireless_scan,
                     "Scan for available wireless networks");
  api_register_route(manager, "/api/wireless/config", METHOD_GET,
                     handle_wireless_config, "Get wireless configuration");
  api_register_route(manager, "/api/wireless/clients", METHOD_GET,
                     handle_wireless_clients,
                     "Get number of connected wireless clients");
  api_register_route(manager, "/api/wireless/restart", METHOD_POST,
                     handle_wireless_restart, "Restart wireless interface");
}
