#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

sqlite3 *db = NULL;

// Database initialization
int db_init(const char *db_path) {
  int rc = sqlite3_open(db_path, &db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  // Enable foreign keys
  sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

  // Create tables
  const char *create_tables =
      "CREATE TABLE IF NOT EXISTS system_snapshots ("
      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  timestamp INTEGER NOT NULL,"
      "  total_processes INTEGER,"
      "  total_ram_kb INTEGER,"
      "  top_process TEXT,"
      "  top_process_ram_kb INTEGER,"
      "  cpu_load REAL,"
      "  memory_total_kb INTEGER,"
      "  memory_free_kb INTEGER,"
      "  memory_used_kb INTEGER,"
      "  memory_usage_percent REAL"
      ");"

      "CREATE TABLE IF NOT EXISTS process_records ("
      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  snapshot_id INTEGER,"
      "  pid INTEGER,"
      "  process_name TEXT,"
      "  ram_kb INTEGER,"
      "  rank_position INTEGER,"
      "  timestamp INTEGER,"
      "  FOREIGN KEY (snapshot_id) REFERENCES system_snapshots(id)"
      ");"

      "CREATE TABLE IF NOT EXISTS system_events ("
      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  timestamp INTEGER NOT NULL,"
      "  event_type TEXT NOT NULL,"
      "  description TEXT,"
      "  data TEXT"
      ");"

      "CREATE TABLE IF NOT EXISTS config_store ("
      "  key TEXT PRIMARY KEY,"
      "  value TEXT,"
      "  updated_at INTEGER"
      ");"

      "CREATE TABLE IF NOT EXISTS network_status ("
      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  timestamp INTEGER NOT NULL,"
      "  interface TEXT NOT NULL,"
      "  status TEXT,"
      "  ip_address TEXT,"
      "  rx_bytes INTEGER,"
      "  tx_bytes INTEGER"
      ");"

      "CREATE INDEX IF NOT EXISTS idx_snapshots_timestamp ON "
      "system_snapshots(timestamp);"
      "CREATE INDEX IF NOT EXISTS idx_processes_snapshot ON "
      "process_records(snapshot_id);"
      "CREATE INDEX IF NOT EXISTS idx_events_type_time ON "
      "system_events(event_type, timestamp);"
      "CREATE INDEX IF NOT EXISTS idx_network_interface_time ON "
      "network_status(interface, timestamp);";

  rc = sqlite3_exec(db, create_tables, NULL, NULL, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  printf("Database initialized successfully at %s\n", db_path);
  return 0;
}

void db_close(void) {
  if (db) {
    sqlite3_close(db);
    db = NULL;
  }
}

int db_execute(const char *sql) {
  char *err_msg = NULL;
  int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return -1;
  }
  return 0;
}

sqlite3_stmt *db_prepare(const char *sql) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
    return NULL;
  }
  return stmt;
}

// Save system snapshot
int db_save_system_snapshot(const system_snapshot_t *snapshot) {
  const char *sql =
      "INSERT INTO system_snapshots "
      "(timestamp, total_processes, total_ram_kb, top_process, "
      "top_process_ram_kb, cpu_load, memory_total_kb, memory_free_kb, "
      "memory_used_kb, memory_usage_percent) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return -1;

  sqlite3_bind_int64(stmt, 1, snapshot->timestamp);
  sqlite3_bind_int(stmt, 2, snapshot->total_processes);
  sqlite3_bind_int(stmt, 3, snapshot->total_ram_kb);
  sqlite3_bind_text(stmt, 4, snapshot->top_process, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 5, snapshot->top_process_ram_kb);
  sqlite3_bind_double(stmt, 6, snapshot->cpu_load);
  sqlite3_bind_int(stmt, 7, snapshot->memory_total_kb);
  sqlite3_bind_int(stmt, 8, snapshot->memory_free_kb);
  sqlite3_bind_int(stmt, 9, snapshot->memory_used_kb);
  sqlite3_bind_double(stmt, 10, snapshot->memory_usage_percent);

  int rc = sqlite3_step(stmt);
  int snapshot_id = -1;
  if (rc == SQLITE_DONE) {
    snapshot_id = sqlite3_last_insert_rowid(db);
  }

  sqlite3_finalize(stmt);
  return snapshot_id;
}

// Save process records
int db_save_process_records(int snapshot_id, const process_record_t *processes,
                            int count) {
  const char *sql =
      "INSERT INTO process_records "
      "(snapshot_id, pid, process_name, ram_kb, rank_position, timestamp) "
      "VALUES (?, ?, ?, ?, ?, ?)";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return -1;

  for (int i = 0; i < count; i++) {
    sqlite3_bind_int(stmt, 1, snapshot_id);
    sqlite3_bind_int(stmt, 2, processes[i].pid);
    sqlite3_bind_text(stmt, 3, processes[i].process_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, processes[i].ram_kb);
    sqlite3_bind_int(stmt, 5, processes[i].rank_position);
    sqlite3_bind_int64(stmt, 6, processes[i].timestamp);

    sqlite3_step(stmt);
    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
  return 0;
}

// Get system snapshots
int db_get_system_snapshots(system_snapshot_t **snapshots, int limit,
                            int offset) {
  const char *sql = "SELECT * FROM system_snapshots "
                    "ORDER BY timestamp DESC LIMIT ? OFFSET ?";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return -1;

  sqlite3_bind_int(stmt, 1, limit);
  sqlite3_bind_int(stmt, 2, offset);

  system_snapshot_t *results = malloc(limit * sizeof(system_snapshot_t));
  int count = 0;

  while (sqlite3_step(stmt) == SQLITE_ROW && count < limit) {
    results[count].id = sqlite3_column_int(stmt, 0);
    results[count].timestamp = sqlite3_column_int64(stmt, 1);
    results[count].total_processes = sqlite3_column_int(stmt, 2);
    results[count].total_ram_kb = sqlite3_column_int(stmt, 3);

    const char *top_proc = (const char *)sqlite3_column_text(stmt, 4);
    if (top_proc)
      strncpy(results[count].top_process, top_proc, 63);

    results[count].top_process_ram_kb = sqlite3_column_int(stmt, 5);
    results[count].cpu_load = sqlite3_column_double(stmt, 6);
    results[count].memory_total_kb = sqlite3_column_int(stmt, 7);
    results[count].memory_free_kb = sqlite3_column_int(stmt, 8);
    results[count].memory_used_kb = sqlite3_column_int(stmt, 9);
    results[count].memory_usage_percent = sqlite3_column_double(stmt, 10);
    count++;
  }

  sqlite3_finalize(stmt);
  *snapshots = results;
  return count;
}

// Log system event
int db_log_event(const char *event_type, const char *description,
                 const char *data) {
  const char *sql =
      "INSERT INTO system_events (timestamp, event_type, description, data) "
      "VALUES (?, ?, ?, ?)";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return -1;

  sqlite3_bind_int64(stmt, 1, time(NULL));
  sqlite3_bind_text(stmt, 2, event_type, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, data ? data : "", -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return (rc == SQLITE_DONE) ? 0 : -1;
}

// Get events
int db_get_events(system_event_t **events, int limit, int offset,
                  const char *event_type) {
  const char *sql = event_type ? "SELECT * FROM system_events WHERE event_type "
                                 "= ? ORDER BY timestamp DESC LIMIT ? OFFSET ?"
                               : "SELECT * FROM system_events ORDER BY "
                                 "timestamp DESC LIMIT ? OFFSET ?";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return -1;

  int param = 1;
  if (event_type)
    sqlite3_bind_text(stmt, param++, event_type, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, param++, limit);
  sqlite3_bind_int(stmt, param, offset);

  system_event_t *results = malloc(limit * sizeof(system_event_t));
  int count = 0;

  while (sqlite3_step(stmt) == SQLITE_ROW && count < limit) {
    results[count].id = sqlite3_column_int(stmt, 0);
    results[count].timestamp = sqlite3_column_int64(stmt, 1);

    const char *type = (const char *)sqlite3_column_text(stmt, 2);
    const char *desc = (const char *)sqlite3_column_text(stmt, 3);
    const char *data = (const char *)sqlite3_column_text(stmt, 4);

    if (type)
      strncpy(results[count].event_type, type, 31);
    if (desc)
      strncpy(results[count].description, desc, 255);
    if (data)
      strncpy(results[count].data, data, 511);

    count++;
  }

  sqlite3_finalize(stmt);
  *events = results;
  return count;
}

// Configuration storage
int db_set_config(const char *key, const char *value) {
  const char *sql =
      "INSERT OR REPLACE INTO config_store (key, value, updated_at) "
      "VALUES (?, ?, ?)";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return -1;

  sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 3, time(NULL));

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return (rc == SQLITE_DONE) ? 0 : -1;
}

char *db_get_config(const char *key) {
  const char *sql = "SELECT value FROM config_store WHERE key = ?";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return NULL;

  sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

  char *result = NULL;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *value = (const char *)sqlite3_column_text(stmt, 0);
    if (value) {
      result = malloc(strlen(value) + 1);
      strcpy(result, value);
    }
  }

  sqlite3_finalize(stmt);
  return result;
}

// Cleanup old data
int db_cleanup_old_data(int days_to_keep) {
  time_t cutoff_time = time(NULL) - (days_to_keep * 24 * 60 * 60);

  char sql[256];
  snprintf(sql, sizeof(sql),
           "DELETE FROM system_snapshots WHERE timestamp < %ld", cutoff_time);

  int rc1 = db_execute(sql);

  snprintf(sql, sizeof(sql), "DELETE FROM system_events WHERE timestamp < %ld",
           cutoff_time);

  int rc2 = db_execute(sql);

  return (rc1 == 0 && rc2 == 0) ? 0 : -1;
}

// Get RAM usage trend
int db_get_ram_usage_trend(char **json_result, int hours) {
  time_t since = time(NULL) - (hours * 60 * 60);

  const char *sql = "SELECT timestamp, total_ram_kb, memory_usage_percent "
                    "FROM system_snapshots WHERE timestamp >= ? "
                    "ORDER BY timestamp";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return -1;

  sqlite3_bind_int64(stmt, 1, since);

  char *result = malloc(8192);
  strcpy(result, "{\"success\":true,\"data\":[");

  int first = 1;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!first)
      strcat(result, ",");

    char entry[256];
    snprintf(entry, sizeof(entry),
             "{\"timestamp\":%lld,\"ram_kb\":%d,\"memory_percent\":%.2f}",
             sqlite3_column_int64(stmt, 0), sqlite3_column_int(stmt, 1),
             sqlite3_column_double(stmt, 2));

    strcat(result, entry);
    first = 0;
  }

  strcat(result, "]}");
  sqlite3_finalize(stmt);

  *json_result = result;
  return 0;
}

// Database maintenance
int db_vacuum(void) { return db_execute("VACUUM;"); }

int db_get_database_size(void) {
  const char *sql = "SELECT page_count * page_size as size FROM "
                    "pragma_page_count(), pragma_page_size()";

  sqlite3_stmt *stmt = db_prepare(sql);
  if (!stmt)
    return -1;

  int size = -1;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    size = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return size;
}
