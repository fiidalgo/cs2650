#!/usr/bin/env python3

import csv
import matplotlib.pyplot as plt
import os

def plot_put_results(csv_file):
    num_records_list = []
    elapsed_list = []
    io_reads_list = []
    io_writes_list = []

    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # read columns
            num_records = float(row["num_records"])
            elapsed_s = float(row["elapsed_s"])
            io_reads = float(row["io_reads"])
            io_writes = float(row["io_writes"])

            num_records_list.append(num_records)
            elapsed_list.append(elapsed_s)
            io_reads_list.append(io_reads)
            io_writes_list.append(io_writes)

    # Plot elapsed time vs num_records
    plt.figure(figsize=(6,4))
    plt.plot(num_records_list, elapsed_list, marker='o', label='Elapsed Time (s)')
    plt.title("PUT Experiment: Time vs #Records")
    plt.xlabel("Number of Records")
    plt.ylabel("Time (seconds)")
    plt.legend()
    plt.grid(True)

    # Save figure
    plt.savefig("put_time_vs_records.png")
    plt.close()

    # Similarly, you can plot I/O if you want:
    plt.figure(figsize=(6,4))
    plt.plot(num_records_list, io_reads_list, marker='x', label='I/O reads')
    plt.plot(num_records_list, io_writes_list, marker='s', label='I/O writes')
    plt.title("PUT Experiment: I/O Reads/Writes vs #Records")
    plt.xlabel("Number of Records")
    plt.ylabel("I/O count")
    plt.legend()
    plt.grid(True)
    plt.savefig("put_io_vs_records.png")
    plt.close()

def plot_get_results(csv_file):
    num_gets_list = []
    elapsed_list = []
    io_reads_list = []
    io_writes_list = []

    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            num_gets = float(row["num_gets"])
            elapsed_s = float(row["elapsed_s"])
            io_reads = float(row["io_reads"])
            io_writes = float(row["io_writes"])

            num_gets_list.append(num_gets)
            elapsed_list.append(elapsed_s)
            io_reads_list.append(io_reads)
            io_writes_list.append(io_writes)

    # Plot time vs #gets
    plt.figure(figsize=(6,4))
    plt.plot(num_gets_list, elapsed_list, marker='o', label='Elapsed Time (s)')
    plt.title("GET Experiment: Time vs #Gets")
    plt.xlabel("Number of GETs")
    plt.ylabel("Time (seconds)")
    plt.legend()
    plt.grid(True)
    plt.savefig("get_time_vs_gets.png")
    plt.close()

    # Plot I/O
    plt.figure(figsize=(6,4))
    plt.plot(num_gets_list, io_reads_list, marker='x', label='I/O reads')
    plt.plot(num_gets_list, io_writes_list, marker='s', label='I/O writes')
    plt.title("GET Experiment: I/O Reads/Writes vs #Gets")
    plt.xlabel("Number of GETs")
    plt.ylabel("I/O count")
    plt.legend()
    plt.grid(True)
    plt.savefig("get_io_vs_gets.png")
    plt.close()


def main():
    # Assume your experiment scripts produce put_results.csv / get_results.csv
    # in the same folder (test/). If they're stored differently, adjust paths.
    put_csv = "put_results.csv"
    get_csv = "get_results.csv"

    if os.path.exists(put_csv):
        plot_put_results(put_csv)
        print("Generated put_time_vs_records.png and put_io_vs_records.png")
    else:
        print(f"{put_csv} not found. Did you run experiment_put.py first?")

    if os.path.exists(get_csv):
        plot_get_results(get_csv)
        print("Generated get_time_vs_gets.png and get_io_vs_gets.png")
    else:
        print(f"{get_csv} not found. Did you run experiment_get.py first?")

if __name__ == "__main__":
    main()
