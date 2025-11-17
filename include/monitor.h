#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <time.h>
#include <sys/types.h>

/* CPU metrics structure */
typedef struct {
    pid_t pid;
    uint64_t utime;                      /* User mode time in clock ticks */
    uint64_t stime;                      /* Kernel mode time in clock ticks */
    uint64_t cutime;                     /* Children user mode time */
    uint64_t cstime;                     /* Children kernel mode time */
    long num_threads;                    /* Number of threads */
    uint64_t voluntary_ctxt_switches;    /* Voluntary context switches */
    uint64_t nonvoluntary_ctxt_switches; /* Involuntary context switches */
    double cpu_percent;                  /* CPU usage percentage */
    struct timespec timestamp;           /* Collection timestamp */
} cpu_metrics_t;

/* Memory metrics structure */
typedef struct {
    pid_t pid;
    uint64_t rss;          /* Resident Set Size in KB */
    uint64_t vsz;          /* Virtual Size in KB */
    uint64_t swap;         /* Swap usage in KB */
    uint64_t major_faults; /* Major page faults */
    uint64_t minor_faults; /* Minor page faults */
    struct timespec timestamp;
} memory_metrics_t;

/* I/O metrics structure */
typedef struct {
    pid_t pid;
    uint64_t rchar;        /* Characters read */
    uint64_t wchar;        /* Characters written */
    uint64_t syscr;        /* Read syscalls */
    uint64_t syscw;        /* Write syscalls */
    uint64_t read_bytes;   /* Bytes actually read from storage */
    uint64_t write_bytes;  /* Bytes actually written to storage */
    double read_rate;      /* Read rate in bytes/sec */
    double write_rate;     /* Write rate in bytes/sec */
    struct timespec timestamp;
} io_metrics_t;

/* Network metrics structure */
typedef struct {
    char interface[32];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_errors;
    uint64_t tx_errors;
    struct timespec timestamp;
} network_metrics_t;

/* CPU monitor functions */
int cpu_monitor_init(void);
void cpu_monitor_cleanup(void);
int cpu_monitor_collect(pid_t pid, cpu_metrics_t *metrics);
int cpu_monitor_calculate_percentage(const cpu_metrics_t *prev,
                                     const cpu_metrics_t *curr,
                                     cpu_metrics_t *result);
int process_exists(pid_t pid);
int get_system_clock_ticks(void);

/* Memory monitor functions */
int memory_monitor_init(void);
void memory_monitor_cleanup(void);
int memory_monitor_collect(pid_t pid, memory_metrics_t *metrics);

/* I/O monitor functions */
int io_monitor_init(void);
void io_monitor_cleanup(void);
int io_monitor_collect(pid_t pid, io_metrics_t *metrics);
int io_monitor_calculate_rates(const io_metrics_t *prev,
                               const io_metrics_t *curr,
                               io_metrics_t *result);

/* Network monitor functions */
int network_monitor_init(void);
void network_monitor_cleanup(void);
int network_monitor_collect(const char *interface, network_metrics_t *metrics);
int network_monitor_list_interfaces(char interfaces[][32], int max_interfaces, int *count);

/* Export functions */
int export_cpu_metrics_json(const cpu_metrics_t *metrics, const char *filename);
int export_cpu_metrics_csv(const cpu_metrics_t *metrics, const char *filename, int append);
int export_memory_metrics_json(const memory_metrics_t *metrics, const char *filename);
int export_memory_metrics_csv(const memory_metrics_t *metrics, const char *filename, int append);
int export_io_metrics_json(const io_metrics_t *metrics, const char *filename);
int export_io_metrics_csv(const io_metrics_t *metrics, const char *filename, int append);
int export_network_metrics_json(const network_metrics_t *metrics, const char *filename);
int export_network_metrics_csv(const network_metrics_t *metrics, const char *filename, int append);

/* Print functions */
void print_cpu_metrics(const cpu_metrics_t *metrics);
void print_memory_metrics(const memory_metrics_t *metrics);
void print_io_metrics(const io_metrics_t *metrics);
void print_network_metrics(const network_metrics_t *metrics);

#endif /* MONITOR_H */
