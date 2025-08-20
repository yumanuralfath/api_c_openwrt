#include "../api_manager.h"
#include "../helpers/response.h"
#include "../helpers/system_info.h"

// Handler for /api/network/interfaces
static void handle_network_interfaces(struct mg_connection *c,
                                      struct mg_http_message *hm) {
  char *interfaces = run_command(
      "ip addr show | grep -E '^[0-9]+:' | awk '{print $2}' | tr -d ':'");
  send_data_response(c, "interfaces", interfaces);
}

// Handler for /api/network/routes
static void handle_network_routes(struct mg_connection *c,
                                  struct mg_http_message *hm) {
  char *routes = run_command("ip route show");
  send_data_response(c, "routes", routes);
}

// Handler for /api/network/wan
static void handle_network_wan(struct mg_connection *c,
                               struct mg_http_message *hm) {
  char *wan_ip =
      run_command("uci get network.wan.ipaddr 2>/dev/null || echo 'DHCP'");
  char *wan_gateway =
      run_command("ip route show default | awk '{print $3}' | head -1");

  char response[512];
  snprintf(response, sizeof(response),
           "{"
           "\"ip_address\": \"%s\","
           "\"gateway\": \"%s\","
           "\"status\": \"connected\""
           "}",
           wan_ip, wan_gateway);

  send_json_response(c, 200, response);
}

// Handler for /api/network/lan
static void handle_network_lan(struct mg_connection *c,
                               struct mg_http_message *hm) {
  char *lan_ip =
      run_command("uci get network.lan.ipaddr 2>/dev/null || echo 'unknown'");
  char *lan_netmask =
      run_command("uci get network.lan.netmask 2>/dev/null || echo 'unknown'");

  char response[512];
  snprintf(response, sizeof(response),
           "{"
           "\"ip_address\": \"%s\","
           "\"netmask\": \"%s\","
           "\"interface\": \"br-lan\""
           "}",
           lan_ip, lan_netmask);

  send_json_response(c, 200, response);
}

// Handler for /api/network/dhcp/leases
static void handle_dhcp_leases(struct mg_connection *c,
                               struct mg_http_message *hm) {
  if (file_exists("/tmp/dhcp.leases")) {
    char *leases = read_file("/tmp/dhcp.leases");
    send_data_response(c, "dhcp_leases", leases);
  } else {
    send_error_response(c, 404, "Not Found", "DHCP leases file not found");
  }
}

// Handler for /api/network/ping
static void handle_network_ping(struct mg_connection *c,
                                struct mg_http_message *hm) {
  // Extract target from query parameters (simplified)
  char *target = "8.8.8.8"; // Default target

  char command[128];
  snprintf(command, sizeof(command), "ping -c 3 -W 2 %s | tail -1", target);
  char *result = run_command(command);

  send_data_response(c, "ping_result", result);
}

// Register all network endpoints
void register_network_endpoints(api_manager_t *manager) {
  api_register_route(manager, "/api/network/interfaces", METHOD_GET,
                     handle_network_interfaces,
                     "Get network interfaces information");
  api_register_route(manager, "/api/network/routes", METHOD_GET,
                     handle_network_routes, "Get routing table information");
  api_register_route(manager, "/api/network/wan", METHOD_GET,
                     handle_network_wan, "Get WAN interface information");
  api_register_route(manager, "/api/network/lan", METHOD_GET,
                     handle_network_lan, "Get LAN interface information");
  api_register_route(manager, "/api/network/dhcp/leases", METHOD_GET,
                     handle_dhcp_leases, "Get DHCP lease information");
  api_register_route(manager, "/api/network/ping", METHOD_GET,
                     handle_network_ping, "Ping connectivity test");
}
