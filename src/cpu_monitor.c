#include "../include/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static long clock_ticks_per_sec = 0;

int cpu_monitor_init(void) {
    clock_ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (clock_ticks_per_sec <= 0) {
        fprintf(stderr, "Failed to get clock ticks per second\n");
        return -1;
    }
    return 0;
}

void cpu_monitor_cleanup(void) {
    /* Nothing to cleanup */
}

int process_exists(pid_t pid) {
    char stat_path[256];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d", pid);
    return (access(stat_path, F_OK) == 0) ? 1 : 0;
}

int get_system_clock_ticks(void) {
    return (int)clock_ticks_per_sec;
}

int cpu_monitor_collect(pid_t pid, cpu_metrics_t *metrics) {
    if (!metrics) {
        fprintf(stderr, "NULL metrics pointer\n");
        return -1;
    }

    if (!process_exists(pid)) {
        fprintf(stderr, "Process %d does not exist\n", pid);
        return -1;
    }

    /* Read /proc/[pid]/stat */
    char stat_path[256];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);

    FILE *fp = fopen(stat_path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", stat_path, strerror(errno));
        return -1;
    }

    /* Parse stat file - format: pid (comm) state ppid ... */
    int items = fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "
                          "%lu %lu %lu %lu %*d %*d %ld",
                          &metrics->utime, &metrics->stime,
                          &metrics->cutime, &metrics->cstime,
                          &metrics->num_threads);
    fclose(fp);

    if (items != 5) {
        fprintf(stderr, "Failed to parse %s\n", stat_path);
        return -1;
    }

    metrics->pid = pid;

    /* Read /proc/[pid]/status for context switches */
    char status_path[256];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);

    fp = fopen(status_path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", status_path, strerror(errno));
        return -1;
    }

    char line[256];
    metrics->voluntary_ctxt_switches = 0;
    metrics->nonvoluntary_ctxt_switches = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "voluntary_ctxt_switches: %lu",
                   &metrics->voluntary_ctxt_switches) == 1) {
            continue;
        }
        if (sscanf(line, "nonvoluntary_ctxt_switches: %lu",
                   &metrics->nonvoluntary_ctxt_switches) == 1) {
            continue;
        }
    }
    fclose(fp);

    /* Get timestamp */
    clock_gettime(CLOCK_MONOTONIC, &metrics->timestamp);
    metrics->cpu_percent = 0.0;

    return 0;
}

int cpu_monitor_calculate_percentage(const cpu_metrics_t *prev,
                                     const cpu_metrics_t *curr,
                                     cpu_metrics_t *result) {
    if (!prev || !curr || !result) {
        fprintf(stderr, "NULL pointer in calculate_percentage\n");
        return -1;
    }

    /* Copy current metrics */
    *result = *curr;

    /* Calculate time delta in seconds */
    double elapsed = (curr->timestamp.tv_sec - prev->timestamp.tv_sec) +
                    (curr->timestamp.tv_nsec - prev->timestamp.tv_nsec) / 1000000000.0;

    if (elapsed <= 0.0) {
        fprintf(stderr, "Invalid time delta: %f\n", elapsed);
        return -1;
    }

    /* Calculate CPU time delta in clock ticks */
    uint64_t total_ticks = (curr->utime + curr->stime) - (prev->utime + prev->stime);

    /* Convert to seconds and calculate percentage */
    double cpu_time = (double)total_ticks / clock_ticks_per_sec;
    result->cpu_percent = (cpu_time / elapsed) * 100.0;

    return 0;
}

void print_cpu_metrics(const cpu_metrics_t *metrics) {
    if (!metrics) {
        return;
    }

    printf("=== CPU Metrics for PID %d ===\n", metrics->pid);
    printf("User time:     %lu ticks\n", metrics->utime);
    printf("System time:   %lu ticks\n", metrics->stime);
    printf("Threads:       %ld\n", metrics->num_threads);
    printf("Voluntary context switches:    %lu\n", metrics->voluntary_ctxt_switches);
    printf("Nonvoluntary context switches: %lu\n", metrics->nonvoluntary_ctxt_switches);
    printf("CPU usage:     %.2f%%\n", metrics->cpu_percent);
    printf("\n");
}

int export_cpu_metrics_json(const cpu_metrics_t *metrics, const char *filename) {
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
    fprintf(fp, "  \"utime\": %lu,\n", metrics->utime);
    fprintf(fp, "  \"stime\": %lu,\n", metrics->stime);
    fprintf(fp, "  \"cutime\": %lu,\n", metrics->cutime);
    fprintf(fp, "  \"cstime\": %lu,\n", metrics->cstime);
    fprintf(fp, "  \"num_threads\": %ld,\n", metrics->num_threads);
    fprintf(fp, "  \"voluntary_ctxt_switches\": %lu,\n", metrics->voluntary_ctxt_switches);
    fprintf(fp, "  \"nonvoluntary_ctxt_switches\": %lu,\n", metrics->nonvoluntary_ctxt_switches);
    fprintf(fp, "  \"cpu_percent\": %.2f,\n", metrics->cpu_percent);
    fprintf(fp, "  \"timestamp\": %ld.%09ld\n", metrics->timestamp.tv_sec, metrics->timestamp.tv_nsec);
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

int export_cpu_metrics_csv(const cpu_metrics_t *metrics, const char *filename, int append) {
    if (!metrics || !filename) {
        return -1;
    }

    FILE *fp = fopen(filename, append ? "a" : "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
        return -1;
    }

    /* Write header if new file */
    if (!append) {
        fprintf(fp, "timestamp,pid,utime,stime,cutime,cstime,num_threads,");
        fprintf(fp, "voluntary_ctxt_switches,nonvoluntary_ctxt_switches,cpu_percent\n");
    }

    fprintf(fp, "%ld.%09ld,%d,%lu,%lu,%lu,%lu,%ld,%lu,%lu,%.2f\n",
            metrics->timestamp.tv_sec, metrics->timestamp.tv_nsec,
            metrics->pid, metrics->utime, metrics->stime,
            metrics->cutime, metrics->cstime, metrics->num_threads,
            metrics->voluntary_ctxt_switches, metrics->nonvoluntary_ctxt_switches,
            metrics->cpu_percent);

    fclose(fp);
    return 0;
}
