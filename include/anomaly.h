#ifndef ANOMALY_H
#define ANOMALY_H

#include <stdint.h>
#include <time.h>

#define MAX_SAMPLES 100
#define ANOMALY_THRESHOLD_SIGMA 2.0  /* 2 standard deviations */

/* Anomaly types */
typedef enum {
    ANOMALY_NONE = 0,
    ANOMALY_CPU_SPIKE,
    ANOMALY_MEMORY_SPIKE,
    ANOMALY_IO_SPIKE,
    ANOMALY_CPU_DROP,
    ANOMALY_MEMORY_LEAK,
    ANOMALY_IO_STALL
} anomaly_type_t;

/* Anomaly severity */
typedef enum {
    SEVERITY_LOW = 1,
    SEVERITY_MEDIUM = 2,
    SEVERITY_HIGH = 3,
    SEVERITY_CRITICAL = 4
} anomaly_severity_t;

/* Statistical metrics for a metric stream */
typedef struct {
    double samples[MAX_SAMPLES];
    int count;
    int index;
    double mean;
    double stddev;
    double min;
    double max;
    time_t first_sample_time;
    time_t last_sample_time;
} metric_stats_t;

/* Anomaly detector for a single process */
typedef struct {
    pid_t pid;
    metric_stats_t cpu_stats;
    metric_stats_t memory_stats;
    metric_stats_t io_read_stats;
    metric_stats_t io_write_stats;
    int initialized;
} anomaly_detector_t;

/* Detected anomaly */
typedef struct {
    anomaly_type_t type;
    anomaly_severity_t severity;
    double value;
    double expected_mean;
    double deviation_sigma;
    time_t detected_at;
    char description[256];
} anomaly_event_t;

/* Function declarations */

/**
 * Initialize anomaly detector for a process
 */
int anomaly_detector_init(anomaly_detector_t *detector, pid_t pid);

/**
 * Update detector with new metric values
 */
void anomaly_detector_update_cpu(anomaly_detector_t *detector, double cpu_percent);
void anomaly_detector_update_memory(anomaly_detector_t *detector, double memory_kb);
void anomaly_detector_update_io(anomaly_detector_t *detector, double read_rate, double write_rate);

/**
 * Check for anomalies and return detected events
 * Returns number of anomalies detected (0 if none)
 */
int anomaly_detector_check(anomaly_detector_t *detector, anomaly_event_t *events, int max_events);

/**
 * Print anomaly event
 */
void anomaly_print_event(const anomaly_event_t *event);

/**
 * Export anomaly events to CSV
 */
int anomaly_export_csv(const anomaly_event_t *events, int count, const char *filename, int append);

/**
 * Get statistics summary
 */
void anomaly_print_stats(const anomaly_detector_t *detector);

/**
 * Reset detector statistics
 */
void anomaly_detector_reset(anomaly_detector_t *detector);

/**
 * Cleanup detector
 */
void anomaly_detector_cleanup(anomaly_detector_t *detector);

#endif /* ANOMALY_H */
