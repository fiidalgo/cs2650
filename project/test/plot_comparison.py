#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Load the comparison results
print("Loading comparison results...")
df = pd.read_csv("comparison_results.csv")

# Split by implementation type
naive_df = df[df['implementation'] == 'naive']
compaction_df = df[df['implementation'] == 'compaction']

print(f"Loaded data for naive and compaction implementations")

# Create figure with subplots
plt.figure(figsize=(15, 12))

# Plot 1: PUT operation time
plt.subplot(2, 2, 1)
bar_width = 0.35
x = np.arange(1)

plt.bar(x - bar_width/2, naive_df['put_time_ms'], bar_width, label='Naive')
plt.bar(x + bar_width/2, compaction_df['put_time_ms'], bar_width, label='Compaction')
plt.title('PUT Operation Time (ms)')
plt.xticks(x, ['Implementation'])
plt.ylabel('Time (ms)')
plt.legend()

# Plot 2: GET operation time (both sequential and random)
plt.subplot(2, 2, 2)
x = np.arange(2)
metrics = ['seq_get_time_ms', 'rand_get_time_ms']
labels = ['Sequential GET', 'Random GET']

naive_values = [naive_df[m].values[0] for m in metrics]
compaction_values = [compaction_df[m].values[0] for m in metrics]

plt.bar(x - bar_width/2, naive_values, bar_width, label='Naive')
plt.bar(x + bar_width/2, compaction_values, bar_width, label='Compaction')
plt.title('GET Operation Time (ms)')
plt.xticks(x, labels)
plt.ylabel('Time (ms)')
plt.legend()

# Plot 3: RANGE operation time
plt.subplot(2, 2, 3)
x = np.arange(1)

plt.bar(x - bar_width/2, naive_df['range_time_ms'], bar_width, label='Naive')
plt.bar(x + bar_width/2, compaction_df['range_time_ms'], bar_width, label='Compaction')
plt.title('RANGE Operation Time (ms)')
plt.xticks(x, ['Implementation'])
plt.ylabel('Time (ms)')
plt.legend()

# Plot 4: I/O operations
plt.subplot(2, 2, 4)
x = np.arange(2)
metrics = ['io_read_count', 'io_write_count']
labels = ['Read I/O', 'Write I/O']

naive_values = [naive_df[m].values[0] for m in metrics]
compaction_values = [compaction_df[m].values[0] for m in metrics]

plt.bar(x - bar_width/2, naive_values, bar_width, label='Naive')
plt.bar(x + bar_width/2, compaction_values, bar_width, label='Compaction')
plt.title('I/O Operations Count')
plt.xticks(x, labels)
plt.ylabel('Count')
plt.legend()

plt.tight_layout()
plt.savefig("operation_comparison.png", dpi=300)

# Create a separate plot for final SSTable count
plt.figure(figsize=(8, 6))
x = np.arange(1)

plt.bar(x - bar_width/2, naive_df['sstable_count'], bar_width, label='Naive')
plt.bar(x + bar_width/2, compaction_df['sstable_count'], bar_width, label='Compaction')
plt.title('Final SSTable Count')
plt.xticks(x, ['Implementation'])
plt.ylabel('Count')
plt.legend()

plt.tight_layout()
plt.savefig("sstable_comparison.png", dpi=300)

print("Plots generated: operation_comparison.png and sstable_comparison.png") 