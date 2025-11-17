#ifndef CGROUP_H
#define CGROUP_H

#include <stdint.h>
#include <time.h>
#include <sys/types.h>

#define MAX_CGROUP_PATH 1024
#define MAX_CONTROLLER_NAME 64
#define MAX_CGROUP_NAME 256

/* Cgroup version */
typedef enum {
    CGROUP_V1,
    CGROUP_V2
} cgroup_version_t;

/* CPU controller metrics */
typedef struct {
    uint64_t usage_usec;         /* Total CPU time in microseconds */
    uint64_t user_usec;          /* User CPU time in microseconds */
    uint64_t system_usec;        /* System CPU time in microseconds */
    uint64_t nr_periods;         /* Number of periods */
    uint64_t nr_throttled;       /* Number of throttled periods */
    uint64_t throttled_usec;     /* Total throttled time in microseconds */
    int64_t quota_usec;          /* CPU quota in microseconds (-1 = unlimited) */
    uint64_t period_usec;        /* CPU period in microseconds */
    double cpu_weight;           /* CPU weight (cgroup v2) or shares (v1) */
    struct timespec timestamp;
} cgroup_cpu_t;

/* Memory controller metrics */
typedef struct {
    uint64_t current;            /* Current memory usage in bytes */
    uint64_t peak;               /* Peak memory usage in bytes */
    uint64_t limit;              /* Memory limit in bytes */
    uint64_t soft_limit;         /* Soft memory limit */
    uint64_t swap_current;       /* Current swap usage */
    uint64_t swap_limit;         /* Swap limit */
    uint64_t cache;              /* Page cache memory */
    uint64_t rss;                /* Anonymous memory */
    uint64_t oom_kill_count;     /* Number of OOM kills */
    struct timespec timestamp;
} cgroup_memory_t;

/* Block I/O controller metrics */
typedef struct {
    uint64_t read_bytes;         /* Total bytes read */
    uint64_t write_bytes;        /* Total bytes written */
    uint64_t read_iops;          /* Read IOPS */
    uint64_t write_iops;         /* Write IOPS */
    uint64_t read_bps_limit;     /* Read BPS limit */
    uint64_t write_bps_limit;    /* Write BPS limit */
    struct timespec timestamp;
} cgroup_blkio_t;

/* PIDs controller metrics */
typedef struct {
    uint64_t current;            /* Current number of PIDs */
    uint64_t limit;              /* PID limit */
    struct timespec timestamp;
} cgroup_pids_t;

/* Complete cgroup metrics */
typedef struct {
    char cgroup_path[MAX_CGROUP_PATH];
    cgroup_version_t version;
    cgroup_cpu_t cpu;
    cgroup_memory_t memory;
    cgroup_blkio_t blkio;
    cgroup_pids_t pids;
    int has_cpu;
    int has_memory;
    int has_blkio;
    int has_pids;
} cgroup_metrics_t;

/* Cgroup configuration for creation */
typedef struct {
    char name[MAX_CGROUP_NAME];
    char parent_path[MAX_CGROUP_PATH];

    /* CPU limits */
    int set_cpu_limit;
    double cpu_limit;            /* CPU cores (e.g., 0.5, 1.0, 2.0) */

    /* Memory limits */
    int set_memory_limit;
    uint64_t memory_limit_bytes;

    /* I/O limits */
    int set_io_limit;
    uint64_t read_bps_limit;
    uint64_t write_bps_limit;

    /* PID limits */
    int set_pid_limit;
    uint64_t pid_limit;
} cgroup_config_t;

/* Throttling analysis result */
typedef struct {
    double configured_limit;     /* Configured CPU limit in cores */
    double measured_usage;       /* Measured CPU usage in cores */
    double throttle_percentage;  /* Percentage of time throttled */
    uint64_t throttle_count;     /* Number of throttle events */
    double precision;            /* How close measured is to configured */
    struct timespec duration;    /* Duration of measurement */
} cgroup_throttle_analysis_t;

/* Cgroup manager functions */
int cgroup_init(void);
void cgroup_cleanup(void);

/* Detection and version support */
cgroup_version_t cgroup_detect_version(void);
int cgroup_is_available(void);
const char* cgroup_get_mount_point(void);

/* Metrics collection */
int cgroup_collect_metrics(const char *cgroup_path, cgroup_metrics_t *metrics);
int cgroup_collect_cpu(const char *cgroup_path, cgroup_cpu_t *cpu);
int cgroup_collect_memory(const char *cgroup_path, cgroup_memory_t *memory);
int cgroup_collect_blkio(const char *cgroup_path, cgroup_blkio_t *blkio);
int cgroup_collect_pids(const char *cgroup_path, cgroup_pids_t *pids);

/* Cgroup manipulation */
int cgroup_create(const cgroup_config_t *config, char *created_path, size_t path_len);
int cgroup_delete(const char *cgroup_path);
int cgroup_move_process(pid_t pid, const char *cgroup_path);
int cgroup_get_process_cgroup(pid_t pid, char *cgroup_path, size_t path_len);

/* Limit configuration */
int cgroup_set_cpu_limit(const char *cgroup_path, double cpu_cores);
int cgroup_set_memory_limit(const char *cgroup_path, uint64_t limit_bytes);
int cgroup_set_io_limit(const char *cgroup_path, uint64_t read_bps, uint64_t write_bps);
int cgroup_set_pid_limit(const char *cgroup_path, uint64_t limit);

/* Analysis functions */
int cgroup_analyze_throttling(const char *cgroup_path, double duration_sec,
                              cgroup_throttle_analysis_t *analysis);
int cgroup_analyze_memory_pressure(const char *cgroup_path, int duration_sec,
                                   double *oom_events_per_sec);

/* Utilization reporting */
int cgroup_generate_utilization_report(const char *cgroup_path,
                                       char *report, size_t report_len);
double cgroup_calculate_cpu_utilization(const cgroup_cpu_t *prev,
                                       const cgroup_cpu_t *current);
double cgroup_calculate_memory_utilization(const cgroup_memory_t *memory);

/* Utility functions */
void cgroup_print_metrics(const cgroup_metrics_t *metrics);
void cgroup_print_cpu(const cgroup_cpu_t *cpu);
void cgroup_print_memory(const cgroup_memory_t *memory);
void cgroup_print_blkio(const cgroup_blkio_t *blkio);
void cgroup_print_pids(const cgroup_pids_t *pids);
void cgroup_print_throttle_analysis(const cgroup_throttle_analysis_t *analysis);

/* Export functions */
int cgroup_export_metrics_json(const cgroup_metrics_t *metrics, const char *filename);
int cgroup_export_metrics_csv(const cgroup_metrics_t *metrics, const char *filename, int append);

#endif /* CGROUP_H */
