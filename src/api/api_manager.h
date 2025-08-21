#ifndef API_MANAGER_H
#define API_MANAGER_H

#include "../mongoose/mongoose.h"

// Maximum number of routes
#define MAX_ROUTES 50

// HTTP methods
typedef enum {
  METHOD_GET,
  METHOD_POST,
  METHOD_PUT,
  METHOD_DELETE,
  METHOD_PATCH
} http_method_t;

// Route handler function type
typedef void (*route_handler_t)(struct mg_connection *c,
                                struct mg_http_message *hm);

// Route structure
typedef struct {
  char path[128];
  http_method_t method;
  route_handler_t handler;
  char description[256];
} route_t;

// API Manager structure
typedef struct {
  route_t routes[MAX_ROUTES];
  int route_count;
} api_manager_t;

// Function declarations
void api_manager_init(api_manager_t *manager);
int api_register_route(api_manager_t *manager, const char *path,
                       http_method_t method, route_handler_t handler,
                       const char *description);
void api_handle_request(api_manager_t *manager, struct mg_connection *c,
                        struct mg_http_message *hm);
void api_list_routes(api_manager_t *manager, struct mg_connection *c,
                     struct mg_http_message *hm);
const char *method_to_string(http_method_t method);
http_method_t string_to_method(const char *method_str);

// Endpoint registration functions (implemented in respective modules)
void register_status_endpoints(api_manager_t *manager);
void register_system_endpoints(api_manager_t *manager);
void register_network_endpoints(api_manager_t *manager);
void register_wireless_endpoints(api_manager_t *manager);
void register_monitoring_endpoints(api_manager_t *manager);
void register_database_endpoints(api_manager_t *manager);

#endif // API_MANAGER_H
