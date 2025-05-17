#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import tempfile
import random
import struct
import signal
import threading

def generate_test_data(size_bytes, output_file):
    """Generate test data file with specified size"""
    print(f"Generating {size_bytes/1024/1024:.2f}MB test data...")
    
    records_count = size_bytes // 16  # Each record is 16 bytes (8-byte key, 8-byte value)
    
    with open(output_file, 'wb') as f:
        for i in range(records_count):
            key = random.randint(0, 2**63 - 1)
            value = random.randint(0, 2**63 - 1)
            f.write(struct.pack("<QQ", key, value))
    
    actual_size = os.path.getsize(output_file)
    print(f"Generated file size: {actual_size/1024/1024:.2f}MB with {records_count:,} records")
    
    return output_file

def cleanup_processes():
    """Kill any running server processes"""
    try:
        os.system("pkill -f 'bin/server'")
        print("Cleaned up any existing server processes")
        time.sleep(1)
    except Exception as e:
        print(f"Error cleaning up processes: {e}")

def monitor_server_logs(server_log_path, stop_event, success_event):
    """Monitor and display server logs in real-time"""
    last_position = 0
    
    print("\n--- Server Log Monitor Started ---")
    
    while not stop_event.is_set():
        # Check if the log file exists
        if os.path.exists(server_log_path):
            with open(server_log_path, 'r') as f:
                # Go to the last position we read
                f.seek(last_position)
                
                # Read and print new content
                new_content = f.read()
                if new_content:
                    print(new_content, end='', flush=True)
                    
                    # Check if server started in non-interactive mode
                    if "Server running in non-interactive mode" in new_content:
                        print("\n>>> Server confirmed running in non-interactive mode <<<\n", flush=True)
                    
                    # Check if bulk load completed
                    if "Bulk load completed successfully" in new_content:
                        print("\n>>> BULK LOAD COMPLETED SUCCESSFULLY - WILL TERMINATE PROCESSES <<<\n", flush=True)
                        success_event.set()
                
                # Update the last position
                last_position = f.tell()
        
        # Wait a bit before checking again
        time.sleep(0.5)
    
    print("\n--- Server Log Monitor Stopped ---")

def test_loading_performance(data_file):
    """Test the loading performance of the LSM-tree"""
    print("\nTesting loading performance...")
    
    # Cleanup any existing processes
    cleanup_processes()
    
    # Create command file for loading
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as cmd_file:
        cmd_file_path = cmd_file.name
        cmd_file.write(f'l "{data_file}"\n')
        cmd_file.write("q\n")
        cmd_file.flush()
    
    print(f"Created command file with load command: l \"{data_file}\"")
    
    # Create server log file
    server_log_path = "server_output.log"
    server_log = open(server_log_path, "w")
    
    # Start server
    print("Starting server in non-interactive mode...")
    server_proc = subprocess.Popen(
        ["./bin/server"],
        stdin=subprocess.DEVNULL,  # Redirect stdin to /dev/null to prevent waiting for input
        stdout=server_log,
        stderr=server_log,
        text=True
    )
    
    # Give the server a moment to start
    time.sleep(2)
    print("Server started with PID:", server_proc.pid)
    
    # Set up events for coordination
    stop_monitor = threading.Event()
    success_event = threading.Event()  # New event to signal successful bulk load
    
    # Set up log monitoring thread
    log_monitor = threading.Thread(
        target=monitor_server_logs, 
        args=(server_log_path, stop_monitor, success_event)
    )
    log_monitor.daemon = True
    log_monitor.start()
    
    # Run client with load command
    print("Running client to load data...")
    start_time = time.time()
    
    client_log = open("client_output.log", "w")
    
    # Use subprocess.call instead of Popen to ensure it completes before continuing
    print("Sending load command to server...")
    client_proc = subprocess.Popen(
        ["./bin/client"],
        stdin=open(cmd_file_path, 'r'),
        stdout=client_log,
        stderr=client_log,
        text=True
    )
    
    # Keep track of whether we need to calculate elapsed time
    elapsed_time_calculated = False
    
    # Wait for either success_event or timeout
    max_wait_time = 120  # 2 minutes timeout
    wait_increment = 0.5  # Check every half second
    waited = 0
    
    while waited < max_wait_time:
        # Check if bulk load completed
        if success_event.is_set():
            # Wait a bit to ensure all logs are captured
            time.sleep(1)
            end_time = time.time()
            elapsed = end_time - start_time
            elapsed_time_calculated = True
            print(f"Bulk load detected as successful! Elapsed time: {elapsed:.2f} seconds")
            break
        
        # Check if client exited
        if client_proc.poll() is not None:
            print(f"Client exited with code: {client_proc.returncode}")
            if not elapsed_time_calculated:
                end_time = time.time()
                elapsed = end_time - start_time
                elapsed_time_calculated = True
            break
        
        # Wait and increment counter
        time.sleep(wait_increment)
        waited += wait_increment
    
    # If we timed out and haven't calculated elapsed time
    if not elapsed_time_calculated:
        end_time = time.time()
        elapsed = end_time - start_time
    
    # Check if bulk load completed successfully based on logs
    print("Checking if bulk load completed successfully...")
    
    bulk_load_completed = False
    with open(server_log_path, 'r') as f:
        for line in f:
            if "Bulk load completed successfully" in line:
                bulk_load_completed = True
                break
    
    if bulk_load_completed:
        print("Bulk load completed successfully!")
    else:
        print("Warning: Could not confirm bulk load completion in logs")
    
    # Force kill the client process if it's still running
    if client_proc.poll() is None:
        print("Client still running, killing it...")
        client_proc.kill()
        try:
            client_proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            print("Client didn't terminate cleanly")
    
    # Force kill the server now that we're done
    print("Terminating server...")
    try:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            print("Server not responding to terminate, killing forcefully")
            server_proc.kill()
            server_proc.wait(timeout=5)
    except Exception as e:
        print(f"Error terminating server: {e}")
        try:
            server_proc.kill()
        except:
            pass
    
    # Stop log monitoring
    stop_monitor.set()
    log_monitor.join(timeout=2)
    
    # Clean up
    server_log.close()
    client_log.close()
    os.unlink(cmd_file_path)
    
    # Print results
    print(f"Loading completed in {elapsed:.2f} seconds")
    print(f"Throughput: {os.path.getsize(data_file)/elapsed/1024/1024:.2f}MB/s")
    
    print("\nClient log summary:")
    os.system("cat client_output.log")
    
    return elapsed

def main():
    # Test data sizes (10MB, 50MB, 100MB, 200MB)
    data_sizes = [
        10 * 1024 * 1024,    # 10 MB
        50 * 1024 * 1024,    # 50 MB
        100 * 1024 * 1024,   # 100 MB
        200 * 1024 * 1024,   # 200 MB
    ]
    
    # Results storage
    results = []
    
    for data_size in data_sizes:
        print(f"\n{'='*80}")
        print(f"TESTING WITH {data_size/1024/1024:.2f}MB DATA")
        print(f"{'='*80}")
        
        # Create temporary test data file
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as temp_file:
            test_data_file = temp_file.name
        
        try:
            # Generate the test data
            generate_test_data(data_size, test_data_file)
            
            # Test the loading performance
            elapsed = test_loading_performance(test_data_file)
            
            if elapsed is not None:
                throughput = data_size/elapsed/1024/1024
                results.append((data_size/1024/1024, elapsed, throughput))
                print(f"\nPerformance Summary:")
                print(f"Data Size: {data_size/1024/1024:.2f}MB")
                print(f"Loading Time: {elapsed:.2f} seconds")
                print(f"Throughput: {throughput:.2f}MB/s")
            
        finally:
            # Clean up
            if os.path.exists(test_data_file):
                os.unlink(test_data_file)
            cleanup_processes()
    
    # Print overall results
    if results:
        print("\n\n" + "="*80)
        print("OVERALL PERFORMANCE RESULTS")
        print("="*80)
        print(f"{'Size (MB)':<15}{'Time (s)':<15}{'Throughput (MB/s)':<20}")
        print("-"*50)
        
        for size, time, throughput in results:
            print(f"{size:<15.2f}{time:<15.2f}{throughput:<20.2f}")

if __name__ == "__main__":
    main() 