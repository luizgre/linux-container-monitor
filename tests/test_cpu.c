#include "../include/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

void test_cpu_monitor_init(void) {
    printf("Test: CPU monitor initialization... ");
    assert(cpu_monitor_init() == 0);
    printf("PASSED\n");
}

void test_process_exists(void) {
    printf("Test: Process existence check... ");
    assert(process_exists(1) == 1);
    assert(process_exists(99999) == 0);
    printf("PASSED\n");
}

void test_cpu_collect_self(void) {
    printf("Test: CPU metrics collection (self)... ");
    cpu_metrics_t metrics;
    assert(cpu_monitor_collect(getpid(), &metrics) == 0);
    assert(metrics.pid == getpid());
    assert(metrics.num_threads >= 1);
    printf("PASSED\n");
}

void test_cpu_percentage_calculation(void) {
    printf("Test: CPU percentage calculation... ");

    cpu_metrics_t prev, curr, result;

    cpu_monitor_collect(getpid(), &prev);

    /* Do some work */
    volatile int sum = 0;
    for (int i = 0; i < 1000000; i++) {
        sum += i;
    }

    sleep(1);
    cpu_monitor_collect(getpid(), &curr);

    assert(cpu_monitor_calculate_percentage(&prev, &curr, &result) == 0);
    assert(result.cpu_percent >= 0.0);

    printf("PASSED (CPU: %.2f%%)\n", result.cpu_percent);
}

void test_cpu_export_json(void) {
    printf("Test: CPU metrics JSON export... ");

    cpu_metrics_t metrics;
    cpu_monitor_collect(getpid(), &metrics);

    const char *filename = "/tmp/test_cpu_metrics.json";
    assert(export_cpu_metrics_json(&metrics, filename) == 0);
    assert(access(filename, F_OK) == 0);

    unlink(filename);
    printf("PASSED\n");
}

void test_cpu_export_csv(void) {
    printf("Test: CPU metrics CSV export... ");

    cpu_metrics_t metrics;
    cpu_monitor_collect(getpid(), &metrics);

    const char *filename = "/tmp/test_cpu_metrics.csv";
    assert(export_cpu_metrics_csv(&metrics, filename, 0) == 0);
    assert(access(filename, F_OK) == 0);

    unlink(filename);
    printf("PASSED\n");
}

void test_get_clock_ticks(void) {
    printf("Test: Get system clock ticks... ");
    int ticks = get_system_clock_ticks();
    assert(ticks > 0);
    printf("PASSED (ticks: %d)\n", ticks);
}

int main(void) {
    printf("\n=== CPU Monitor Test Suite ===\n\n");

    test_cpu_monitor_init();
    test_process_exists();
    test_get_clock_ticks();
    test_cpu_collect_self();
    test_cpu_percentage_calculation();
    test_cpu_export_json();
    test_cpu_export_csv();

    cpu_monitor_cleanup();

    printf("\n=== All CPU Monitor Tests PASSED ===\n\n");
    return 0;
}
