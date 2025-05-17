#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import random
import struct
import signal
import csv
import json
import datetime
import re
import matplotlib.pyplot as plt
import threading
import shutil
from pathlib import Path

# Get the absolute paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
BIN_DIR = os.path.join(PROJECT_ROOT, "bin")
DATA_DIR = os.path.join(PROJECT_ROOT, "data", "test_data")
RESULTS_DIR = os.path.join(PROJECT_ROOT, "results")
PLOTS_DIR = os.path.join(PROJECT_ROOT, "plots")

# Enable/disable debug output
DEBUG_MODE = True

# Global tracker for bulk load completions
class BulkLoadTracker:
    def __init__(self):
        self.success_events = {}
        self.lock = threading.Lock()
    
    def create_event(self, data_file):
        """Create a new completion event for a data file"""
        with self.lock:
            event = threading.Event()
            self.success_events[data_file] = event
            return event
    
    def get_event(self, data_file):
        """Get the completion event for a data file"""
        with self.lock:
            return self.success_events.get(data_file)
    
    def cleanup(self, data_file):
        """Remove an event when done"""
        with self.lock:
            if data_file in self.success_events:
                del self.success_events[data_file]

# Create global instance
bulk_load_tracker = BulkLoadTracker()

# Default/baseline configuration
BASELINE = {
    "buffer_size": 4 * 1024 * 1024,  # 4MB
    "size_ratio": 4,                 # Ratio between levels
    "thread_count": 4,               # Thread count for multithreading
    "data_size": 100 * 1024 * 1024,   # 100MB (baseline)
    "data_distribution": "uniform",  # Data distribution
    "query_distribution": "uniform", # Query distribution
    "read_write_ratio": 1,           # Equal reads and writes (1:1)
    "client_count": 1                # Number of concurrent clients
}

# Test configurations for each dimension
TEST_CONFIGS = {
    "data_size": {
        "name": "Data Size",
        "env_var": None,  # Not an env var, handled differently
        "values": [100*1024*1024, 250*1024*1024, 500*1024*1024, 1024*1024*1024],  # 100MB to 1GB
        "units": "MB",
        "scale_factor": 1/(1024*1024),  # Convert bytes to MB for display
        "plot_dir": os.path.join(PLOTS_DIR, "data_size")
    },
    "data_distribution": {
        "name": "Data Distribution",
        "env_var": None,  # Not an env var, handled differently
        "values": ["uniform", "skewed"],
        "units": "",
        "scale_factor": 1,
        "plot_dir": os.path.join(PLOTS_DIR, "data_distribution")
    },
    "query_distribution": {
        "name": "Query Distribution",
        "env_var": None,  # Not an env var, handled differently
        "values": ["uniform", "skewed"],
        "units": "",
        "scale_factor": 1,
        "plot_dir": os.path.join(PLOTS_DIR, "query_distribution")
    },
    "read_write_ratio": {
        "name": "Read-Write Ratio",
        "env_var": None,  # Not an env var, handled differently
        "values": [10, 5, 1, 1/5, 1/10],  # 10:1, 5:1, 1:1, 1:5, 1:10
        "units": "",
        "scale_factor": 1,
        "plot_dir": os.path.join(PLOTS_DIR, "read_write_ratio")
    },
    "buffer_size": {
        "name": "Buffer Size",
        "env_var": "LSMTREE_BUFFER_SIZE",
        "values": [4*1024, 16*1024, 64*1024, 256*1024, 1024*1024, 4*1024*1024, 16*1024*1024, 100*1024*1024],  # 4KB to 100MB
        "units": "KB",
        "scale_factor": 1/1024,  # Convert bytes to KB for display
        "plot_dir": os.path.join(PLOTS_DIR, "buffer_size")
    },
    "size_ratio": {
        "name": "Size Ratio Between Levels",
        "env_var": "LSMTREE_SIZE_RATIO",
        "values": [2, 4, 6, 8, 10],  # 2 to 10
        "units": "",
        "scale_factor": 1,
        "plot_dir": os.path.join(PLOTS_DIR, "size_ratio")
    },
    "thread_count": {
        "name": "Thread Count",
        "env_var": "LSMTREE_THREAD_COUNT",
        "values": [1, 2, 4, 8, 16],  # From 1 to number of cores
        "units": "",
        "scale_factor": 1,
        "plot_dir": os.path.join(PLOTS_DIR, "thread_count")
    },
    "client_count": {
        "name": "Client Count",
        "env_var": None,  # Not an env var, handled differently
        "values": [1, 2, 4, 8, 16, 32, 64],  # Up to 64 clients
        "units": "",
        "scale_factor": 1,
        "plot_dir": os.path.join(PLOTS_DIR, "client_count")
    }
}

def debug_print(message):
    """Print debug message if debug mode is enabled"""
    if DEBUG_MODE:
        timestamp = time.strftime("%H:%M:%S")
        print(f"[DEBUG {timestamp}] {message}")

def generate_test_data(size_bytes, distribution="uniform"):
    """Generate test data file with specified size and distribution"""
    os.makedirs(DATA_DIR, exist_ok=True)
    
    records_count = size_bytes // 16  # Each record is 16 bytes (8-byte key, 8-byte value)
    size_kb = size_bytes / 1024
    
    # Create output file path
    output_file = os.path.join(DATA_DIR, f"{int(size_kb)}kb_{distribution}.bin")
    
    # If file already exists with correct size, use it
    if os.path.exists(output_file) and os.path.getsize(output_file) >= size_bytes:
        debug_print(f"Using existing {int(size_kb)}KB {distribution} data file")
        return output_file
    
    debug_print(f"Generating {int(size_kb)}KB {distribution} data file with {records_count:,} records")
    
    # Generate the data based on distribution
    with open(output_file, 'wb') as f:
        if distribution == "uniform":
            # Uniform distribution - completely random keys
            for i in range(records_count):
                key = random.randint(0, 2**63 - 1)
                value = random.randint(0, 2**63 - 1)
                f.write(struct.pack("<QQ", key, value))
        elif distribution == "skewed":
            # Skewed distribution - 80% of keys in 20% of key space
            for i in range(records_count):
                if random.random() < 0.8:
                    # 80% of keys in lower 20% of key space
                    key = random.randint(0, int(0.2 * (2**63 - 1)))
                else:
                    # 20% of keys in the rest of key space
                    key = random.randint(int(0.2 * (2**63 - 1)), 2**63 - 1)
                value = random.randint(0, 2**63 - 1)
                f.write(struct.pack("<QQ", key, value))
    
    # Verify the file size
    actual_size = os.path.getsize(output_file)
    debug_print(f"Generated file size: {actual_size / 1024:.2f}KB")
    
    return output_file

def cleanup_processes():
    """Kill any server processes that might be running"""
    try:
        # Use both killall and pkill for better reliability
        subprocess.run("killall server 2>/dev/null || true", shell=True)
        subprocess.run("pkill -f 'bin/server' 2>/dev/null || true", shell=True)
        
        # Give processes time to terminate
        time.sleep(1)
        
        # Check if any processes are still running and force kill if needed
        if subprocess.run("pgrep -f 'bin/server'", shell=True, stdout=subprocess.PIPE).returncode == 0:
            print("Some server processes still running, using SIGKILL...")
            subprocess.run("pkill -9 -f 'bin/server' 2>/dev/null || true", shell=True)
            time.sleep(1)
        
        print("Cleaned up any existing server processes")
    except Exception as e:
        print(f"Error during process cleanup: {e}")
        # Continue execution even if cleanup fails

def start_server(env_vars=None):
    """Start the LSM-tree server with specified parameters"""
    print("\nStarting LSM-tree server...")
    
    # Set environment variables for configuration
    env = os.environ.copy()
    
    # Set defaults from BASELINE for valid env vars
    env["LSMTREE_BUFFER_SIZE"] = str(BASELINE["buffer_size"])
    env["LSMTREE_SIZE_RATIO"] = str(BASELINE["size_ratio"])
    env["LSMTREE_THREAD_COUNT"] = str(BASELINE["thread_count"])
    
    # Override with any provided env vars
    if env_vars:
        for key, value in env_vars.items():
            env[key] = str(value)
    
    # Enable debug output in the server
    env["LSMTREE_DEBUG"] = "1"
    
    # Start the server with stdout/stderr connected directly to pipes
    server_bin = os.path.join(BIN_DIR, "server")
    server_proc = subprocess.Popen(
        [server_bin],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,  # Line buffered
        env=env
    )
    
    # Create log file for saving output
    log_file_path = os.path.join(RESULTS_DIR, "server_logs.txt")
    log_file = open(log_file_path, "w")
    
    # Start monitoring stdout and stderr
    def monitor_output(pipe, prefix, log_file):
        for line in iter(pipe.readline, ''):
            timestamp = time.strftime("%H:%M:%S")
            output = f"[{prefix} {timestamp}] {line.strip()}"
            print(output)
            log_file.write(output + "\n")
            log_file.flush()
            
            # Check for bulk load completion
            if "Bulk load completed successfully" in line:
                print(f"[{prefix} {timestamp}] BULK LOAD DETECTED AS COMPLETED SUCCESSFULLY")
                # Signal all active events, as we don't know which specific load this is for
                with bulk_load_tracker.lock:
                    for event in bulk_load_tracker.success_events.values():
                        event.set()
            
            # Check for server started message to know when it's ready
            if "Server started on port" in line:
                debug_print(f"Server detected as ready on port")
    
    # Start monitoring stdout and stderr
    stdout_thread = threading.Thread(
        target=monitor_output, 
        args=(server_proc.stdout, "SERVER", log_file),
        daemon=True,
        name="monitor_stdout"  # Named thread to find it later
    )
    stderr_thread = threading.Thread(
        target=monitor_output, 
        args=(server_proc.stderr, "SERVER ERR", log_file),
        daemon=True,
        name="monitor_stderr"  # Named thread to find it later
    )
    
    stdout_thread.start()
    stderr_thread.start()
    
    # Wait a moment for the server to start
    time.sleep(2)
    
    # Check if server is running
    if server_proc.poll() is not None:
        print(f"Server failed to start. Exit code: {server_proc.returncode}")
        log_file.close()
        sys.exit(1)
    
    print("Server started successfully")
    
    # Wait for the server to fully initialize and start listening on the port
    # This is important to prevent connection failures when clients connect immediately
    time.sleep(3)  # Increased wait time to ensure the server is fully ready
    debug_print("Added extra delay to ensure server is fully initialized")
    
    # Store the log file in the server process object so we can close it later
    server_proc.log_file = log_file
    
    return server_proc

def load_data(data_file):
    """Load a data file into the LSM-tree"""
    if not os.path.exists(data_file):
        print(f"Error: Data file {data_file} does not exist")
        return False
    
    # Print detailed information about the file
    file_size_bytes = os.path.getsize(data_file)
    file_size_mb = file_size_bytes / (1024 * 1024)
    debug_print(f"Loading data file: {data_file}")
    debug_print(f"File size: {file_size_mb:.2f} MB ({file_size_bytes:,} bytes)")
    
    # Create a load command file
    load_file = os.path.join(SCRIPT_DIR, "load_command.txt")
    with open(load_file, 'w') as f:
        f.write(f'l "{data_file}"\n')
        f.write("q\n")
    
    debug_print(f"Created command file with load command: l \"{data_file}\"")
    
    # Setup event for monitoring completion
    completion_event = bulk_load_tracker.create_event(data_file)
    debug_print(f"Created completion event for {data_file}")
    
    # Run the client with load command
    start_time = time.time()
    debug_print(f"Starting client process at {time.strftime('%H:%M:%S')}")
    
    client_bin = os.path.join(BIN_DIR, "client")
    client_proc = subprocess.Popen(
        [client_bin],
        stdin=open(load_file, 'r'),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    debug_print(f"Client process started with PID: {client_proc.pid}")
    
    try:
        # For larger data sizes, we need a longer timeout
        # Calculate timeout based on data size (approximately 1 minute per 10MB)
        file_size_mb = file_size_bytes / (1024 * 1024)
        timeout_seconds = max(300, int(file_size_mb / 10) * 60)  # Minimum 5 minutes, or 1 minute per 10MB
        
        debug_print(f"Waiting for load to complete with timeout of {timeout_seconds} seconds")
        
        # Poll for either client completion or bulk load completion detection
        elapsed_time_calculated = False
        wait_start_time = time.time()
        
        while True:
            # Check if bulk load completed via log detection
            if completion_event.is_set():
                elapsed = time.time() - start_time
                debug_print(f"Bulk load detected as successful in logs! Elapsed time: {elapsed:.2f} seconds")
                # Kill the client as we're done
                client_proc.kill()
                elapsed_time_calculated = True
                break
            
            # Check if client exited
            if client_proc.poll() is not None:
                elapsed = time.time() - start_time
                debug_print(f"Client exited with code: {client_proc.returncode}")
                elapsed_time_calculated = True
                break
            
            # Check if we've timed out
            if time.time() - wait_start_time > timeout_seconds:
                elapsed = time.time() - start_time
                debug_print(f"Timed out after {timeout_seconds} seconds waiting for load to complete")
                client_proc.kill()
                elapsed_time_calculated = False
                break
            
            # Wait a bit before checking again
            time.sleep(0.5)
        
        # If client didn't exit, wait for it to finish with a short timeout
        if client_proc.poll() is None:
            try:
                stdout, stderr = client_proc.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                client_proc.kill()
                stdout, stderr = client_proc.communicate()
        else:
            stdout, stderr = client_proc.communicate()
        
        if not elapsed_time_calculated:
            elapsed = time.time() - start_time
        
        debug_print(f"Load completed in {elapsed:.2f} seconds")
        
        # Clean up the completion event
        bulk_load_tracker.cleanup(data_file)
        
        if completion_event.is_set() or client_proc.returncode == 0:
            debug_print(f"Data loaded successfully!")
            return True
        else:
            debug_print(f"Load failed with exit code: {client_proc.returncode}")
            debug_print(f"STDERR: {stderr}")
            return False
    except Exception as e:
        debug_print(f"Exception during loading: {str(e)}")
        client_proc.kill()
        bulk_load_tracker.cleanup(data_file)
        return False
    finally:
        if os.path.exists(load_file):
            os.remove(load_file)
            debug_print(f"Removed temporary command file: {load_file}")

def verify_data_loaded():
    """Verify data is loaded by checking stats"""
    debug_print("Verifying data is loaded by checking stats...")
    
    # Create a stats command file
    stats_file = os.path.join(SCRIPT_DIR, "stats_command.txt")
    with open(stats_file, 'w') as f:
        f.write("s\n")
        f.write("q\n")
    
    try:
        client_bin = os.path.join(BIN_DIR, "client")
        client_proc = subprocess.Popen(
            [client_bin],
            stdin=open(stats_file, 'r'),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        stdout, stderr = client_proc.communicate(timeout=10)
        
        if client_proc.returncode == 0:
            # Check if data is loaded by looking for entries and logical pairs
            stats_lines = [line for line in stdout.split('\n') if "entries" in line.lower() or "logical pairs" in line.lower()]
            
            # Look for both mentions of entries and the Logical Pairs count
            entries_found = any(line for line in stats_lines if "0 entries" not in line)
            logical_pairs_line = next((line for line in stats_lines if "logical pairs" in line.lower()), None)
            
            if entries_found or (logical_pairs_line and "0" not in logical_pairs_line.split(":")[1].strip()):
                debug_print("Data verified: Found non-zero entries or logical pairs")
                for line in stats_lines:
                    debug_print(f"  {line}")
                
                # Additional check for run files in data directory - just informational now
                runs_dir = os.path.join(DATA_DIR, "runs")
                if os.path.exists(runs_dir):
                    run_files = os.listdir(runs_dir)
                    debug_print(f"Found {len(run_files)} run files: {run_files}")
                else:
                    debug_print("No runs directory found - data may still be in buffer only")
                
                return True
            else:
                debug_print("No data found in the LSM-tree!")
                return False
        else:
            debug_print(f"Stats check failed with exit code: {client_proc.returncode}")
            debug_print(f"STDERR: {stderr}")
            return False
            
    except subprocess.TimeoutExpired:
        debug_print("Stats command timed out")
        return False
    finally:
        if os.path.exists(stats_file):
            os.remove(stats_file)

def extract_metrics(benchmark_output, operation_count, elapsed_time, read_ops=0, write_ops=0):
    """Extract or calculate metrics from benchmark operations"""
    print(f"Extracting metrics from {operation_count} operations ({read_ops} reads, {write_ops} writes)")
    
    metrics = {
        'throughput': None,         # Total operations per second
        'read_throughput': None,    # Read operations per second
        'write_throughput': None,   # Write operations per second
        'read_ios': None,           # Read I/O operations
        'write_ios': None,          # Write I/O operations
        'cache_misses': None,       # Cache misses
        'runtime': elapsed_time,    # Total runtime in seconds
        'runtime_per_op': None,     # Runtime per operation in milliseconds
        'read_runtime_per_op': None,# Runtime per read operation in milliseconds
        'write_runtime_per_op': None# Runtime per write operation in milliseconds
    }
    
    # Try to extract metrics directly from stats output first
    # Average operation times
    read_time_pattern = r'Reads:\s*([\d.]+)\s*ms/op'
    write_time_pattern = r'Writes:\s*([\d.]+)\s*ms/op'
    
    # Throughput
    read_throughput_pattern = r'Reads:\s*([\d.]+)\s*ops/sec'
    write_throughput_pattern = r'Writes:\s*([\d.]+)\s*ops/sec'
    
    # I/O counts
    read_io_pattern = r'Read I/Os:\s*([\d.]+)'
    write_io_pattern = r'Write I/Os:\s*([\d.]+)'
    
    # Extract metrics from the output
    read_time_match = re.search(read_time_pattern, benchmark_output)
    write_time_match = re.search(write_time_pattern, benchmark_output)
    read_throughput_match = re.search(read_throughput_pattern, benchmark_output)
    write_throughput_match = re.search(write_throughput_pattern, benchmark_output)
    read_io_match = re.search(read_io_pattern, benchmark_output)
    write_io_match = re.search(write_io_pattern, benchmark_output)
    
    # Update metrics with direct measurements if available
    if read_time_match:
        metrics['read_runtime_per_op'] = float(read_time_match.group(1))
    
    if write_time_match:
        metrics['write_runtime_per_op'] = float(write_time_match.group(1))
    
    if read_throughput_match:
        metrics['read_throughput'] = float(read_throughput_match.group(1))
    
    if write_throughput_match:
        metrics['write_throughput'] = float(write_throughput_match.group(1))
    
    if read_io_match:
        metrics['read_ios'] = float(read_io_match.group(1))
    
    if write_io_match:
        metrics['write_ios'] = float(write_io_match.group(1))
    
    # Calculate overall metrics based on the measured read/write metrics
    if metrics['read_runtime_per_op'] is not None and metrics['write_runtime_per_op'] is not None:
        # If we have both read and write times, calculate weighted average
        if read_ops > 0 and write_ops > 0:
            metrics['runtime_per_op'] = ((metrics['read_runtime_per_op'] * read_ops) + 
                                        (metrics['write_runtime_per_op'] * write_ops)) / operation_count
        elif read_ops > 0:
            metrics['runtime_per_op'] = metrics['read_runtime_per_op']
        elif write_ops > 0:
            metrics['runtime_per_op'] = metrics['write_runtime_per_op']
    
    # Calculate overall throughput based on the measured read/write throughputs
    if metrics['read_throughput'] is not None and metrics['write_throughput'] is not None:
        # If we have both read and write throughputs, calculate weighted average
        if read_ops > 0 and write_ops > 0:
            read_weight = read_ops / operation_count
            write_weight = write_ops / operation_count
            metrics['throughput'] = (metrics['read_throughput'] * read_weight) + (metrics['write_throughput'] * write_weight)
        elif read_ops > 0:
            metrics['throughput'] = metrics['read_throughput']
        elif write_ops > 0:
            metrics['throughput'] = metrics['write_throughput']
    
    # Fall back to end-to-end measurements if we couldn't extract direct metrics
    if metrics['throughput'] is None and operation_count > 0 and elapsed_time > 0:
        metrics['throughput'] = operation_count / elapsed_time
    
    if metrics['runtime_per_op'] is None and operation_count > 0 and elapsed_time > 0:
        metrics['runtime_per_op'] = (elapsed_time * 1000) / operation_count  # ms per op
    
    if metrics['read_throughput'] is None and read_ops > 0 and elapsed_time > 0:
        metrics['read_throughput'] = read_ops / elapsed_time
    
    if metrics['write_throughput'] is None and write_ops > 0 and elapsed_time > 0:
        metrics['write_throughput'] = write_ops / elapsed_time
    
    if metrics['read_runtime_per_op'] is None and read_ops > 0 and elapsed_time > 0:
        metrics['read_runtime_per_op'] = (elapsed_time * 1000) / read_ops
    
    if metrics['write_runtime_per_op'] is None and write_ops > 0 and elapsed_time > 0:
        metrics['write_runtime_per_op'] = (elapsed_time * 1000) / write_ops
    
    # Make sure read_ios and write_ios have default values if not extracted
    if metrics['read_ios'] is None:
        metrics['read_ios'] = 0
    
    if metrics['write_ios'] is None:
        metrics['write_ios'] = 0
    
    # Print the metrics for debugging
    print("Debug - Metrics calculated:")
    print(f"  Throughput: {metrics['throughput']} ops/sec")
    print(f"  Read throughput: {metrics['read_throughput']} reads/sec")
    print(f"  Write throughput: {metrics['write_throughput']} writes/sec")
    print(f"  Read I/Os: {metrics['read_ios']}")
    print(f"  Write I/Os: {metrics['write_ios']}")
    print(f"  Runtime per op: {metrics['runtime_per_op']} ms")
    print(f"  Read runtime per op: {metrics['read_runtime_per_op']} ms")
    print(f"  Write runtime per op: {metrics['write_runtime_per_op']} ms")
    
    return metrics

def run_benchmark_test(operation="mixed", query_distribution="uniform", read_write_ratio=1, client_count=1):
    """Run performance benchmark for the current configuration"""
    print(f"\nRunning benchmark for {operation} operation...")
    print(f"  Query distribution: {query_distribution}")
    print(f"  Read-write ratio: {read_write_ratio}")
    print(f"  Client count: {client_count}")
    
    # Calculate the number of operations based on ratio
    total_operations = 1000
    
    # For special operations, handle differently
    if operation == "get":
        # Force all reads
        read_ops = total_operations
        write_ops = 0
    elif operation == "put":
        # Force all writes
        read_ops = 0
        write_ops = total_operations
    elif operation == "mixed" or operation == "delete":
        # Use the read/write ratio for mixed operations
        if read_write_ratio >= 1:
            # More reads than writes
            read_ops = int(total_operations * (read_write_ratio / (read_write_ratio + 1)))
            write_ops = total_operations - read_ops
        else:
            # More writes than reads (ratio is a fraction)
            write_ops = int(total_operations * (1 / (1 + read_write_ratio)))
            read_ops = total_operations - write_ops
    else:
        # Default to 1:1 ratio
        read_ops = total_operations // 2
        write_ops = total_operations - read_ops
    
    print(f"  Operations breakdown: {read_ops} reads, {write_ops} writes")
    
    # Create a benchmark command file with operations
    bench_file = os.path.join(SCRIPT_DIR, "bench_command.txt")
    try:
        with open(bench_file, 'w') as f:
            # First, reset the stats to get clean metrics
            f.write("r\n")  # 'r' for reset stats
            
            operations_executed = 0
            read_operations_executed = 0
            write_operations_executed = 0
            
            # First, generate all the read operations
            for i in range(read_ops):
                # Generate reads
                if query_distribution == "uniform":
                    # Uniform distribution of queries
                    key = random.randint(0, 2**63 - 1)
                elif query_distribution == "skewed":
                    # Skewed distribution - 80% of queries target 20% of key space
                    if random.random() < 0.8:
                        key = random.randint(0, int(0.2 * (2**63 - 1)))
                    else:
                        key = random.randint(int(0.2 * (2**63 - 1)), 2**63 - 1)
                
                f.write(f"g {key}\n")
                operations_executed += 1
                read_operations_executed += 1
            
            # Then, generate all the write operations
            for i in range(write_ops):
                # Generate writes
                if query_distribution == "uniform":
                    # Uniform distribution of queries
                    key = random.randint(0, 2**63 - 1)
                elif query_distribution == "skewed":
                    # Skewed distribution - 80% of queries target 20% of key space
                    if random.random() < 0.8:
                        key = random.randint(0, int(0.2 * (2**63 - 1)))
                    else:
                        key = random.randint(int(0.2 * (2**63 - 1)), 2**63 - 1)
                
                value = random.randint(0, 2**63 - 1)
                f.write(f"p {key} {value}\n")
                operations_executed += 1
                write_operations_executed += 1
            
            # Add stats command to get I/O info
            f.write("s\n")
            # Add quit command at the end
            f.write("q\n")
        
        # Function to run a client
        def run_client(client_id, result_queue):
            client_start_time = time.time()
            
            client_bin = os.path.join(BIN_DIR, "client")
            client_proc = subprocess.Popen(
                [client_bin],
                stdin=open(bench_file, 'r'),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            try:
                # Increased timeout for larger datasets - 5 minutes per client
                stdout, stderr = client_proc.communicate(timeout=300)
                
                client_elapsed = time.time() - client_start_time
                print(f"Client {client_id} completed in {client_elapsed:.2f} seconds")
                
                success = client_proc.returncode == 0
                result_queue.put((client_id, success, stdout, stderr, client_elapsed, read_operations_executed, write_operations_executed))
            except subprocess.TimeoutExpired:
                print(f"Client {client_id} timed out after 5 minutes")
                client_proc.kill()
                result_queue.put((client_id, False, "", "Timeout", 300, 0, 0))
        
        # Run multiple clients in parallel
        start_time = time.time()
        
        if client_count == 1:
            # Single client is simple
            client_bin = os.path.join(BIN_DIR, "client")
            client_proc = subprocess.Popen(
                [client_bin],
                stdin=open(bench_file, 'r'),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            try:
                # Increased timeout for larger datasets - 5 minutes
                stdout, stderr = client_proc.communicate(timeout=300)
                
                elapsed = time.time() - start_time
                print(f"Benchmark completed in {elapsed:.2f} seconds")
                
                if client_proc.returncode == 0:
                    # Extract metrics - pass the actual executed operations
                    metrics = extract_metrics(stdout, operations_executed, elapsed, read_operations_executed, write_operations_executed)
                    
                    print("\nExtracted Metrics:")
                    for metric, value in metrics.items():
                        if value is not None:
                            print(f"  {metric}: {value}")
                            
                        # Save full output for debugging
                        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
                        output_file = os.path.join(RESULTS_DIR, f"benchmark_{operation}_{timestamp}.txt")
                        with open(output_file, 'w') as f:
                            f.write(stdout)
                    
                    return True, metrics
                else:
                    print(f"Benchmark failed with exit code: {client_proc.returncode}")
                    print(f"STDERR: {stderr}")
                    return False, None
                    
            except subprocess.TimeoutExpired:
                print(f"Benchmark timed out after 5 minutes")
                client_proc.kill()
                return False, None
        else:
            # Run multiple clients in parallel
            import queue
            result_queue = queue.Queue()
            threads = []
            
            for i in range(client_count):
                thread = threading.Thread(
                    target=run_client, 
                    args=(i, result_queue)
                )
                threads.append(thread)
                thread.start()
            
            # Wait for all clients to finish
            for thread in threads:
                thread.join()
            
            # Get results
            elapsed = time.time() - start_time
            print(f"All clients completed in {elapsed:.2f} seconds")
            
            # Collect results
            success_count = 0
            total_ops = 0
            total_read_ops = 0
            total_write_ops = 0
            combined_output = ""
            
            while not result_queue.empty():
                client_id, success, stdout, stderr, client_elapsed, read_ops_exec, write_ops_exec = result_queue.get()
                if success:
                    success_count += 1
                    combined_output += f"--- CLIENT {client_id} OUTPUT ---\n{stdout}\n\n"
                    total_ops += operations_executed
                    total_read_ops += read_ops_exec
                    total_write_ops += write_ops_exec
                else:
                    print(f"Client {client_id} failed: {stderr}")
            
            if success_count > 0:
                # Extract metrics from combined output
                metrics = extract_metrics(combined_output, total_ops, elapsed, total_read_ops, total_write_ops)
                
                print("\nExtracted Metrics:")
                for metric, value in metrics.items():
                    if value is not None:
                        print(f"  {metric}: {value}")
                
                # Save combined output
                timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
                output_file = os.path.join(RESULTS_DIR, f"benchmark_{operation}_{client_count}clients_{timestamp}.txt")
                with open(output_file, 'w') as f:
                    f.write(combined_output)
                
                return success_count == client_count, metrics
            else:
                print(f"All clients failed")
                return False, None
    
    finally:
        # Clean up the command file
        if os.path.exists(bench_file):
            os.remove(bench_file)

def save_metrics_to_csv(dimension, values, metrics_dict, operation, plot_dir):
    """Save metrics for a dimension to a CSV file"""
    os.makedirs(plot_dir, exist_ok=True)
    
    csv_file = os.path.join(plot_dir, f"{dimension}_{operation}_metrics.csv")
    
    # Define the fields
    fieldnames = [
        'value', 
        'throughput', 'read_throughput', 'write_throughput',
        'read_ios', 'write_ios', 
        'cache_misses', 
        'runtime', 'runtime_per_op', 
        'read_runtime_per_op', 'write_runtime_per_op'
    ]
    
    with open(csv_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        
        # Write each value's metrics
        for value in values:
            if value in metrics_dict:
                row = {'value': value}
                row.update(metrics_dict[value])
                writer.writerow(row)
    
    print(f"Metrics saved to CSV: {csv_file}")
    return csv_file

def generate_plots(dimension, values, metrics_dict, operation, plot_dir, config):
    """Generate plots for the dimension test results"""
    print(f"\nGenerating plots for {dimension}...")
    os.makedirs(plot_dir, exist_ok=True)
    
    # Only plot values that have metrics
    plot_values = [v for v in values if v in metrics_dict]
    
    if not plot_values:
        print("No metrics to plot")
        return []
        
    # Get display values (scaled) and labels
    scale_factor = config["scale_factor"]
    units = config["units"]
    
    # Special handling for read_write_ratio to make them evenly spaced
    if dimension == "read_write_ratio":
        # Use even spacing for plotting
        x_positions = list(range(len(plot_values)))
        
        # Create labels for the x-axis that show the actual ratio values
        if all(isinstance(v, (int, float)) for v in plot_values):
            # Format the labels based on whether they're > 1 (read-biased) or < 1 (write-biased)
            x_labels = []
            for v in plot_values:
                if v >= 1:
                    x_labels.append(f"{int(v)}:1")  # e.g., "10:1" for read-biased
                else:
                    x_labels.append(f"1:{int(1/v)}")  # e.g., "1:10" for write-biased
        else:
            x_labels = [str(v) for v in plot_values]
        
        x_values = x_positions
    else:
        x_values = [v * scale_factor for v in plot_values] if isinstance(plot_values[0], (int, float)) else plot_values
        x_labels = x_values
    
    dimension_name = config["name"]
    
    # Create read and write I/O operations plot
    plt.figure(figsize=(10, 6))
    read_ios = [metrics_dict[v].get('read_ios', 0) for v in plot_values]
    write_ios = [metrics_dict[v].get('write_ios', 0) for v in plot_values]
    
    # Use bar charts for data_distribution and query_distribution
    if dimension in ["data_distribution", "query_distribution"]:
        bar_width = 0.35
        x_pos = range(len(x_values))
        
        plt.bar([p - bar_width/2 for p in x_pos], read_ios, bar_width, label='Read I/Os')
        plt.bar([p + bar_width/2 for p in x_pos], write_ios, bar_width, label='Write I/Os')
        plt.xticks(x_pos, x_labels)
    else:
        plt.plot(x_values, read_ios, 'o-', linewidth=2, markersize=8, label='Read I/Os')
        plt.plot(x_values, write_ios, 's-', linewidth=2, markersize=8, label='Write I/Os')
        plt.xticks(x_values, x_labels)
    
    plt.title(f'I/O Operations vs. {dimension_name} ({operation.upper()} operations)')
    plt.xlabel(f'{dimension_name} ({units})' if units else dimension_name)
    plt.ylabel('I/O Operations')
    plt.grid(True)
    plt.legend()
    
    io_plot = os.path.join(plot_dir, f"{dimension}_{operation}_io.png")
    plt.savefig(io_plot)
    plt.close()
    
    # Create throughput plot with separate read/write lines
    plt.figure(figsize=(10, 6))
    total_throughputs = [metrics_dict[v].get('throughput', 0) for v in plot_values]
    read_throughputs = [metrics_dict[v].get('read_throughput', 0) for v in plot_values]
    write_throughputs = [metrics_dict[v].get('write_throughput', 0) for v in plot_values]
    
    # Make sure read_throughputs has valid values (not None)
    read_throughputs = [0 if t is None else t for t in read_throughputs]
    write_throughputs = [0 if t is None else t for t in write_throughputs]
    
    # Debugging the throughput values to check what's being plotted
    print(f"Debug - Read throughputs: {read_throughputs}")
    print(f"Debug - Write throughputs: {write_throughputs}")
    print(f"Debug - Total throughputs: {total_throughputs}")
    
    # Use bar charts for data_distribution and query_distribution
    if dimension in ["data_distribution", "query_distribution"]:
        bar_width = 0.3
        x_pos = range(len(x_values))
        
        plt.bar([p - bar_width for p in x_pos], read_throughputs, bar_width, label='Read Throughput')
        plt.bar([p for p in x_pos], total_throughputs, bar_width, label='Total Throughput')
        plt.bar([p + bar_width for p in x_pos], write_throughputs, bar_width, label='Write Throughput')
        plt.xticks(x_pos, x_labels)
    else:
        plt.plot(x_values, read_throughputs, 'o-', linewidth=2, markersize=8, label='Read Throughput', color='blue')
        plt.plot(x_values, total_throughputs, '*-', linewidth=2, markersize=8, label='Total Throughput', color='green')
        plt.plot(x_values, write_throughputs, 's-', linewidth=2, markersize=8, label='Write Throughput', color='red')
        plt.xticks(x_values, x_labels)
    
    plt.title(f'Throughput vs. {dimension_name} ({operation.upper()} operations)')
    plt.xlabel(f'{dimension_name} ({units})' if units else dimension_name)
    plt.ylabel('Throughput (ops/sec)')
    plt.grid(True)
    plt.legend()
    
    throughput_plot = os.path.join(plot_dir, f"{dimension}_{operation}_throughput.png")
    plt.savefig(throughput_plot)
    plt.close()
    
    # Create runtime per operation plot with separate read/write lines
    plt.figure(figsize=(10, 6))
    total_runtime_per_op = [metrics_dict[v].get('runtime_per_op', 0) for v in plot_values]
    read_runtime_per_op = [metrics_dict[v].get('read_runtime_per_op', 0) for v in plot_values]
    write_runtime_per_op = [metrics_dict[v].get('write_runtime_per_op', 0) for v in plot_values]
    
    # Make sure runtime values are valid (not None)
    read_runtime_per_op = [0 if t is None else t for t in read_runtime_per_op]
    write_runtime_per_op = [0 if t is None else t for t in write_runtime_per_op]
    
    # Debugging the runtime values to check what's being plotted
    print(f"Debug - Read runtime per op: {read_runtime_per_op}")
    print(f"Debug - Write runtime per op: {write_runtime_per_op}")
    print(f"Debug - Total runtime per op: {total_runtime_per_op}")
    
    # Use bar charts for data_distribution and query_distribution
    if dimension in ["data_distribution", "query_distribution"]:
        bar_width = 0.3
        x_pos = range(len(x_values))
        
        plt.bar([p - bar_width for p in x_pos], read_runtime_per_op, bar_width, label='Read Runtime')
        plt.bar([p for p in x_pos], total_runtime_per_op, bar_width, label='Total Runtime')
        plt.bar([p + bar_width for p in x_pos], write_runtime_per_op, bar_width, label='Write Runtime')
        plt.xticks(x_pos, x_labels)
    else:
        plt.plot(x_values, read_runtime_per_op, 'o-', linewidth=2, markersize=8, label='Read Runtime per Op', color='blue')
        plt.plot(x_values, total_runtime_per_op, '*-', linewidth=2, markersize=8, label='Overall Runtime per Op', color='green')
        plt.plot(x_values, write_runtime_per_op, 's-', linewidth=2, markersize=8, label='Write Runtime per Op', color='red')
        plt.xticks(x_values, x_labels)
    
    # Use logarithmic scale for y-axis to better visualize wide range of values
    plt.yscale('log')
    
    plt.title(f'Runtime per Operation vs. {dimension_name} ({operation.upper()} operations)')
    plt.xlabel(f'{dimension_name} ({units})' if units else dimension_name)
    plt.ylabel('Runtime (ms/op) - Log Scale')
    plt.grid(True, which="both", ls="-")
    plt.legend()
    
    runtime_plot = os.path.join(plot_dir, f"{dimension}_{operation}_runtime_per_op.png")
    plt.savefig(runtime_plot)
    plt.close()
    
    # Create a second runtime plot with linear scale for reference
    plt.figure(figsize=(10, 6))
    if dimension in ["data_distribution", "query_distribution"]:
        bar_width = 0.3
        x_pos = range(len(x_values))
        
        plt.bar([p - bar_width for p in x_pos], read_runtime_per_op, bar_width, label='Read Runtime')
        plt.bar([p for p in x_pos], total_runtime_per_op, bar_width, label='Total Runtime')
        plt.bar([p + bar_width for p in x_pos], write_runtime_per_op, bar_width, label='Write Runtime')
        plt.xticks(x_pos, x_labels)
    else:
        plt.plot(x_values, read_runtime_per_op, 'o-', linewidth=2, markersize=8, label='Read Runtime per Op', color='blue')
        plt.plot(x_values, total_runtime_per_op, '*-', linewidth=2, markersize=8, label='Overall Runtime per Op', color='green')
        plt.plot(x_values, write_runtime_per_op, 's-', linewidth=2, markersize=8, label='Write Runtime per Op', color='red')
        plt.xticks(x_values, x_labels)
    
    plt.title(f'Runtime per Operation vs. {dimension_name} ({operation.upper()} operations)')
    plt.xlabel(f'{dimension_name} ({units})' if units else dimension_name)
    plt.ylabel('Runtime (ms/op) - Linear Scale')
    plt.grid(True)
    
    linear_runtime_plot = os.path.join(plot_dir, f"{dimension}_{operation}_runtime_per_op_linear.png")
    plt.savefig(linear_runtime_plot)
    plt.close()
    
    # Keep the total runtime plot for reference
    plt.figure(figsize=(10, 6))
    runtimes = [metrics_dict[v].get('runtime', 0) for v in plot_values]
    
    # Use bar charts for data_distribution and query_distribution
    if dimension in ["data_distribution", "query_distribution"]:
        plt.bar(x_values, runtimes)
        plt.xticks(range(len(x_values)), x_labels)
    else:
        plt.plot(x_values, runtimes, 'o-', linewidth=2, markersize=8)
        plt.xticks(x_values, x_labels)
    
    plt.title(f'Total Runtime vs. {dimension_name} ({operation.upper()} operations)')
    plt.xlabel(f'{dimension_name} ({units})' if units else dimension_name)
    plt.ylabel('Total Runtime (seconds)')
    plt.grid(True)
    
    total_runtime_plot = os.path.join(plot_dir, f"{dimension}_{operation}_total_runtime.png")
    plt.savefig(total_runtime_plot)
    plt.close()
    
    # Create cache misses plot
    plt.figure(figsize=(10, 6))
    cache_misses = [metrics_dict[v].get('cache_misses', 0) for v in plot_values]
    
    # Use bar charts for data_distribution and query_distribution
    if dimension in ["data_distribution", "query_distribution"]:
        plt.bar(x_values, cache_misses)
        plt.xticks(range(len(x_values)), x_labels)
    else:
        plt.plot(x_values, cache_misses, 'o-', linewidth=2, markersize=8)
        plt.xticks(x_values, x_labels)
    
    plt.title(f'Cache Misses vs. {dimension_name} ({operation.upper()} operations)')
    plt.xlabel(f'{dimension_name} ({units})' if units else dimension_name)
    plt.ylabel('Cache Misses')
    plt.grid(True)
    
    cache_plot = os.path.join(plot_dir, f"{dimension}_{operation}_cache_misses.png")
    plt.savefig(cache_plot)
    plt.close()
    
    print(f"Generated plots for {dimension} in {plot_dir}")
    return [io_plot, throughput_plot, runtime_plot, linear_runtime_plot, total_runtime_plot, cache_plot]

def test_dimension(dimension, operation="get", auto_continue=True):
    """Run tests for a specific dimension with all its values"""
    config = TEST_CONFIGS[dimension]
    values = config["values"]
    env_var = config["env_var"]
    plot_dir = config["plot_dir"]
    
    print(f"\n{'='*80}")
    print(f"TESTING DIMENSION: {config['name']} ({dimension})")
    print(f"{'='*80}")
    print(f"Testing values: {values}")
    print(f"Operation: {operation}")
    
    # Create results directory for this dimension
    os.makedirs(plot_dir, exist_ok=True)
    
    metrics_dict = {}  # To store metrics for each value
    
    # Test each value of the dimension
    for value in values:
        print(f"\n{'-'*60}")
        print(f"Testing {dimension} = {value}")
        print(f"{'-'*60}")
        
        # Clean up previous server
        cleanup_processes()
        
        # Special handling for different dimensions
        if dimension == "data_size":
            # Generate data file
            data_file = generate_test_data(value, BASELINE["data_distribution"])
            
            # Start server with baseline settings
            server_proc = start_server()
            
            try:
                # Load the data
                if not load_data(data_file):
                    print(f"Failed to load data for {dimension} = {value}. Skipping.")
                    continue
                    
                # Verify data loaded
                if not verify_data_loaded():
                    print(f"Warning: Could not verify data was loaded for {dimension} = {value}.")
                    if not auto_continue:
                        response = input("Continue anyway? (y/n): ")
                        if response.lower() != 'y':
                            continue
                
                # Run benchmark test - make sure we specify the correct operation
                if operation == "get":
                    # Run get-only benchmark 
                    success, metrics = run_benchmark_test("get",
                                                       query_distribution=BASELINE["query_distribution"],
                                                       read_write_ratio=1,  # All reads
                                                       client_count=BASELINE["client_count"])
                elif operation == "put":
                    # Run put-only benchmark
                    success, metrics = run_benchmark_test("put",
                                                       query_distribution=BASELINE["query_distribution"],
                                                       read_write_ratio=0,  # All writes
                                                       client_count=BASELINE["client_count"])
                else:
                    # Mixed mode or other operations
                    success, metrics = run_benchmark_test(operation,
                                                       query_distribution=BASELINE["query_distribution"],
                                                       read_write_ratio=BASELINE["read_write_ratio"],
                                                       client_count=BASELINE["client_count"])
                
                if success and metrics:
                    metrics_dict[value] = metrics
                
            finally:
                # Clean up server
                try:
                    server_proc.terminate()
                    server_proc.wait(timeout=5)
                except:
                    server_proc.kill()
                if hasattr(server_proc, 'log_file'):
                    server_proc.log_file.close()
                    
        elif dimension == "data_distribution":
            # Generate data file with specific distribution
            data_file = generate_test_data(BASELINE["data_size"], value)
            
            # Start server with baseline settings
            server_proc = start_server()
            
            try:
                # Load the data
                if not load_data(data_file):
                    print(f"Failed to load data for {dimension} = {value}. Skipping.")
                    continue
                    
                # Verify data loaded
                if not verify_data_loaded():
                    print(f"Warning: Could not verify data was loaded for {dimension} = {value}.")
                    if not auto_continue:
                        response = input("Continue anyway? (y/n): ")
                        if response.lower() != 'y':
                            continue
                
                # Run benchmark test
                success, metrics = run_benchmark_test(operation)
                if success and metrics:
                    metrics_dict[value] = metrics
                
            finally:
                # Clean up server
                try:
                    server_proc.terminate()
                    server_proc.wait(timeout=5)
                except:
                    server_proc.kill()
                if hasattr(server_proc, 'log_file'):
                    server_proc.log_file.close()
                    
        elif dimension == "query_distribution":
            # Generate baseline data file
            data_file = generate_test_data(BASELINE["data_size"], BASELINE["data_distribution"])
            
            # Start server with baseline settings
            server_proc = start_server()
            
            try:
                # Load the data
                if not load_data(data_file):
                    print(f"Failed to load data for {dimension} = {value}. Skipping.")
                    continue
                    
                # Verify data loaded
                if not verify_data_loaded():
                    print(f"Warning: Could not verify data was loaded for {dimension} = {value}.")
                    if not auto_continue:
                        response = input("Continue anyway? (y/n): ")
                        if response.lower() != 'y':
                            continue
                
                # Run benchmark test with specified query distribution
                success, metrics = run_benchmark_test(operation, query_distribution=value)
                if success and metrics:
                    metrics_dict[value] = metrics
                
            finally:
                # Clean up server
                try:
                    server_proc.terminate()
                    server_proc.wait(timeout=5)
                except:
                    server_proc.kill()
                if hasattr(server_proc, 'log_file'):
                    server_proc.log_file.close()
                    
        elif dimension == "read_write_ratio":
            # Generate baseline data file
            data_file = generate_test_data(BASELINE["data_size"], BASELINE["data_distribution"])
            
            # Start server with baseline settings
            server_proc = start_server()
            
            try:
                # Load the data
                if not load_data(data_file):
                    print(f"Failed to load data for {dimension} = {value}. Skipping.")
                    continue
                    
                # Verify data loaded
                if not verify_data_loaded():
                    print(f"Warning: Could not verify data was loaded for {dimension} = {value}.")
                    if not auto_continue:
                        response = input("Continue anyway? (y/n): ")
                        if response.lower() != 'y':
                            continue
                
                # Always use "mixed" operation for read/write ratio
                success, metrics = run_benchmark_test("mixed", 
                                                     query_distribution=BASELINE["query_distribution"],
                                                     read_write_ratio=value)
                if success and metrics:
                    metrics_dict[value] = metrics
                
            finally:
                # Clean up server
                try:
                    server_proc.terminate()
                    server_proc.wait(timeout=5)
                except:
                    server_proc.kill()
                if hasattr(server_proc, 'log_file'):
                    server_proc.log_file.close()
                    
        elif dimension == "client_count":
            # Generate baseline data file
            data_file = generate_test_data(BASELINE["data_size"], BASELINE["data_distribution"])
            
            # Start server with baseline settings
            server_proc = start_server()
            
            try:
                # Load the data
                if not load_data(data_file):
                    print(f"Failed to load data for {dimension} = {value}. Skipping.")
                    continue
                    
                # Verify data loaded
                if not verify_data_loaded():
                    print(f"Warning: Could not verify data was loaded for {dimension} = {value}.")
                    if not auto_continue:
                        response = input("Continue anyway? (y/n): ")
                        if response.lower() != 'y':
                            continue
                
                # Run benchmark test with specified client count
                success, metrics = run_benchmark_test(operation, 
                                                    query_distribution=BASELINE["query_distribution"],
                                                    read_write_ratio=BASELINE["read_write_ratio"],
                                                    client_count=value)
                if success and metrics:
                    metrics_dict[value] = metrics
                
            finally:
                # Clean up server
                try:
                    server_proc.terminate()
                    server_proc.wait(timeout=5)
                except:
                    server_proc.kill()
                if hasattr(server_proc, 'log_file'):
                    server_proc.log_file.close()
        
        # Standard env var dimensions (buffer_size, size_ratio, thread_count)
        else:
            # Set the env var for this test
            env_vars = {env_var: value}
            
            # Start server with this setting
            server_proc = start_server(env_vars)
            
            try:
                # Generate and load baseline data
                data_file = generate_test_data(BASELINE["data_size"], BASELINE["data_distribution"])
                
                # Load the data
                if not load_data(data_file):
                    print(f"Failed to load data for {dimension} = {value}. Skipping.")
                    continue
                    
                # Verify data loaded
                if not verify_data_loaded():
                    print(f"Warning: Could not verify data was loaded for {dimension} = {value}.")
                    if not auto_continue:
                        response = input("Continue anyway? (y/n): ")
                        if response.lower() != 'y':
                            continue
                
                # Run benchmark test
                success, metrics = run_benchmark_test(operation)
                if success and metrics:
                    metrics_dict[value] = metrics
                    
            finally:
                # Clean up server
                try:
                    server_proc.terminate()
                    server_proc.wait(timeout=5)
                except:
                    server_proc.kill()
                if hasattr(server_proc, 'log_file'):
                    server_proc.log_file.close()
    
    # If we collected metrics, save to CSV and generate plots
    if metrics_dict:
        save_metrics_to_csv(dimension, values, metrics_dict, operation, plot_dir)
        generate_plots(dimension, values, metrics_dict, operation, plot_dir, config)
        print(f"\nCompleted testing for {dimension} dimension")
        return True
    else:
        print(f"\nNo metrics collected for {dimension} dimension!")
        return False

def run_performance_benchmark():
    """Run specific performance benchmarks to measure if we meet expected metrics"""
    print("\n=== LSM-TREE PERFORMANCE BENCHMARK ===")
    
    # Create necessary directories
    os.makedirs(DATA_DIR, exist_ok=True)
    os.makedirs(RESULTS_DIR, exist_ok=True)
    
    # Clean up any existing server processes
    cleanup_processes()
    
    # Performance results
    results = {
        "in_memory_reads": None,  # Expected: 1K-5K ops/sec
        "in_memory_writes": None, # Expected: 100K-1M ops/sec
        "disk_reads": None,       # Expected: 20-100 ops/sec
        "disk_writes": None       # For comparison
    }
    
    # Generate test data
    data_file = generate_test_data(50 * 1024 * 1024, "uniform")  # 50MB file
    
    try:
        # Start with standard buffer
        env_vars = {"LSMTREE_BUFFER_SIZE": 4 * 1024 * 1024}  # 4MB buffer
        server_proc = start_server(env_vars)
        
        # Load the data
        if not load_data(data_file):
            print("Failed to load test data. Aborting performance benchmark.")
            return
            
        # Verify data loaded
        if not verify_data_loaded():
            print("Warning: Could not verify data was loaded.")
            
        print("\n--- Testing disk operations ---")
        print("Running read benchmark (key lookup from disk)...")
        
        # Run read benchmark - keys should be on disk due to the smaller buffer
        success, metrics = run_benchmark_test(operation="get", 
                                             query_distribution="uniform",
                                             read_write_ratio=1,
                                             client_count=1)
                                             
        if success and metrics:
            results["disk_reads"] = metrics.get('read_throughput')
            print(f"Disk read performance: {results['disk_reads']:.2f} operations/second")
            
        # Run write benchmark
        print("Running write benchmark (puts that cause disk I/O)...")
        success, metrics = run_benchmark_test(operation="put", 
                                             query_distribution="uniform",
                                             read_write_ratio=0,
                                             client_count=1)
                                             
        if success and metrics:
            results["disk_writes"] = metrics.get('write_throughput')
            print(f"Disk write performance: {results['disk_writes']:.2f} operations/second")
            
        # Clean up server - improved shutdown process
        print("Shutting down server after disk tests...")
        try:
            # Send SIGTERM signal
            server_proc.terminate()
            
            # Give some time for graceful shutdown
            for _ in range(10):  # Try for up to 10 seconds
                if server_proc.poll() is not None:
                    break  # Process has terminated
                time.sleep(1)
                
            # If still running, force kill
            if server_proc.poll() is None:
                print("Server didn't terminate gracefully, forcing...")
                server_proc.kill()
                server_proc.wait(timeout=2)  # Shorter timeout for kill
            
            # Close log file
            if hasattr(server_proc, 'log_file'):
                server_proc.log_file.close()
        except Exception as e:
            print(f"Error during server shutdown: {e}")
        
        # Ensure clean slate
        cleanup_processes()
        time.sleep(2)  # Additional delay to ensure ports are released
        
        # Now test with a massive buffer to keep everything in memory
        print("\n--- Testing in-memory operations ---")
        env_vars = {"LSMTREE_BUFFER_SIZE": 1024 * 1024 * 1024}  # 1GB buffer
        server_proc = start_server(env_vars)
        
        # Load a small amount of data
        small_data_file = generate_test_data(1 * 1024 * 1024, "uniform")  # 1MB file
        
        if not load_data(small_data_file):
            print("Failed to load small test data. Skipping memory benchmark.")
        else:
            # Run read benchmark - keys should be in memory
            print("Running in-memory read benchmark...")
            success, metrics = run_benchmark_test(operation="get", 
                                                query_distribution="uniform",
                                                read_write_ratio=1,
                                                client_count=1)
                                                
            if success and metrics:
                results["in_memory_reads"] = metrics.get('read_throughput')
                print(f"In-memory read performance: {results['in_memory_reads']:.2f} operations/second")
                
            # Run write benchmark - should stay in buffer
            print("Running in-memory write benchmark...")
            success, metrics = run_benchmark_test(operation="put", 
                                                query_distribution="uniform",
                                                read_write_ratio=0,
                                                client_count=1)
                                                
            if success and metrics:
                results["in_memory_writes"] = metrics.get('write_throughput')
                print(f"In-memory write performance: {results['in_memory_writes']:.2f} operations/second")
            
    finally:
        # Ensure cleanup happens regardless of any errors
        print("Cleaning up resources...")
        try:
            # Send SIGTERM signal
            if 'server_proc' in locals() and server_proc.poll() is None:
                server_proc.terminate()
                
                # Give some time for graceful shutdown
                for _ in range(5):  # Try for up to 5 seconds
                    if server_proc.poll() is not None:
                        break  # Process has terminated
                    time.sleep(1)
                    
                # If still running, force kill
                if server_proc.poll() is None:
                    print("Server didn't terminate gracefully, forcing...")
                    server_proc.kill()
                
                # Close log file
                if hasattr(server_proc, 'log_file'):
                    server_proc.log_file.close()
        except Exception as e:
            print(f"Error during final cleanup: {e}")
        
        # Always make sure to clean up any stray processes
        cleanup_processes()
        
    # Evaluate against expected metrics
    print("\n=== PERFORMANCE EVALUATION ===")
    
    if results["in_memory_reads"]:
        expected_min, expected_max = 1000, 5000
        meets_expectation = results["in_memory_reads"] >= expected_min
        print(f"In-memory reads: {results['in_memory_reads']:.2f} ops/sec")
        print(f"  Expected: {expected_min}-{expected_max} ops/sec")
        print(f"  Result: {'MEETS' if meets_expectation else 'BELOW'} expectations")
        
    if results["in_memory_writes"]:
        expected_min, expected_max = 100000, 1000000
        meets_expectation = results["in_memory_writes"] >= expected_min
        print(f"In-memory writes: {results['in_memory_writes']:.2f} ops/sec")
        print(f"  Expected: {expected_min}-{expected_max} ops/sec")
        print(f"  Result: {'MEETS' if meets_expectation else 'BELOW'} expectations")
        
    if results["disk_reads"]:
        expected_min, expected_max = 20, 100
        meets_expectation = results["disk_reads"] >= expected_min
        print(f"Disk reads: {results['disk_reads']:.2f} ops/sec")
        print(f"  Expected: {expected_min}-{expected_max} ops/sec")
        print(f"  Result: {'MEETS' if meets_expectation else 'BELOW'} expectations")
    
    return results

def main():
    """Run tests for all dimensions or specific dimension(s)"""
    # Create necessary directories
    os.makedirs(DATA_DIR, exist_ok=True)
    os.makedirs(RESULTS_DIR, exist_ok=True)
    os.makedirs(PLOTS_DIR, exist_ok=True)
    
    # Clean up any existing server processes
    cleanup_processes()
    
    # Parse command-line arguments
    import argparse
    parser = argparse.ArgumentParser(description='Run LSM-Tree performance tests')
    parser.add_argument('--dimensions', nargs='+', choices=TEST_CONFIGS.keys(),
                        help='Specific dimensions to test (default: all)')
    parser.add_argument('--operation', choices=['get', 'put', 'delete', 'mixed'], default='mixed',
                        help='Operation to benchmark (default: mixed)')
    parser.add_argument('--auto-continue', action='store_true', default=True,
                        help='Automatically continue if verification fails')
    parser.add_argument('--performance-profile', action='store_true',
                        help='Run a specific performance profile to check against expected metrics')
    
    args = parser.parse_args()
    
    # If performance profile is requested, run it and exit
    if args.performance_profile:
        run_performance_benchmark()
        return
    
    # Determine which dimensions to test
    dimensions_to_test = args.dimensions if args.dimensions else list(TEST_CONFIGS.keys())
    
    print(f"LSM-Tree Performance Testing")
    print(f"Testing dimensions: {', '.join(dimensions_to_test)}")
    print(f"Operation: {args.operation}")
    print(f"Data directory: {DATA_DIR}")
    print(f"Results directory: {RESULTS_DIR}")
    print(f"Plots directory: {PLOTS_DIR}")
    
    # Record start time
    overall_start = time.time()
    
    # Test each dimension
    successful_dimensions = 0
    failed_dimensions = 0
    
    for dimension in dimensions_to_test:
        start_time = time.time()
        
        success = test_dimension(
            dimension=dimension,
            operation=args.operation,
            auto_continue=args.auto_continue
        )
        
        elapsed = time.time() - start_time
        minutes, seconds = divmod(elapsed, 60)
        
        print(f"Dimension {dimension} completed in {int(minutes)}m {seconds:.2f}s")
        
        if success:
            successful_dimensions += 1
        else:
            failed_dimensions += 1
    
    # Record end time and print summary
    overall_elapsed = time.time() - overall_start
    hours, remainder = divmod(overall_elapsed, 3600)
    minutes, seconds = divmod(remainder, 60)
    
    print("\n" + "="*80)
    print("LSM-TREE PERFORMANCE TESTS COMPLETE")
    print("="*80)
    print(f"Total time: {int(hours)}h {int(minutes)}m {seconds:.2f}s")
    print(f"Dimensions tested: {len(dimensions_to_test)}")
    print(f"Successful: {successful_dimensions}")
    print(f"Failed: {failed_dimensions}")
    print(f"Results saved to: {RESULTS_DIR}")
    print(f"Plots saved to: {PLOTS_DIR}")
    
    # Generate a report with all results
    print("\nGenerating consolidated report and plots...")
    generate_consolidated_report(dimensions_to_test, args.operation)

def generate_consolidated_report(dimensions, operation):
    """Generate a consolidated report of all test results"""
    report_path = os.path.join(RESULTS_DIR, f"consolidated_report_{operation}.txt")
    
    with open(report_path, 'w') as f:
        f.write(f"LSM-TREE PERFORMANCE REPORT - {operation.upper()} OPERATIONS\n")
        f.write("="*80 + "\n\n")
        
        # Add date and time
        f.write(f"Generated: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        
        # Add baseline configuration
        f.write("Baseline Configuration:\n")
        for key, value in BASELINE.items():
            if key == "data_size":
                f.write(f"  {key}: {value/(1024*1024)} MB\n")
            elif key == "buffer_size":
                f.write(f"  {key}: {value/1024} KB\n")
            else:
                f.write(f"  {key}: {value}\n")
        f.write("\n")
        
        # Add summary for each dimension
        for dimension in dimensions:
            config = TEST_CONFIGS[dimension]
            f.write(f"Dimension: {config['name']}\n")
            
            # Format the values properly
            if isinstance(config['values'][0], (int, float)) and config['scale_factor'] != 1:
                values = [f"{v * config['scale_factor']} {config['units']}" for v in config['values']]
            else:
                values = [str(v) for v in config['values']]
                
            f.write(f"  Tested values: {', '.join(values)}\n")
            f.write(f"  Plots: {config['plot_dir']}\n\n")
    
    print(f"Consolidated report saved to {report_path}")

if __name__ == "__main__":
    main() 