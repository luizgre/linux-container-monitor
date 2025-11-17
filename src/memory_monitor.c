#include "../include/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int memory_monitor_init(void) {
    /* Nothing to initialize currently */
    return 0;
}

void memory_monitor_cleanup(void) {
    /* Nothing to clean up currently */
}

int memory_monitor_collect(pid_t pid, memory_metrics_t *metrics) {
    if (!metrics) {
        fprintf(stderr, "NULL metrics pointer\n");
        return -1;
    }

    if (!process_exists(pid)) {
        fprintf(stderr, "Process %d does not exist\n", pid);
        return -1;
    }

    /* Initialize all fields */
    memset(metrics, 0, sizeof(memory_metrics_t));
    metrics->pid = pid;

    /* Read /proc/[pid]/status for memory metrics */
    char status_path[256];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);

    FILE *fp = fopen(status_path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", status_path, strerror(errno));
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%lu", &metrics->rss);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%lu", &metrics->vsz);
        } else if (strncmp(line, "RssAnon:", 8) == 0) {
            /* RssAnon is anonymous memory */
        } else if (strncmp(line, "RssShmem:", 9) == 0) {
            sscanf(line + 9, "%lu", &metrics->shared);
        } else if (strncmp(line, "VmData:", 7) == 0) {
            sscanf(line + 7, "%lu", &metrics->data);
        } else if (strncmp(line, "VmStk:", 6) == 0) {
            sscanf(line + 6, "%lu", &metrics->stack);
        } else if (strncmp(line, "VmExe:", 6) == 0) {
            sscanf(line + 6, "%lu", &metrics->text);
        } else if (strncmp(line, "VmSwap:", 7) == 0) {
            sscanf(line + 7, "%lu", &metrics->swap);
        }
    }
    fclose(fp);

    /* Read /proc/[pid]/stat for page faults */
    char stat_path[256];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);

    fp = fopen(stat_path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", stat_path, strerror(errno));
        return -1;
    }

    /* Parse stat file for page faults (fields 10 and 12) */
    unsigned long minflt, majflt;
    int items_read = fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u "
                           "%lu %*u %lu",
                           &minflt, &majflt);
    fclose(fp);

    if (items_read == 2) {
        metrics->minor_faults = minflt;
        metrics->major_faults = majflt;
    }

    clock_gettime(CLOCK_MONOTONIC, &metrics->timestamp);

    return 0;
}

void print_memory_metrics(const memory_metrics_t *metrics) {
    if (!metrics) {
        return;
    }

    printf("\n=== Memory Metrics for PID %d ===\n", metrics->pid);
    printf("RSS (Resident Set Size):   %lu KB\n", metrics->rss);
    printf("VSZ (Virtual Memory):      %lu KB\n", metrics->vsz);
    printf("Shared Memory:             %lu KB\n", metrics->shared);
    printf("Data Segment:              %lu KB\n", metrics->data);
    printf("Stack:                     %lu KB\n", metrics->stack);
    printf("Text (Code):               %lu KB\n", metrics->text);
    printf("Swap Usage:                %lu KB\n", metrics->swap);
    printf("Minor Page Faults:         %lu\n", metrics->minor_faults);
    printf("Major Page Faults:         %lu\n", metrics->major_faults);
    printf("=================================\n\n");
}

int export_memory_metrics_json(const memory_metrics_t *metrics, const char *filename) {
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
    fprintf(fp, "  \"rss_kb\": %lu,\n", metrics->rss);
    fprintf(fp, "  \"vsz_kb\": %lu,\n", metrics->vsz);
    fprintf(fp, "  \"shared_kb\": %lu,\n", metrics->shared);
    fprintf(fp, "  \"data_kb\": %lu,\n", metrics->data);
    fprintf(fp, "  \"stack_kb\": %lu,\n", metrics->stack);
    fprintf(fp, "  \"text_kb\": %lu,\n", metrics->text);
    fprintf(fp, "  \"swap_kb\": %lu,\n", metrics->swap);
    fprintf(fp, "  \"minor_faults\": %lu,\n", metrics->minor_faults);
    fprintf(fp, "  \"major_faults\": %lu,\n", metrics->major_faults);
    fprintf(fp, "  \"timestamp\": %ld.%09ld\n", metrics->timestamp.tv_sec, metrics->timestamp.tv_nsec);
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

int export_memory_metrics_csv(const memory_metrics_t *metrics, const char *filename, int append) {
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
        fprintf(fp, "pid,rss_kb,vsz_kb,shared_kb,data_kb,stack_kb,text_kb,swap_kb,"
                   "minor_faults,major_faults,timestamp\n");
    }

    fprintf(fp, "%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%ld.%09ld\n",
            metrics->pid, metrics->rss, metrics->vsz, metrics->shared,
            metrics->data, metrics->stack, metrics->text, metrics->swap,
            metrics->minor_faults, metrics->major_faults,
            metrics->timestamp.tv_sec, metrics->timestamp.tv_nsec);

    fclose(fp);
    return 0;
}
