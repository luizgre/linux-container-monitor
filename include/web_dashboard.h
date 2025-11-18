#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include "monitor.h"
#include "anomaly.h"
#include <sys/types.h>

#define WEB_DEFAULT_PORT 8080
#define WEB_MAX_CLIENTS 10
#define WEB_BUFFER_SIZE 8192

/**
 * Web dashboard configuration
 */
typedef struct {
    int port;
    pid_t monitored_pid;
    int interval;
    int enable_anomaly;
    volatile int *running;
} web_config_t;

/**
 * Initialize web dashboard server
 * Returns server socket fd on success, -1 on failure
 */
int web_dashboard_init(int port);

/**
 * Start web dashboard server (blocking)
 * Runs in main thread and serves HTTP requests
 */
int web_dashboard_start(web_config_t *config);

/**
 * Cleanup and close web dashboard
 */
void web_dashboard_cleanup(int server_fd);

/**
 * Generate JSON response with current metrics
 */
int web_generate_metrics_json(pid_t pid, char *buffer, size_t buffer_size,
                              int enable_anomaly, anomaly_detector_t *detector);

/**
 * Generate HTML dashboard page
 */
int web_generate_html(char *buffer, size_t buffer_size, pid_t pid);

/**
 * Handle HTTP request
 */
int web_handle_request(int client_fd, const char *request, web_config_t *config);

#endif /* WEB_DASHBOARD_H */
