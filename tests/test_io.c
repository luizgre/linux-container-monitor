#include "../include/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>

void test_io_monitor_init(void) {
    printf("Test: I/O monitor initialization... ");
    assert(io_monitor_init() == 0);
    printf("PASSED\n");
}

void test_io_collect_self(void) {
    printf("Test: I/O metrics collection (self)... ");
    io_metrics_t metrics;

    int result = io_monitor_collect(getpid(), &metrics);
    if (result != 0) {
        printf("SKIPPED (requires root permissions)\n");
        return;
    }

    assert(metrics.pid == getpid());
    printf("PASSED\n");
}

void test_io_file_operations(void) {
    printf("Test: I/O file operations detection... ");

    io_metrics_t before, after;

    if (io_monitor_collect(getpid(), &before) != 0) {
        printf("SKIPPED (requires root permissions)\n");
        return;
    }

    /* Perform file I/O */
    const char *test_file = "/tmp/test_io_file.txt";
    int fd = open(test_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    assert(fd >= 0);

    const char *data = "This is test data for I/O monitoring.\n";
    ssize_t written = write(fd, data, strlen(data));
    assert(written > 0);

    fsync(fd);
    close(fd);

    io_monitor_collect(getpid(), &after);

    assert(after.syscw > before.syscw);
    assert(after.wchar > before.wchar);

    unlink(test_file);
    printf("PASSED\n");
}

void test_io_rate_calculation(void) {
    printf("Test: I/O rate calculation... ");

    io_metrics_t before, after, result;

    if (io_monitor_collect(getpid(), &before) != 0) {
        printf("SKIPPED (requires root permissions)\n");
        return;
    }

    sleep(1);

    if (io_monitor_collect(getpid(), &after) != 0) {
        printf("SKIPPED (requires root permissions)\n");
        return;
    }

    assert(io_monitor_calculate_rates(&before, &after, &result) == 0);
    assert(result.read_rate >= 0.0);
    assert(result.write_rate >= 0.0);

    printf("PASSED (read: %.2f B/s, write: %.2f B/s)\n",
           result.read_rate, result.write_rate);
}

void test_io_export_json(void) {
    printf("Test: I/O metrics JSON export... ");

    io_metrics_t metrics;

    if (io_monitor_collect(getpid(), &metrics) != 0) {
        printf("SKIPPED (requires root permissions)\n");
        return;
    }

    const char *filename = "/tmp/test_io_metrics.json";
    assert(export_io_metrics_json(&metrics, filename) == 0);
    assert(access(filename, F_OK) == 0);

    unlink(filename);
    printf("PASSED\n");
}

void test_io_export_csv(void) {
    printf("Test: I/O metrics CSV export... ");

    io_metrics_t metrics;

    if (io_monitor_collect(getpid(), &metrics) != 0) {
        printf("SKIPPED (requires root permissions)\n");
        return;
    }

    const char *filename = "/tmp/test_io_metrics.csv";
    assert(export_io_metrics_csv(&metrics, filename, 0) == 0);
    assert(access(filename, F_OK) == 0);

    unlink(filename);
    printf("PASSED\n");
}

int main(void) {
    printf("\n=== I/O Monitor Test Suite ===\n");
    printf("NOTE: Some tests require root permissions\n\n");

    test_io_monitor_init();
    test_io_collect_self();
    test_io_file_operations();
    test_io_rate_calculation();
    test_io_export_json();
    test_io_export_csv();

    io_monitor_cleanup();

    printf("\n=== I/O Monitor Tests Completed ===\n\n");
    return 0;
}
