#include "../api_manager.h"
#include "../helpers/response.h"
#include "../helpers/system_info.h"

// Handler for /api/status
static void handle_status(struct mg_connection *c, struct mg_http_message *hm) {
  const char *kvp[] = {
      "status",  "running", "message",  "OpenWrt API Server is working",
      "version", "1.0",     "hostname", get_hostname()};

  char *json = build_json_object(kvp, 8);
  send_json_response(c, 200, json);
  free_json_string(json);
}

// Handler for /api/health
static void handle_health(struct mg_connection *c, struct mg_http_message *hm) {
  char *uptime = get_system_uptime();
  char *load = get_system_load();

  char response[512];
  snprintf(response, sizeof(response),
           "{"
           "\"status\": \"healthy\","
           "\"uptime_seconds\": \"%s\","
           "\"system_load\": \"%s\","
           "\"timestamp\": \"%ld\""
           "}",
           uptime, load, time(NULL));

  send_json_response(c, 200, response);
}

// Handler for /api/version
static void handle_version(struct mg_connection *c,
                           struct mg_http_message *hm) {
  const char *kvp[] = {"api_version",     "1.0",
                       "openwrt_version", get_openwrt_version(),
                       "kernel_version",  get_kernel_version()};

  char *json = build_json_object(kvp, 6);
  send_json_response(c, 200, json);
  free_json_string(json);
}

// Register all status endpoints
void register_status_endpoints(api_manager_t *manager) {
  api_register_route(manager, "/api/status", METHOD_GET, handle_status,
                     "Get API server status");
  api_register_route(manager, "/api/health", METHOD_GET, handle_health,
                     "Get system health information");
  api_register_route(manager, "/api/version", METHOD_GET, handle_version,
                     "Get version information");
}
