#include "../api_manager.h"
#include "../helpers/response.h"
#include "../helpers/system_info.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structure to hold process information
typedef struct {
  int pid;
  char name[21];
  int rss_kb;
  double rss_mb;
} process_info_t;

// Compare function for qsort (descending order by RSS)
static int compare_processes(const void *a, const void *b) {
  const process_info_t *pa = (const process_info_t *)a;
  const process_info_t *pb = (const process_info_t *)b;
  return pb->rss_kb - pa->rss_kb; // Descending order
}

// Check if string is a number (for PID validation)
static int is_number(const char *str) {
  if (!str || !*str)
    return 0;
  while (*str) {
    if (!isdigit(*str))
      return 0;
    str++;
  }
  return 1;
}

// Convert kB to MB
static double kb_to_mb(int kb) { return (double)kb / 1024.0; }

// Get process information
static int get_process_list(process_info_t **processes) {
  DIR *proc_dir = opendir("/proc");
  if (!proc_dir)
    return 0;

  struct dirent *entry;
  process_info_t *proc_list = malloc(1000 * sizeof(process_info_t));
  int count = 0;

  while ((entry = readdir(proc_dir)) != NULL && count < 1000) {
    if (!is_number(entry->d_name))
      continue;

    int pid = atoi(entry->d_name);
    char comm_path[64], status_path[64];
    snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);

    FILE *comm_file = fopen(comm_path, "r");
    FILE *status_file = fopen(status_path, "r");

    if (comm_file && status_file) {
      char name[21] = {0};
      int rss_kb = 0;
      char line[128];

      // Get process name
      if (fgets(name, sizeof(name), comm_file)) {
        // Remove newline
        char *nl = strchr(name, '\n');
        if (nl)
          *nl = '\0';
      }

      // Get RSS from status file
      while (fgets(line, sizeof(line), status_file)) {
        if (sscanf(line, "VmRSS: %d kB", &rss_kb) == 1) {
          break;
        }
      }

      if (strlen(name) > 0 && rss_kb > 0) {
        proc_list[count].pid = pid;
        strncpy(proc_list[count].name, name, 20);
        proc_list[count].name[20] = '\0';
        proc_list[count].rss_kb = rss_kb;
        proc_list[count].rss_mb = kb_to_mb(rss_kb);
        count++;
      }
    }

    if (comm_file)
      fclose(comm_file);
    if (status_file)
      fclose(status_file);
  }

  closedir(proc_dir);

  // Sort processes by RSS (descending)
  qsort(proc_list, count, sizeof(process_info_t), compare_processes);

  *processes = proc_list;
  return count;
}

// Handler for /api/monitoring/processes
static void handle_monitoring_processes(struct mg_connection *c,
                                        struct mg_http_message *hm) {
  process_info_t *processes;
  int count = get_process_list(&processes);

  if (count == 0) {
    send_error_response(c, 500, "Internal Server Error",
                        "Failed to get process list");
    return;
  }

  // Build JSON response
  char *response = malloc(count * 200 + 1000); // Rough estimate
  strcpy(response, "{\n  \"success\": true,\n  \"processes\": [\n");

  for (int i = 0; i < count; i++) {
    char proc_json[200];
    snprintf(proc_json, sizeof(proc_json),
             "    {\n"
             "      \"rank\": %d,\n"
             "      \"pid\": %d,\n"
             "      \"name\": \"%s\",\n"
             "      \"rss_kb\": %d,\n"
             "      \"rss_mb\": %.2f\n"
             "    }%s\n",
             i + 1, processes[i].pid, processes[i].name, processes[i].rss_kb,
             processes[i].rss_mb, (i < count - 1) ? "," : "");
    strcat(response, proc_json);
  }

  // Calculate totals
  int total_kb = 0;
  for (int i = 0; i < count; i++) {
    total_kb += processes[i].rss_kb;
  }

  char summary[300];
  snprintf(summary, sizeof(summary),
           "  ],\n"
           "  \"summary\": {\n"
           "    \"total_processes\": %d,\n"
           "    \"total_ram_kb\": %d,\n"
           "    \"total_ram_mb\": %.2f,\n"
           "    \"highest_process\": \"%s\",\n"
           "    \"highest_ram_kb\": %d,\n"
           "    \"highest_ram_mb\": %.2f\n"
           "  }\n}",
           count, total_kb, kb_to_mb(total_kb),
           count > 0 ? processes[0].name : "none",
           count > 0 ? processes[0].rss_kb : 0,
           count > 0 ? processes[0].rss_mb : 0.0);
  strcat(response, summary);

  send_json_response(c, 200, response);

  free(response);
  free(processes);
}

// Handler for /api/monitoring/processes/top/{N}
static void handle_monitoring_top_processes(struct mg_connection *c,
                                            struct mg_http_message *hm) {
  // Extract number from URL path (simplified parsing)
  char *path_copy = malloc(hm->uri.len + 1);
  memcpy(path_copy, hm->uri.buf, hm->uri.len);
  path_copy[hm->uri.len] = '\0';

  int limit = 10; // default
  char *last_slash = strrchr(path_copy, '/');
  if (last_slash && is_number(last_slash + 1)) {
    limit = atoi(last_slash + 1);
    if (limit <= 0)
      limit = 10;
    if (limit > 100)
      limit = 100; // Cap at 100
  }
  free(path_copy);

  process_info_t *processes;
  int count = get_process_list(&processes);

  if (count == 0) {
    send_error_response(c, 500, "Internal Server Error",
                        "Failed to get process list");
    return;
  }

  // Limit the count to requested number
  if (limit < count)
    count = limit;

  // Build JSON response for top N processes
  char *response = malloc(count * 200 + 1000);
  sprintf(response,
          "{\n  \"success\": true,\n  \"limit\": %d,\n  \"processes\": [\n",
          limit);

  for (int i = 0; i < count; i++) {
    char proc_json[200];
    snprintf(proc_json, sizeof(proc_json),
             "    {\n"
             "      \"rank\": %d,\n"
             "      \"pid\": %d,\n"
             "      \"name\": \"%s\",\n"
             "      \"rss_kb\": %d,\n"
             "      \"rss_mb\": %.2f\n"
             "    }%s\n",
             i + 1, processes[i].pid, processes[i].name, processes[i].rss_kb,
             processes[i].rss_mb, (i < count - 1) ? "," : "");
    strcat(response, proc_json);
  }
  strcat(response, "  ]\n}");

  send_json_response(c, 200, response);

  free(response);
  free(processes);
}

// Handler for /api/monitoring/memory/summary
static void handle_memory_summary(struct mg_connection *c,
                                  struct mg_http_message *hm) {
  FILE *meminfo = fopen("/proc/meminfo", "r");
  if (!meminfo) {
    send_error_response(c, 500, "Internal Server Error",
                        "Cannot read memory information");
    return;
  }

  char line[128];
  int mem_total = 0, mem_free = 0, mem_available = 0, mem_buffers = 0,
      mem_cached = 0;

  while (fgets(line, sizeof(line), meminfo)) {
    if (sscanf(line, "MemTotal: %d kB", &mem_total))
      continue;
    if (sscanf(line, "MemFree: %d kB", &mem_free))
      continue;
    if (sscanf(line, "MemAvailable: %d kB", &mem_available))
      continue;
    if (sscanf(line, "Buffers: %d kB", &mem_buffers))
      continue;
    if (sscanf(line, "Cached: %d kB", &mem_cached))
      continue;
  }
  fclose(meminfo);

  int mem_used = mem_total - mem_free;
  double usage_percent = (double)mem_used / mem_total * 100.0;

  char response[512];
  snprintf(response, sizeof(response),
           "{\n"
           "  \"success\": true,\n"
           "  \"memory\": {\n"
           "    \"total_kb\": %d,\n"
           "    \"total_mb\": %.2f,\n"
           "    \"free_kb\": %d,\n"
           "    \"free_mb\": %.2f,\n"
           "    \"used_kb\": %d,\n"
           "    \"used_mb\": %.2f,\n"
           "    \"available_kb\": %d,\n"
           "    \"available_mb\": %.2f,\n"
           "    \"buffers_kb\": %d,\n"
           "    \"cached_kb\": %d,\n"
           "    \"usage_percent\": %.2f\n"
           "  }\n}",
           mem_total, kb_to_mb(mem_total), mem_free, kb_to_mb(mem_free),
           mem_used, kb_to_mb(mem_used), mem_available, kb_to_mb(mem_available),
           mem_buffers, mem_cached, usage_percent);

  send_json_response(c, 200, response);
}

// Handler for /api/monitoring/system/stats
static void handle_system_stats(struct mg_connection *c,
                                struct mg_http_message *hm) {
  // Get system stats similar to the script
  process_info_t *processes;
  int proc_count = get_process_list(&processes);

  int total_ram_kb = 0;
  for (int i = 0; i < proc_count; i++) {
    total_ram_kb += processes[i].rss_kb;
  }

  char *uptime = get_system_uptime();
  char *load = get_system_load();

  char response[1024];
  snprintf(response, sizeof(response),
           "{\n"
           "  \"success\": true,\n"
           "  \"system_stats\": {\n"
           "    \"uptime_seconds\": \"%s\",\n"
           "    \"load_average\": \"%s\",\n"
           "    \"total_processes\": %d,\n"
           "    \"total_process_ram_kb\": %d,\n"
           "    \"total_process_ram_mb\": %.2f,\n"
           "    \"top_process\": {\n"
           "      \"name\": \"%s\",\n"
           "      \"pid\": %d,\n"
           "      \"ram_kb\": %d,\n"
           "      \"ram_mb\": %.2f\n"
           "    }\n"
           "  }\n}",
           uptime, load, proc_count, total_ram_kb, kb_to_mb(total_ram_kb),
           proc_count > 0 ? processes[0].name : "none",
           proc_count > 0 ? processes[0].pid : 0,
           proc_count > 0 ? processes[0].rss_kb : 0,
           proc_count > 0 ? processes[0].rss_mb : 0.0);

  send_json_response(c, 200, response);

  if (processes)
    free(processes);
}

// Register all monitoring endpoints
void register_monitoring_endpoints(api_manager_t *manager) {
  api_register_route(manager, "/api/monitoring/processes", METHOD_GET,
                     handle_monitoring_processes,
                     "Get all processes sorted by RAM usage");

  api_register_route(manager, "/api/monitoring/processes/top", METHOD_GET,
                     handle_monitoring_top_processes,
                     "Get top N processes by RAM usage (append /N to URL)");

  api_register_route(manager, "/api/monitoring/memory/summary", METHOD_GET,
                     handle_memory_summary,
                     "Get detailed memory usage summary");

  api_register_route(manager, "/api/monitoring/system/stats", METHOD_GET,
                     handle_system_stats,
                     "Get comprehensive system monitoring statistics");
}
