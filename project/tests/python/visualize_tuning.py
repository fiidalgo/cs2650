import json
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
import os
import glob
from pathlib import Path

def find_latest_tuning_dir():
    """Find the most recent tuning directory in ../data/compaction/"""
    base_dir = "../data/compaction"
    tuning_dirs = glob.glob(f"{base_dir}/tuning_*")
    if not tuning_dirs:
        raise FileNotFoundError("No tuning results found in ../data/compaction/")
    return sorted(tuning_dirs)[-1]  # Get the most recent directory

def load_json(filename):
    with open(filename, 'r') as f:
        return json.load(f)

def plot_l0_threshold_tuning(tuning_dir):
    data = load_json(f"{tuning_dir}/l0_threshold_tuning.json")
    
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    
    # Plot get latencies
    axes[0, 0].plot(data['thresholds'], data['get_latencies'], 'o-', label='Get Latency')
    axes[0, 0].set_xlabel('L0 SSTable Threshold')
    axes[0, 0].set_ylabel('Average Get Latency (ms)')
    axes[0, 0].set_title('Get Latency vs L0 Threshold')
    axes[0, 0].grid(True)
    
    # Plot write throughput
    axes[0, 1].plot(data['thresholds'], data['write_throughputs'], 'o-', color='green', label='Write Throughput')
    axes[0, 1].set_xlabel('L0 SSTable Threshold')
    axes[0, 1].set_ylabel('Write Throughput (ops/sec)')
    axes[0, 1].set_title('Write Throughput vs L0 Threshold')
    axes[0, 1].grid(True)
    
    # Plot compaction frequency
    axes[1, 0].plot(data['thresholds'], data['compaction_frequencies'], 'o-', color='red', label='Compaction Frequency')
    axes[1, 0].set_xlabel('L0 SSTable Threshold')
    axes[1, 0].set_ylabel('Compaction Frequency')
    axes[1, 0].set_title('Compaction Frequency vs L0 Threshold')
    axes[1, 0].grid(True)
    
    # Plot total bytes written
    axes[1, 1].plot(data['thresholds'], data['total_bytes_written'], 'o-', color='purple', label='Total Bytes Written')
    axes[1, 1].set_xlabel('L0 SSTable Threshold')
    axes[1, 1].set_ylabel('Total Bytes Written')
    axes[1, 1].set_title('Total I/O vs L0 Threshold')
    axes[1, 1].grid(True)
    
    plt.tight_layout()
    plt.savefig(f"{tuning_dir}/l0_threshold_tuning.png")
    plt.close()

def plot_size_ratio_tuning(tuning_dir):
    data = load_json(f"{tuning_dir}/size_ratio_tuning.json")
    
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    
    # Plot get latencies
    axes[0, 0].plot(data['ratios'], data['get_latencies'], 'o-', label='Get Latency')
    axes[0, 0].set_xlabel('Size Ratio')
    axes[0, 0].set_ylabel('Average Get Latency (ms)')
    axes[0, 0].set_title('Get Latency vs Size Ratio')
    axes[0, 0].grid(True)
    
    # Plot write throughput
    axes[0, 1].plot(data['ratios'], data['write_throughputs'], 'o-', color='green', label='Write Throughput')
    axes[0, 1].set_xlabel('Size Ratio')
    axes[0, 1].set_ylabel('Write Throughput (ops/sec)')
    axes[0, 1].set_title('Write Throughput vs Size Ratio')
    axes[0, 1].grid(True)
    
    # Plot total bytes written
    axes[1, 0].plot(data['ratios'], data['total_bytes_written'], 'o-', color='purple', label='Total Bytes Written')
    axes[1, 0].set_xlabel('Size Ratio')
    axes[1, 0].set_ylabel('Total Bytes Written')
    axes[1, 0].set_title('Total I/O vs Size Ratio')
    axes[1, 0].grid(True)
    
    # Plot level counts
    axes[1, 1].plot(data['ratios'], data['level_counts'], 'o-', color='orange', label='Level Count')
    axes[1, 1].set_xlabel('Size Ratio')
    axes[1, 1].set_ylabel('Number of Levels Created')
    axes[1, 1].set_title('Level Depth vs Size Ratio')
    axes[1, 1].grid(True)
    
    plt.tight_layout()
    plt.savefig(f"{tuning_dir}/size_ratio_tuning.png")
    plt.close()

def plot_policy_tuning(tuning_dir):
    data = load_json(f"{tuning_dir}/policy_tuning.json")
    
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    
    x = np.arange(len(data['policies']))
    width = 0.35
    
    # Plot get latencies
    axes[0, 0].bar(x, data['get_latencies'], width, label='Get Latency')
    axes[0, 0].set_xlabel('Compaction Policy')
    axes[0, 0].set_ylabel('Average Get Latency (ms)')
    axes[0, 0].set_title('Get Latency by Policy')
    axes[0, 0].set_xticks(x)
    axes[0, 0].set_xticklabels(data['policies'])
    axes[0, 0].grid(True)
    
    # Plot write throughput
    axes[0, 1].bar(x, data['write_throughputs'], width, color='green', label='Write Throughput')
    axes[0, 1].set_xlabel('Compaction Policy')
    axes[0, 1].set_ylabel('Write Throughput (ops/sec)')
    axes[0, 1].set_title('Write Throughput by Policy')
    axes[0, 1].set_xticks(x)
    axes[0, 1].set_xticklabels(data['policies'])
    axes[0, 1].grid(True)
    
    # Plot compaction frequency
    axes[1, 0].bar(x, data['compaction_frequencies'], width, color='red', label='Compaction Frequency')
    axes[1, 0].set_xlabel('Compaction Policy')
    axes[1, 0].set_ylabel('Compaction Frequency')
    axes[1, 0].set_title('Compaction Frequency by Policy')
    axes[1, 0].set_xticks(x)
    axes[1, 0].set_xticklabels(data['policies'])
    axes[1, 0].grid(True)
    
    # Plot total bytes written
    axes[1, 1].bar(x, data['total_bytes_written'], width, color='purple', label='Total Bytes Written')
    axes[1, 1].set_xlabel('Compaction Policy')
    axes[1, 1].set_ylabel('Total Bytes Written')
    axes[1, 1].set_title('Total I/O by Policy')
    axes[1, 1].set_xticks(x)
    axes[1, 1].set_xticklabels(data['policies'])
    axes[1, 1].grid(True)
    
    plt.tight_layout()
    plt.savefig(f"{tuning_dir}/policy_tuning.png")
    plt.close()

def plot_range_query_tuning(tuning_dir):
    data = load_json(f"{tuning_dir}/range_query_tuning.json")
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    # Plot range latencies
    ax1.plot(data['range_sizes'], data['range_latencies'], 'o-', label='Range Latency')
    ax1.set_xlabel('Range Size')
    ax1.set_ylabel('Average Range Latency (ms)')
    ax1.set_title('Range Query Latency vs Range Size')
    ax1.grid(True)
    
    # Plot bytes read
    ax2.plot(data['range_sizes'], data['bytes_read'], 'o-', color='orange', label='Bytes Read')
    ax2.set_xlabel('Range Size')
    ax2.set_ylabel('Average Bytes Read')
    ax2.set_title('Bytes Read vs Range Size')
    ax2.grid(True)
    
    plt.tight_layout()
    plt.savefig(f"{tuning_dir}/range_query_tuning.png")
    plt.close()

def plot_summary(tuning_dir):
    """Create a summary dashboard of all results"""
    l0_data = load_json(f"{tuning_dir}/l0_threshold_tuning.json")
    ratio_data = load_json(f"{tuning_dir}/size_ratio_tuning.json")
    policy_data = load_json(f"{tuning_dir}/policy_tuning.json")
    
    fig, axes = plt.subplots(3, 3, figsize=(18, 15))
    
    # Plot key metrics from L0 threshold tuning
    axes[0, 0].plot(l0_data['thresholds'], l0_data['get_latencies'], 'o-', label='Get Latency')
    axes[0, 0].set_xlabel('L0 Threshold')
    axes[0, 0].set_ylabel('Get Latency (ms)')
    axes[0, 0].set_title('L0 Threshold: Get Latency')
    axes[0, 0].grid(True)
    
    axes[0, 1].plot(l0_data['thresholds'], l0_data['write_throughputs'], 'o-', color='green', label='Write Throughput')
    axes[0, 1].set_xlabel('L0 Threshold')
    axes[0, 1].set_ylabel('Write Throughput (ops/sec)')
    axes[0, 1].set_title('L0 Threshold: Write Throughput')
    axes[0, 1].grid(True)
    
    # Plot key metrics from size ratio tuning
    axes[1, 0].plot(ratio_data['ratios'], ratio_data['get_latencies'], 'o-', label='Get Latency')
    axes[1, 0].set_xlabel('Size Ratio')
    axes[1, 0].set_ylabel('Get Latency (ms)')
    axes[1, 0].set_title('Size Ratio: Get Latency')
    axes[1, 0].grid(True)
    
    axes[1, 1].plot(ratio_data['ratios'], ratio_data['write_throughputs'], 'o-', color='green', label='Write Throughput')
    axes[1, 1].set_xlabel('Size Ratio')
    axes[1, 1].set_ylabel('Write Throughput (ops/sec)')
    axes[1, 1].set_title('Size Ratio: Write Throughput')
    axes[1, 1].grid(True)
    
    # Plot key metrics from policy tuning
    x = np.arange(len(policy_data['policies']))
    width = 0.35
    
    axes[2, 0].bar(x, policy_data['get_latencies'], width, label='Get Latency')
    axes[2, 0].set_xlabel('Compaction Policy')
    axes[2, 0].set_ylabel('Get Latency (ms)')
    axes[2, 0].set_title('Policy: Get Latency')
    axes[2, 0].set_xticks(x)
    axes[2, 0].set_xticklabels(policy_data['policies'])
    axes[2, 0].grid(True)
    
    axes[2, 1].bar(x, policy_data['write_throughputs'], width, color='green', label='Write Throughput')
    axes[2, 1].set_xlabel('Compaction Policy')
    axes[2, 1].set_ylabel('Write Throughput (ops/sec)')
    axes[2, 1].set_title('Policy: Write Throughput')
    axes[2, 1].set_xticks(x)
    axes[2, 1].set_xticklabels(policy_data['policies'])
    axes[2, 1].grid(True)
    
    # Empty last subplot or potentially use for additional metrics
    axes[0, 2].axis('off')
    axes[1, 2].axis('off')
    axes[2, 2].axis('off')
    
    plt.tight_layout()
    plt.savefig(f"{tuning_dir}/tuning_summary.png")
    plt.close()

def main():
    # Set style - use a default matplotlib style instead of seaborn
    plt.style.use('ggplot')  # Alternative styles: 'bmh', 'fivethirtyeight', 'ggplot', etc.
    
    try:
        # Find the most recent tuning directory
        tuning_dir = find_latest_tuning_dir()
        print(f"Found tuning results in: {tuning_dir}")
        
        # Create plots
        plot_l0_threshold_tuning(tuning_dir)
        plot_size_ratio_tuning(tuning_dir)
        plot_policy_tuning(tuning_dir)
        plot_range_query_tuning(tuning_dir)
        plot_summary(tuning_dir)
        
        print("Generated tuning visualization plots in:", tuning_dir)
        print("- l0_threshold_tuning.png")
        print("- size_ratio_tuning.png")
        print("- policy_tuning.png")
        print("- range_query_tuning.png")
        print("- tuning_summary.png")
    
    except FileNotFoundError as e:
        print(f"Error: {e}")
        print("Please run the tuning tests first.")

if __name__ == '__main__':
    main() 