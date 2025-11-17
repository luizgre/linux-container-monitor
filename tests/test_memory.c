#include "../include/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

void test_memory_monitor_init(void) {
    printf("Test: Memory monitor initialization... ");
    assert(memory_monitor_init() == 0);
    printf("PASSED\n");
}

void test_memory_collect_self(void) {
    printf("Test: Memory metrics collection (self)... ");
    memory_metrics_t metrics;
    assert(memory_monitor_collect(getpid(), &metrics) == 0);
    assert(metrics.pid == getpid());
    assert(metrics.rss > 0);
    assert(metrics.vsz > 0);
    printf("PASSED\n");
}

void test_memory_allocation_detection(void) {
    printf("Test: Memory allocation detection... ");

    memory_metrics_t before, after;

    memory_monitor_collect(getpid(), &before);

    /* Allocate 10MB */
    const size_t alloc_size = 10 * 1024 * 1024;
    char *buffer = malloc(alloc_size);
    assert(buffer != NULL);

    /* Touch the memory to ensure it's actually allocated */
    for (size_t i = 0; i < alloc_size; i += 4096) {
        buffer[i] = 1;
    }

    memory_monitor_collect(getpid(), &after);

    /* RSS should have increased */
    assert(after.rss >= before.rss);

    free(buffer);
    printf("PASSED (RSS before: %lu KB, after: %lu KB)\n",
           before.rss, after.rss);
}

void test_memory_export_json(void) {
    printf("Test: Memory metrics JSON export... ");

    memory_metrics_t metrics;
    memory_monitor_collect(getpid(), &metrics);

    const char *filename = "/tmp/test_memory_metrics.json";
    assert(export_memory_metrics_json(&metrics, filename) == 0);
    assert(access(filename, F_OK) == 0);

    unlink(filename);
    printf("PASSED\n");
}

void test_memory_export_csv(void) {
    printf("Test: Memory metrics CSV export... ");

    memory_metrics_t metrics;
    memory_monitor_collect(getpid(), &metrics);

    const char *filename = "/tmp/test_memory_metrics.csv";
    assert(export_memory_metrics_csv(&metrics, filename, 0) == 0);
    assert(access(filename, F_OK) == 0);

    unlink(filename);
    printf("PASSED\n");
}

void test_memory_print(void) {
    printf("Test: Memory metrics printing... ");

    memory_metrics_t metrics;
    memory_monitor_collect(getpid(), &metrics);

    print_memory_metrics(&metrics);
    printf("PASSED\n");
}

int main(void) {
    printf("\n=== Memory Monitor Test Suite ===\n\n");

    test_memory_monitor_init();
    test_memory_collect_self();
    test_memory_allocation_detection();
    test_memory_export_json();
    test_memory_export_csv();
    test_memory_print();

    memory_monitor_cleanup();

    printf("\n=== All Memory Monitor Tests PASSED ===\n\n");
    return 0;
}
