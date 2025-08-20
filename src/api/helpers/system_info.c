#include "system_info.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *get_system_uptime(void) {
  FILE *fp = fopen("/proc/uptime", "r");
  static char uptime[64];
  if (fp) {
    fscanf(fp, "%s", uptime);
    fclose(fp);
    return uptime;
  }
  return "unknown";
}

char *get_system_load(void) {
  FILE *fp = fopen("/proc/loadavg", "r");
  static char load[128];
  if (fp) {
    fgets(load, sizeof(load), fp);
    fclose(fp);
    trim_whitespace(load);
    return load;
  }
  return "unknown";
}

char *get_memory_info(void) {
  FILE *fp = fopen("/proc/meminfo", "r");
  static char mem_info[512];
  char line[128];
  int total = 0, free = 0, available = 0;

  if (fp) {
    while (fgets(line, sizeof(line), fp)) {
      if (sscanf(line, "MemTotal: %d kB", &total))
        continue;
      if (sscanf(line, "MemFree: %d kB", &free))
        continue;
      if (sscanf(line, "MemAvailable: %d kB", &available))
        break;
    }
    fclose(fp);
    snprintf(mem_info, sizeof(mem_info),
             "Total: %d kB, Free: %d kB, Available: %d kB", total, free,
             available);
    return mem_info;
  }
  return "unknown";
}

char *get_cpu_info(void) {
  FILE *fp = fopen("/proc/cpuinfo", "r");
  static char cpu_info[256];
  char line[128];

  if (fp) {
    while (fgets(line, sizeof(line), fp)) {
      if (strstr(line, "model name")) {
        char *colon = strchr(line, ':');
        if (colon) {
          strncpy(cpu_info, colon + 2, sizeof(cpu_info) - 1);
          trim_whitespace(cpu_info);
          fclose(fp);
          return cpu_info;
        }
      }
    }
    fclose(fp);
  }
  return "unknown";
}

char *get_kernel_version(void) { return run_command("uname -r"); }

char *get_hostname(void) {
  static char hostname[64];
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    return hostname;
  }
  return "unknown";
}

char *run_command(const char *command) {
  FILE *fp = popen(command, "r");
  static char result[512];

  if (fp) {
    fgets(result, sizeof(result), fp);
    pclose(fp);
    trim_whitespace(result);
    return result;
  }
  return "command failed";
}

char *get_openwrt_version(void) {
  if (file_exists("/etc/openwrt_release")) {
    return run_command("grep DISTRIB_DESCRIPTION /etc/openwrt_release | cut "
                       "-d'=' -f2 | tr -d '\"'");
  }
  return "unknown";
}

char *get_uci_config(const char *config_name) {
  char command[128];
  snprintf(command, sizeof(command), "uci show %s 2>/dev/null", config_name);
  return run_command(command);
}

int file_exists(const char *filename) {
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

char *read_file(const char *filename) {
  FILE *fp = fopen(filename, "r");
  static char content[1024];

  if (fp) {
    fread(content, 1, sizeof(content) - 1, fp);
    fclose(fp);
    content[sizeof(content) - 1] = '\0';
    return content;
  }
  return "file not found";
}

void trim_whitespace(char *str) {
  char *end;

  // Trim leading space
  while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
    str++;

  if (*str == 0)
    return;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str &&
         (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
    end--;

  end[1] = '\0';
}
