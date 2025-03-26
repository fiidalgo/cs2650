#!/usr/bin/env python3
import subprocess
import time
import os
import csv
import random

DATA_DIR = "../data/"
LSM_SERVER = "../build/lsm_server"

# e.g. we test different # of GET queries in these runs
RUN_GETS_LIST = [1000, 2000, 5000, 10000]

def run_server():
    proc = subprocess.Popen(
        [LSM_SERVER, "--data-dir", DATA_DIR],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    return proc

def main():
    output_csv = "get_results.csv"
    # Overwrite file with new header
    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["num_gets","elapsed_s","io_reads","io_writes","io_read_bytes","io_write_bytes"])

    # (Optional) You might want the data to remain the same across runs,
    # so you only load once at the beginning. Let's assume you already
    # loaded e.g. 1M records in data/.
    # If data/ is empty, then each GET run won't do much. 
    # So you might want to do an initial load here or you rely on experiment_put.py's final state.

    for num_gets in RUN_GETS_LIST:
        print(f"\n=== GET Experiment: {num_gets} GETs ===")

        # We'll create random GET commands
        commands_list = []
        for _ in range(num_gets):
            key = random.randint(0, 1200000)  # if we loaded ~1M keys
            commands_list.append(f"g {key}")

        # We'll run the server fresh, or if you prefer to keep the same server, do so
        proc = run_server()
        time.sleep(1)

        # We measure total time to process these GETs
        start_time = time.time()
        cmd_input = "\n".join(commands_list) + "\n" + "s\nquit\n"
        out, err = proc.communicate(cmd_input)
        end_time = time.time()
        elapsed = end_time - start_time

        # parse I/O stats
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

        with open(output_csv, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                num_gets,
                f"{elapsed:.3f}",
                io_reads,
                io_writes,
                io_read_bytes,
                io_write_bytes
            ])

    print("\nAll GET runs complete. Results in", output_csv)

if __name__ == "__main__":
    main()
