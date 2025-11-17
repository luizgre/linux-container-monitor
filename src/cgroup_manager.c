#include "../include/cgroup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

static cgroup_version_t cgroup_version = CGROUP_V2;
static char cgroup_mount[MAX_CGROUP_PATH] = "/sys/fs/cgroup";

int cgroup_init(void) {
    cgroup_version = cgroup_detect_version();
    return 0;
}

void cgroup_cleanup(void) {
    /* Nothing to clean up currently */
}

cgroup_version_t cgroup_detect_version(void) {
    /* Check if cgroup v2 is mounted */
    struct stat st;
    if (stat("/sys/fs/cgroup/cgroup.controllers", &st) == 0) {
        return CGROUP_V2;
    }
    return CGROUP_V1;
}

int cgroup_is_available(void) {
    return (access(cgroup_mount, F_OK) == 0) ? 1 : 0;
}

const char* cgroup_get_mount_point(void) {
    return cgroup_mount;
}

static int read_cgroup_file(const char *path, char *buffer, size_t size) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, size - 1, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    return 0;
}

static int write_cgroup_file(const char *path, const char *value) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (fputs(value, fp) == EOF) {
        fprintf(stderr, "Failed to write to %s: %s\n", path, strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int cgroup_collect_cpu(const char *cgroup_path, cgroup_cpu_t *cpu) {
    if (!cgroup_path || !cpu) {
        return -1;
    }

    memset(cpu, 0, sizeof(cgroup_cpu_t));
    clock_gettime(CLOCK_MONOTONIC, &cpu->timestamp);

    if (cgroup_version == CGROUP_V2) {
        /* Read cpu.stat */
        char stat_path[MAX_CGROUP_PATH];
        snprintf(stat_path, sizeof(stat_path), "%s/%s/cpu.stat",
                cgroup_mount, cgroup_path);

        FILE *fp = fopen(stat_path, "r");
        if (!fp) {
            return -1;
        }

        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "usage_usec %lu", &cpu->usage_usec) == 1) continue;
            if (sscanf(line, "user_usec %lu", &cpu->user_usec) == 1) continue;
            if (sscanf(line, "system_usec %lu", &cpu->system_usec) == 1) continue;
            if (sscanf(line, "nr_periods %lu", &cpu->nr_periods) == 1) continue;
            if (sscanf(line, "nr_throttled %lu", &cpu->nr_throttled) == 1) continue;
            if (sscanf(line, "throttled_usec %lu", &cpu->throttled_usec) == 1) continue;
        }
        fclose(fp);

        /* Read cpu.max for quota */
        char max_path[MAX_CGROUP_PATH];
        snprintf(max_path, sizeof(max_path), "%s/%s/cpu.max",
                cgroup_mount, cgroup_path);

        fp = fopen(max_path, "r");
        if (fp) {
            char quota_str[32], period_str[32];
            if (fscanf(fp, "%s %s", quota_str, period_str) == 2) {
                if (strcmp(quota_str, "max") == 0) {
                    cpu->quota_usec = -1;
                } else {
                    cpu->quota_usec = atoll(quota_str);
                }
                cpu->period_usec = atoll(period_str);
            }
            fclose(fp);
        }
    }

    return 0;
}

int cgroup_collect_memory(const char *cgroup_path, cgroup_memory_t *memory) {
    if (!cgroup_path || !memory) {
        return -1;
    }

    memset(memory, 0, sizeof(cgroup_memory_t));
    clock_gettime(CLOCK_MONOTONIC, &memory->timestamp);

    if (cgroup_version == CGROUP_V2) {
        /* Read memory.current */
        char current_path[MAX_CGROUP_PATH];
        snprintf(current_path, sizeof(current_path), "%s/%s/memory.current",
                cgroup_mount, cgroup_path);

        char buffer[64];
        if (read_cgroup_file(current_path, buffer, sizeof(buffer)) == 0) {
            memory->current = strtoull(buffer, NULL, 10);
        }

        /* Read memory.max */
        char max_path[MAX_CGROUP_PATH];
        snprintf(max_path, sizeof(max_path), "%s/%s/memory.max",
                cgroup_mount, cgroup_path);

        if (read_cgroup_file(max_path, buffer, sizeof(buffer)) == 0) {
            if (strcmp(buffer, "max\n") == 0) {
                memory->limit = UINT64_MAX;
            } else {
                memory->limit = strtoull(buffer, NULL, 10);
            }
        }

        /* Read memory.stat */
        char stat_path[MAX_CGROUP_PATH];
        snprintf(stat_path, sizeof(stat_path), "%s/%s/memory.stat",
                cgroup_mount, cgroup_path);

        FILE *fp = fopen(stat_path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (sscanf(line, "file %lu", &memory->cache) == 1) continue;
                if (sscanf(line, "anon %lu", &memory->rss) == 1) continue;
            }
            fclose(fp);
        }

        /* Read memory.events for OOM kills */
        char events_path[MAX_CGROUP_PATH];
        snprintf(events_path, sizeof(events_path), "%s/%s/memory.events",
                cgroup_mount, cgroup_path);

        fp = fopen(events_path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (sscanf(line, "oom_kill %lu", &memory->oom_kill_count) == 1) {
                    break;
                }
            }
            fclose(fp);
        }
    }

    return 0;
}

int cgroup_collect_blkio(const char *cgroup_path, cgroup_blkio_t *blkio) {
    if (!cgroup_path || !blkio) {
        return -1;
    }

    memset(blkio, 0, sizeof(cgroup_blkio_t));
    clock_gettime(CLOCK_MONOTONIC, &blkio->timestamp);

    if (cgroup_version == CGROUP_V2) {
        /* Read io.stat */
        char stat_path[MAX_CGROUP_PATH];
        snprintf(stat_path, sizeof(stat_path), "%s/%s/io.stat",
                cgroup_mount, cgroup_path);

        FILE *fp = fopen(stat_path, "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                unsigned long rbytes, wbytes, rios, wios;
                if (sscanf(line, "%*s rbytes=%lu wbytes=%lu rios=%lu wios=%lu",
                          &rbytes, &wbytes, &rios, &wios) == 4) {
                    blkio->read_bytes += rbytes;
                    blkio->write_bytes += wbytes;
                    blkio->read_iops += rios;
                    blkio->write_iops += wios;
                }
            }
            fclose(fp);
        }
    }

    return 0;
}

int cgroup_collect_pids(const char *cgroup_path, cgroup_pids_t *pids) {
    if (!cgroup_path || !pids) {
        return -1;
    }

    memset(pids, 0, sizeof(cgroup_pids_t));
    clock_gettime(CLOCK_MONOTONIC, &pids->timestamp);

    if (cgroup_version == CGROUP_V2) {
        /* Read pids.current */
        char current_path[MAX_CGROUP_PATH];
        snprintf(current_path, sizeof(current_path), "%s/%s/pids.current",
                cgroup_mount, cgroup_path);

        char buffer[64];
        if (read_cgroup_file(current_path, buffer, sizeof(buffer)) == 0) {
            pids->current = strtoull(buffer, NULL, 10);
        }

        /* Read pids.max */
        char max_path[MAX_CGROUP_PATH];
        snprintf(max_path, sizeof(max_path), "%s/%s/pids.max",
                cgroup_mount, cgroup_path);

        if (read_cgroup_file(max_path, buffer, sizeof(buffer)) == 0) {
            if (strcmp(buffer, "max\n") == 0) {
                pids->limit = UINT64_MAX;
            } else {
                pids->limit = strtoull(buffer, NULL, 10);
            }
        }
    }

    return 0;
}

int cgroup_collect_metrics(const char *cgroup_path, cgroup_metrics_t *metrics) {
    if (!cgroup_path || !metrics) {
        return -1;
    }

    memset(metrics, 0, sizeof(cgroup_metrics_t));
    strncpy(metrics->cgroup_path, cgroup_path, sizeof(metrics->cgroup_path) - 1);
    metrics->version = cgroup_version;

    metrics->has_cpu = (cgroup_collect_cpu(cgroup_path, &metrics->cpu) == 0);
    metrics->has_memory = (cgroup_collect_memory(cgroup_path, &metrics->memory) == 0);
    metrics->has_blkio = (cgroup_collect_blkio(cgroup_path, &metrics->blkio) == 0);
    metrics->has_pids = (cgroup_collect_pids(cgroup_path, &metrics->pids) == 0);

    return 0;
}

int cgroup_create(const cgroup_config_t *config, char *created_path, size_t path_len) {
    if (!config || !created_path) {
        return -1;
    }

    /* Build full cgroup path */
    char full_path[MAX_CGROUP_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s/%s",
            cgroup_mount, config->parent_path, config->name);

    /* Create directory */
    if (mkdir(full_path, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "Failed to create cgroup %s: %s\n",
               full_path, strerror(errno));
        return -1;
    }

    /* Apply CPU limits if requested */
    if (config->set_cpu_limit) {
        cgroup_set_cpu_limit(full_path, config->cpu_limit);
    }

    /* Apply memory limits if requested */
    if (config->set_memory_limit) {
        cgroup_set_memory_limit(full_path, config->memory_limit_bytes);
    }

    snprintf(created_path, path_len, "%s/%s", config->parent_path, config->name);
    return 0;
}

int cgroup_set_cpu_limit(const char *cgroup_path, double cpu_cores) {
    if (!cgroup_path || cpu_cores <= 0) {
        return -1;
    }

    char max_path[MAX_CGROUP_PATH];
    snprintf(max_path, sizeof(max_path), "%s/%s/cpu.max",
            cgroup_mount, cgroup_path);

    /* Convert CPU cores to quota/period */
    uint64_t period = 100000;  /* 100ms in microseconds */
    uint64_t quota = (uint64_t)(cpu_cores * period);

    char value[64];
    snprintf(value, sizeof(value), "%lu %lu\n", quota, period);

    return write_cgroup_file(max_path, value);
}

int cgroup_set_memory_limit(const char *cgroup_path, uint64_t limit_bytes) {
    if (!cgroup_path) {
        return -1;
    }

    char max_path[MAX_CGROUP_PATH];
    snprintf(max_path, sizeof(max_path), "%s/%s/memory.max",
            cgroup_mount, cgroup_path);

    char value[64];
    snprintf(value, sizeof(value), "%lu\n", limit_bytes);

    return write_cgroup_file(max_path, value);
}

int cgroup_move_process(pid_t pid, const char *cgroup_path) {
    if (!cgroup_path) {
        return -1;
    }

    char procs_path[MAX_CGROUP_PATH];
    snprintf(procs_path, sizeof(procs_path), "%s/%s/cgroup.procs",
            cgroup_mount, cgroup_path);

    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", pid);

    return write_cgroup_file(procs_path, pid_str);
}

void cgroup_print_cpu(const cgroup_cpu_t *cpu) {
    if (!cpu) return;

    printf("  CPU:\n");
    printf("    Usage:          %lu us\n", cpu->usage_usec);
    printf("    User:           %lu us\n", cpu->user_usec);
    printf("    System:         %lu us\n", cpu->system_usec);
    printf("    Periods:        %lu\n", cpu->nr_periods);
    printf("    Throttled:      %lu\n", cpu->nr_throttled);
    printf("    Throttled time: %lu us\n", cpu->throttled_usec);
    if (cpu->quota_usec > 0) {
        printf("    Quota:          %ld us / %lu us\n",
               cpu->quota_usec, cpu->period_usec);
    } else {
        printf("    Quota:          unlimited\n");
    }
}

void cgroup_print_memory(const cgroup_memory_t *memory) {
    if (!memory) return;

    printf("  Memory:\n");
    printf("    Current:        %lu bytes (%.2f MB)\n",
           memory->current, memory->current / 1048576.0);
    printf("    Limit:          ");
    if (memory->limit == UINT64_MAX) {
        printf("unlimited\n");
    } else {
        printf("%lu bytes (%.2f MB)\n",
               memory->limit, memory->limit / 1048576.0);
    }
    printf("    Cache:          %lu bytes\n", memory->cache);
    printf("    RSS:            %lu bytes\n", memory->rss);
    printf("    OOM kills:      %lu\n", memory->oom_kill_count);
}

void cgroup_print_blkio(const cgroup_blkio_t *blkio) {
    if (!blkio) return;

    printf("  Block I/O:\n");
    printf("    Read bytes:     %lu\n", blkio->read_bytes);
    printf("    Write bytes:    %lu\n", blkio->write_bytes);
    printf("    Read IOPS:      %lu\n", blkio->read_iops);
    printf("    Write IOPS:     %lu\n", blkio->write_iops);
}

void cgroup_print_metrics(const cgroup_metrics_t *metrics) {
    if (!metrics) return;

    printf("\n=== Cgroup Metrics: %s ===\n", metrics->cgroup_path);
    printf("Version: cgroup v%d\n\n", metrics->version == CGROUP_V2 ? 2 : 1);

    if (metrics->has_cpu) cgroup_print_cpu(&metrics->cpu);
    if (metrics->has_memory) cgroup_print_memory(&metrics->memory);
    if (metrics->has_blkio) cgroup_print_blkio(&metrics->blkio);

    printf("========================================\n\n");
}
