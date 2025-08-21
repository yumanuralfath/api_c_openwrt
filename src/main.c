#include "api/api_manager.h"
#include "api/helpers/database.h"
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
static void event_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
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
  register_database_endpoints(&api_manager);

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
  printf("  - POST /api/database/save/snapshot - Save system snapshot\n");
  printf("  - GET  /api/database/snapshots - Get saved snapshots\n");
  printf("  - GET  /api/database/events - Get system events\n");
  printf("\nPress Ctrl+C to stop.\n");
  printf("=====================================\n\n");
}

int main(int argc, char *argv[]) {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Initialize database
  const char *db_path = "/tmp/openwrt_api.db";
  if (argc > 2 && strcmp(argv[2], "--db") == 0 && argc > 3) {
    db_path = argv[3];
  }

  if (db_init(db_path) != 0) {
    fprintf(stderr, "Failed to initialize database at %s\n", db_path);
    return 1;
  }

  // Log startup event
  db_log_event("STARTUP", "API server starting", NULL);

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
    db_close();
    return 1;
  }

  // Print startup information
  print_startup_info();
  printf("Database: %s\n\n", db_path);

  // Main event loop
  while (server_running) {
    mg_mgr_poll(&mgr, 1000);
  }

  // Cleanup
  printf("Cleaning up...\n");
  db_log_event("SHUTDOWN", "API server shutting down", NULL);
  mg_mgr_free(&mgr);
  db_close();
  printf("Server stopped.\n");

  return 0;
}
