#!/bin/sh

# Automatic snapshot collection script for OpenWrt API database
# This script can be run via cron to collect system snapshots regularly

API_URL="http://localhost:9000"
LOG_FILE="/var/log/api_snapshot.log"
MAX_LOG_SIZE=100000 # 100KB

# Function to log messages
log_message() {
  echo "$(date): $1" >>"$LOG_FILE"

  # Rotate log if it gets too big
  if [ -f "$LOG_FILE" ] && [ $(wc -c <"$LOG_FILE") -gt $MAX_LOG_SIZE ]; then
    tail -n 50 "$LOG_FILE" >"$LOG_FILE.tmp"
    mv "$LOG_FILE.tmp" "$LOG_FILE"
  fi
}

# Function to save snapshot
save_snapshot() {
  log_message "Attempting to save system snapshot"

  # Check if API server is running
  if ! curl -s --connect-timeout 5 "$API_URL/api/status" >/dev/null 2>&1; then
    log_message "ERROR: API server not responding at $API_URL"
    return 1
  fi

  # Save snapshot
  response=$(curl -s -X POST "$API_URL/api/database/save/snapshot" 2>&1)

  if [ $? -eq 0 ]; then
    # Check if response contains success
    if echo "$response" | grep -q '"success":true'; then
      snapshot_id=$(echo "$response" | sed -n 's/.*"snapshot_id":\([0-9]*\).*/\1/p')
      processes=$(echo "$response" | sed -n 's/.*"processes_saved":\([0-9]*\).*/\1/p')
      log_message "SUCCESS: Saved snapshot ID $snapshot_id with $processes processes"
    else
      log_message "ERROR: API returned error - $response"
      return 1
    fi
  else
    log_message "ERROR: Failed to connect to API - $response"
    return 1
  fi

  return 0
}

# Function to cleanup old data
cleanup_old_data() {
  local days_to_keep="${1:-7}"

  log_message "Starting cleanup of data older than $days_to_keep days"

  response=$(curl -s -X POST "$API_URL/api/database/cleanup?days=$days_to_keep" 2>&1)

  if [ $? -eq 0 ] && echo "$response" | grep -q '"success":true'; then
    log_message "SUCCESS: Database cleanup completed"
  else
    log_message "ERROR: Database cleanup failed - $response"
  fi
}

# Function to get database stats
show_stats() {
  echo "=== OpenWrt API Database Statistics ==="

  # Get database stats
  stats=$(curl -s "$API_URL/api/database/stats" 2>/dev/null)
  if [ $? -eq 0 ] && echo "$stats" | grep -q '"success":true'; then
    size_mb=$(echo "$stats" | sed -n 's/.*"size_mb":\([0-9.]*\).*/\1/p')
    snapshots=$(echo "$stats" | sed -n 's/.*"snapshots":\([0-9]*\).*/\1/p')
    processes=$(echo "$stats" | sed -n 's/.*"process_records":\([0-9]*\).*/\1/p')
    events=$(echo "$stats" | sed -n 's/.*"events":\([0-9]*\).*/\1/p')

    echo "Database Size: ${size_mb} MB"
    echo "System Snapshots: $snapshots"
    echo "Process Records: $processes"
    echo "System Events: $events"
  else
    echo "ERROR: Could not retrieve database statistics"
  fi

  echo ""
  echo "=== Recent Log Entries ==="
  if [ -f "$LOG_FILE" ]; then
    tail -n 10 "$LOG_FILE"
  else
    echo "No log file found at $LOG_FILE"
  fi
}

# Function to show usage
show_usage() {
  echo "Usage: $0 [command] [options]"
  echo ""
  echo "Commands:"
  echo "  snapshot              Save current system snapshot"
  echo "  cleanup [days]        Cleanup data older than [days] (default: 7)"
  echo "  stats                 Show database statistics"
  echo "  setup-cron [interval] Setup automatic snapshots"
  echo "  remove-cron          Remove automatic snapshots"
  echo ""
  echo "Examples:"
  echo "  $0 snapshot                    # Save one snapshot"
  echo "  $0 setup-cron 15                  # Snapshot every 15 minutes"
  echo "  $0 setup-cron hourly              # Snapshot every hour"
  echo "  $0 stats                          # Show database info"
}

# Function to setup cron job
setup_cron() {
  local interval="${1:-15}"
  local cron_entry=""

  case "$interval" in
  "hourly" | "60")
    cron_entry="0 * * * * /usr/share/api_c/auto_snapshot.sh snapshot"
    echo "Setting up hourly snapshots"
    ;;
  "daily" | "1440")
    cron_entry="0 0 * * * /usr/share/api_c/auto_snapshot.sh snapshot"
    echo "Setting up daily snapshots"
    ;;
  [0-9]*)
    if [ "$interval" -lt 5 ]; then
      echo "ERROR: Minimum interval is 5 minutes"
      return 1
    fi
    cron_entry="*/$interval * * * * /usr/share/api_c/auto_snapshot.sh snapshot"
    echo "Setting up snapshots every $interval minutes"
    ;;
  *)
    echo "ERROR: Invalid interval. Use number (minutes), 'hourly', or 'daily'"
    return 1
    ;;
  esac

  # Remove existing cron job
  crontab -l 2>/dev/null | grep -v "auto_snapshot.sh" | crontab -

  # Add new cron job
  (
    crontab -l 2>/dev/null
    echo "$cron_entry"
  ) | crontab -

  log_message "Cron job setup: $cron_entry"
  echo "Automatic snapshots configured successfully"
  echo "Log file: $LOG_FILE"

  # Also setup weekly cleanup
  cleanup_cron="0 2 * * 0 /usr/share/api_c/auto_snapshot.sh cleanup 7"
  (
    crontab -l 2>/dev/null | grep -v "auto_snapshot.sh cleanup"
    echo "$cleanup_cron"
  ) | crontab -
  echo "Weekly cleanup also configured (Sundays at 2 AM)"
}

# Function to remove cron job
remove_cron() {
  crontab -l 2>/dev/null | grep -v "auto_snapshot.sh" | crontab -
  log_message "Automatic snapshots disabled"
  echo "Automatic snapshots removed from cron"
}

# Function to test API connection
test_api() {
  echo "Testing API connection..."

  if curl -s --connect-timeout 5 "$API_URL/api/status" >/dev/null; then
    echo "✓ API server is responding"

    # Test database functionality
    if curl -s "$API_URL/api/database/stats" | grep -q '"success":true'; then
      echo "✓ Database is accessible"
      return 0
    else
      echo "✗ Database is not accessible"
      return 1
    fi
  else
    echo "✗ API server is not responding at $API_URL"
    echo "  Make sure the API service is running: /etc/init.d/api_c status"
    return 1
  fi
}

# Main script logic
case "$1" in
"snapshot")
  save_snapshot
  exit $?
  ;;

"cleanup")
  days=${2:-7}
  if [ "$days" -lt 1 ]; then
    echo "ERROR: Days must be at least 1"
    exit 1
  fi
  cleanup_old_data "$days"
  ;;

"stats")
  show_stats
  ;;

"setup-cron")
  if ! test_api; then
    echo "Cannot setup cron - API is not working"
    exit 1
  fi
  setup_cron "$2"
  ;;

"remove-cron")
  remove_cron
  ;;

"test")
  test_api
  exit $?
  ;;

"help" | "--help" | "-h" | "")
  show_usage
  ;;

*)
  echo "Unknown command: $1"
  echo "Use '$0 help' for usage information"
  exit 1
  ;;
esac

exit 0
