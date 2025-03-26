#!/usr/bin/env python3
import subprocess
import time
import os
import csv

# Configure experiment
DATA_DIR = "../data/"
LSM_SERVER = "../build/lsm_server"  # Path to your compiled server executable
RUN_SIZES = [100000, 200000, 500000, 1000000]  # the different record counts

def generate_input_data(file_path, num_records):
    """Generate a file of 'key value' lines for loading."""
    with open(file_path, 'w') as f:
        for i in range(num_records):
            f.write(f"{i} {i*10}\n")

def run_server():
    """Launch the LSM server in interactive mode, feed commands, capture output."""
    proc = subprocess.Popen(
        [LSM_SERVER, "--data-dir", DATA_DIR],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    return proc

def clean_data_dir():
    """Remove data/ folder so each run starts fresh."""
    if os.path.exists(DATA_DIR):
        for root, dirs, files in os.walk(DATA_DIR, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
        try:
            os.rmdir(DATA_DIR)
        except OSError:
            pass

def main():
    output_csv = "put_results.csv"

    # 1) Initialize the CSV (overwrite old if any)
    with open(output_csv, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["num_records","elapsed_s","io_reads","io_writes","io_read_bytes","io_write_bytes"])

    # 2) For each size, do a full run
    for num_records in RUN_SIZES:
        print(f"\n=== PUT Experiment: {num_records} records ===")

        # Clean data dir so no old SSTables remain
        clean_data_dir()

        # Generate data file
        input_file = f"put_data_{num_records}.txt"
        generate_input_data(input_file, num_records)

        # Start LSM server
        proc = run_server()
        time.sleep(1)

        # Construct commands:
        commands = f"l {input_file}\n"  # load
        commands += "s\n"               # stats
        commands += "quit\n"

        start_time = time.time()
        out, err = proc.communicate(commands)
        end_time = time.time()
        elapsed = end_time - start_time

        # parse I/O stats from output
        io_reads = 0
        io_writes = 0
        io_read_bytes = 0
        io_write_bytes = 0

        for line in out.splitlines():
            if "I/O reads:" in line:
                io_reads = line.split(":")[-1].strip()
            elif "I/O writes:" in line:
                io_writes = line.split(":")[-1].strip()
            elif "I/O read bytes:" in line:
                io_read_bytes = line.split(":")[-1].strip()
            elif "I/O write bytes:" in line:
                io_write_bytes = line.split(":")[-1].strip()

        print(f"Time = {elapsed:.3f}s, reads={io_reads}, writes={io_writes}")

        # 3) Append row to CSV
        with open(output_csv, "a", newline="") as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow([
                num_records,
                f"{elapsed:.3f}",
                io_reads,
                io_writes,
                io_read_bytes,
                io_write_bytes
            ])

    print("\nAll runs complete. Results in", output_csv)

if __name__ == "__main__":
    main()
