#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <stdint.h>
#include <time.h>
#include <sys/types.h>

/* Namespace types */
#define NS_TYPE_IPC    "ipc"
#define NS_TYPE_MNT    "mnt"
#define NS_TYPE_NET    "net"
#define NS_TYPE_PID    "pid"
#define NS_TYPE_USER   "user"
#define NS_TYPE_UTS    "uts"
#define NS_TYPE_CGROUP "cgroup"

#define MAX_NS_TYPES 7
#define MAX_PROCESSES 4096

/* Namespace information structure */
typedef struct {
    char type[16];               /* Namespace type (ipc, mnt, net, etc.) */
    ino_t inode;                 /* Namespace inode number */
    char path[256];              /* Path to namespace file */
} namespace_info_t;

/* Process namespace set */
typedef struct {
    pid_t pid;
    namespace_info_t namespaces[MAX_NS_TYPES];
    int ns_count;
    char comm[256];              /* Process command name */
} process_namespaces_t;

/* Namespace comparison result */
typedef struct {
    char type[16];
    int shared;                  /* 1 if shared, 0 if different */
    ino_t inode1;
    ino_t inode2;
} namespace_comparison_t;

/* Namespace creation timing */
typedef struct {
    char type[16];
    double creation_time_ms;     /* Time to create namespace in milliseconds */
    int success;                 /* 1 if successful, 0 otherwise */
} namespace_timing_t;

/* System-wide namespace report */
typedef struct {
    int total_processes;
    int total_unique_namespaces[MAX_NS_TYPES];
    char ns_type_names[MAX_NS_TYPES][16];
    struct timespec timestamp;
} namespace_report_t;

/* Namespace analyzer functions */
int namespace_init(void);
void namespace_cleanup(void);

/* List all namespaces for a specific process */
int namespace_list_process(pid_t pid, process_namespaces_t *proc_ns);

/* Find all processes in a specific namespace */
int namespace_find_processes(const char *ns_type, ino_t ns_inode,
                            pid_t *pid_list, int max_pids, int *count);

/* Compare namespaces between two processes */
int namespace_compare(pid_t pid1, pid_t pid2,
                     namespace_comparison_t *comparisons, int *count);

/* Generate system-wide namespace report */
int namespace_generate_report(namespace_report_t *report);

/* Measure namespace creation overhead */
int namespace_measure_creation(const char *ns_type, namespace_timing_t *timing);
int namespace_measure_all_types(namespace_timing_t timings[MAX_NS_TYPES], int *count);

/* Namespace manipulation functions */
int namespace_create(int clone_flags);
int namespace_enter(pid_t pid, const char *ns_type);
int namespace_unshare(int clone_flags);

/* Utility functions */
int namespace_get_inode(pid_t pid, const char *ns_type, ino_t *inode);
const char* namespace_get_clone_flag_name(int flag);
int namespace_type_to_clone_flag(const char *ns_type);
void namespace_print_process_info(const process_namespaces_t *proc_ns);
void namespace_print_comparison(const namespace_comparison_t *comparisons, int count);
void namespace_print_report(const namespace_report_t *report);
void namespace_print_timing(const namespace_timing_t *timing);

/* Export functions */
int namespace_export_report_json(const namespace_report_t *report, const char *filename);
int namespace_export_comparison_json(const namespace_comparison_t *comparisons,
                                    int count, const char *filename);

#endif /* NAMESPACE_H */
