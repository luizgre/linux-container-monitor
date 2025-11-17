#!/usr/bin/env python3
"""
Resource Monitoring Visualization Script
Generates plots from CSV metrics collected by the resource monitor
"""

import sys
import csv
import argparse
from datetime import datetime
import json

try:
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not found. Install with: pip3 install matplotlib")

def parse_csv_metrics(filename):
    """Parse CSV file and return list of metrics"""
    metrics = []
    try:
        with open(filename, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                metrics.append(row)
    except Exception as e:
        print(f"Error reading {filename}: {e}")
        return None
    return metrics

def plot_cpu_metrics(metrics, output_file=None):
    """Generate CPU usage plot from metrics"""
    if not HAS_MATPLOTLIB:
        print("Cannot plot without matplotlib")
        return

    timestamps = []
    cpu_percentages = []

    for m in metrics:
        try:
            ts = float(m['timestamp'].split('.')[0])
            timestamps.append(datetime.fromtimestamp(ts))
            cpu_percentages.append(float(m['cpu_percent']))
        except (KeyError, ValueError) as e:
            print(f"Warning: Skipping invalid row: {e}")
            continue

    if not timestamps:
        print("No valid data to plot")
        return

    plt.figure(figsize=(12, 6))
    plt.plot(timestamps, cpu_percentages, 'b-', linewidth=2, marker='o')
    plt.xlabel('Time')
    plt.ylabel('CPU Usage (%)')
    plt.title('CPU Usage Over Time')
    plt.grid(True, alpha=0.3)
    plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
    plt.xticks(rotation=45)
    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=300)
        print(f"Plot saved to {output_file}")
    else:
        plt.show()

def plot_memory_metrics(metrics, output_file=None):
    """Generate memory usage plot from metrics"""
    if not HAS_MATPLOTLIB:
        print("Cannot plot without matplotlib")
        return

    timestamps = []
    rss_values = []
    vsz_values = []

    for m in metrics:
        try:
            ts = float(m['timestamp'].split('.')[0])
            timestamps.append(datetime.fromtimestamp(ts))
            rss_values.append(float(m['rss_kb']) / 1024)  # Convert to MB
            vsz_values.append(float(m['vsz_kb']) / 1024)  # Convert to MB
        except (KeyError, ValueError) as e:
            print(f"Warning: Skipping invalid row: {e}")
            continue

    if not timestamps:
        print("No valid data to plot")
        return

    plt.figure(figsize=(12, 6))
    plt.plot(timestamps, rss_values, 'r-', linewidth=2, marker='o', label='RSS')
    plt.plot(timestamps, vsz_values, 'g-', linewidth=2, marker='s', label='VSZ')
    plt.xlabel('Time')
    plt.ylabel('Memory (MB)')
    plt.title('Memory Usage Over Time')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
    plt.xticks(rotation=45)
    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=300)
        print(f"Plot saved to {output_file}")
    else:
        plt.show()

def plot_io_metrics(metrics, output_file=None):
    """Generate I/O usage plot from metrics"""
    if not HAS_MATPLOTLIB:
        print("Cannot plot without matplotlib")
        return

    timestamps = []
    read_rates = []
    write_rates = []

    for m in metrics:
        try:
            ts = float(m['timestamp'].split('.')[0])
            timestamps.append(datetime.fromtimestamp(ts))
            # Convert to KB/s
            read_rates.append(float(m['read_rate']) / 1024)
            write_rates.append(float(m['write_rate']) / 1024)
        except (KeyError, ValueError) as e:
            print(f"Warning: Skipping invalid row: {e}")
            continue

    if not timestamps:
        print("No valid data to plot")
        return

    plt.figure(figsize=(12, 6))
    plt.plot(timestamps, read_rates, 'b-', linewidth=2, marker='o', label='Read Rate')
    plt.plot(timestamps, write_rates, 'r-', linewidth=2, marker='s', label='Write Rate')
    plt.xlabel('Time')
    plt.ylabel('I/O Rate (KB/s)')
    plt.title('I/O Throughput Over Time')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
    plt.xticks(rotation=45)
    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=300)
        print(f"Plot saved to {output_file}")
    else:
        plt.show()

def generate_summary_report(cpu_file=None, memory_file=None, io_file=None):
    """Generate a summary report from metrics files"""
    print("\n" + "="*60)
    print("RESOURCE MONITORING SUMMARY REPORT")
    print("="*60 + "\n")

    if cpu_file:
        metrics = parse_csv_metrics(cpu_file)
        if metrics:
            cpu_values = [float(m['cpu_percent']) for m in metrics if 'cpu_percent' in m]
            if cpu_values:
                print("CPU Metrics:")
                print(f"  Average CPU: {sum(cpu_values)/len(cpu_values):.2f}%")
                print(f"  Peak CPU: {max(cpu_values):.2f}%")
                print(f"  Min CPU: {min(cpu_values):.2f}%")
                print()

    if memory_file:
        metrics = parse_csv_metrics(memory_file)
        if metrics:
            rss_values = [float(m['rss_kb'])/1024 for m in metrics if 'rss_kb' in m]
            if rss_values:
                print("Memory Metrics:")
                print(f"  Average RSS: {sum(rss_values)/len(rss_values):.2f} MB")
                print(f"  Peak RSS: {max(rss_values):.2f} MB")
                print(f"  Min RSS: {min(rss_values):.2f} MB")
                print()

    if io_file:
        metrics = parse_csv_metrics(io_file)
        if metrics:
            read_rates = [float(m['read_rate'])/1024 for m in metrics if 'read_rate' in m]
            write_rates = [float(m['write_rate'])/1024 for m in metrics if 'write_rate' in m]
            if read_rates and write_rates:
                print("I/O Metrics:")
                print(f"  Average Read Rate: {sum(read_rates)/len(read_rates):.2f} KB/s")
                print(f"  Average Write Rate: {sum(write_rates)/len(write_rates):.2f} KB/s")
                print(f"  Peak Read Rate: {max(read_rates):.2f} KB/s")
                print(f"  Peak Write Rate: {max(write_rates):.2f} KB/s")
                print()

    print("="*60 + "\n")

def main():
    parser = argparse.ArgumentParser(description='Visualize resource monitoring metrics')
    parser.add_argument('--cpu', help='CPU metrics CSV file')
    parser.add_argument('--memory', help='Memory metrics CSV file')
    parser.add_argument('--io', help='I/O metrics CSV file')
    parser.add_argument('--output', '-o', help='Output file for plot (PNG)')
    parser.add_argument('--type', choices=['cpu', 'memory', 'io', 'all'],
                       default='all', help='Type of plot to generate')
    parser.add_argument('--summary', action='store_true',
                       help='Generate summary report')

    args = parser.parse_args()

    if args.summary:
        generate_summary_report(args.cpu, args.memory, args.io)
        return

    if args.type == 'cpu' or args.type == 'all':
        if args.cpu:
            metrics = parse_csv_metrics(args.cpu)
            if metrics:
                output = args.output if args.type == 'cpu' else \
                        (args.output.replace('.png', '_cpu.png') if args.output else None)
                plot_cpu_metrics(metrics, output)
        else:
            print("Error: --cpu file required for CPU plot")

    if args.type == 'memory' or args.type == 'all':
        if args.memory:
            metrics = parse_csv_metrics(args.memory)
            if metrics:
                output = args.output if args.type == 'memory' else \
                        (args.output.replace('.png', '_memory.png') if args.output else None)
                plot_memory_metrics(metrics, output)
        else:
            print("Error: --memory file required for memory plot")

    if args.type == 'io' or args.type == 'all':
        if args.io:
            metrics = parse_csv_metrics(args.io)
            if metrics:
                output = args.output if args.type == 'io' else \
                        (args.output.replace('.png', '_io.png') if args.output else None)
                plot_io_metrics(metrics, output)
        else:
            print("Error: --io file required for I/O plot")

if __name__ == '__main__':
    main()
