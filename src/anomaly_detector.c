#include "../include/anomaly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Helper function to update statistics */
static void update_stats(metric_stats_t *stats, double value) {
    if (stats->count == 0) {
        stats->first_sample_time = time(NULL);
        stats->min = value;
        stats->max = value;
    }

    /* Add sample to circular buffer */
    stats->samples[stats->index] = value;
    stats->index = (stats->index + 1) % MAX_SAMPLES;
    if (stats->count < MAX_SAMPLES) {
        stats->count++;
    }
    stats->last_sample_time = time(NULL);

    /* Update min/max */
    if (value < stats->min) stats->min = value;
    if (value > stats->max) stats->max = value;

    /* Calculate mean */
    double sum = 0.0;
    for (int i = 0; i < stats->count; i++) {
        sum += stats->samples[i];
    }
    stats->mean = sum / stats->count;

    /* Calculate standard deviation */
    double variance = 0.0;
    for (int i = 0; i < stats->count; i++) {
        double diff = stats->samples[i] - stats->mean;
        variance += diff * diff;
    }
    stats->stddev = sqrt(variance / stats->count);
}

/* Helper function to check if value is anomalous */
static int is_anomaly(const metric_stats_t *stats, double value, double *sigma_out) {
    if (stats->count < 10) {
        /* Need at least 10 samples for statistical significance */
        return 0;
    }

    if (stats->stddev < 0.001) {
        /* Nearly constant values - check for sudden change */
        if (fabs(value - stats->mean) > stats->mean * 0.5) {
            *sigma_out = 10.0; /* Arbitrary large value */
            return 1;
        }
        return 0;
    }

    double deviation = fabs(value - stats->mean);
    double sigma = deviation / stats->stddev;
    *sigma_out = sigma;

    return sigma > ANOMALY_THRESHOLD_SIGMA;
}

int anomaly_detector_init(anomaly_detector_t *detector, pid_t pid) {
    if (!detector) {
        return -1;
    }

    memset(detector, 0, sizeof(anomaly_detector_t));
    detector->pid = pid;
    detector->initialized = 1;

    return 0;
}

void anomaly_detector_update_cpu(anomaly_detector_t *detector, double cpu_percent) {
    if (!detector || !detector->initialized) {
        return;
    }

    update_stats(&detector->cpu_stats, cpu_percent);
}

void anomaly_detector_update_memory(anomaly_detector_t *detector, double memory_kb) {
    if (!detector || !detector->initialized) {
        return;
    }

    update_stats(&detector->memory_stats, memory_kb);
}

void anomaly_detector_update_io(anomaly_detector_t *detector, double read_rate, double write_rate) {
    if (!detector || !detector->initialized) {
        return;
    }

    update_stats(&detector->io_read_stats, read_rate);
    update_stats(&detector->io_write_stats, write_rate);
}

int anomaly_detector_check(anomaly_detector_t *detector, anomaly_event_t *events, int max_events) {
    if (!detector || !detector->initialized || !events || max_events <= 0) {
        return 0;
    }

    int event_count = 0;
    double sigma;

    /* Check CPU anomalies */
    if (detector->cpu_stats.count > 0) {
        double current_cpu = detector->cpu_stats.samples[(detector->cpu_stats.index - 1 + MAX_SAMPLES) % MAX_SAMPLES];

        if (is_anomaly(&detector->cpu_stats, current_cpu, &sigma)) {
            if (event_count < max_events) {
                anomaly_event_t *evt = &events[event_count++];
                evt->value = current_cpu;
                evt->expected_mean = detector->cpu_stats.mean;
                evt->deviation_sigma = sigma;
                evt->detected_at = time(NULL);

                if (current_cpu > detector->cpu_stats.mean) {
                    evt->type = ANOMALY_CPU_SPIKE;
                    snprintf(evt->description, sizeof(evt->description),
                            "CPU spike detected: %.2f%% (expected %.2f%%, %.1fσ deviation)",
                            current_cpu, detector->cpu_stats.mean, sigma);
                } else {
                    evt->type = ANOMALY_CPU_DROP;
                    snprintf(evt->description, sizeof(evt->description),
                            "CPU drop detected: %.2f%% (expected %.2f%%, %.1fσ deviation)",
                            current_cpu, detector->cpu_stats.mean, sigma);
                }

                /* Determine severity */
                if (sigma > 4.0) {
                    evt->severity = SEVERITY_CRITICAL;
                } else if (sigma > 3.0) {
                    evt->severity = SEVERITY_HIGH;
                } else if (sigma > 2.5) {
                    evt->severity = SEVERITY_MEDIUM;
                } else {
                    evt->severity = SEVERITY_LOW;
                }
            }
        }
    }

    /* Check memory anomalies */
    if (detector->memory_stats.count > 0 && event_count < max_events) {
        double current_mem = detector->memory_stats.samples[(detector->memory_stats.index - 1 + MAX_SAMPLES) % MAX_SAMPLES];

        if (is_anomaly(&detector->memory_stats, current_mem, &sigma)) {
            anomaly_event_t *evt = &events[event_count++];
            evt->value = current_mem;
            evt->expected_mean = detector->memory_stats.mean;
            evt->deviation_sigma = sigma;
            evt->detected_at = time(NULL);

            if (current_mem > detector->memory_stats.mean) {
                evt->type = ANOMALY_MEMORY_SPIKE;
                snprintf(evt->description, sizeof(evt->description),
                        "Memory spike detected: %.0f KB (expected %.0f KB, %.1fσ deviation)",
                        current_mem, detector->memory_stats.mean, sigma);
            } else {
                /* Unusual drop in memory - could indicate normal operation */
                evt->type = ANOMALY_MEMORY_SPIKE;
                snprintf(evt->description, sizeof(evt->description),
                        "Memory drop detected: %.0f KB (expected %.0f KB, %.1fσ deviation)",
                        current_mem, detector->memory_stats.mean, sigma);
            }

            if (sigma > 4.0) {
                evt->severity = SEVERITY_CRITICAL;
            } else if (sigma > 3.0) {
                evt->severity = SEVERITY_HIGH;
            } else if (sigma > 2.5) {
                evt->severity = SEVERITY_MEDIUM;
            } else {
                evt->severity = SEVERITY_LOW;
            }
        }

        /* Detect memory leak (steady increase over time) */
        if (detector->memory_stats.count >= 20 && event_count < max_events) {
            int increasing_count = 0;
            for (int i = 1; i < detector->memory_stats.count; i++) {
                int curr_idx = (detector->memory_stats.index - i + MAX_SAMPLES) % MAX_SAMPLES;
                int prev_idx = (detector->memory_stats.index - i - 1 + MAX_SAMPLES) % MAX_SAMPLES;
                if (detector->memory_stats.samples[curr_idx] > detector->memory_stats.samples[prev_idx]) {
                    increasing_count++;
                }
            }

            /* If memory increased in >80% of samples, likely a leak */
            if (increasing_count > detector->memory_stats.count * 0.8) {
                double increase_rate = (current_mem - detector->memory_stats.min) /
                                      (detector->memory_stats.last_sample_time - detector->memory_stats.first_sample_time);

                if (increase_rate > 10.0) { /* > 10 KB/s increase */
                    anomaly_event_t *evt = &events[event_count++];
                    evt->type = ANOMALY_MEMORY_LEAK;
                    evt->value = current_mem;
                    evt->expected_mean = detector->memory_stats.min;
                    evt->deviation_sigma = (current_mem - detector->memory_stats.min) / detector->memory_stats.stddev;
                    evt->detected_at = time(NULL);

                    snprintf(evt->description, sizeof(evt->description),
                            "Potential memory leak: growing from %.0f KB to %.0f KB (rate: %.1f KB/s)",
                            detector->memory_stats.min, current_mem, increase_rate);

                    if (increase_rate > 100.0) {
                        evt->severity = SEVERITY_CRITICAL;
                    } else if (increase_rate > 50.0) {
                        evt->severity = SEVERITY_HIGH;
                    } else if (increase_rate > 20.0) {
                        evt->severity = SEVERITY_MEDIUM;
                    } else {
                        evt->severity = SEVERITY_LOW;
                    }
                }
            }
        }
    }

    /* Check I/O anomalies */
    if (detector->io_write_stats.count > 0 && event_count < max_events) {
        double current_write = detector->io_write_stats.samples[(detector->io_write_stats.index - 1 + MAX_SAMPLES) % MAX_SAMPLES];

        if (is_anomaly(&detector->io_write_stats, current_write, &sigma)) {
            anomaly_event_t *evt = &events[event_count++];
            evt->value = current_write;
            evt->expected_mean = detector->io_write_stats.mean;
            evt->deviation_sigma = sigma;
            evt->detected_at = time(NULL);

            if (current_write > detector->io_write_stats.mean) {
                evt->type = ANOMALY_IO_SPIKE;
                snprintf(evt->description, sizeof(evt->description),
                        "I/O write spike detected: %.2f KB/s (expected %.2f KB/s, %.1fσ deviation)",
                        current_write, detector->io_write_stats.mean, sigma);
            } else {
                evt->type = ANOMALY_IO_STALL;
                snprintf(evt->description, sizeof(evt->description),
                        "I/O write stall detected: %.2f KB/s (expected %.2f KB/s, %.1fσ deviation)",
                        current_write, detector->io_write_stats.mean, sigma);
            }

            if (sigma > 4.0) {
                evt->severity = SEVERITY_CRITICAL;
            } else if (sigma > 3.0) {
                evt->severity = SEVERITY_HIGH;
            } else if (sigma > 2.5) {
                evt->severity = SEVERITY_MEDIUM;
            } else {
                evt->severity = SEVERITY_LOW;
            }
        }
    }

    return event_count;
}

void anomaly_print_event(const anomaly_event_t *event) {
    if (!event) {
        return;
    }

    const char *severity_str[] = {"UNKNOWN", "LOW", "MEDIUM", "HIGH", "CRITICAL"};
    const char *severity_color[] = {"\033[0m", "\033[32m", "\033[33m", "\033[31m", "\033[1;31m"};

    char time_str[64];
    struct tm *tm_info = localtime(&event->detected_at);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("%s[%s] %s%s\033[0m\n",
           severity_color[event->severity],
           severity_str[event->severity],
           event->description,
           "");
    printf("         Time: %s | Value: %.2f | Expected: %.2f | Deviation: %.1fσ\n",
           time_str, event->value, event->expected_mean, event->deviation_sigma);
}

int anomaly_export_csv(const anomaly_event_t *events, int count, const char *filename, int append) {
    if (!events || !filename || count <= 0) {
        return -1;
    }

    FILE *fp = fopen(filename, append ? "a" : "w");
    if (!fp) {
        return -1;
    }

    /* Write header if not appending */
    if (!append) {
        fprintf(fp, "timestamp,type,severity,value,expected,deviation_sigma,description\n");
    }

    for (int i = 0; i < count; i++) {
        const anomaly_event_t *evt = &events[i];
        char time_str[64];
        struct tm *tm_info = localtime(&evt->detected_at);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

        fprintf(fp, "%s,%d,%d,%.2f,%.2f,%.2f,\"%s\"\n",
                time_str,
                evt->type,
                evt->severity,
                evt->value,
                evt->expected_mean,
                evt->deviation_sigma,
                evt->description);
    }

    fclose(fp);
    return 0;
}

void anomaly_print_stats(const anomaly_detector_t *detector) {
    if (!detector || !detector->initialized) {
        return;
    }

    printf("\n=== Anomaly Detector Statistics for PID %d ===\n", detector->pid);

    if (detector->cpu_stats.count > 0) {
        printf("\nCPU Statistics:\n");
        printf("  Samples: %d\n", detector->cpu_stats.count);
        printf("  Mean: %.2f%%\n", detector->cpu_stats.mean);
        printf("  StdDev: %.2f%%\n", detector->cpu_stats.stddev);
        printf("  Min: %.2f%% | Max: %.2f%%\n", detector->cpu_stats.min, detector->cpu_stats.max);
    }

    if (detector->memory_stats.count > 0) {
        printf("\nMemory Statistics:\n");
        printf("  Samples: %d\n", detector->memory_stats.count);
        printf("  Mean: %.0f KB\n", detector->memory_stats.mean);
        printf("  StdDev: %.0f KB\n", detector->memory_stats.stddev);
        printf("  Min: %.0f KB | Max: %.0f KB\n", detector->memory_stats.min, detector->memory_stats.max);
    }

    if (detector->io_write_stats.count > 0) {
        printf("\nI/O Write Statistics:\n");
        printf("  Samples: %d\n", detector->io_write_stats.count);
        printf("  Mean: %.2f KB/s\n", detector->io_write_stats.mean);
        printf("  StdDev: %.2f KB/s\n", detector->io_write_stats.stddev);
        printf("  Min: %.2f KB/s | Max: %.2f KB/s\n",
               detector->io_write_stats.min, detector->io_write_stats.max);
    }
}

void anomaly_detector_reset(anomaly_detector_t *detector) {
    if (!detector) {
        return;
    }

    pid_t saved_pid = detector->pid;
    memset(detector, 0, sizeof(anomaly_detector_t));
    detector->pid = saved_pid;
    detector->initialized = 1;
}

void anomaly_detector_cleanup(anomaly_detector_t *detector) {
    if (!detector) {
        return;
    }

    memset(detector, 0, sizeof(anomaly_detector_t));
}
