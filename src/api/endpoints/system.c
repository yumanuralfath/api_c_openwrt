#include "../api_manager.h"
#include "../helpers/response.h"
#include "../helpers/system_info.h"
#include <time.h>

// Handler for /api/system/info
static void handle_system_info(struct mg_connection *c,
                               struct mg_http_message *hm) {
  char response[1024];
  snprintf(response, sizeof(response),
           "{"
           "\"hostname\": \"%s\","
           "\"uptime\": \"%s\","
           "\"kernel\": \"%s\","
           "\"cpu\": \"%s\","
           "\"memory\": \"%s\","
           "\"load\": \"%s\""
           "}",
           get_hostname(), get_system_uptime(), get_kernel_version(),
           get_cpu_info(), get_memory_info(), get_system_load());

  send_json_response(c, 200, response);
}

// Handler for /api/system/uptime
static void handle_system_uptime(struct mg_connection *c,
                                 struct mg_http_message *hm) {
  send_data_response(c, "uptime_seconds", get_system_uptime());
}

// Handler for /api/system/memory
static void handle_system_memory(struct mg_connection *c,
                                 struct mg_http_message *hm) {
  send_data_response(c, "memory_info", get_memory_info());
}

// Handler for /api/system/load
static void handle_system_load(struct mg_connection *c,
                               struct mg_http_message *hm) {
  send_data_response(c, "system_load", get_system_load());
}

// Handler for /api/system/reboot (POST)
static void handle_system_reboot(struct mg_connection *c,
                                 struct mg_http_message *hm) {
  // In a real implementation, you might want to add authentication here
  send_success_response(c, "Reboot scheduled", NULL);

  // Schedule reboot (be careful with this in production!)
  // system("reboot &");
}

// Handler for /api/system/datetime
static void handle_system_datetime(struct mg_connection *c,
                                   struct mg_http_message *hm) {
  time_t now;
  time(&now);
  char *datetime = ctime(&now);
  trim_whitespace(datetime);

  char response[256];
  snprintf(response, sizeof(response),
           "{"
           "\"timestamp\": %ld,"
           "\"datetime\": \"%s\","
           "\"timezone\": \"UTC\""
           "}",
           now, datetime);

  send_json_response(c, 200, response);
}

// Register all system endpoints
void register_system_endpoints(api_manager_t *manager) {
  api_register_route(manager, "/api/system/info", METHOD_GET,
                     handle_system_info,
                     "Get comprehensive system information");
  api_register_route(manager, "/api/system/uptime", METHOD_GET,
                     handle_system_uptime, "Get system uptime in seconds");
  api_register_route(manager, "/api/system/memory", METHOD_GET,
                     handle_system_memory, "Get memory usage information");
  api_register_route(manager, "/api/system/load", METHOD_GET,
                     handle_system_load, "Get system load average");
  api_register_route(manager, "/api/system/datetime", METHOD_GET,
                     handle_system_datetime,
                     "Get current system date and time");
  api_register_route(manager, "/api/system/reboot", METHOD_POST,
                     handle_system_reboot, "Schedule system reboot");
}
