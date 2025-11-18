#include "../include/ncurses_ui.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COLOR_PAIR_HEADER 1
#define COLOR_PAIR_NORMAL 2
#define COLOR_PAIR_WARNING 3
#define COLOR_PAIR_CRITICAL 4
#define COLOR_PAIR_GOOD 5

static WINDOW *header_win = NULL;
static WINDOW *metrics_win = NULL;
static WINDOW *status_win = NULL;

int ncurses_ui_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(COLOR_PAIR_HEADER, COLOR_CYAN, COLOR_BLACK);
        init_pair(COLOR_PAIR_NORMAL, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_PAIR_WARNING, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COLOR_PAIR_CRITICAL, COLOR_RED, COLOR_BLACK);
        init_pair(COLOR_PAIR_GOOD, COLOR_GREEN, COLOR_BLACK);
    }

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    header_win = newwin(3, max_x, 0, 0);
    metrics_win = newwin(max_y - 5, max_x, 3, 0);
    status_win = newwin(2, max_x, max_y - 2, 0);

    if (!header_win || !metrics_win || !status_win) {
        ncurses_ui_cleanup();
        return -1;
    }

    scrollok(metrics_win, TRUE);

    return 0;
}

void ncurses_ui_cleanup(void) {
    if (header_win) delwin(header_win);
    if (metrics_win) delwin(metrics_win);
    if (status_win) delwin(status_win);
    endwin();
}

void ncurses_ui_draw_header(const char *title, pid_t pid, int elapsed) {
    if (!header_win) return;

    werase(header_win);
    wattron(header_win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);

    box(header_win, 0, 0);

    mvwprintw(header_win, 1, 2, "%s - PID: %d | Elapsed: %ds | Press 'q' to quit",
              title, pid, elapsed);

    wattroff(header_win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    wrefresh(header_win);
}

void ncurses_ui_draw_cpu_metrics(const cpu_metrics_t *cpu, int line) {
    if (!metrics_win || !cpu) return;

    int color = COLOR_PAIR_NORMAL;
    if (cpu->cpu_percent > 90.0) {
        color = COLOR_PAIR_CRITICAL;
    } else if (cpu->cpu_percent > 70.0) {
        color = COLOR_PAIR_WARNING;
    } else if (cpu->cpu_percent < 30.0) {
        color = COLOR_PAIR_GOOD;
    }

    wattron(metrics_win, COLOR_PAIR(color));
    mvwprintw(metrics_win, line, 2, "CPU Usage: %6.2f%%", cpu->cpu_percent);
    wattroff(metrics_win, COLOR_PAIR(color));

    mvwprintw(metrics_win, line, 25, "| User: %lu ticks", cpu->utime);
    mvwprintw(metrics_win, line, 50, "| System: %lu ticks", cpu->stime);

    mvwprintw(metrics_win, line + 1, 2, "Threads: %ld", cpu->num_threads);
    mvwprintw(metrics_win, line + 1, 25, "| Context Switches: %lu (vol) %lu (invol)",
              cpu->voluntary_ctxt_switches, cpu->nonvoluntary_ctxt_switches);
}

void ncurses_ui_draw_memory_metrics(const memory_metrics_t *mem, int line) {
    if (!metrics_win || !mem) return;

    double rss_mb = mem->rss / 1024.0;
    int color = COLOR_PAIR_NORMAL;

    if (rss_mb > 1000.0) {
        color = COLOR_PAIR_CRITICAL;
    } else if (rss_mb > 500.0) {
        color = COLOR_PAIR_WARNING;
    } else if (rss_mb < 100.0) {
        color = COLOR_PAIR_GOOD;
    }

    wattron(metrics_win, COLOR_PAIR(color));
    mvwprintw(metrics_win, line, 2, "Memory RSS: %8.2f MB", rss_mb);
    wattroff(metrics_win, COLOR_PAIR(color));

    mvwprintw(metrics_win, line, 30, "| VSZ: %.2f MB", mem->vsz / 1024.0);
    mvwprintw(metrics_win, line, 55, "| Shared: %lu KB", mem->shared);

    mvwprintw(metrics_win, line + 1, 2, "Data: %lu KB", mem->data);
    mvwprintw(metrics_win, line + 1, 25, "| Stack: %lu KB", mem->stack);
    mvwprintw(metrics_win, line + 1, 45, "| Text: %lu KB", mem->text);
    mvwprintw(metrics_win, line + 1, 65, "| Swap: %lu KB", mem->swap);
}

void ncurses_ui_draw_io_metrics(const io_metrics_t *io, int line) {
    if (!metrics_win || !io) return;

    double read_mb = io->read_rate / 1024.0;
    double write_mb = io->write_rate / 1024.0;

    int color = COLOR_PAIR_NORMAL;
    if (read_mb > 100.0 || write_mb > 100.0) {
        color = COLOR_PAIR_WARNING;
    } else if (read_mb < 1.0 && write_mb < 1.0) {
        color = COLOR_PAIR_GOOD;
    }

    wattron(metrics_win, COLOR_PAIR(color));
    mvwprintw(metrics_win, line, 2, "I/O Read:  %8.2f KB/s", io->read_rate);
    wattroff(metrics_win, COLOR_PAIR(color));

    wattron(metrics_win, COLOR_PAIR(color));
    mvwprintw(metrics_win, line, 35, "| Write: %8.2f KB/s", io->write_rate);
    wattroff(metrics_win, COLOR_PAIR(color));

    mvwprintw(metrics_win, line + 1, 2, "Read syscalls: %lu", io->syscr);
    mvwprintw(metrics_win, line + 1, 30, "| Write syscalls: %lu", io->syscw);
}

void ncurses_ui_draw_anomaly(const anomaly_event_t *anomaly, int line) {
    if (!metrics_win || !anomaly) return;

    int color;
    const char *severity_str;

    switch (anomaly->severity) {
        case SEVERITY_CRITICAL:
            color = COLOR_PAIR_CRITICAL;
            severity_str = "CRITICAL";
            break;
        case SEVERITY_HIGH:
            color = COLOR_PAIR_WARNING;
            severity_str = "HIGH";
            break;
        case SEVERITY_MEDIUM:
            color = COLOR_PAIR_WARNING;
            severity_str = "MEDIUM";
            break;
        default:
            color = COLOR_PAIR_NORMAL;
            severity_str = "LOW";
            break;
    }

    wattron(metrics_win, COLOR_PAIR(color) | A_BOLD);
    mvwprintw(metrics_win, line, 2, "[%s ANOMALY]", severity_str);
    wattroff(metrics_win, COLOR_PAIR(color) | A_BOLD);

    mvwprintw(metrics_win, line, 20, "%s", anomaly->description);
}

void ncurses_ui_draw_separator(int line) {
    if (!metrics_win) return;

    int max_y, max_x;
    getmaxyx(metrics_win, max_y, max_x);
    (void)max_y;

    mvwhline(metrics_win, line, 1, ACS_HLINE, max_x - 2);
}

void ncurses_ui_update_status(const char *status) {
    if (!status_win) return;

    werase(status_win);
    box(status_win, 0, 0);

    wattron(status_win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(status_win, 0, 2, " Status ");
    wattroff(status_win, COLOR_PAIR(COLOR_PAIR_HEADER));

    mvwprintw(status_win, 0, 12, "%s", status);
    wrefresh(status_win);
}

void ncurses_ui_refresh(void) {
    if (metrics_win) wrefresh(metrics_win);
}

void ncurses_ui_clear_metrics(void) {
    if (metrics_win) {
        werase(metrics_win);
        box(metrics_win, 0, 0);
        wattron(metrics_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        mvwprintw(metrics_win, 0, 2, " Metrics ");
        wattroff(metrics_win, COLOR_PAIR(COLOR_PAIR_HEADER));
    }
}

int ncurses_ui_check_quit(void) {
    int ch = getch();
    return (ch == 'q' || ch == 'Q');
}
