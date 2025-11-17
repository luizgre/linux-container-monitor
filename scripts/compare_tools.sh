#!/bin/bash
# Compare resource-monitor with existing tools (docker stats, systemd-cgtop)

set -e

echo "=================================================="
echo "Resource Monitoring Tools Comparison"
echo "=================================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if required tools are available
check_tool() {
    if command -v $1 &> /dev/null; then
        echo -e "${GREEN}[✓]${NC} $1 is available"
        return 0
    else
        echo -e "${RED}[✗]${NC} $1 is not available"
        return 1
    fi
}

echo "Checking available tools:"
check_tool "resource-monitor" || echo "  (Will use ./bin/resource-monitor)"
check_tool "docker"
check_tool "systemd-cgtop"
check_tool "ps"
check_tool "top"
echo ""

# Find a process to monitor (use current shell)
TEST_PID=$$
echo "Test process PID: $TEST_PID"
echo ""

# Test 1: Basic process monitoring
echo "=================================================="
echo "Test 1: Basic Process Monitoring"
echo "=================================================="
echo ""

echo -e "${YELLOW}resource-monitor output:${NC}"
if [ -f "./bin/resource-monitor" ]; then
    ./bin/resource-monitor -p $TEST_PID -i 1 -d 5 -m all
else
    resource-monitor -p $TEST_PID -i 1 -d 5 -m all
fi
echo ""

echo -e "${YELLOW}ps output (for comparison):${NC}"
ps -p $TEST_PID -o pid,comm,%cpu,%mem,vsz,rss,stat
echo ""

# Test 2: Namespace analysis
echo "=================================================="
echo "Test 2: Namespace Analysis"
echo "=================================================="
echo ""

echo -e "${YELLOW}resource-monitor namespace listing:${NC}"
if [ -f "./bin/resource-monitor" ]; then
    ./bin/resource-monitor -l $TEST_PID
else
    resource-monitor -l $TEST_PID
fi
echo ""

echo -e "${YELLOW}System namespace info (for comparison):${NC}"
ls -la /proc/$TEST_PID/ns/
echo ""

# Test 3: Cgroup analysis (if available)
if [ -d "/sys/fs/cgroup" ]; then
    echo "=================================================="
    echo "Test 3: Cgroup Analysis"
    echo "=================================================="
    echo ""

    # Get cgroup for current process
    CGROUP_PATH=$(cat /proc/$TEST_PID/cgroup | head -1 | cut -d: -f3)
    echo "Process cgroup: $CGROUP_PATH"
    echo ""

    if [ -f "./bin/resource-monitor" ]; then
        ./bin/resource-monitor -g "$CGROUP_PATH" || echo "Cgroup monitoring requires appropriate permissions"
    else
        resource-monitor -g "$CGROUP_PATH" || echo "Cgroup monitoring requires appropriate permissions"
    fi
    echo ""

    if command -v systemd-cgtop &> /dev/null; then
        echo -e "${YELLOW}systemd-cgtop output (for comparison):${NC}"
        timeout 3 systemd-cgtop --batch -n 1 2>/dev/null || echo "systemd-cgtop not available or permission denied"
        echo ""
    fi
fi

# Test 4: Feature comparison matrix
echo "=================================================="
echo "Test 4: Feature Comparison Matrix"
echo "=================================================="
echo ""

printf "%-30s %-15s %-15s %-15s %-15s\n" "Feature" "resource-monitor" "ps" "docker stats" "systemd-cgtop"
printf "%s\n" "--------------------------------------------------------------------------------"
printf "%-30s %-15s %-15s %-15s %-15s\n" "CPU monitoring" "✓" "✓" "✓" "✓"
printf "%-30s %-15s %-15s %-15s %-15s\n" "Memory monitoring" "✓" "✓" "✓" "✓"
printf "%-30s %-15s %-15s %-15s %-15s\n" "I/O monitoring" "✓" "✗" "✓" "✓"
printf "%-30s %-15s %-15s %-15s %-15s\n" "Network monitoring" "Partial" "✗" "✓" "✗"
printf "%-30s %-15s %-15s %-15s %-15s\n" "Namespace analysis" "✓" "✗" "Partial" "✗"
printf "%-30s %-15s %-15s %-15s %-15s\n" "Cgroup analysis" "✓" "✗" "Partial" "✓"
printf "%-30s %-15s %-15s %-15s %-15s\n" "CSV export" "✓" "✗" "✗" "✗"
printf "%-30s %-15s %-15s %-15s %-15s\n" "JSON export" "✓" "✗" "✗" "✗"
printf "%-30s %-15s %-15s %-15s %-15s\n" "Configurable interval" "✓" "✗" "✗" "✗"
printf "%-30s %-15s %-15s %-15s %-15s\n" "Cgroup manipulation" "✓" "✗" "✗" "✗"
printf "%-30s %-15s %-15s %-15s %-15s\n" "Real-time visualization" "Python" "✗" "✓" "✓"
echo ""

# Test 5: Performance overhead comparison
echo "=================================================="
echo "Test 5: Performance Overhead Test"
echo "=================================================="
echo ""

echo "Running CPU-intensive workload..."
echo ""

# Create a simple CPU-intensive script
cat > /tmp/cpu_workload.sh << 'EOF'
#!/bin/bash
end=$((SECONDS+10))
while [ $SECONDS -lt $end ]; do
    echo "scale=1000; 4*a(1)" | bc -l > /dev/null
done
EOF
chmod +x /tmp/cpu_workload.sh

echo -e "${YELLOW}Without monitoring:${NC}"
/usr/bin/time -f "Time: %E\nCPU: %P" /tmp/cpu_workload.sh 2>&1
echo ""

echo -e "${YELLOW}With resource-monitor:${NC}"
/tmp/cpu_workload.sh &
WORKLOAD_PID=$!
if [ -f "./bin/resource-monitor" ]; then
    ./bin/resource-monitor -p $WORKLOAD_PID -i 1 -o /tmp/overhead_test.csv -f csv &
else
    resource-monitor -p $WORKLOAD_PID -i 1 -o /tmp/overhead_test.csv -f csv &
fi
MONITOR_PID=$!
wait $WORKLOAD_PID
kill $MONITOR_PID 2>/dev/null || true
wait $MONITOR_PID 2>/dev/null || true
echo "Monitoring complete. Metrics saved to /tmp/overhead_test.csv"
echo ""

# Cleanup
rm -f /tmp/cpu_workload.sh

echo "=================================================="
echo "Comparison Complete"
echo "=================================================="
echo ""
echo "Summary:"
echo "  - resource-monitor provides comprehensive Linux container resource monitoring"
echo "  - Combines features from multiple tools (ps, docker stats, systemd-cgtop)"
echo "  - Adds unique features: namespace analysis, cgroup manipulation, data export"
echo "  - Suitable for academic study and production container monitoring"
echo ""
