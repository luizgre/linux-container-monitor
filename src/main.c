#include "../include/monitor.h"
#include "../include/namespace.h"
#include "../include/cgroup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

static volatile int running = 1;

void signal_handler(int signum) {
    (void)signum;
    running = 0;
}

void print_usage(const char *program_name) {
    printf("Linux Container Resource Monitoring System\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Resource Profiler Options:\n");
    printf("  -p, --pid PID         Monitor process with PID\n");
    printf("  -i, --interval SEC    Sampling interval in seconds (default: 1)\n");
    printf("  -d, --duration SEC    Monitoring duration in seconds (default: infinite)\n");
    printf("  -o, --output FILE     Output file for metrics\n");
    printf("  -f, --format FORMAT   Output format: json, csv, console (default: console)\n");
    printf("  -m, --metrics TYPE    Metric types: cpu, memory, io, all (default: all)\n\n");
    printf("Namespace Analyzer Options:\n");
    printf("  -n, --namespace       Enable namespace analysis\n");
    printf("  -l, --list-ns PID     List namespaces for PID\n");
    printf("  -c, --compare P1,P2   Compare namespaces between two PIDs\n");
    printf("  -r, --report          Generate system-wide namespace report\n");
    printf("  -t, --timing          Measure namespace creation overhead\n\n");
    printf("Control Group Manager Options:\n");
    printf("  -g, --cgroup PATH     Monitor cgroup at PATH\n");
    printf("  --create-cgroup NAME  Create new cgroup with NAME\n");
    printf("  --cpu-limit CORES     Set CPU limit in cores (e.g., 0.5, 1.0)\n");
    printf("  --mem-limit MB        Set memory limit in MB\n");
    printf("  --move-to-cgroup PID  Move process to cgroup\n\n");
    printf("General Options:\n");
    printf("  -h, --help            Show this help message\n");
    printf("  -v, --verbose         Enable verbose output\n\n");
    printf("Examples:\n");
    printf("  %s -p 1234 -i 1 -d 60 -o metrics.csv -f csv\n", program_name);
    printf("  %s -l 1234\n", program_name);
    printf("  %s -c 1234,5678\n", program_name);
    printf("  %s -g /test --cpu-limit 1.0 --mem-limit 100\n", program_name);
    printf("\n");
}

int monitor_process(pid_t pid, int interval, int duration, const char *output_file,
                   const char *format, const char *metrics_type) {
    printf("Monitoring PID %d (interval: %ds, duration: %ds)\n",
           pid, interval, duration);

    int monitor_cpu = (strcmp(metrics_type, "all") == 0 ||
                      strcmp(metrics_type, "cpu") == 0);
    int monitor_memory = (strcmp(metrics_type, "all") == 0 ||
                         strcmp(metrics_type, "memory") == 0);
    int monitor_io = (strcmp(metrics_type, "all") == 0 ||
                     strcmp(metrics_type, "io") == 0);

    cpu_metrics_t prev_cpu, curr_cpu, result_cpu;
    memory_metrics_t memory;
    io_metrics_t prev_io, curr_io, result_io;

    int is_csv = (strcmp(format, "csv") == 0);
    int is_json = (strcmp(format, "json") == 0);
    int append = 0;

    /* Initialize monitors */
    if (monitor_cpu) {
        if (cpu_monitor_init() != 0) {
            fprintf(stderr, "Failed to initialize CPU monitor\n");
            return -1;
        }
        /* Get initial reading */
        if (cpu_monitor_collect(pid, &prev_cpu) != 0) {
            fprintf(stderr, "Failed to collect initial CPU metrics\n");
            return -1;
        }
    }

    if (monitor_memory) {
        if (memory_monitor_init() != 0) {
            fprintf(stderr, "Failed to initialize memory monitor\n");
            return -1;
        }
    }

    if (monitor_io) {
        if (io_monitor_init() != 0) {
            fprintf(stderr, "Failed to initialize I/O monitor\n");
            return -1;
        }
        /* Get initial reading */
        if (io_monitor_collect(pid, &prev_io) != 0) {
            fprintf(stderr, "Warning: Could not collect I/O metrics (may need sudo)\n");
            monitor_io = 0;
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int elapsed = 0;
    while (running && (duration == 0 || elapsed < duration)) {
        sleep(interval);
        elapsed += interval;

        /* Collect CPU metrics */
        if (monitor_cpu) {
            if (cpu_monitor_collect(pid, &curr_cpu) != 0) {
                fprintf(stderr, "Process %d no longer exists\n", pid);
                break;
            }
            cpu_monitor_calculate_percentage(&prev_cpu, &curr_cpu, &result_cpu);

            if (is_csv && output_file) {
                export_cpu_metrics_csv(&result_cpu, output_file, append);
                append = 1;
            } else if (is_json && output_file) {
                char json_file[512];
                snprintf(json_file, sizeof(json_file), "%s.cpu.%d.json",
                        output_file, elapsed);
                export_cpu_metrics_json(&result_cpu, json_file);
            } else {
                print_cpu_metrics(&result_cpu);
            }

            prev_cpu = curr_cpu;
        }

        /* Collect memory metrics */
        if (monitor_memory) {
            if (memory_monitor_collect(pid, &memory) == 0) {
                if (is_csv && output_file) {
                    char mem_file[512];
                    snprintf(mem_file, sizeof(mem_file), "%s.memory.csv", output_file);
                    export_memory_metrics_csv(&memory, mem_file, append);
                } else if (is_json && output_file) {
                    char json_file[512];
                    snprintf(json_file, sizeof(json_file), "%s.memory.%d.json",
                            output_file, elapsed);
                    export_memory_metrics_json(&memory, json_file);
                } else {
                    print_memory_metrics(&memory);
                }
            }
        }

        /* Collect I/O metrics */
        if (monitor_io) {
            if (io_monitor_collect(pid, &curr_io) == 0) {
                io_monitor_calculate_rates(&prev_io, &curr_io, &result_io);

                if (is_csv && output_file) {
                    char io_file[512];
                    snprintf(io_file, sizeof(io_file), "%s.io.csv", output_file);
                    export_io_metrics_csv(&result_io, io_file, append);
                } else if (is_json && output_file) {
                    char json_file[512];
                    snprintf(json_file, sizeof(json_file), "%s.io.%d.json",
                            output_file, elapsed);
                    export_io_metrics_json(&result_io, json_file);
                } else {
                    print_io_metrics(&result_io);
                }

                prev_io = curr_io;
            }
        }
    }

    /* Cleanup */
    if (monitor_cpu) cpu_monitor_cleanup();
    if (monitor_memory) memory_monitor_cleanup();
    if (monitor_io) io_monitor_cleanup();

    printf("\nMonitoring completed.\n");
    return 0;
}

int main(int argc, char *argv[]) {
    int opt;
    pid_t pid = 0;
    int interval = 1;
    int duration = 0;
    char output_file[512] = "";
    char format[32] = "console";
    char metrics_type[32] = "all";
    int list_ns_pid = 0;
    int compare_ns = 0;
    pid_t compare_pid1 = 0, compare_pid2 = 0;
    int gen_report = 0;
    int measure_timing = 0;
    char cgroup_path[MAX_CGROUP_PATH] = "";
    int verbose __attribute__((unused)) = 0;

    static struct option long_options[] = {
        {"pid",           required_argument, 0, 'p'},
        {"interval",      required_argument, 0, 'i'},
        {"duration",      required_argument, 0, 'd'},
        {"output",        required_argument, 0, 'o'},
        {"format",        required_argument, 0, 'f'},
        {"metrics",       required_argument, 0, 'm'},
        {"list-ns",       required_argument, 0, 'l'},
        {"compare",       required_argument, 0, 'c'},
        {"report",        no_argument,       0, 'r'},
        {"timing",        no_argument,       0, 't'},
        {"cgroup",        required_argument, 0, 'g'},
        {"verbose",       no_argument,       0, 'v'},
        {"help",          no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "p:i:d:o:f:m:l:c:rtg:vh",
                             long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                pid = atoi(optarg);
                break;
            case 'i':
                interval = atoi(optarg);
                break;
            case 'd':
                duration = atoi(optarg);
                break;
            case 'o':
                strncpy(output_file, optarg, sizeof(output_file) - 1);
                break;
            case 'f':
                strncpy(format, optarg, sizeof(format) - 1);
                break;
            case 'm':
                strncpy(metrics_type, optarg, sizeof(metrics_type) - 1);
                break;
            case 'l':
                list_ns_pid = atoi(optarg);
                break;
            case 'c':
                compare_ns = 1;
                if (sscanf(optarg, "%d,%d", &compare_pid1, &compare_pid2) != 2) {
                    fprintf(stderr, "Invalid format for --compare. Use: PID1,PID2\n");
                    return 1;
                }
                break;
            case 'r':
                gen_report = 1;
                break;
            case 't':
                measure_timing = 1;
                break;
            case 'g':
                strncpy(cgroup_path, optarg, sizeof(cgroup_path) - 1);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Handle namespace operations */
    if (list_ns_pid > 0) {
        namespace_init();
        process_namespaces_t proc_ns;
        if (namespace_list_process(list_ns_pid, &proc_ns) == 0) {
            namespace_print_process_info(&proc_ns);
        } else {
            fprintf(stderr, "Failed to list namespaces for PID %d\n", list_ns_pid);
        }
        namespace_cleanup();
        return 0;
    }

    if (compare_ns) {
        namespace_init();
        namespace_comparison_t comparisons[MAX_NS_TYPES];
        int count;
        if (namespace_compare(compare_pid1, compare_pid2, comparisons, &count) == 0) {
            printf("Comparing namespaces between PID %d and PID %d:\n",
                   compare_pid1, compare_pid2);
            namespace_print_comparison(comparisons, count);
        } else {
            fprintf(stderr, "Failed to compare namespaces\n");
        }
        namespace_cleanup();
        return 0;
    }

    if (gen_report) {
        namespace_init();
        namespace_report_t report;
        if (namespace_generate_report(&report) == 0) {
            namespace_print_report(&report);
        } else {
            fprintf(stderr, "Failed to generate namespace report\n");
        }
        namespace_cleanup();
        return 0;
    }

    if (measure_timing) {
        namespace_init();
        namespace_timing_t timings[MAX_NS_TYPES];
        int count;
        printf("Measuring namespace creation overhead...\n\n");
        if (namespace_measure_all_types(timings, &count) >= 0) {
            printf("Namespace Creation Timing:\n");
            for (int i = 0; i < count; i++) {
                namespace_print_timing(&timings[i]);
            }
        }
        namespace_cleanup();
        return 0;
    }

    /* Handle cgroup operations */
    if (strlen(cgroup_path) > 0) {
        cgroup_init();
        cgroup_metrics_t metrics;
        if (cgroup_collect_metrics(cgroup_path, &metrics) == 0) {
            cgroup_print_metrics(&metrics);
        } else {
            fprintf(stderr, "Failed to collect cgroup metrics for %s\n", cgroup_path);
        }
        cgroup_cleanup();
        return 0;
    }

    /* Handle process monitoring */
    if (pid > 0) {
        return monitor_process(pid, interval, duration, output_file,
                             format, metrics_type);
    }

    /* No valid operation specified */
    fprintf(stderr, "Error: No operation specified\n\n");
    print_usage(argv[0]);
    return 1;
}
