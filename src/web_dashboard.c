#include "../include/web_dashboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

static cpu_metrics_t prev_cpu, curr_cpu, result_cpu;
static memory_metrics_t memory;
static io_metrics_t prev_io, curr_io, result_io;
static int monitors_initialized = 0;
static anomaly_detector_t global_detector;

int web_dashboard_init(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    /* Create socket */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return -1;
    }

    /* Set socket options */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    /* Bind socket */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }

    /* Listen */
    if (listen(server_fd, WEB_MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    printf("Web dashboard server started on http://localhost:%d\n", port);
    return server_fd;
}

void web_dashboard_cleanup(int server_fd) {
    if (server_fd >= 0) {
        close(server_fd);
    }

    if (monitors_initialized) {
        cpu_monitor_cleanup();
        memory_monitor_cleanup();
        io_monitor_cleanup();
        monitors_initialized = 0;
    }
}

int web_generate_metrics_json(pid_t pid, char *buffer, size_t buffer_size,
                              int enable_anomaly, anomaly_detector_t *detector) {
    /* Initialize monitors if needed */
    if (!monitors_initialized) {
        if (cpu_monitor_init() != 0) return -1;
        if (memory_monitor_init() != 0) return -1;
        if (io_monitor_init() != 0) return -1;

        /* Get initial readings */
        if (cpu_monitor_collect(pid, &prev_cpu) != 0) return -1;
        if (io_monitor_collect(pid, &prev_io) != 0) {
            /* I/O might fail without sudo */
        }

        monitors_initialized = 1;
        sleep(1);  /* Wait 1 second for first meaningful measurement */
    }

    /* Collect current metrics */
    if (cpu_monitor_collect(pid, &curr_cpu) != 0) {
        return -1;  /* Process no longer exists */
    }
    cpu_monitor_calculate_percentage(&prev_cpu, &curr_cpu, &result_cpu);

    if (memory_monitor_collect(pid, &memory) != 0) {
        return -1;
    }

    if (io_monitor_collect(pid, &curr_io) == 0) {
        io_monitor_calculate_rates(&prev_io, &curr_io, &result_io);
        prev_io = curr_io;
    }

    /* Update anomaly detector */
    anomaly_event_t anomalies[10];
    int anomaly_count = 0;

    if (enable_anomaly && detector) {
        anomaly_detector_update_cpu(detector, result_cpu.cpu_percent);
        anomaly_detector_update_memory(detector, (double)memory.rss);
        anomaly_detector_update_io(detector, result_io.read_rate, result_io.write_rate);

        anomaly_count = anomaly_detector_check(detector, anomalies, 10);
    }

    /* Generate JSON */
    int len = snprintf(buffer, buffer_size,
        "{\n"
        "  \"timestamp\": %ld,\n"
        "  \"pid\": %d,\n"
        "  \"cpu\": {\n"
        "    \"percent\": %.2f,\n"
        "    \"utime\": %lu,\n"
        "    \"stime\": %lu,\n"
        "    \"threads\": %ld,\n"
        "    \"ctxt_switches_vol\": %lu,\n"
        "    \"ctxt_switches_invol\": %lu\n"
        "  },\n"
        "  \"memory\": {\n"
        "    \"rss_kb\": %lu,\n"
        "    \"rss_mb\": %.2f,\n"
        "    \"vsz_kb\": %lu,\n"
        "    \"vsz_mb\": %.2f,\n"
        "    \"shared_kb\": %lu,\n"
        "    \"data_kb\": %lu,\n"
        "    \"stack_kb\": %lu,\n"
        "    \"text_kb\": %lu,\n"
        "    \"swap_kb\": %lu\n"
        "  },\n"
        "  \"io\": {\n"
        "    \"read_rate_kbs\": %.2f,\n"
        "    \"write_rate_kbs\": %.2f,\n"
        "    \"read_bytes\": %lu,\n"
        "    \"write_bytes\": %lu,\n"
        "    \"syscr\": %lu,\n"
        "    \"syscw\": %lu\n"
        "  },\n"
        "  \"anomalies\": [\n",
        time(NULL),
        pid,
        result_cpu.cpu_percent,
        result_cpu.utime,
        result_cpu.stime,
        result_cpu.num_threads,
        result_cpu.voluntary_ctxt_switches,
        result_cpu.nonvoluntary_ctxt_switches,
        memory.rss,
        memory.rss / 1024.0,
        memory.vsz,
        memory.vsz / 1024.0,
        memory.shared,
        memory.data,
        memory.stack,
        memory.text,
        memory.swap,
        result_io.read_rate,
        result_io.write_rate,
        result_io.read_bytes,
        result_io.write_bytes,
        result_io.syscr,
        result_io.syscw
    );

    /* Add anomalies to JSON */
    for (int i = 0; i < anomaly_count && len < (int)buffer_size - 200; i++) {
        if (i > 0) len += snprintf(buffer + len, buffer_size - len, ",\n");

        const char *severity_str = "LOW";
        if (anomalies[i].severity == SEVERITY_CRITICAL) severity_str = "CRITICAL";
        else if (anomalies[i].severity == SEVERITY_HIGH) severity_str = "HIGH";
        else if (anomalies[i].severity == SEVERITY_MEDIUM) severity_str = "MEDIUM";

        len += snprintf(buffer + len, buffer_size - len,
            "    {\n"
            "      \"type\": %d,\n"
            "      \"severity\": \"%s\",\n"
            "      \"description\": \"%s\",\n"
            "      \"value\": %.2f,\n"
            "      \"expected\": %.2f,\n"
            "      \"deviation_sigma\": %.2f\n"
            "    }",
            anomalies[i].type,
            severity_str,
            anomalies[i].description,
            anomalies[i].value,
            anomalies[i].expected_mean,
            anomalies[i].deviation_sigma
        );
    }

    len += snprintf(buffer + len, buffer_size - len, "\n  ]\n}\n");

    prev_cpu = curr_cpu;

    return len;
}

int web_generate_html(char *buffer, size_t buffer_size, pid_t pid) {
    return snprintf(buffer, buffer_size,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "  <title>Resource Monitor - PID %d</title>\n"
        "  <style>\n"
        "    * { margin: 0; padding: 0; box-sizing: border-box; }\n"
        "    body {\n"
        "      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n"
        "      background: linear-gradient(135deg, #667eea 0%%, #764ba2 100%%);\n"
        "      color: #333;\n"
        "      padding: 20px;\n"
        "      min-height: 100vh;\n"
        "    }\n"
        "    .container {\n"
        "      max-width: 1400px;\n"
        "      margin: 0 auto;\n"
        "    }\n"
        "    header {\n"
        "      background: white;\n"
        "      padding: 30px;\n"
        "      border-radius: 15px;\n"
        "      box-shadow: 0 10px 30px rgba(0,0,0,0.2);\n"
        "      margin-bottom: 30px;\n"
        "      text-align: center;\n"
        "    }\n"
        "    h1 {\n"
        "      color: #667eea;\n"
        "      font-size: 2.5em;\n"
        "      margin-bottom: 10px;\n"
        "    }\n"
        "    .subtitle {\n"
        "      color: #666;\n"
        "      font-size: 1.2em;\n"
        "    }\n"
        "    .metrics-grid {\n"
        "      display: grid;\n"
        "      grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));\n"
        "      gap: 20px;\n"
        "      margin-bottom: 30px;\n"
        "    }\n"
        "    .metric-card {\n"
        "      background: white;\n"
        "      padding: 25px;\n"
        "      border-radius: 15px;\n"
        "      box-shadow: 0 5px 15px rgba(0,0,0,0.1);\n"
        "      transition: transform 0.3s, box-shadow 0.3s;\n"
        "    }\n"
        "    .metric-card:hover {\n"
        "      transform: translateY(-5px);\n"
        "      box-shadow: 0 10px 25px rgba(0,0,0,0.2);\n"
        "    }\n"
        "    .metric-title {\n"
        "      font-size: 1.3em;\n"
        "      font-weight: 600;\n"
        "      margin-bottom: 15px;\n"
        "      color: #667eea;\n"
        "    }\n"
        "    .metric-value {\n"
        "      font-size: 2.5em;\n"
        "      font-weight: bold;\n"
        "      margin-bottom: 10px;\n"
        "    }\n"
        "    .cpu-value { color: #4CAF50; }\n"
        "    .mem-value { color: #2196F3; }\n"
        "    .io-value { color: #FF9800; }\n"
        "    .metric-details {\n"
        "      font-size: 0.9em;\n"
        "      color: #666;\n"
        "      margin-top: 10px;\n"
        "    }\n"
        "    .metric-details div {\n"
        "      padding: 5px 0;\n"
        "      border-bottom: 1px solid #eee;\n"
        "    }\n"
        "    .metric-details div:last-child {\n"
        "      border-bottom: none;\n"
        "    }\n"
        "    .chart-container {\n"
        "      background: white;\n"
        "      padding: 25px;\n"
        "      border-radius: 15px;\n"
        "      box-shadow: 0 5px 15px rgba(0,0,0,0.1);\n"
        "      margin-bottom: 30px;\n"
        "      height: 400px;\n"
        "    }\n"
        "    canvas {\n"
        "      max-width: 100%%;\n"
        "      height: 100%%;\n"
        "    }\n"
        "    .anomalies {\n"
        "      background: white;\n"
        "      padding: 25px;\n"
        "      border-radius: 15px;\n"
        "      box-shadow: 0 5px 15px rgba(0,0,0,0.1);\n"
        "    }\n"
        "    .anomaly-item {\n"
        "      padding: 15px;\n"
        "      margin-bottom: 10px;\n"
        "      border-radius: 10px;\n"
        "      border-left: 5px solid;\n"
        "    }\n"
        "    .anomaly-critical {\n"
        "      background: #ffebee;\n"
        "      border-color: #f44336;\n"
        "    }\n"
        "    .anomaly-high {\n"
        "      background: #fff3e0;\n"
        "      border-color: #ff9800;\n"
        "    }\n"
        "    .anomaly-medium {\n"
        "      background: #fff9c4;\n"
        "      border-color: #ffc107;\n"
        "    }\n"
        "    .anomaly-low {\n"
        "      background: #e8f5e9;\n"
        "      border-color: #4caf50;\n"
        "    }\n"
        "    .last-update {\n"
        "      text-align: center;\n"
        "      color: white;\n"
        "      font-size: 0.9em;\n"
        "      margin-top: 20px;\n"
        "    }\n"
        "    .loading {\n"
        "      text-align: center;\n"
        "      padding: 50px;\n"
        "      color: white;\n"
        "      font-size: 1.5em;\n"
        "    }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <div class=\"container\">\n"
        "    <header>\n"
        "      <h1>Resource Monitor Dashboard</h1>\n"
        "      <div class=\"subtitle\">Process ID: %d</div>\n"
        "    </header>\n"
        "\n"
        "    <div id=\"loading\" class=\"loading\">Loading metrics...</div>\n"
        "\n"
        "    <div id=\"content\" style=\"display: none;\">\n"
        "      <div class=\"metrics-grid\">\n"
        "        <div class=\"metric-card\">\n"
        "          <div class=\"metric-title\">CPU Usage</div>\n"
        "          <div class=\"metric-value cpu-value\" id=\"cpu-percent\">--%%</div>\n"
        "          <div class=\"metric-details\">\n"
        "            <div>Threads: <span id=\"cpu-threads\">--</span></div>\n"
        "            <div>User time: <span id=\"cpu-utime\">--</span> ticks</div>\n"
        "            <div>System time: <span id=\"cpu-stime\">--</span> ticks</div>\n"
        "            <div>Context switches: <span id=\"cpu-ctxt\">--</span></div>\n"
        "          </div>\n"
        "        </div>\n"
        "\n"
        "        <div class=\"metric-card\">\n"
        "          <div class=\"metric-title\">Memory Usage</div>\n"
        "          <div class=\"metric-value mem-value\" id=\"mem-rss\">-- MB</div>\n"
        "          <div class=\"metric-details\">\n"
        "            <div>VSZ: <span id=\"mem-vsz\">--</span> MB</div>\n"
        "            <div>Shared: <span id=\"mem-shared\">--</span> KB</div>\n"
        "            <div>Data: <span id=\"mem-data\">--</span> KB</div>\n"
        "            <div>Stack: <span id=\"mem-stack\">--</span> KB</div>\n"
        "          </div>\n"
        "        </div>\n"
        "\n"
        "        <div class=\"metric-card\">\n"
        "          <div class=\"metric-title\">I/O Activity</div>\n"
        "          <div class=\"metric-value io-value\" id=\"io-read\">-- KB/s</div>\n"
        "          <div class=\"metric-details\">\n"
        "            <div>Write rate: <span id=\"io-write\">--</span> KB/s</div>\n"
        "            <div>Read bytes: <span id=\"io-read-bytes\">--</span></div>\n"
        "            <div>Write bytes: <span id=\"io-write-bytes\">--</span></div>\n"
        "            <div>Syscalls: <span id=\"io-syscalls\">--</span></div>\n"
        "          </div>\n"
        "        </div>\n"
        "      </div>\n"
        "\n"
        "      <div class=\"chart-container\">\n"
        "        <canvas id=\"metricsChart\"></canvas>\n"
        "      </div>\n"
        "\n"
        "      <div class=\"anomalies\">\n"
        "        <h2 style=\"margin-bottom: 15px; color: #667eea;\">Anomaly Detection</h2>\n"
        "        <div id=\"anomalies-list\">No anomalies detected</div>\n"
        "      </div>\n"
        "\n"
        "      <div class=\"last-update\">Last updated: <span id=\"last-update\">--</span></div>\n"
        "    </div>\n"
        "  </div>\n"
        "\n"
        "  <script src=\"https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js\"></script>\n"
        "  <script>\n"
        "    const maxDataPoints = 60;\n"
        "    const chartData = {\n"
        "      labels: [],\n"
        "      datasets: [\n"
        "        {\n"
        "          label: 'CPU %%',\n"
        "          data: [],\n"
        "          borderColor: '#4CAF50',\n"
        "          backgroundColor: 'rgba(76, 175, 80, 0.1)',\n"
        "          tension: 0.4,\n"
        "          yAxisID: 'y'\n"
        "        },\n"
        "        {\n"
        "          label: 'Memory (MB)',\n"
        "          data: [],\n"
        "          borderColor: '#2196F3',\n"
        "          backgroundColor: 'rgba(33, 150, 243, 0.1)',\n"
        "          tension: 0.4,\n"
        "          yAxisID: 'y1'\n"
        "        }\n"
        "      ]\n"
        "    };\n"
        "\n"
        "    const ctx = document.getElementById('metricsChart').getContext('2d');\n"
        "    const chart = new Chart(ctx, {\n"
        "      type: 'line',\n"
        "      data: chartData,\n"
        "      options: {\n"
        "        responsive: true,\n"
        "        maintainAspectRatio: false,\n"
        "        interaction: {\n"
        "          mode: 'index',\n"
        "          intersect: false\n"
        "        },\n"
        "        scales: {\n"
        "          y: {\n"
        "            type: 'linear',\n"
        "            display: true,\n"
        "            position: 'left',\n"
        "            title: { display: true, text: 'CPU %%' },\n"
        "            min: 0,\n"
        "            max: 100\n"
        "          },\n"
        "          y1: {\n"
        "            type: 'linear',\n"
        "            display: true,\n"
        "            position: 'right',\n"
        "            title: { display: true, text: 'Memory (MB)' },\n"
        "            grid: { drawOnChartArea: false }\n"
        "          }\n"
        "        },\n"
        "        plugins: {\n"
        "          legend: { display: true, position: 'top' }\n"
        "        }\n"
        "      }\n"
        "    });\n"
        "\n"
        "    function updateMetrics() {\n"
        "      fetch('/api/metrics')\n"
        "        .then(response => response.json())\n"
        "        .then(data => {\n"
        "          document.getElementById('loading').style.display = 'none';\n"
        "          document.getElementById('content').style.display = 'block';\n"
        "\n"
        "          document.getElementById('cpu-percent').textContent = data.cpu.percent.toFixed(2) + '%%';\n"
        "          document.getElementById('cpu-threads').textContent = data.cpu.threads;\n"
        "          document.getElementById('cpu-utime').textContent = data.cpu.utime;\n"
        "          document.getElementById('cpu-stime').textContent = data.cpu.stime;\n"
        "          document.getElementById('cpu-ctxt').textContent = \n"
        "            data.cpu.ctxt_switches_vol + ' / ' + data.cpu.ctxt_switches_invol;\n"
        "\n"
        "          document.getElementById('mem-rss').textContent = data.memory.rss_mb.toFixed(2) + ' MB';\n"
        "          document.getElementById('mem-vsz').textContent = data.memory.vsz_mb.toFixed(2);\n"
        "          document.getElementById('mem-shared').textContent = data.memory.shared_kb;\n"
        "          document.getElementById('mem-data').textContent = data.memory.data_kb;\n"
        "          document.getElementById('mem-stack').textContent = data.memory.stack_kb;\n"
        "\n"
        "          document.getElementById('io-read').textContent = data.io.read_rate_kbs.toFixed(2) + ' KB/s';\n"
        "          document.getElementById('io-write').textContent = data.io.write_rate_kbs.toFixed(2);\n"
        "          document.getElementById('io-read-bytes').textContent = data.io.read_bytes;\n"
        "          document.getElementById('io-write-bytes').textContent = data.io.write_bytes;\n"
        "          document.getElementById('io-syscalls').textContent = \n"
        "            data.io.syscr + ' / ' + data.io.syscw;\n"
        "\n"
        "          const now = new Date();\n"
        "          const timeLabel = now.toLocaleTimeString();\n"
        "\n"
        "          chartData.labels.push(timeLabel);\n"
        "          chartData.datasets[0].data.push(data.cpu.percent);\n"
        "          chartData.datasets[1].data.push(data.memory.rss_mb);\n"
        "\n"
        "          if (chartData.labels.length > maxDataPoints) {\n"
        "            chartData.labels.shift();\n"
        "            chartData.datasets[0].data.shift();\n"
        "            chartData.datasets[1].data.shift();\n"
        "          }\n"
        "\n"
        "          chart.update('none');\n"
        "\n"
        "          const anomaliesList = document.getElementById('anomalies-list');\n"
        "          if (data.anomalies && data.anomalies.length > 0) {\n"
        "            anomaliesList.innerHTML = data.anomalies.map(a => \n"
        "              `<div class=\"anomaly-item anomaly-${a.severity.toLowerCase()}\">\n"
        "                <strong>${a.severity}</strong>: ${a.description}\n"
        "                <br><small>Value: ${a.value.toFixed(2)}, Expected: ${a.expected.toFixed(2)}, \n"
        "                Deviation: ${a.deviation_sigma.toFixed(2)}σ</small>\n"
        "              </div>`\n"
        "            ).join('');\n"
        "          } else {\n"
        "            anomaliesList.innerHTML = '<div style=\"color: #4caf50; padding: 10px;\">No anomalies detected</div>';\n"
        "          }\n"
        "\n"
        "          document.getElementById('last-update').textContent = now.toLocaleString();\n"
        "        })\n"
        "        .catch(error => {\n"
        "          console.error('Error fetching metrics:', error);\n"
        "          document.getElementById('loading').textContent = 'Error loading metrics. Process may have terminated.';\n"
        "        });\n"
        "    }\n"
        "\n"
        "    updateMetrics();\n"
        "    setInterval(updateMetrics, 2000);\n"
        "  </script>\n"
        "</body>\n"
        "</html>\n",
        pid, pid
    );
}

int web_handle_request(int client_fd, const char *request, web_config_t *config) {
    char response[WEB_BUFFER_SIZE];
    char content[WEB_BUFFER_SIZE];
    int content_len;

    /* Parse request */
    if (strstr(request, "GET /api/metrics") != NULL) {
        /* API endpoint - return JSON */
        content_len = web_generate_metrics_json(config->monitored_pid, content,
                                                sizeof(content), config->enable_anomaly,
                                                config->enable_anomaly ? &global_detector : NULL);

        if (content_len < 0) {
            const char *error_json = "{\"error\": \"Process no longer exists\"}";
            content_len = strlen(error_json);
            strcpy(content, error_json);
        }

        snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            content_len, content
        );
    } else {
        /* Main page - return HTML */
        content_len = web_generate_html(content, sizeof(content), config->monitored_pid);

        snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            content_len, content
        );
    }

    send(client_fd, response, strlen(response), 0);
    return 0;
}

int web_dashboard_start(web_config_t *config) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[WEB_BUFFER_SIZE];

    /* Initialize server */
    server_fd = web_dashboard_init(config->port);
    if (server_fd < 0) {
        return -1;
    }

    /* Initialize anomaly detector if needed */
    if (config->enable_anomaly) {
        if (anomaly_detector_init(&global_detector, config->monitored_pid) != 0) {
            fprintf(stderr, "Warning: Failed to initialize anomaly detector\n");
            config->enable_anomaly = 0;
        }
    }

    printf("Dashboard available at: http://localhost:%d\n", config->port);
    printf("API endpoint: http://localhost:%d/api/metrics\n", config->port);
    printf("Press Ctrl+C to stop the server.\n\n");

    /* Main server loop */
    while (config->running && *(config->running)) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted by signal */
            }
            perror("accept");
            continue;
        }

        /* Read request */
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            web_handle_request(client_fd, buffer, config);
        }

        close(client_fd);
    }

    /* Cleanup */
    if (config->enable_anomaly) {
        anomaly_detector_cleanup(&global_detector);
    }
    web_dashboard_cleanup(server_fd);

    return 0;
}
