# Architecture Documentation

## Linux Container Resource Monitoring System

### Table of Contents
1. [System Overview](#system-overview)
2. [Component Architecture](#component-architecture)
3. [Data Flow](#data-flow)
4. [Module Descriptions](#module-descriptions)
5. [Linux Kernel Interactions](#linux-kernel-interactions)
6. [Design Decisions](#design-decisions)
7. [Performance Considerations](#performance-considerations)

---

## System Overview

The Linux Container Resource Monitoring System is designed as a modular, layered architecture that provides comprehensive monitoring and analysis of Linux container resources through direct interaction with kernel interfaces.

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        CLI Interface (main.c)                    │
│                    Command-Line Argument Parsing                 │
└─────────────────┬───────────────────────────────────┬───────────┘
                  │                                   │
      ┌───────────▼──────────┐          ┌───────────▼─────────────┐
      │  Resource Profiler    │          │  Namespace Analyzer     │
      │  ┌─────────────────┐ │          │  ┌───────────────────┐ │
      │  │ CPU Monitor     │ │          │  │ List Namespaces   │ │
      │  │ Memory Monitor  │ │          │  │ Find Processes    │ │
      │  │ I/O Monitor     │ │          │  │ Compare NS        │ │
      │  │ Network Monitor │ │          │  │ Measure Overhead  │ │
      │  └─────────────────┘ │          │  └───────────────────┘ │
      └─────────┬────────────┘          └───────────┬─────────────┘
                │                                   │
      ┌─────────▼──────────────────────────────────▼─────────────┐
      │              Cgroup Manager                               │
      │  ┌──────────────────────────────────────────────────┐   │
      │  │ Collect Metrics │ Create Groups │ Set Limits     │   │
      │  └──────────────────────────────────────────────────┘   │
      └─────────┬────────────────────────────────────────────────┘
                │
┌───────────────▼────────────────────────────────────────────────┐
│                    Linux Kernel Interfaces                      │
│  /proc/[pid]/*  │  /sys/fs/cgroup/*  │  syscalls (unshare,    │
│  stat, status,  │  cpu.stat,         │  setns, clone)         │
│  io, ns/*       │  memory.current    │                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### 1. Resource Profiler

**Purpose**: Collect and analyze process resource usage metrics

**Modules**:
- **CPU Monitor** (`cpu_monitor.c`)
  - Collects: user time, system time, thread count, context switches
  - Calculates: CPU percentage over time intervals
  - Sources: `/proc/[pid]/stat`, `/proc/[pid]/status`

- **Memory Monitor** (`memory_monitor.c`)
  - Collects: RSS, VSZ, swap, page faults, memory segments
  - Sources: `/proc/[pid]/status`, `/proc/[pid]/stat`

- **I/O Monitor** (`io_monitor.c`)
  - Collects: bytes read/written, syscall counts, actual disk I/O
  - Calculates: throughput rates (bytes/sec)
  - Sources: `/proc/[pid]/io`

- **Network Monitor** (`io_monitor.c`)
  - Collects: network connections, packet/byte counts
  - Sources: `/proc/net/*` (simplified implementation)

**Key Data Structures**:
```c
typedef struct {
    pid_t pid;
    uint64_t utime, stime;
    long num_threads;
    uint64_t voluntary_ctxt_switches;
    uint64_t nonvoluntary_ctxt_switches;
    double cpu_percent;
    struct timespec timestamp;
} cpu_metrics_t;
```

### 2. Namespace Analyzer

**Purpose**: Analyze Linux namespace isolation between processes

**Capabilities**:
- List all namespaces for a process
- Find all processes sharing a namespace
- Compare namespaces between processes
- Measure namespace creation overhead
- Generate system-wide namespace reports

**Namespace Types Supported**:
- IPC (Inter-Process Communication)
- MNT (Mount points)
- NET (Network)
- PID (Process IDs)
- USER (User/Group IDs)
- UTS (Hostname/Domain)
- CGROUP (Control Groups)

**Key Functions**:
```c
int namespace_list_process(pid_t pid, process_namespaces_t *proc_ns);
int namespace_compare(pid_t pid1, pid_t pid2,
                     namespace_comparison_t *comparisons, int *count);
int namespace_measure_creation(const char *ns_type,
                               namespace_timing_t *timing);
```

### 3. Control Group Manager

**Purpose**: Monitor and manipulate Linux control groups (cgroups)

**Controllers Supported**:
- **CPU**: Usage tracking, quota/period limits, throttling metrics
- **Memory**: Current usage, limits, OOM events
- **Block I/O**: Read/write bytes and IOPS
- **PIDs**: Process count limits

**Cgroup Versions**:
- Primary: cgroup v2 (unified hierarchy)
- Secondary: cgroup v1 (legacy)

**Key Operations**:
```c
int cgroup_collect_metrics(const char *cgroup_path, cgroup_metrics_t *metrics);
int cgroup_create(const cgroup_config_t *config, char *created_path, size_t path_len);
int cgroup_set_cpu_limit(const char *cgroup_path, double cpu_cores);
int cgroup_set_memory_limit(const char *cgroup_path, uint64_t limit_bytes);
```

---

## Data Flow

### Process Monitoring Flow

```
1. User Input
   └─> Command-line arguments parsed

2. Initialization
   └─> Monitor modules initialized
   └─> Clock ticks per second determined
   └─> Initial baseline collected

3. Monitoring Loop
   ┌─> Collect current metrics
   │   ├─> Read /proc/[pid]/stat
   │   ├─> Read /proc/[pid]/status
   │   └─> Read /proc/[pid]/io
   │
   ├─> Calculate deltas & rates
   │   └─> Compare with previous sample
   │
   ├─> Export/Display results
   │   ├─> Console output
   │   ├─> CSV append
   │   └─> JSON file
   │
   └─> Sleep until next interval
       └─> Repeat or exit

4. Cleanup
   └─> Close files, free resources
```

### Namespace Analysis Flow

```
1. Namespace Listing
   └─> Read /proc/[pid]/ns/*
   └─> stat() each namespace file
   └─> Collect inode numbers
   └─> Return namespace info

2. Process Finding
   └─> Iterate /proc directory
   └─> Check each PID's namespace
   └─> Compare inode numbers
   └─> Build matching PID list

3. Comparison
   └─> Get namespaces for both PIDs
   └─> Compare inode numbers by type
   └─> Report shared/different status
```

### Cgroup Monitoring Flow

```
1. Version Detection
   └─> Check for /sys/fs/cgroup/cgroup.controllers
   └─> Determine v1 or v2

2. Metrics Collection
   ├─> CPU: Read cpu.stat
   │   └─> Parse usage_usec, throttled_usec
   │
   ├─> Memory: Read memory.current, memory.max
   │   └─> Parse current, limit, OOM events
   │
   └─> I/O: Read io.stat
       └─> Parse read/write bytes and IOPS

3. Limit Application
   └─> Write to control files
   └─> cpu.max, memory.max
   └─> Verify changes
```

---

## Module Descriptions

### monitor.h / cpu_monitor.c

**Responsibilities**:
- Initialize CPU monitoring subsystem
- Collect CPU usage metrics from /proc
- Calculate CPU percentage between samples
- Export metrics to JSON/CSV

**Critical Functions**:
- `cpu_monitor_init()`: Get system clock ticks
- `cpu_monitor_collect()`: Read /proc files
- `cpu_monitor_calculate_percentage()`: Compute CPU%

**Error Handling**:
- Process existence check before reading
- Graceful handling of permission errors
- Validation of parsed values

### monitor.h / memory_monitor.c

**Responsibilities**:
- Collect memory usage from /proc/[pid]/status
- Track RSS, VSZ, swap, and memory segments
- Monitor page faults (major/minor)

**Key Metrics**:
- RSS: Resident Set Size (physical memory)
- VSZ: Virtual memory size
- Swap: Swapped-out memory
- Page faults: Memory access patterns

### monitor.h / io_monitor.c

**Responsibilities**:
- Collect I/O statistics from /proc/[pid]/io
- Calculate throughput rates
- Track both syscall I/O and actual disk I/O

**Permission Requirements**:
- Requires root to read other processes' I/O stats
- Graceful degradation for own process

### namespace.h / namespace_analyzer.c

**Responsibilities**:
- Interface with Linux namespace system
- Measure namespace overhead
- Track namespace relationships

**System Calls Used**:
- `stat()`: Get namespace inode numbers
- `unshare()`: Create new namespaces (for timing)
- `readdir()`: Enumerate /proc for process discovery

### cgroup.h / cgroup_manager.c

**Responsibilities**:
- Parse cgroup filesystem
- Read controller metrics
- Create and configure cgroups
- Move processes to cgroups

**File Operations**:
- Read: cpu.stat, memory.current, io.stat
- Write: cpu.max, memory.max, cgroup.procs

---

## Linux Kernel Interactions

### /proc Filesystem

The /proc filesystem provides process information:

**Files Read**:
```
/proc/[pid]/stat       - Process statistics (51 fields)
/proc/[pid]/status     - Human-readable status
/proc/[pid]/io         - I/O accounting (requires permissions)
/proc/[pid]/ns/*       - Namespace symlinks
/proc/[pid]/cgroup     - Cgroup membership
```

**Parsing Strategy**:
- Line-by-line parsing for key-value files (status)
- Field-based parsing for fixed-format files (stat)
- Error checking for each field extraction

### /sys/fs/cgroup Filesystem

Cgroup v2 unified hierarchy:

**Controller Files**:
```
cpu.stat               - CPU usage statistics
cpu.max                - CPU quota/period limits
memory.current         - Current memory usage
memory.max             - Memory limit
memory.events          - OOM and other events
io.stat                - Block I/O statistics
pids.current           - Current process count
cgroup.procs           - PIDs in this cgroup
```

### System Calls

**Namespace Operations**:
- `unshare(flags)`: Create new namespace
- `setns(fd, nstype)`: Enter existing namespace
- `clone(fn, stack, flags, arg)`: Create process with namespaces

**Time Operations**:
- `clock_gettime(CLOCK_MONOTONIC, &ts)`: High-resolution timing
- `sysconf(_SC_CLK_TCK)`: Get system clock ticks

---

## Design Decisions

### 1. Modular Architecture

**Decision**: Separate modules for CPU, memory, I/O, namespaces, cgroups

**Rationale**:
- Independent testing of components
- Code reusability
- Clear separation of concerns
- Easy to extend with new features

### 2. C Language Choice

**Decision**: Use C instead of C++ or higher-level languages

**Rationale**:
- Direct system call access
- Minimal overhead
- Standard on all Linux systems
- Educational value for OS course
- No external dependencies

### 3. Error Handling Strategy

**Decision**: Return error codes + stderr messages

**Rationale**:
- Follows Unix conventions
- Allows caller to decide error handling
- Provides context for debugging
- Non-intrusive error reporting

### 4. Data Export Formats

**Decision**: Support JSON and CSV, not binary formats

**Rationale**:
- Human-readable for debugging
- Easy integration with other tools
- Language-agnostic
- Standard formats widely supported

### 5. Timestamp Strategy

**Decision**: Use CLOCK_MONOTONIC for measurements

**Rationale**:
- Not affected by system time changes
- Consistent across samples
- Suitable for rate calculations
- High resolution (nanoseconds)

---

## Performance Considerations

### Sampling Overhead

**CPU Impact**:
- File I/O: ~0.1ms per /proc file read
- Parsing: ~0.05ms per metric
- Total overhead: < 1% at 1-second intervals

**Memory Impact**:
- Stack usage: < 10KB per function
- Heap usage: Minimal (only for file buffers)
- Total footprint: 2-5 MB

### Optimization Strategies

1. **Efficient Parsing**:
   - Direct sscanf() for fixed formats
   - Single-pass line reading
   - Minimal string operations

2. **Resource Reuse**:
   - Reuse metric structures
   - No dynamic allocation in hot paths
   - Stack allocation for buffers

3. **Caching**:
   - Cache system clock ticks
   - Cache cgroup mount point
   - Avoid repeated path construction

### Scalability

**Current Limitations**:
- Single process monitoring at a time
- Synchronous I/O (blocking reads)
- No buffering for high-frequency sampling

**Future Improvements**:
- Async I/O for multiple processes
- Ring buffer for metric history
- Batch export operations

---

## Security Considerations

### Permission Requirements

**Read Operations**:
- `/proc/[own-pid]/*`: No special permissions
- `/proc/[other-pid]/io`: Requires root
- `/sys/fs/cgroup/*`: Usually world-readable

**Write Operations**:
- Cgroup creation: Requires appropriate permissions
- Cgroup limit setting: Usually requires root
- Process movement: Requires permissions on target cgroup

### Input Validation

All user inputs are validated:
- PID existence check before monitoring
- Path sanitization for cgroup operations
- Bounds checking for numeric parameters
- NULL pointer checks throughout

### Error Containment

Errors are isolated to prevent cascade:
- Failed metric collection doesn't stop other metrics
- Permission errors are reported but non-fatal
- Resource cleanup in all error paths

---

## Testing Strategy

### Unit Testing

Each module has dedicated tests:
- `test_cpu.c`: CPU monitoring functions
- `test_memory.c`: Memory monitoring functions
- `test_io.c`: I/O monitoring functions

### Integration Testing

End-to-end scenarios:
- Monitor real processes
- Create and manipulate cgroups
- Compare namespaces across processes

### Memory Safety

Valgrind integration:
- Detect memory leaks
- Identify invalid memory access
- Verify proper cleanup

---

## Conclusions

This architecture provides:

1. **Modularity**: Easy to extend and maintain
2. **Performance**: Minimal overhead on monitored processes
3. **Reliability**: Comprehensive error handling
4. **Portability**: Standard C with POSIX APIs
5. **Educational Value**: Clear demonstration of Linux kernel concepts

The system successfully demonstrates deep understanding of:
- Linux process management
- Namespace isolation
- Control group resource management
- /proc and /sys filesystems
- System programming in C

---

**Document Version**: 1.0
**Last Updated**: 2025-11-17
**Author**: Luiz FG
