# Linux Container Resource Monitoring System

A comprehensive monitoring and analysis system for Linux container resources, demonstrating deep understanding of Linux kernel mechanisms (namespaces and cgroups) used in containerization.

## Author

**Luiz Felipe Greboge**
Operating Systems Course - RA3 Assignment
Individual Project

## Project Overview

This project implements a professional-grade resource monitoring system with three main components:

1. **Resource Profiler**: Collects detailed process metrics (CPU, Memory, I/O, Network)
2. **Namespace Analyzer**: Analyzes and reports namespace isolation
3. **Control Group Manager**: Analyzes and manipulates cgroups with resource limits
4.  **Monitoring Interfaces**: Provides real-time UIs (Ncurses, Web) and anomaly detection.

## Features

### Resource Profiler
- Monitor processes by PID with configurable sampling interval
- Collect comprehensive metrics from `/proc/[pid]/stat`, `/proc/[pid]/status`, `/proc/[pid]/io`
- CPU metrics: user time, system time, context switches, threads, CPU percentage
- Memory metrics: RSS, VSZ, page faults, swap usage
- I/O metrics: bytes read/written, syscalls, disk operations, throughput rates
- Export data in CSV or JSON format
- Real-time console output

### Namespace Analyzer
- List all namespaces for a process
- Find all processes in a specific namespace
- Compare namespaces between two processes
- Generate system-wide namespace report
- Measure namespace creation overhead
- Support for all namespace types: IPC, MNT, NET, PID, USER, UTS, CGROUP

### Control Group Manager
- Read metrics from all cgroup controllers (CPU, Memory, BlkIO, PIDs)
- Parse `/sys/fs/cgroup/` hierarchy
- Create experimental cgroups
- Move processes to cgroups
- Apply CPU and Memory limits
- Generate utilization vs limits reports
- Support for both cgroup v1 and v2

### Monitoring & UI
- **Anomaly Detection**: Real-time statistical anomaly detection (using moving average and standard deviation) for CPU, Memory, and I/O metrics.
- **Ncurses UI**: A real-time terminal dashboard for monitoring a single process (activated with `--ui ncurses`).
- **Web Dashboard**: A web-based interface for real-time metrics (activated with `--web PORT`).

## Requirements

### System Requirements
- Linux kernel 4.x or higher
- Ubuntu 24.04+ or similar distribution
- WSL (Windows Subsystem for Linux) compatible

### Build Dependencies
- GCC compiler with C11 support
- GNU Make
- Standard C library (glibc)
- `libncurses` (for Ncurses UI)
- `librt`, `libm`

### Optional Dependencies
- Python 3.x (for visualization scripts)
- matplotlib (for plotting: `pip3 install matplotlib`)
- valgrind (for memory leak detection)

## Installation

### Clone and Build

```bash
# Navigate to the project directory
cd resource-monitor

# Build the project
make

# Run tests (optional)
make test
make run-tests

# Check for memory leaks (optional)
make valgrind

# Install system-wide (optional, requires sudo)
sudo make install
```

### Build Targets

- `make` or `make all` - Build release version
- `make release` - Build optimized release version
- `make debug` - Build debug version with symbols
- `make test` - Build test suite
- `make run-tests` - Build and run all tests
- `make valgrind` - Run valgrind memory leak check
- `make clean` - Remove build artifacts
- `make install` - Install to /usr/local/bin
- `make uninstall` - Remove from /usr/local/bin

## Usage

### Basic Process Monitoring

```bash
# Monitor a process by PID
./bin/resource-monitor -p 1234

# Monitor with specific interval and duration
./bin/resource-monitor -p 1234 -i 2 -d 60

# Monitor and export to CSV
./bin/resource-monitor -p 1234 -i 1 -d 30 -o metrics.csv -f csv

# Monitor specific metrics only
./bin/resource-monitor -p 1234 -m cpu
./bin/resource-monitor -p 1234 -m memory
./bin/resource-monitor -p 1234 -m io
```

### Namespace Analysis

```bash
# List namespaces for a process
./bin/resource-monitor -l 1234

# Compare namespaces between two processes
./bin/resource-monitor -c 1234,5678

# Generate system-wide namespace report
./bin/resource-monitor -r

# Measure namespace creation overhead
./bin/resource-monitor -t
```

### Cgroup Management

```bash
# View cgroup metrics
./bin/resource-monitor -g /user.slice

# Monitor specific cgroup
sudo ./bin/resource-monitor -g /test-cgroup
```

### Ncurses UI

```bash
# Launch the real-time terminal dashboard
./bin/resource-monitor -p 1234 --ui ncurses
```

### Web Dashboard

```bash
# Start the web dashboard on port 8080
./bin/resource-monitor -p 1234 --web 8080
# Now open http://localhost:8080 in your browser
```

### Visualization

```bash
# Generate plots from collected data
./scripts/visualize.py --cpu metrics.csv --output cpu_plot.png

# Generate summary report
./scripts/visualize.py --cpu metrics.csv --memory metrics.memory.csv --summary

# Plot all metrics
./scripts/visualize.py --cpu cpu.csv --memory mem.csv --io io.csv --type all --output report.png
```

### Tool Comparison

```bash
# Compare with existing tools
./scripts/compare_tools.sh
```

## Command-Line Options

### Resource Profiler Options
- `-p, --pid PID` - Monitor process with PID
- `-i, --interval SEC` - Sampling interval in seconds (default: 1)
- `-d, --duration SEC` - Monitoring duration in seconds (default: infinite)
- `-o, --output FILE` - Output file for metrics
- `-f, --format FORMAT` - Output format: json, csv, console (default: console)
- `-m, --metrics TYPE` - Metric types: cpu, memory, io, all (default: all)

### Namespace Analyzer Options
- `-l, --list-ns PID` - List namespaces for PID
- `-c, --compare P1,P2` - Compare namespaces between two PIDs
- `-r, --report` - Generate system-wide namespace report
- `-t, --timing` - Measure namespace creation overhead

### Control Group Manager Options
- `-g, --cgroup PATH` - Monitor cgroup at PATH
- `--cpu-limit CORES` - Set CPU limit in cores (e.g., 0.5, 1.0)
- `--mem-limit MB` - Set memory limit in MB

### Anomaly Detection Options
- `-a, --anomaly` - Enable anomaly detection
- `--anomaly-stats` - Print anomaly detection statistics

### Web Dashboard Options
- `--web PORT` - Start web dashboard on PORT (default: 8080)

### Display Options
- `--ui MODE` - User interface mode: console, ncurses (default: console)

### General Options
- `-h, --help` - Show help message
- `-v, --verbose` - Enable verbose output

## Project Structure

```
resource-monitor/
├── README.md              # This file
├── Makefile              # Build configuration
├── docs/
│   ├── ARCHITECTURE.md   # Architecture documentation
│   └── EXPERIMENTS.md    # Experiments documentation
├── include/
│   ├── monitor.h         # Resource monitoring header
│   ├── namespace.h       # Namespace analysis header
│   ├── cgroup.h          # Cgroup management header
│   ├── anomaly.h         # Anomaly detection header
│   ├── ncurses_ui.h      # Ncurses UI header
│   └── web_dashboard.h   # Web dashboard header
├── src/
│   ├── cpu_monitor.c     # CPU monitoring implementation
│   ├── memory_monitor.c  # Memory monitoring implementation
│   ├── io_monitor.c      # I/O monitoring implementation
│   ├── namespace_analyzer.c  # Namespace analysis implementation
│   ├── cgroup_manager.c  # Cgroup management implementation
│   ├── anomaly_detector.c  # Anomaly detection implementation
│   ├── ncurses_ui.c      # Ncurses UI implementation
│   ├── web_dashboard.c   # Web dashboard implementation
│   └── main.c            # Main program and CLI
├── tests/
│   ├── test_cpu.c        # CPU monitor tests
│   ├── test_memory.c     # Memory monitor tests
│   └── test_io.c         # I/O monitor tests
└── scripts/
    ├── visualize.py      # Visualization script
    └── compare_tools.sh  # Tool comparison script
```

## Implementation Details

### Data Collection Sources
- `/proc/[pid]/stat` - Process statistics (CPU time, threads, etc.)
- `/proc/[pid]/status` - Process status (memory, context switches)
- `/proc/[pid]/io` - I/O statistics (requires root for other processes)
- `/proc/[pid]/ns/*` - Namespace information
- `/sys/fs/cgroup/` - Cgroup hierarchy and metrics

### System Calls Used
- `clock_gettime()` - High-resolution timestamps
- `unshare()` - Create new namespaces
- `setns()` - Enter existing namespace
- `clone()` - Create process with new namespaces

### Error Handling
All system calls and file operations include comprehensive error checking with appropriate error messages. The code handles:
- Non-existent processes
- Permission denied errors
- Missing or inaccessible files
- Invalid input parameters
- Resource allocation failures

## Testing

### Unit Tests

The project includes comprehensive unit tests for all major components:

```bash
# Build and run all tests
make run-tests

# Run individual test suites
./bin/test_cpu
./bin/test_memory
sudo ./bin/test_io  # I/O tests require root
```

### Memory Leak Testing

```bash
# Run valgrind memory check
make valgrind

# Check the generated report
cat valgrind-report.txt
```

### Integration Testing

Test the complete system by monitoring real processes:

```bash
# Start a long-running process
sleep 300 &
SLEEP_PID=$!

# Monitor it
./bin/resource-monitor -p $SLEEP_PID -i 1 -d 10 -o test.csv -f csv

# Visualize results
./scripts/visualize.py --cpu test.csv --summary
```

## Compilation Flags

The project compiles with strict warning flags to ensure code quality:

- `-Wall` - Enable all warnings
- `-Wextra` - Enable extra warnings
- `-std=c11` - Use C11 standard
- `-D_GNU_SOURCE` - Enable GNU extensions
- `-O2` - Optimization level 2 (release build)
- `-g -O0` - Debug symbols with no optimization (debug build)

## Known Limitations

1. **I/O Monitoring**: Reading `/proc/[pid]/io` requires root permissions for processes not owned by the current user
2. **Network Metrics**: Network monitoring is simplified and may not cover all use cases
3. **Cgroup v1 Support**: Primary focus is on cgroup v2; v1 support is partial
4. **WSL Limitations**: Some namespace operations may be restricted in WSL environments

## Troubleshooting

### Permission Denied Errors

```bash
# Run with sudo for full I/O and cgroup access
sudo ./bin/resource-monitor -p 1234

# Or use for your own processes
./bin/resource-monitor -p $$
```

### Compilation Warnings

The project should compile with ZERO warnings. If you see warnings:

```bash
# Clean and rebuild
make clean
make

# Check GCC version
gcc --version  # Should be 7.0 or higher
```

### Missing Dependencies

```bash
# Install build essentials
sudo apt-get update
sudo apt-get install build-essential libncurses-dev

# Install Python dependencies
pip3 install matplotlib
```

## Performance

The monitoring system is designed to have minimal overhead:

- CPU overhead: < 1% for 1-second sampling interval
- Memory footprint: ~2-5 MB
- I/O impact: Minimal (reads small /proc files)

## License

This project is developed for academic purposes as part of the Operating Systems course.

## Contributing

This is an individual academic project. No external contributions are accepted.

## References

- Linux Kernel Documentation: https://www.kernel.org/doc/html/latest/
- man pages: proc(5), cgroups(7), namespaces(7)
- Docker Documentation: Container Runtime Architecture
- systemd Documentation: Control Group Interface

## Contact

For questions or issues related to this project, please contact the author through the course platform.

---

**Built with C for maximum performance and compatibility**
**Tested on Ubuntu 24.04 LTS with Linux Kernel 5.15+**
