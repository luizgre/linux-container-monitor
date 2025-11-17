#include "../include/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

int io_monitor_init(void) {
    /* Nothing to initialize currently */
    return 0;
}

void io_monitor_cleanup(void) {
    /* Nothing to clean up currently */
}

int io_monitor_collect(pid_t pid, io_metrics_t *metrics) {
    if (!metrics) {
        fprintf(stderr, "NULL metrics pointer\n");
        return -1;
    }

    if (!process_exists(pid)) {
        fprintf(stderr, "Process %d does not exist\n", pid);
        return -1;
    }

    /* Initialize all fields */
    memset(metrics, 0, sizeof(io_metrics_t));
    metrics->pid = pid;

    /* Read /proc/[pid]/io for I/O metrics */
    char io_path[256];
    snprintf(io_path, sizeof(io_path), "/proc/%d/io", pid);

    FILE *fp = fopen(io_path, "r");
    if (!fp) {
        /* Process may not have permission or /proc/[pid]/io may not exist */
        if (errno == EACCES) {
            fprintf(stderr, "Permission denied reading %s (try running with sudo)\n", io_path);
        } else {
            fprintf(stderr, "Failed to open %s: %s\n", io_path, strerror(errno));
        }
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "rchar:", 6) == 0) {
            sscanf(line + 6, "%lu", &metrics->rchar);
        } else if (strncmp(line, "wchar:", 6) == 0) {
            sscanf(line + 6, "%lu", &metrics->wchar);
        } else if (strncmp(line, "syscr:", 6) == 0) {
            sscanf(line + 6, "%lu", &metrics->syscr);
        } else if (strncmp(line, "syscw:", 6) == 0) {
            sscanf(line + 6, "%lu", &metrics->syscw);
        } else if (strncmp(line, "read_bytes:", 11) == 0) {
            sscanf(line + 11, "%lu", &metrics->read_bytes);
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            sscanf(line + 12, "%lu", &metrics->write_bytes);
        } else if (strncmp(line, "cancelled_write_bytes:", 22) == 0) {
            sscanf(line + 22, "%lu", &metrics->cancelled_write_bytes);
        }
    }
    fclose(fp);

    clock_gettime(CLOCK_MONOTONIC, &metrics->timestamp);

    return 0;
}

int io_monitor_calculate_rates(const io_metrics_t *prev,
                               const io_metrics_t *current,
                               io_metrics_t *result) {
    if (!prev || !current || !result) {
        fprintf(stderr, "NULL pointer in calculate_rates\n");
        return -1;
    }

    /* Copy current metrics to result */
    memcpy(result, current, sizeof(io_metrics_t));

    /* Calculate time difference in seconds */
    double time_diff = (current->timestamp.tv_sec - prev->timestamp.tv_sec) +
                      (current->timestamp.tv_nsec - prev->timestamp.tv_nsec) / 1e9;

    if (time_diff <= 0) {
        result->read_rate = 0.0;
        result->write_rate = 0.0;
        return 0;
    }

    /* Calculate rates in bytes per second */
    uint64_t read_diff = current->read_bytes - prev->read_bytes;
    uint64_t write_diff = current->write_bytes - prev->write_bytes;

    result->read_rate = (double)read_diff / time_diff;
    result->write_rate = (double)write_diff / time_diff;

    return 0;
}

void print_io_metrics(const io_metrics_t *metrics) {
    if (!metrics) {
        return;
    }

    printf("\n=== I/O Metrics for PID %d ===\n", metrics->pid);
    printf("Characters read:           %lu bytes\n", metrics->rchar);
    printf("Characters written:        %lu bytes\n", metrics->wchar);
    printf("Read syscalls:             %lu\n", metrics->syscr);
    printf("Write syscalls:            %lu\n", metrics->syscw);
    printf("Actual bytes read:         %lu bytes\n", metrics->read_bytes);
    printf("Actual bytes written:      %lu bytes\n", metrics->write_bytes);
    printf("Cancelled write bytes:     %lu bytes\n", metrics->cancelled_write_bytes);
    printf("Read rate:                 %.2f bytes/sec\n", metrics->read_rate);
    printf("Write rate:                %.2f bytes/sec\n", metrics->write_rate);
    printf("================================\n\n");
}

int export_io_metrics_json(const io_metrics_t *metrics, const char *filename) {
    if (!metrics || !filename) {
        return -1;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
        return -1;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"pid\": %d,\n", metrics->pid);
    fprintf(fp, "  \"rchar\": %lu,\n", metrics->rchar);
    fprintf(fp, "  \"wchar\": %lu,\n", metrics->wchar);
    fprintf(fp, "  \"syscr\": %lu,\n", metrics->syscr);
    fprintf(fp, "  \"syscw\": %lu,\n", metrics->syscw);
    fprintf(fp, "  \"read_bytes\": %lu,\n", metrics->read_bytes);
    fprintf(fp, "  \"write_bytes\": %lu,\n", metrics->write_bytes);
    fprintf(fp, "  \"cancelled_write_bytes\": %lu,\n", metrics->cancelled_write_bytes);
    fprintf(fp, "  \"read_rate\": %.2f,\n", metrics->read_rate);
    fprintf(fp, "  \"write_rate\": %.2f,\n", metrics->write_rate);
    fprintf(fp, "  \"timestamp\": %ld.%09ld\n", metrics->timestamp.tv_sec, metrics->timestamp.tv_nsec);
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

int export_io_metrics_csv(const io_metrics_t *metrics, const char *filename, int append) {
    if (!metrics || !filename) {
        return -1;
    }

    const char *mode = append ? "a" : "w";
    FILE *fp = fopen(filename, mode);
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
        return -1;
    }

    /* Write header if not appending */
    if (!append) {
        fprintf(fp, "pid,rchar,wchar,syscr,syscw,read_bytes,write_bytes,"
                   "cancelled_write_bytes,read_rate,write_rate,timestamp\n");
    }

    fprintf(fp, "%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%.2f,%.2f,%ld.%09ld\n",
            metrics->pid, metrics->rchar, metrics->wchar,
            metrics->syscr, metrics->syscw, metrics->read_bytes,
            metrics->write_bytes, metrics->cancelled_write_bytes,
            metrics->read_rate, metrics->write_rate,
            metrics->timestamp.tv_sec, metrics->timestamp.tv_nsec);

    fclose(fp);
    return 0;
}

int network_monitor_init(void) {
    /* Network monitoring implementation - placeholder */
    return 0;
}

void network_monitor_cleanup(void) {
    /* Network monitoring cleanup - placeholder */
}

int network_monitor_collect(pid_t pid, network_metrics_t *metrics) {
    if (!metrics) {
        fprintf(stderr, "NULL metrics pointer\n");
        return -1;
    }

    if (!process_exists(pid)) {
        fprintf(stderr, "Process %d does not exist\n", pid);
        return -1;
    }

    /* Initialize all fields */
    memset(metrics, 0, sizeof(network_metrics_t));
    metrics->pid = pid;

    /* Network stats are complex and require parsing /proc/net files */
    /* This is a simplified implementation */

    clock_gettime(CLOCK_MONOTONIC, &metrics->timestamp);

    return 0;
}

void print_network_metrics(const network_metrics_t *metrics) {
    if (!metrics) {
        return;
    }

    printf("\n=== Network Metrics for PID %d ===\n", metrics->pid);
    printf("RX bytes:                  %lu\n", metrics->rx_bytes);
    printf("TX bytes:                  %lu\n", metrics->tx_bytes);
    printf("RX packets:                %lu\n", metrics->rx_packets);
    printf("TX packets:                %lu\n", metrics->tx_packets);
    printf("TCP connections:           %d\n", metrics->tcp_connections);
    printf("UDP connections:           %d\n", metrics->udp_connections);
    printf("===================================\n\n");
}
