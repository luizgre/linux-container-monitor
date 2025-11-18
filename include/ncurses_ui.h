#ifndef NCURSES_UI_H
#define NCURSES_UI_H

#include "monitor.h"
#include "anomaly.h"
#include <sys/types.h>

/**
 * Initialize ncurses UI
 * Returns 0 on success, -1 on failure
 */
int ncurses_ui_init(void);

/**
 * Cleanup and restore terminal
 */
void ncurses_ui_cleanup(void);

/**
 * Draw header with title and process info
 */
void ncurses_ui_draw_header(const char *title, pid_t pid, int elapsed);

/**
 * Draw CPU metrics at specified line
 */
void ncurses_ui_draw_cpu_metrics(const cpu_metrics_t *cpu, int line);

/**
 * Draw memory metrics at specified line
 */
void ncurses_ui_draw_memory_metrics(const memory_metrics_t *mem, int line);

/**
 * Draw I/O metrics at specified line
 */
void ncurses_ui_draw_io_metrics(const io_metrics_t *io, int line);

/**
 * Draw anomaly alert at specified line
 */
void ncurses_ui_draw_anomaly(const anomaly_event_t *anomaly, int line);

/**
 * Draw horizontal separator at specified line
 */
void ncurses_ui_draw_separator(int line);

/**
 * Update status bar
 */
void ncurses_ui_update_status(const char *status);

/**
 * Refresh metrics window
 */
void ncurses_ui_refresh(void);

/**
 * Clear metrics window
 */
void ncurses_ui_clear_metrics(void);

/**
 * Check if user pressed 'q' to quit
 * Returns 1 if should quit, 0 otherwise
 */
int ncurses_ui_check_quit(void);

#endif /* NCURSES_UI_H */
