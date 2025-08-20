#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

// System information functions
char *get_system_uptime(void);
char *get_system_load(void);
char *get_memory_info(void);
char *get_cpu_info(void);
char *get_disk_usage(void);
char *get_kernel_version(void);
char *get_hostname(void);
char *run_command(const char *command);

// Network information
char *get_network_interfaces(void);
char *get_routing_table(void);
char *get_wireless_info(void);

// OpenWrt specific
char *get_openwrt_version(void);
char *get_installed_packages(void);
char *get_uci_config(const char *config_name);

// Utility functions
int file_exists(const char *filename);
char *read_file(const char *filename);
void trim_whitespace(char *str);

#endif // SYSTEM_INFO_H
