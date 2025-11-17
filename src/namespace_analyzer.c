#include "../include/namespace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/wait.h>

static const char *ns_types[] = {
    NS_TYPE_IPC, NS_TYPE_MNT, NS_TYPE_NET,
    NS_TYPE_PID, NS_TYPE_USER, NS_TYPE_UTS,
    NS_TYPE_CGROUP
};

int namespace_init(void) {
    /* Nothing to initialize currently */
    return 0;
}

void namespace_cleanup(void) {
    /* Nothing to clean up currently */
}

int namespace_get_inode(pid_t pid, const char *ns_type, ino_t *inode) {
    if (!ns_type || !inode) {
        return -1;
    }

    char ns_path[256];
    snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/%s", pid, ns_type);

    struct stat st;
    if (stat(ns_path, &st) == -1) {
        return -1;
    }

    *inode = st.st_ino;
    return 0;
}

int namespace_list_process(pid_t pid, process_namespaces_t *proc_ns) {
    if (!proc_ns) {
        fprintf(stderr, "NULL process_namespaces_t pointer\n");
        return -1;
    }

    memset(proc_ns, 0, sizeof(process_namespaces_t));
    proc_ns->pid = pid;

    /* Read process command name */
    char comm_path[256];
    snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    FILE *fp = fopen(comm_path, "r");
    if (fp) {
        if (fgets(proc_ns->comm, sizeof(proc_ns->comm), fp)) {
            /* Remove newline */
            proc_ns->comm[strcspn(proc_ns->comm, "\n")] = '\0';
        }
        fclose(fp);
    }

    /* Iterate through all namespace types */
    int count = 0;
    for (int i = 0; i < MAX_NS_TYPES; i++) {
        char ns_path[256];
        snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/%s", pid, ns_types[i]);

        struct stat st;
        if (stat(ns_path, &st) == 0) {
            strncpy(proc_ns->namespaces[count].type, ns_types[i],
                   sizeof(proc_ns->namespaces[count].type) - 1);
            proc_ns->namespaces[count].inode = st.st_ino;
            strncpy(proc_ns->namespaces[count].path, ns_path,
                   sizeof(proc_ns->namespaces[count].path) - 1);
            count++;
        }
    }

    proc_ns->ns_count = count;
    return 0;
}

int namespace_find_processes(const char *ns_type, ino_t ns_inode,
                            pid_t *pid_list, int max_pids, int *count) {
    if (!ns_type || !pid_list || !count) {
        return -1;
    }

    *count = 0;
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        fprintf(stderr, "Failed to open /proc: %s\n", strerror(errno));
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL && *count < max_pids) {
        /* Check if directory name is a number (PID) */
        if (entry->d_type != DT_DIR) {
            continue;
        }

        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) {
            continue;
        }

        /* Check namespace inode */
        ino_t inode;
        if (namespace_get_inode(pid, ns_type, &inode) == 0) {
            if (inode == ns_inode) {
                pid_list[*count] = pid;
                (*count)++;
            }
        }
    }

    closedir(proc_dir);
    return 0;
}

int namespace_compare(pid_t pid1, pid_t pid2,
                     namespace_comparison_t *comparisons, int *count) {
    if (!comparisons || !count) {
        return -1;
    }

    *count = 0;

    for (int i = 0; i < MAX_NS_TYPES; i++) {
        ino_t inode1, inode2;

        int ret1 = namespace_get_inode(pid1, ns_types[i], &inode1);
        int ret2 = namespace_get_inode(pid2, ns_types[i], &inode2);

        if (ret1 == 0 && ret2 == 0) {
            strncpy(comparisons[*count].type, ns_types[i],
                   sizeof(comparisons[*count].type) - 1);
            comparisons[*count].inode1 = inode1;
            comparisons[*count].inode2 = inode2;
            comparisons[*count].shared = (inode1 == inode2) ? 1 : 0;
            (*count)++;
        }
    }

    return 0;
}

int namespace_generate_report(namespace_report_t *report) {
    if (!report) {
        return -1;
    }

    memset(report, 0, sizeof(namespace_report_t));

    for (int i = 0; i < MAX_NS_TYPES; i++) {
        strncpy(report->ns_type_names[i], ns_types[i], 15);
    }

    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        fprintf(stderr, "Failed to open /proc: %s\n", strerror(errno));
        return -1;
    }

    /* Track unique namespace inodes */
    ino_t unique_inodes[MAX_NS_TYPES][MAX_PROCESSES];
    int unique_counts[MAX_NS_TYPES] = {0};

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type != DT_DIR) {
            continue;
        }

        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) {
            continue;
        }

        report->total_processes++;

        /* Check each namespace type */
        for (int i = 0; i < MAX_NS_TYPES; i++) {
            ino_t inode;
            if (namespace_get_inode(pid, ns_types[i], &inode) == 0) {
                /* Check if this inode is already tracked */
                int found = 0;
                for (int j = 0; j < unique_counts[i]; j++) {
                    if (unique_inodes[i][j] == inode) {
                        found = 1;
                        break;
                    }
                }

                if (!found && unique_counts[i] < MAX_PROCESSES) {
                    unique_inodes[i][unique_counts[i]++] = inode;
                }
            }
        }
    }

    closedir(proc_dir);

    for (int i = 0; i < MAX_NS_TYPES; i++) {
        report->total_unique_namespaces[i] = unique_counts[i];
    }

    clock_gettime(CLOCK_REALTIME, &report->timestamp);
    return 0;
}

int namespace_type_to_clone_flag(const char *ns_type) {
    if (strcmp(ns_type, NS_TYPE_IPC) == 0) return CLONE_NEWIPC;
    if (strcmp(ns_type, NS_TYPE_MNT) == 0) return CLONE_NEWNS;
    if (strcmp(ns_type, NS_TYPE_NET) == 0) return CLONE_NEWNET;
    if (strcmp(ns_type, NS_TYPE_PID) == 0) return CLONE_NEWPID;
    if (strcmp(ns_type, NS_TYPE_USER) == 0) return CLONE_NEWUSER;
    if (strcmp(ns_type, NS_TYPE_UTS) == 0) return CLONE_NEWUTS;
    if (strcmp(ns_type, NS_TYPE_CGROUP) == 0) return CLONE_NEWCGROUP;
    return 0;
}

int namespace_measure_creation(const char *ns_type, namespace_timing_t *timing) {
    if (!ns_type || !timing) {
        return -1;
    }

    strncpy(timing->type, ns_type, sizeof(timing->type) - 1);
    timing->success = 0;

    int clone_flag = namespace_type_to_clone_flag(ns_type);
    if (clone_flag == 0) {
        fprintf(stderr, "Invalid namespace type: %s\n", ns_type);
        return -1;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Attempt to create namespace using unshare */
    int ret = unshare(clone_flag);

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                       (end.tv_nsec - start.tv_nsec) / 1e6;

    timing->creation_time_ms = elapsed_ms;
    timing->success = (ret == 0) ? 1 : 0;

    if (ret != 0) {
        fprintf(stderr, "Failed to create %s namespace: %s\n", ns_type, strerror(errno));
    }

    return timing->success ? 0 : -1;
}

int namespace_measure_all_types(namespace_timing_t timings[MAX_NS_TYPES], int *count) {
    if (!timings || !count) {
        return -1;
    }

    *count = 0;
    for (int i = 0; i < MAX_NS_TYPES; i++) {
        if (namespace_measure_creation(ns_types[i], &timings[*count]) >= 0 ||
            timings[*count].success == 0) {
            (*count)++;
        }
    }

    return 0;
}

void namespace_print_process_info(const process_namespaces_t *proc_ns) {
    if (!proc_ns) {
        return;
    }

    printf("\n=== Namespace Info for PID %d (%s) ===\n",
           proc_ns->pid, proc_ns->comm);
    printf("Total namespaces: %d\n\n", proc_ns->ns_count);

    for (int i = 0; i < proc_ns->ns_count; i++) {
        printf("  %-8s inode: %lu\n",
               proc_ns->namespaces[i].type,
               (unsigned long)proc_ns->namespaces[i].inode);
    }
    printf("==========================================\n\n");
}

void namespace_print_comparison(const namespace_comparison_t *comparisons, int count) {
    if (!comparisons) {
        return;
    }

    printf("\n=== Namespace Comparison ===\n");
    for (int i = 0; i < count; i++) {
        printf("%-8s: %s (inode1: %lu, inode2: %lu)\n",
               comparisons[i].type,
               comparisons[i].shared ? "SHARED" : "DIFFERENT",
               (unsigned long)comparisons[i].inode1,
               (unsigned long)comparisons[i].inode2);
    }
    printf("============================\n\n");
}

void namespace_print_report(const namespace_report_t *report) {
    if (!report) {
        return;
    }

    printf("\n=== System-wide Namespace Report ===\n");
    printf("Total processes: %d\n\n", report->total_processes);
    printf("Unique namespaces by type:\n");

    for (int i = 0; i < MAX_NS_TYPES; i++) {
        printf("  %-8s: %d\n", report->ns_type_names[i],
               report->total_unique_namespaces[i]);
    }
    printf("====================================\n\n");
}

void namespace_print_timing(const namespace_timing_t *timing) {
    if (!timing) {
        return;
    }

    printf("%-8s: %.3f ms (%s)\n",
           timing->type,
           timing->creation_time_ms,
           timing->success ? "SUCCESS" : "FAILED");
}
