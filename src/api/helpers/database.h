#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <time.h>

// Database connection
extern sqlite3 *db;

// Database init and cleanup
int db_init(const char *db_path);
void db_close(void);
int db_execute(const char *sql);
sqlite3_stmt *db_prepare(const char *sql);

// System monitoring data structures
typedef struct {
  int id;
  time_t timestamp;
  int total_processes;
  int total_ram_kb;
  char top_process[64];
  int top_process_ram_kb;
  double cpu_load;
  int memory_total_kb;
  int memory_free_kb;
  int memory_used_kb;
  double memory_usage_percent;
} system_snapshot_t;

typedef struct {
  int id;
  int snapshot_id;
  int pid;
  char process_name[64];
  int ram_kb;
  int rank_position;
  time_t timestamp;
} process_record_t;

typedef struct {
  int id;
  time_t timestamp;
  char event_type[32];
  char description[256];
  char data[512];
} system_event_t;

// System monitoring functions
int db_save_system_snapshot(const system_snapshot_t *snapshot);
int db_save_process_records(int snapshot_id, const process_record_t *processes,
                            int count);
int db_get_system_snapshots(system_snapshot_t **snapshots, int limit,
                            int offset);
int db_get_process_records(int snapshot_id, process_record_t **processes);
int db_cleanup_old_data(int days_to_keep);

// Event logging functions
int db_log_event(const char *event_type, const char *description,
                 const char *data);
int db_get_events(system_event_t **events, int limit, int offset,
                  const char *event_type);

// Configuration storage functions
int db_set_config(const char *key, const char *value);
char *db_get_config(const char *key);
int db_delete_config(const char *key);

// Network monitoring functions
int db_save_network_status(const char *interface, const char *status,
                           const char *ip, int rx_bytes, int tx_bytes);
int db_get_network_history(const char *interface, char **json_result,
                           int hours);

// Statistics and analytics
int db_get_ram_usage_trend(char **json_result, int hours);
int db_get_process_usage_stats(const char *process_name, char **json_result,
                               int days);
int db_get_system_summary_stats(char **json_result);

// Database maintenance
int db_vacuum(void);
int db_get_database_size(void);
int db_backup(const char *backup_path);

#endif // !DATABASE_H
