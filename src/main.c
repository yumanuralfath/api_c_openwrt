#include "api/api_manager.h"
#include "mongoose/mongoose.h"
#include <signal.h>
#include <stdio.h>

static struct mg_mgr mgr;
static api_manager_t api_manager;
static int server_running = 1;

void signal_handler(int sig) {
  printf("Shutting down server...\n");
  server_running = 0;
}

// Main event handler - delegates to API manager
// static void event_handler(struct mg_connection *c, int ev, void *ev_data) {
//   if (ev == MG_EV_HTTP_MSG) {
//     struct mg_http_message *hm = (struct mg_http_message *)ev_data;
//     api_handle_request(&api_manager, c, hm);
//   }
// }

static void event_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    // Handle CORS preflight
    if (mg_strcmp(hm->method, mg_str("OPTIONS")) == 0) {
      mg_http_reply(
          c, 200,
          "Access-Control-Allow-Origin: *\r\n"
          "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, "
          "OPTIONS\r\n"
          "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
          "");
      return;
    }

    // Normal request
    api_handle_request(&api_manager, c, hm);
  }
}

// Initialize all API endpoints
void initialize_api_endpoints() {
  printf("Initializing API endpoints...\n");

  // Register endpoint modules
  register_status_endpoints(&api_manager);
  register_system_endpoints(&api_manager);
  register_network_endpoints(&api_manager);
  register_wireless_endpoints(&api_manager);
  register_monitoring_endpoints(&api_manager);

  printf("Registered %d API endpoints\n", api_manager.route_count);
}

// Print startup information
void print_startup_info() {
  printf("\n=== OpenWrt Modular API Server ===\n");
  printf("Server started on port 9000\n");
  printf("API Documentation: http://0.0.0.0:9000/api\n");
  printf("\nQuick Start Endpoints:\n");
  printf("  - GET  /api              - API documentation\n");
  printf("  - GET  /api/status       - Server status\n");
  printf("  - GET  /api/health       - System health\n");
  printf("  - GET  /api/system/info  - System information\n");
  printf("  - GET  /api/network/wan  - WAN status\n");
  printf("  - GET  /api/wireless/config - WiFi configuration\n");
  printf("  - GET  /api/monitoring/processes - Process RAM usage\n");
  printf("  - GET  /api/monitoring/processes/top/10 - Top 10 processes\n");
  printf("  - GET  /api/monitoring/memory/summary - Memory statistics\n");
  printf("\nPress Ctrl+C to stop.\n");
  printf("=====================================\n\n");
}

int main(int argc, char *argv[]) {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Initialize API manager
  api_manager_init(&api_manager);

  // Register all endpoint modules
  initialize_api_endpoints();

  // Initialize Mongoose manager
  mg_mgr_init(&mgr);

  // Parse command line arguments for port (optional)
  const char *port = "9000";
  if (argc > 1) {
    port = argv[1];
  }

  // Create HTTP server
  char listen_addr[64];
  snprintf(listen_addr, sizeof(listen_addr), "http://0.0.0.0:%s", port);

  struct mg_connection *c =
      mg_http_listen(&mgr, listen_addr, event_handler, NULL);
  if (c == NULL) {
    fprintf(stderr, "Failed to create HTTP server on port %s\n", port);
    return 1;
  }

  // Print startup information
  print_startup_info();

  // Main event loop
  while (server_running) {
    mg_mgr_poll(&mgr, 1000);
  }

  // Cleanup
  printf("Cleaning up...\n");
  mg_mgr_free(&mgr);
  printf("Server stopped.\n");

  return 0;
}
