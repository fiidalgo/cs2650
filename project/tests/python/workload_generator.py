#!/usr/bin/env python3
import random
import string
import numpy as np
import json
import os
from enum import Enum
from typing import List, Dict, Tuple, Any, Optional

class DistributionType(Enum):
    UNIFORM = "uniform"
    ZIPFIAN = "zipfian"
    SEQUENTIAL = "sequential"
    
class OperationType(Enum):
    PUT = "put"
    GET = "get"
    RANGE = "range"
    DELETE = "delete"

class WorkloadGenerator:
    def __init__(self, seed: int = 42):
        """Initialize workload generator with a specific seed for reproducibility"""
        self.seed = seed
        random.seed(seed)
        np.random.seed(seed)
        
    def generate_keys(self, 
                      count: int, 
                      distribution: DistributionType = DistributionType.UNIFORM,
                      key_length: int = 16,
                      zipf_param: float = 1.2,
                      existing_keys: List[str] = None) -> List[str]:
        """Generate a list of keys based on the specified distribution"""
        
        if distribution == DistributionType.UNIFORM:
            # Generate random string keys
            keys = [''.join(random.choices(string.ascii_letters + string.digits, k=key_length)) 
                   for _ in range(count)]
            
        elif distribution == DistributionType.ZIPFIAN:
            # If existing keys provided, sample from them based on Zipfian
            if existing_keys:
                # Generate Zipfian distribution over indices
                zipf_dist = np.random.zipf(zipf_param, count)
                # Map to existing key indices (modulo length to stay within bounds)
                keys = [existing_keys[idx % len(existing_keys)] for idx in zipf_dist]
            else:
                # Generate fresh keys with Zipfian-like pattern
                # First generate sequential keys
                keys = [f"key_{i:010d}" for i in range(count)]
                # Then sample with Zipfian distribution for access pattern
                indices = np.random.zipf(zipf_param, count)
                keys = [keys[idx % count] for idx in indices]
                
        elif distribution == DistributionType.SEQUENTIAL:
            # Generate sequential keys like key_0000000001, key_0000000002, ...
            keys = [f"key_{i:010d}" for i in range(count)]
            
        return keys
    
    def generate_values(self, count: int, value_size: int = 100) -> List[str]:
        """Generate random values of specified size"""
        return [''.join(random.choices(string.ascii_letters + string.digits, k=value_size)) 
                for _ in range(count)]
    
    def generate_range_bounds(self, 
                             keys: List[str], 
                             range_count: int, 
                             min_range: int = 10, 
                             max_range: int = 1000) -> List[Tuple[str, str]]:
        """Generate start and end keys for range queries"""
        ranges = []
        
        for _ in range(range_count):
            # Decide range length
            range_length = random.randint(min_range, max_range)
            
            # Pick a random starting index
            if len(keys) > range_length:
                start_idx = random.randint(0, len(keys) - range_length)
                end_idx = start_idx + range_length - 1
                
                # Get the actual keys at these indices
                start_key = keys[start_idx]
                end_key = keys[end_idx]
                
                ranges.append((start_key, end_key))
            else:
                # If not enough keys, just use the first and last
                ranges.append((keys[0], keys[-1]))
                
        return ranges
    
    def generate_put_workload(self, 
                              key_count: int, 
                              distribution: DistributionType = DistributionType.UNIFORM,
                              value_size: int = 100) -> List[Dict[str, Any]]:
        """Generate a PUT-only workload"""
        keys = self.generate_keys(key_count, distribution)
        values = self.generate_values(key_count, value_size)
        
        workload = []
        for i in range(key_count):
            workload.append({
                "op": OperationType.PUT.value,
                "key": keys[i],
                "value": values[i]
            })
            
        return workload, keys  # Return keys for potential use in GET workloads
    
    def generate_get_workload(self, 
                             keys: List[str], 
                             get_count: int,
                             existing_ratio: float = 0.7) -> List[Dict[str, Any]]:
        """Generate a GET-only workload with a mix of existing and non-existing keys"""
        workload = []
        
        # Calculate how many gets should be for existing keys
        existing_gets = int(get_count * existing_ratio)
        non_existing_gets = get_count - existing_gets
        
        # Sample existing keys
        if existing_gets > 0:
            sampled_keys = random.choices(keys, k=existing_gets)
            for key in sampled_keys:
                workload.append({
                    "op": OperationType.GET.value,
                    "key": key
                })
        
        # Generate non-existent keys
        if non_existing_gets > 0:
            non_existent_keys = self.generate_keys(non_existing_gets)
            # Make sure they don't overlap with existing keys
            # (simplified approach, might not be perfect for large datasets)
            non_existent_keys = [key + "_nonexistent" for key in non_existent_keys]
            
            for key in non_existent_keys:
                workload.append({
                    "op": OperationType.GET.value,
                    "key": key
                })
        
        # Shuffle to mix existing and non-existing gets
        random.shuffle(workload)
        
        return workload
    
    def generate_range_workload(self,
                               keys: List[str],
                               range_count: int,
                               min_range: int = 10,
                               max_range: int = 1000) -> List[Dict[str, Any]]:
        """Generate a RANGE-only workload"""
        # Sort keys to ensure ranges make sense
        sorted_keys = sorted(keys)
        
        ranges = self.generate_range_bounds(sorted_keys, range_count, min_range, max_range)
        
        workload = []
        for start_key, end_key in ranges:
            workload.append({
                "op": OperationType.RANGE.value,
                "start_key": start_key,
                "end_key": end_key
            })
            
        return workload
    
    def generate_mixed_workload(self,
                               put_ratio: float = 0.5,
                               get_ratio: float = 0.3,
                               range_ratio: float = 0.2,
                               operation_count: int = 10000,
                               existing_keys: List[str] = None) -> List[Dict[str, Any]]:
        """Generate a mixed workload with specified ratios of operations"""
        workload = []
        
        # Calculate operation counts
        put_count = int(operation_count * put_ratio)
        get_count = int(operation_count * get_ratio)
        range_count = operation_count - put_count - get_count  # Ensure we get exactly operation_count
        
        # Generate keys for put operations and a separate set of keys if needed
        if existing_keys is None:
            put_workload, new_keys = self.generate_put_workload(put_count)
            keys_for_queries = new_keys
        else:
            # Use a mix of existing and new keys for puts
            new_key_count = put_count
            put_workload, new_keys = self.generate_put_workload(new_key_count)
            keys_for_queries = existing_keys + new_keys
        
        # Add puts to the workload
        workload.extend(put_workload)
        
        # Generate gets
        get_workload = self.generate_get_workload(keys_for_queries, get_count)
        workload.extend(get_workload)
        
        # Generate ranges
        range_workload = self.generate_range_workload(keys_for_queries, range_count)
        workload.extend(range_workload)
        
        # Shuffle all operations to intermix them
        random.shuffle(workload)
        
        return workload, new_keys
    
    def save_workload(self, workload: List[Dict[str, Any]], filename: str) -> None:
        """Save the workload to a JSON file"""
        os.makedirs(os.path.dirname(filename), exist_ok=True)
        with open(filename, 'w') as f:
            json.dump(workload, f, indent=2)
    
    def load_workload(self, filename: str) -> List[Dict[str, Any]]:
        """Load a workload from a JSON file"""
        with open(filename, 'r') as f:
            return json.load(f)

# Examples of usage
if __name__ == "__main__":
    # Example of generating and saving various workloads
    generator = WorkloadGenerator(seed=42)
    
    # Generate 1000 puts with uniform key distribution
    put_workload, keys = generator.generate_put_workload(1000)
    generator.save_workload(put_workload, "project/tests/configs/put_uniform.json")
    
    # Generate 500 gets, 70% for existing keys
    get_workload = generator.generate_get_workload(keys, 500, existing_ratio=0.7)
    generator.save_workload(get_workload, "project/tests/configs/get_mixed.json")
    
    # Generate 100 range queries
    range_workload = generator.generate_range_workload(keys, 100, min_range=10, max_range=100)
    generator.save_workload(range_workload, "project/tests/configs/range_queries.json")
    
    # Generate a balanced workload of 1000 operations
    mixed_workload, _ = generator.generate_mixed_workload(
        put_ratio=0.5, get_ratio=0.3, range_ratio=0.2, 
        operation_count=1000, existing_keys=keys
    )
    generator.save_workload(mixed_workload, "project/tests/configs/balanced.json")
    
    print("Workloads generated and saved to project/tests/configs/") 