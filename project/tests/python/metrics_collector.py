#!/usr/bin/env python3
import time
import json
import os
import csv
import matplotlib.pyplot as plt
from typing import Dict, List, Any, Optional
import numpy as np

class MetricsCollector:
    def __init__(self, experiment_name: str, system_name: str):
        """Initialize metrics collector with experiment and system names"""
        self.experiment_name = experiment_name
        self.system_name = system_name
        self.metrics = {
            "operation_latencies": [],  # List of (op_type, latency) tuples
            "read_latencies": [],       # GET operation latencies
            "write_latencies": [],      # PUT operation latencies
            "range_latencies": [],      # RANGE operation latencies
            "flush_latencies": [],      # Time taken for MemTable -> SSTable flushes
            "flush_sizes": [],          # Size of each flushed SSTable
            "sstables_per_read": [],    # How many SSTables were checked for each read
            "io_read_bytes": [],        # IO bytes read per operation
            "io_write_bytes": [],       # IO bytes written per operation
            "operation_counts": {       # Count of each operation type
                "put": 0,
                "get": 0,
                "range": 0,
                "delete": 0,
                "flush": 0
            },
            "metadata": {               # Additional metadata about the experiment
                "start_time": time.time(),
                "end_time": None,
                "experiment_name": experiment_name,
                "system_name": system_name,
                "total_operations": 0
            }
        }
        
    def start_operation(self, op_type: str) -> float:
        """Start timing an operation and return the start time"""
        return time.time()
    
    def end_operation(self, op_type: str, start_time: float, metadata: Dict[str, Any] = None) -> float:
        """Record the end of an operation and store its latency"""
        end_time = time.time()
        latency = end_time - start_time
        
        # Store in appropriate collection based on operation type
        self.metrics["operation_latencies"].append((op_type, latency))
        self.metrics["operation_counts"][op_type] += 1
        self.metrics["metadata"]["total_operations"] += 1
        
        if op_type == "get":
            self.metrics["read_latencies"].append(latency)
            if metadata and "sstables_accessed" in metadata:
                self.metrics["sstables_per_read"].append(metadata["sstables_accessed"])
            if metadata and "bytes_read" in metadata:
                self.metrics["io_read_bytes"].append(metadata["bytes_read"])
                
        elif op_type == "put":
            self.metrics["write_latencies"].append(latency)
            if metadata and "bytes_written" in metadata:
                self.metrics["io_write_bytes"].append(metadata["bytes_written"])
                
        elif op_type == "range":
            self.metrics["range_latencies"].append(latency)
            
        elif op_type == "flush":
            self.metrics["flush_latencies"].append(latency)
            if metadata and "flush_size" in metadata:
                self.metrics["flush_sizes"].append(metadata["flush_size"])
                
        return latency
    
    def record_flush(self, latency: float, size_bytes: int) -> None:
        """Record a MemTable flush operation"""
        self.metrics["flush_latencies"].append(latency)
        self.metrics["flush_sizes"].append(size_bytes)
        self.metrics["operation_counts"]["flush"] += 1
    
    def record_bloom_filter_metrics(self, false_positive_rate: float, hash_count: int) -> None:
        """Record bloom filter statistics"""
        if "bloom_filter" not in self.metrics:
            self.metrics["bloom_filter"] = {
                "false_positive_rates": [],
                "hash_counts": []
            }
        
        self.metrics["bloom_filter"]["false_positive_rates"].append(false_positive_rate)
        self.metrics["bloom_filter"]["hash_counts"].append(hash_count)
    
    def record_fence_pointer_metrics(self, seek_time: float, blocks_accessed: int) -> None:
        """Record fence pointer statistics"""
        if "fence_pointers" not in self.metrics:
            self.metrics["fence_pointers"] = {
                "seek_times": [],
                "blocks_accessed": []
            }
        
        self.metrics["fence_pointers"]["seek_times"].append(seek_time)
        self.metrics["fence_pointers"]["blocks_accessed"].append(blocks_accessed)
    
    def compute_summary_stats(self) -> Dict[str, Any]:
        """Compute summary statistics from the collected metrics"""
        summary = {}
        
        # Compute read latency statistics if we have reads
        if self.metrics["read_latencies"]:
            read_latencies = np.array(self.metrics["read_latencies"])
            summary["read_latency"] = {
                "mean": np.mean(read_latencies),
                "median": np.median(read_latencies),
                "p95": np.percentile(read_latencies, 95),
                "p99": np.percentile(read_latencies, 99),
                "min": np.min(read_latencies),
                "max": np.max(read_latencies)
            }
        
        # Compute write latency statistics if we have writes
        if self.metrics["write_latencies"]:
            write_latencies = np.array(self.metrics["write_latencies"])
            summary["write_latency"] = {
                "mean": np.mean(write_latencies),
                "median": np.median(write_latencies),
                "p95": np.percentile(write_latencies, 95),
                "p99": np.percentile(write_latencies, 99),
                "min": np.min(write_latencies),
                "max": np.max(write_latencies)
            }
        
        # Compute range query latency statistics if we have range queries
        if self.metrics["range_latencies"]:
            range_latencies = np.array(self.metrics["range_latencies"])
            summary["range_latency"] = {
                "mean": np.mean(range_latencies),
                "median": np.median(range_latencies),
                "p95": np.percentile(range_latencies, 95),
                "p99": np.percentile(range_latencies, 99),
                "min": np.min(range_latencies),
                "max": np.max(range_latencies)
            }
            
        # Compute SSTable access statistics if we have reads
        if self.metrics["sstables_per_read"]:
            sstables_per_read = np.array(self.metrics["sstables_per_read"])
            summary["sstables_per_read"] = {
                "mean": np.mean(sstables_per_read),
                "median": np.median(sstables_per_read),
                "max": np.max(sstables_per_read)
            }
            
        # Compute IO statistics
        if self.metrics["io_read_bytes"]:
            io_read_bytes = np.array(self.metrics["io_read_bytes"])
            summary["io_read_bytes"] = {
                "total": np.sum(io_read_bytes),
                "mean_per_op": np.mean(io_read_bytes)
            }
            
        if self.metrics["io_write_bytes"]:
            io_write_bytes = np.array(self.metrics["io_write_bytes"])
            summary["io_write_bytes"] = {
                "total": np.sum(io_write_bytes),
                "mean_per_op": np.mean(io_write_bytes)
            }
            
        # Add operation counts
        summary["operation_counts"] = self.metrics["operation_counts"]
        
        # Calculate throughput
        total_time = self.metrics["metadata"]["end_time"] - self.metrics["metadata"]["start_time"]
        total_ops = self.metrics["metadata"]["total_operations"]
        if total_time > 0:
            summary["throughput"] = {
                "ops_per_second": total_ops / total_time
            }
            
            # Calculate specific operation throughputs
            for op_type, count in self.metrics["operation_counts"].items():
                if count > 0:
                    summary["throughput"][f"{op_type}_per_second"] = count / total_time
        
        return summary
        
    def end_experiment(self) -> None:
        """Mark the end of the experiment and calculate final metrics"""
        self.metrics["metadata"]["end_time"] = time.time()
    
    def save_metrics(self, base_path: str = "project/results") -> None:
        """Save the metrics to JSON and CSV files"""
        # Create directory if it doesn't exist
        os.makedirs(base_path, exist_ok=True)
        
        # Create experiment directory
        experiment_dir = os.path.join(base_path, f"{self.experiment_name}_{self.system_name}")
        os.makedirs(experiment_dir, exist_ok=True)
        
        # Save raw metrics as JSON
        json_path = os.path.join(experiment_dir, "raw_metrics.json")
        with open(json_path, 'w') as f:
            json.dump(self.metrics, f, indent=2)
            
        # Save summary statistics
        summary = self.compute_summary_stats()
        summary_path = os.path.join(experiment_dir, "summary.json")
        with open(summary_path, 'w') as f:
            json.dump(summary, f, indent=2)
            
        # Save operation latencies as CSV for easy analysis
        op_latency_path = os.path.join(experiment_dir, "operation_latencies.csv")
        with open(op_latency_path, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["operation_type", "latency_seconds"])
            for op_type, latency in self.metrics["operation_latencies"]:
                writer.writerow([op_type, latency])
        
        # Generate some basic plots
        self._generate_plots(experiment_dir, summary)
        
        print(f"Metrics saved to {experiment_dir}")
        
    def _generate_plots(self, output_dir: str, summary: Dict[str, Any]) -> None:
        """Generate basic plots from the metrics"""
        # Plot operation counts
        plt.figure(figsize=(10, 6))
        op_types = list(self.metrics["operation_counts"].keys())
        op_counts = [self.metrics["operation_counts"][op] for op in op_types]
        plt.bar(op_types, op_counts)
        plt.title("Operation Counts")
        plt.ylabel("Count")
        plt.savefig(os.path.join(output_dir, "operation_counts.png"))
        plt.close()
        
        # Plot latency distributions if we have data
        if self.metrics["read_latencies"]:
            plt.figure(figsize=(10, 6))
            plt.hist(self.metrics["read_latencies"], bins=30, alpha=0.7, label="Read")
            if self.metrics["write_latencies"]:
                plt.hist(self.metrics["write_latencies"], bins=30, alpha=0.7, label="Write")
            plt.title("Operation Latency Distribution")
            plt.xlabel("Latency (seconds)")
            plt.ylabel("Frequency")
            plt.legend()
            plt.savefig(os.path.join(output_dir, "latency_distribution.png"))
            plt.close()
            
        # Plot SSTables accessed per read if we have data
        if self.metrics["sstables_per_read"]:
            plt.figure(figsize=(10, 6))
            plt.hist(self.metrics["sstables_per_read"], bins=max(10, min(30, max(self.metrics["sstables_per_read"]))))
            plt.title("SSTables Accessed per Read")
            plt.xlabel("Number of SSTables")
            plt.ylabel("Frequency")
            plt.savefig(os.path.join(output_dir, "sstables_per_read.png"))
            plt.close()

# Example usage
if __name__ == "__main__":
    # Create a metrics collector for a sample experiment
    collector = MetricsCollector("sample_experiment", "naive_system")
    
    # Simulate some operations
    for i in range(100):
        # Simulate PUT operation
        start = collector.start_operation("put")
        time.sleep(0.01)  # Simulate operation time
        collector.end_operation("put", start, {"bytes_written": 1024})
        
        # Simulate GET operation
        start = collector.start_operation("get")
        time.sleep(0.005)  # Simulate operation time
        collector.end_operation("get", start, {"sstables_accessed": 3, "bytes_read": 512})
        
        # Simulate occasional RANGE operation
        if i % 10 == 0:
            start = collector.start_operation("range")
            time.sleep(0.02)  # Simulate operation time
            collector.end_operation("range", start)
            
        # Simulate occasional flush operation
        if i % 20 == 0:
            collector.record_flush(0.1, 10240)
    
    # End the experiment
    collector.end_experiment()
    
    # Save the metrics
    collector.save_metrics()
    
    print("Example metrics collection completed") 