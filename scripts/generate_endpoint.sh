#!/usr/bin/env bash

# Script to generate new API endpoint modules
# Usage: ./generate_endpoint.sh module_name

if [ $# -eq 0 ]; then
  echo "Usage: $0 <module_name>"
  echo "Example: $0 logs"
  exit 1
fi

MODULE_NAME="$1"
MODULE_LOWER=$(echo "$MODULE_NAME" | tr '[:upper:]' '[:lower:]')

ENDPOINT_FILE="src/api/endpoints/${MODULE_LOWER}.c"

echo "Generating endpoint module: $MODULE_NAME"

# Create the endpoint file
cat >"$ENDPOINT_FILE" <<EOF
#include "../api_manager.h"
#include "../helpers/response.h"
#include "../helpers/system_info.h"

// Handler for /api/${MODULE_LOWER}/status
static void handle_${MODULE_LOWER}_status(struct mg_connection *c, struct mg_http_message *hm) {
    send_success_response(c, "${MODULE_NAME} module is working", NULL);
}

// Handler for /api/${MODULE_LOWER}/info
static void handle_${MODULE_LOWER}_info(struct mg_connection *c, struct mg_http_message *hm) {
    const char *kvp[] = {
        "module", "${MODULE_LOWER}",
        "description", "${MODULE_NAME} management endpoint",
        "version", "1.0"
    };
    
    char *json = build_json_object(kvp, 6);
    send_json_response(c, 200, json);
    free_json_string(json);
}

// Add more handlers here as needed
// Example:
// static void handle_${MODULE_LOWER}_list(struct mg_connection *c, struct mg_http_message *hm) {
//     // Implementation here
// }

// Register all ${MODULE_LOWER} endpoints
void register_${MODULE_LOWER}_endpoints(api_manager_t *manager) {
    api_register_route(manager, "/api/${MODULE_LOWER}/status", METHOD_GET, 
                      handle_${MODULE_LOWER}_status, "Get ${MODULE_LOWER} module status");
    api_register_route(manager, "/api/${MODULE_LOWER}/info", METHOD_GET, 
                      handle_${MODULE_LOWER}_info, "Get ${MODULE_LOWER} module information");
    
    // Add more routes here
    // api_register_route(manager, "/api/${MODULE_LOWER}/list", METHOD_GET, 
    //                   handle_${MODULE_LOWER}_list, "List ${MODULE_LOWER} items");
}
EOF

echo "Created: $ENDPOINT_FILE"

# Update api_manager.h with the new function declaration
echo ""
echo "Add this line to api/api_manager.h in the endpoint registration section:"
echo "void register_${MODULE_LOWER}_endpoints(api_manager_t *manager);"
echo ""

# Update main.c
echo "Add this line to main.c in the initialize_api_endpoints() function:"
echo "register_${MODULE_LOWER}_endpoints(&api_manager);"
echo ""

# Update Makefile
echo "Add this line to the SOURCES section in Makefile:"
echo "\$(PKG_BUILD_DIR)/api/endpoints/${MODULE_LOWER}.c \\"
echo ""

echo "Module template generated successfully!"
echo "Don't forget to:"
echo "1. Update api_manager.h with the function declaration"
echo "2. Update main.c to register the new endpoints"
echo "3. Update Makefile to include the new source file"
echo "4. Implement your specific endpoint handlers"
