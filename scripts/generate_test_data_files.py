#!/usr/bin/env python3
import os
import sys
import subprocess
import time

def generate_test_data(size_mb, distribution, output_path=None):
    """Generate a test data file with the given parameters"""
    if output_path is None:
        output_path = f"../data/test_data/{size_mb}mb_{distribution}.bin"
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    print(f"Generating {size_mb}MB {distribution} data file: {output_path}")
    start_time = time.time()
    
    # Run the data generator - adjust path to be relative to project root
    result = subprocess.run(
        ["../bin/generate_test_data", 
         "--size", str(size_mb), 
         "--distribution", distribution,
         "--output", output_path],
        capture_output=True,
        text=True
    )
    
    elapsed = time.time() - start_time
    
    if result.returncode == 0:
        print(f"✅ Generated in {elapsed:.2f} seconds")
        return True
    else:
        print(f"❌ Error: {result.stderr}")
        return False

def main():
    """Generate all test data files needed for testing"""
    # Define the sizes and distributions we need
    data_configs = [
        # Small files for quick tests
        (1, "uniform"),
        (5, "uniform"),
        (10, "uniform"),
        (50, "uniform"),
        (100, "uniform"),
        
        # Medium files
        (500, "uniform"),
        
        # Baseline (1GB)
        (1024, "uniform"),
        
        # Large files for data size tests
        (2048, "uniform"),   # 2GB
        (5120, "uniform"),   # 5GB
        (10240, "uniform"),  # 10GB
        
        # Different distributions at baseline size
        (1024, "skewed"),
    ]
    
    # Check if specific configs were requested
    if len(sys.argv) > 1:
        if sys.argv[1] == "all":
            pass  # Use the default configs above
        elif sys.argv[1] == "baseline":
            # Just generate the baseline 1GB uniform file
            data_configs = [(1024, "uniform")]
        elif sys.argv[1] == "quick":
            # Just generate smaller files for quick testing
            data_configs = [(1, "uniform"), (10, "uniform"), (100, "uniform")]
        elif sys.argv[1] == "large":
            # Just generate larger files for data size tests
            data_configs = [(1024, "uniform"), (2048, "uniform"), (5120, "uniform"), (10240, "uniform")]
        else:
            try:
                # Format: size distribution
                size_mb = int(sys.argv[1])
                distribution = sys.argv[2] if len(sys.argv) > 2 else "uniform"
                
                # Validate distribution
                if distribution not in ["uniform", "skewed"]:
                    print(f"Invalid distribution: {distribution}. Must be 'uniform' or 'skewed'.")
                    print("Using 'uniform' instead.")
                    distribution = "uniform"
                    
                data_configs = [(size_mb, distribution)]
            except ValueError:
                print(f"Invalid size: {sys.argv[1]}. Using default configs.")
    
    # Generate all the required data files
    print(f"Will generate {len(data_configs)} data files")
    print("Note: Large files may take a long time to generate\n")
    
    successful = 0
    failed = 0
    
    for size_mb, distribution in data_configs:
        if generate_test_data(size_mb, distribution):
            successful += 1
        else:
            failed += 1
    
    # Print summary
    print("\n==============================")
    print(f"Data Generation Complete:")
    print(f"  ✅ Successful: {successful}")
    print(f"  ❌ Failed: {failed}")
    print("==============================")
    
    # Print instructions
    print("\nTo use these files:")
    print("1. Start a server:  ../bin/server")
    print("2. Start a client:  ../bin/client")
    print("3. Load data with:  l \"../data/test_data/1024mb_uniform.bin\"  (adjust filename as needed)")
    print("4. Exit client")
    print("5. Run tests from project root directory")
    print("\nValid distributions: 'uniform' and 'skewed'")
    print("\nEnjoy testing!")

if __name__ == "__main__":
    main() 