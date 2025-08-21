#include "../helpers/database.h"
#include "../api_manager.h"
#include "../helpers/response.h"
#include "../helpers/system_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Forward declaration for monitoring integration
extern int get_process_list_for_db(process_record_t **processes);

// Handler for /api/database/save/snapshot (POST)
static void handle_save_snapshot(struct mg_connection *c,
                                 struct mg_http_message *hm) {
  system_snapshot_t snapshot = {0};
  snapshot.timestamp = time(NULL);

  // Get system information
  char *uptime = get_system_uptime();
  char *load_str = get_system_load();

  // Parse load average (first value)
  snapshot.cpu_load = 0.0;
  if (load_str) {
    sscanf(load_str, "%lf", &snapshot.cpu_load);
  }

  // Get memory information from /proc/meminfo
  FILE *meminfo = fopen("/proc/meminfo", "r");
  if (meminfo) {
    char line[128];
    while (fgets(line, sizeof(line), meminfo)) {
      if (sscanf(line, "MemTotal: %d kB", &snapshot.memory_total_kb))
        continue;
      if (sscanf(line, "MemFree: %d kB", &snapshot.memory_free_kb))
        continue;
    }
    fclose(meminfo);
  }

  snapshot.memory_used_kb = snapshot.memory_total_kb - snapshot.memory_free_kb;
  if (snapshot.memory_total_kb > 0) {
    snapshot.memory_usage_percent =
        (double)snapshot.memory_used_kb / snapshot.memory_total_kb * 100.0;
  }

  // Get process information
  process_record_t *processes;
  int proc_count = get_process_list_for_db(&processes);

  if (proc_count > 0) {
    snapshot.total_processes = proc_count;
    snapshot.total_ram_kb = 0;

    // Calculate total RAM and find top process
    for (int i = 0; i < proc_count; i++) {
      snapshot.total_ram_kb += processes[i].ram_kb;
    }

    // Top process is first (already sorted)
    if (processes[0].ram_kb > 0) {
      strncpy(snapshot.top_process, processes[0].process_name, 63);
      snapshot.top_process_ram_kb = processes[0].ram_kb;
    }
  }

  // Save snapshot to database
  int snapshot_id = db_save_system_snapshot(&snapshot);
  if (snapshot_id > 0) {
    // Save process records
    if (proc_count > 0) {
      // Set snapshot_id and timestamp for all processes
      for (int i = 0; i < proc_count; i++) {
        processes[i].snapshot_id = snapshot_id;
        processes[i].timestamp = snapshot.timestamp;
        processes[i].rank_position = i + 1;
      }
      db_save_process_records(snapshot_id, processes, proc_count);
    }

    // Log event
    char desc[128];
    snprintf(desc, sizeof(desc), "System snapshot saved with %d processes",
             proc_count);
    db_log_event("SNAPSHOT", desc, NULL);

    char response[256];
    snprintf(response, sizeof(response),
             "{"
             "\"success\": true,"
             "\"message\": \"Snapshot saved successfully\","
             "\"snapshot_id\": %d,"
             "\"processes_saved\": %d,"
             "\"timestamp\": %ld"
             "}",
             snapshot_id, proc_count, snapshot.timestamp);

    send_json_response(c, 200, response);
  } else {
    send_error_response(c, 500, "Database Error", "Failed to save snapshot");
  }

  if (processes)
    free(processes);
}

// Handler for /api/database/snapshots
static void handle_get_snapshots(struct mg_connection *c,
                                 struct mg_http_message *hm) {
  // Parse query parameters (simplified)
  int limit = 10;
  int offset = 0;

  // Get query string
  char query[256] = {0};
  if (hm->query.len > 0 && hm->query.len < 255) {
    memcpy(query, hm->query.buf, hm->query.len);
    query[hm->query.len] = '\0';

    // Simple parameter parsing
    char *limit_param = strstr(query, "limit=");
    if (limit_param) {
      sscanf(limit_param, "limit=%d", &limit);
      if (limit > 100)
        limit = 100;
    }

    char *offset_param = strstr(query, "offset=");
    if (offset_param) {
      sscanf(offset_param, "offset=%d", &offset);
    }
  }

  system_snapshot_t *snapshots;
  int count = db_get_system_snapshots(&snapshots, limit, offset);

  if (count >= 0) {
    char *response = malloc(count * 300 + 1000);
    sprintf(response,
            "{\n  \"success\": true,\n  \"count\": %d,\n  \"snapshots\": [\n",
            count);

    for (int i = 0; i < count; i++) {
      char snapshot_json[300];
      snprintf(
          snapshot_json, sizeof(snapshot_json),
          "    {\n"
          "      \"id\": %d,\n"
          "      \"timestamp\": %ld,\n"
          "      \"datetime\": \"%s\",\n"
          "      \"total_processes\": %d,\n"
          "      \"total_ram_kb\": %d,\n"
          "      \"total_ram_mb\": %.2f,\n"
          "      \"top_process\": \"%s\",\n"
          "      \"top_process_ram_kb\": %d,\n"
          "      \"cpu_load\": %.2f,\n"
          "      \"memory_usage_percent\": %.2f\n"
          "    }%s\n",
          snapshots[i].id, snapshots[i].timestamp,
          ctime(&snapshots[i].timestamp), // This includes \n, but we'll trim it
          snapshots[i].total_processes, snapshots[i].total_ram_kb,
          (double)snapshots[i].total_ram_kb / 1024.0, snapshots[i].top_process,
          snapshots[i].top_process_ram_kb, snapshots[i].cpu_load,
          snapshots[i].memory_usage_percent, (i < count - 1) ? "," : "");

      // Remove newline from ctime
      char *nl = strstr(snapshot_json, "\\n");
      while (nl) {
        *nl = ' ';
        *(nl + 1) = ' ';
        nl = strstr(nl + 2, "\\n");
      }

      strcat(response, snapshot_json);
    }

    strcat(response, "  ]\n}");
    send_json_response(c, 200, response);

    free(response);
    free(snapshots);
  } else {
    send_error_response(c, 500, "Database Error",
                        "Failed to retrieve snapshots");
  }
}

// Handler for /api/database/events
static void handle_get_events(struct mg_connection *c,
                              struct mg_http_message *hm) {
  int limit = 50;
  int offset = 0;
  char event_type_filter[64] = {0};

  // Parse query parameters
  char query[256] = {0};
  if (hm->query.len > 0 && hm->query.len < 255) {
    memcpy(query, hm->query.buf, hm->query.len);
    query[hm->query.len] = '\0';

    char *limit_param = strstr(query, "limit=");
    if (limit_param)
      sscanf(limit_param, "limit=%d", &limit);

    char *offset_param = strstr(query, "offset=");
    if (offset_param)
      sscanf(offset_param, "offset=%d", &offset);

    char *type_param = strstr(query, "type=");
    if (type_param) {
      sscanf(type_param, "type=%63s", event_type_filter);
    }
  }

  system_event_t *events;
  const char *filter =
      (strlen(event_type_filter) > 0) ? event_type_filter : NULL;
  int count = db_get_events(&events, limit, offset, filter);

  if (count >= 0) {
    char *response = malloc(count * 400 + 1000);
    sprintf(response,
            "{\n  \"success\": true,\n  \"count\": %d,\n  \"events\": [\n",
            count);

    for (int i = 0; i < count; i++) {
      char event_json[400];
      char *datetime_str = ctime(&events[i].timestamp);
      if (datetime_str) {
        char *nl = strchr(datetime_str, '\n');
        if (nl)
          *nl = '\0';
      }

      snprintf(event_json, sizeof(event_json),
               "    {\n"
               "      \"id\": %d,\n"
               "      \"timestamp\": %ld,\n"
               "      \"datetime\": \"%s\",\n"
               "      \"event_type\": \"%s\",\n"
               "      \"description\": \"%s\",\n"
               "      \"data\": \"%s\"\n"
               "    }%s\n",
               events[i].id, events[i].timestamp,
               datetime_str ? datetime_str : "unknown", events[i].event_type,
               events[i].description, events[i].data,
               (i < count - 1) ? "," : "");

      strcat(response, event_json);
    }

    strcat(response, "  ]\n}");
    send_json_response(c, 200, response);

    free(response);
    free(events);
  } else {
    send_error_response(c, 500, "Database Error", "Failed to retrieve events");
  }
}

// Handler for /api/database/config (GET/POST)
static void handle_config(struct mg_connection *c, struct mg_http_message *hm) {
  if (strncmp(hm->method.buf, "GET", 3) == 0) {
    // GET - retrieve configuration
    char query[256] = {0};
    char key[64] = {0};

    if (hm->query.len > 0 && hm->query.len < 255) {
      memcpy(query, hm->query.buf, hm->query.len);
      query[hm->query.len] = '\0';

      char *key_param = strstr(query, "key=");
      if (key_param) {
        sscanf(key_param, "key=%63s", key);
      }
    }

    if (strlen(key) > 0) {
      // Get specific key
      char *value = db_get_config(key);
      if (value) {
        char response[512];
        snprintf(response, sizeof(response),
                 "{"
                 "\"success\": true,"
                 "\"key\": \"%s\","
                 "\"value\": \"%s\""
                 "}",
                 key, value);
        send_json_response(c, 200, response);
        free(value);
      } else {
        send_error_response(c, 404, "Not Found", "Configuration key not found");
      }
    } else {
      send_error_response(c, 400, "Bad Request", "Key parameter required");
    }
  } else if (strncmp(hm->method.buf, "POST", 4) == 0) {
    // POST - set configuration
    // Parse JSON body (simplified)
    char body[512] = {0};
    if (hm->body.len > 0 && hm->body.len < 511) {
      memcpy(body, hm->body.buf, hm->body.len);
      body[hm->body.len] = '\0';

      char key[64] = {0}, value[256] = {0};

      // Simple JSON parsing for {"key":"...", "value":"..."}
      char *key_start = strstr(body, "\"key\":");
      char *value_start = strstr(body, "\"value\":");

      if (key_start && value_start) {
        // Extract key
        key_start = strchr(key_start + 6, '"');
        if (key_start) {
          key_start++;
          char *key_end = strchr(key_start, '"');
          if (key_end) {
            int key_len = key_end - key_start;
            if (key_len < 63) {
              strncpy(key, key_start, key_len);
              key[key_len] = '\0';
            }
          }
        }

        // Extract value
        value_start = strchr(value_start + 8, '"');
        if (value_start) {
          value_start++;
          char *value_end = strchr(value_start, '"');
          if (value_end) {
            int value_len = value_end - value_start;
            if (value_len < 255) {
              strncpy(value, value_start, value_len);
              value[value_len] = '\0';
            }
          }
        }

        if (strlen(key) > 0 && strlen(value) > 0) {
          if (db_set_config(key, value) == 0) {
            db_log_event("CONFIG", "Configuration updated", key);
            send_success_response(c, "Configuration saved", NULL);
          } else {
            send_error_response(c, 500, "Database Error",
                                "Failed to save configuration");
          }
        } else {
          send_error_response(c, 400, "Bad Request", "Invalid JSON format");
        }
      } else {
        send_error_response(c, 400, "Bad Request",
                            "Missing key or value in JSON");
      }
    } else {
      send_error_response(c, 400, "Bad Request", "Invalid request body");
    }
  }
}

// Handler for /api/database/analytics/ram-trend
static void handle_ram_trend(struct mg_connection *c,
                             struct mg_http_message *hm) {
  int hours = 24; // Default 24 hours

  // Parse hours parameter
  char query[256] = {0};
  if (hm->query.len > 0 && hm->query.len < 255) {
    memcpy(query, hm->query.buf, hm->query.len);
    query[hm->query.len] = '\0';

    char *hours_param = strstr(query, "hours=");
    if (hours_param) {
      sscanf(hours_param, "hours=%d", &hours);
      if (hours > 168)
        hours = 168; // Max 1 week
    }
  }

  char *json_result;
  if (db_get_ram_usage_trend(&json_result, hours) == 0) {
    send_json_response(c, 200, json_result);
    free(json_result);
  } else {
    send_error_response(c, 500, "Database Error",
                        "Failed to get RAM trend data");
  }
}

// Handler for /api/database/cleanup
static void handle_cleanup(struct mg_connection *c,
                           struct mg_http_message *hm) {
  int days_to_keep = 7; // Default keep 7 days

  // Parse days parameter
  char query[256] = {0};
  if (hm->query.len > 0 && hm->query.len < 255) {
    memcpy(query, hm->query.buf, hm->query.len);
    query[hm->query.len] = '\0';

    char *days_param = strstr(query, "days=");
    if (days_param) {
      sscanf(days_param, "days=%d", &days_to_keep);
      if (days_to_keep < 1)
        days_to_keep = 1;
    }
  }

  if (db_cleanup_old_data(days_to_keep) == 0) {
    char desc[128];
    snprintf(desc, sizeof(desc), "Cleaned up data older than %d days",
             days_to_keep);
    db_log_event("MAINTENANCE", desc, NULL);

    char response[256];
    snprintf(response, sizeof(response),
             "{"
             "\"success\": true,"
             "\"message\": \"Database cleanup completed\","
             "\"days_kept\": %d"
             "}",
             days_to_keep);
    send_json_response(c, 200, response);
  } else {
    send_error_response(c, 500, "Database Error", "Failed to cleanup database");
  }
}

// Handler for /api/database/stats
static void handle_database_stats(struct mg_connection *c,
                                  struct mg_http_message *hm) {
  int db_size = db_get_database_size();

  // Get record counts
  const char *count_sql =
      "SELECT "
      "(SELECT COUNT(*) FROM system_snapshots) as snapshots, "
      "(SELECT COUNT(*) FROM process_records) as processes, "
      "(SELECT COUNT(*) FROM system_events) as events, "
      "(SELECT COUNT(*) FROM config_store) as configs";

  sqlite3_stmt *stmt = db_prepare(count_sql);
  int snapshots = 0, processes = 0, events = 0, configs = 0;

  if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
    snapshots = sqlite3_column_int(stmt, 0);
    processes = sqlite3_column_int(stmt, 1);
    events = sqlite3_column_int(stmt, 2);
    configs = sqlite3_column_int(stmt, 3);
  }

  if (stmt)
    sqlite3_finalize(stmt);

  char response[512];
  snprintf(response, sizeof(response),
           "{"
           "\"success\": true,"
           "\"database_stats\": {"
           "\"size_bytes\": %d,"
           "\"size_mb\": %.2f,"
           "\"snapshots\": %d,"
           "\"process_records\": %d,"
           "\"events\": %d,"
           "\"config_entries\": %d"
           "}"
           "}",
           db_size, (double)db_size / (1024.0 * 1024.0), snapshots, processes,
           events, configs);

  send_json_response(c, 200, response);
}

// Helper function to get process list compatible with database format
int get_process_list_for_db(process_record_t **processes) {
  DIR *proc_dir = opendir("/proc");
  if (!proc_dir)
    return 0;

  struct dirent *entry;
  process_record_t *proc_list = malloc(1000 * sizeof(process_record_t));
  int count = 0;

  while ((entry = readdir(proc_dir)) != NULL && count < 1000) {
    // Check if entry name is numeric (PID)
    int is_pid = 1;
    for (char *p = entry->d_name; *p; p++) {
      if (*p < '0' || *p > '9') {
        is_pid = 0;
        break;
      }
    }
    if (!is_pid)
      continue;

    int pid = atoi(entry->d_name);
    char comm_path[64], status_path[64];
    snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);

    FILE *comm_file = fopen(comm_path, "r");
    FILE *status_file = fopen(status_path, "r");

    if (comm_file && status_file) {
      char name[64] = {0};
      int rss_kb = 0;
      char line[128];

      // Get process name
      if (fgets(name, sizeof(name), comm_file)) {
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
        strncpy(proc_list[count].process_name, name, 63);
        proc_list[count].process_name[63] = '\0';
        proc_list[count].ram_kb = rss_kb;
        count++;
      }
    }

    if (comm_file)
      fclose(comm_file);
    if (status_file)
      fclose(status_file);
  }

  closedir(proc_dir);

  // Sort by RAM usage (descending)
  for (int i = 0; i < count - 1; i++) {
    for (int j = 0; j < count - i - 1; j++) {
      if (proc_list[j].ram_kb < proc_list[j + 1].ram_kb) {
        process_record_t temp = proc_list[j];
        proc_list[j] = proc_list[j + 1];
        proc_list[j + 1] = temp;
      }
    }
  }

  *processes = proc_list;
  return count;
}

// Register all database endpoints
void register_database_endpoints(api_manager_t *manager) {
  api_register_route(manager, "/api/database/save/snapshot", METHOD_POST,
                     handle_save_snapshot,
                     "Save current system snapshot to database");

  api_register_route(manager, "/api/database/snapshots", METHOD_GET,
                     handle_get_snapshots,
                     "Get system snapshots from database");

  api_register_route(manager, "/api/database/events", METHOD_GET,
                     handle_get_events, "Get system events from database");

  api_register_route(manager, "/api/database/config", METHOD_GET, handle_config,
                     "Get configuration from database");

  api_register_route(manager, "/api/database/config", METHOD_POST,
                     handle_config, "Set configuration in database");

  api_register_route(manager, "/api/database/analytics/ram-trend", METHOD_GET,
                     handle_ram_trend, "Get RAM usage trend analytics");

  api_register_route(manager, "/api/database/cleanup", METHOD_POST,
                     handle_cleanup, "Cleanup old database records");

  api_register_route(manager, "/api/database/stats", METHOD_GET,
                     handle_database_stats, "Get database statistics");
}
