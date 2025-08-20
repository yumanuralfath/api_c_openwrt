#include "api_manager.h"
#include "helpers/response.h"
#include <stdio.h>
#include <string.h>

void api_manager_init(api_manager_t *manager) {
  manager->route_count = 0;
  memset(manager->routes, 0, sizeof(manager->routes));
}

int api_register_route(api_manager_t *manager, const char *path,
                       http_method_t method, route_handler_t handler,
                       const char *description) {
  if (manager->route_count >= MAX_ROUTES) {
    printf("Error: Maximum routes limit reached\n");
    return -1;
  }

  route_t *route = &manager->routes[manager->route_count];
  strncpy(route->path, path, sizeof(route->path) - 1);
  route->method = method;
  route->handler = handler;
  strncpy(route->description, description, sizeof(route->description) - 1);

  manager->route_count++;
  printf("Registered route: %s %s - %s\n", method_to_string(method), path,
         description);
  return 0;
}

void api_handle_request(api_manager_t *manager, struct mg_connection *c,
                        struct mg_http_message *hm) {
  http_method_t method = string_to_method(hm->method.buf);

  // Check for help/documentation endpoint
  if (mg_match(hm->uri, mg_str("/api"), NULL) ||
      mg_match(hm->uri, mg_str("/api/help"), NULL)) {
    api_list_routes(manager, c, hm);
    return;
  }

  // Find matching route
  for (int i = 0; i < manager->route_count; i++) {
    route_t *route = &manager->routes[i];
    if (route->method == method &&
        mg_match(hm->uri, mg_str(route->path), NULL)) {
      route->handler(c, hm);
      return;
    }
  }

  // No route found
  send_error_response(c, 404, "Not Found",
                      "The requested endpoint does not exist");
}

void api_list_routes(api_manager_t *manager, struct mg_connection *c,
                     struct mg_http_message *hm) {
  char *response = malloc(4096);
  strcpy(response, "{\n  \"api\": \"OpenWrt Modular API\",\n  \"version\": "
                   "\"1.0\",\n  \"endpoints\": [\n");

  for (int i = 0; i < manager->route_count; i++) {
    route_t *route = &manager->routes[i];
    char endpoint[512];
    snprintf(endpoint, sizeof(endpoint),
             "    {\n"
             "      \"method\": \"%s\",\n"
             "      \"path\": \"%s\",\n"
             "      \"description\": \"%s\"\n"
             "    }%s\n",
             method_to_string(route->method), route->path, route->description,
             (i < manager->route_count - 1) ? "," : "");
    strcat(response, endpoint);
  }

  strcat(response, "  ]\n}");

  mg_http_reply(c, 200,
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                "%s", response);
  free(response);
}

const char *method_to_string(http_method_t method) {
  switch (method) {
  case METHOD_GET:
    return "GET";
  case METHOD_POST:
    return "POST";
  case METHOD_PUT:
    return "PUT";
  case METHOD_DELETE:
    return "DELETE";
  case METHOD_PATCH:
    return "PATCH";
  default:
    return "UNKNOWN";
  }
}

http_method_t string_to_method(const char *method_str) {
  if (strncmp(method_str, "GET", 3) == 0)
    return METHOD_GET;
  if (strncmp(method_str, "POST", 4) == 0)
    return METHOD_POST;
  if (strncmp(method_str, "PUT", 3) == 0)
    return METHOD_PUT;
  if (strncmp(method_str, "DELETE", 6) == 0)
    return METHOD_DELETE;
  if (strncmp(method_str, "PATCH", 5) == 0)
    return METHOD_PATCH;
  return METHOD_GET; // Default
}
