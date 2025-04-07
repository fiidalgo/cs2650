"""
Naive LSM-Tree implementation with C++ backend
"""

try:
    # Import the C++ binding module
    from lsm_tree_binding.naive import LSMTree as CppLSMTree
except ImportError:
    raise ImportError(
        "Failed to import C++ LSM-Tree implementation. "
        "Make sure you have built the C++ module using the build script."
    )

class LSMTree:
    """
    Python wrapper for the C++ LSM-Tree implementation
    """
    
    def __init__(self, data_dir, memtable_size_bytes=1024*1024):
        """
        Initialize the LSM-Tree
        
        Args:
            data_dir (str): Directory to store SSTable files
            memtable_size_bytes (int): Maximum size of the MemTable in bytes
        """
        self.tree = CppLSMTree(data_dir, memtable_size_bytes)
    
    def put(self, key, value):
        """
        Insert or update a key-value pair
        
        Args:
            key (str): Key to insert/update
            value (str): Value to store
        """
        self.tree.put(key, value)
    
    def get(self, key, return_metadata=False):
        """
        Retrieve a value for a given key
        
        Args:
            key (str): Key to look up
            return_metadata (bool): Whether to return metadata about the operation
            
        Returns:
            str or None: The value if found, None otherwise
            dict: Metadata about the operation (if return_metadata is True)
        """
        result, metadata = self.tree.get(key, return_metadata)
        return (result, metadata) if return_metadata else result
    
    def range(self, start_key, end_key):
        """
        Perform a range query from start_key to end_key (inclusive)
        
        Args:
            start_key (str): Start of the key range
            end_key (str): End of the key range
            
        Returns:
            list: List of (key, value) tuples in the range
        """
        return self.tree.range(start_key, end_key)
    
    def delete(self, key):
        """
        Delete a key-value pair
        
        Args:
            key (str): Key to delete
        """
        self.tree.delete(key)
    
    def flush(self):
        """
        Manually flush the MemTable to disk
        """
        self.tree.flush()
    
    def compact(self):
        """
        Trigger compaction (not implemented in naive version)
        """
        self.tree.compact()
    
    def close(self):
        """
        Close the LSM-Tree and flush any pending data
        """
        self.tree.close()
    
    def get_sstable_count(self):
        """
        Get the number of SSTables
        
        Returns:
            int: Number of SSTables
        """
        return self.tree.get_sstable_count()
    
    def get_memtable_size(self):
        """
        Get the current size of the MemTable in bytes
        
        Returns:
            int: Size in bytes
        """
        return self.tree.get_memtable_size()
    
    def get_total_size_bytes(self):
        """
        Get the total size of all SSTables in bytes
        
        Returns:
            int: Size in bytes
        """
        return self.tree.get_total_size_bytes()
    
    def get_stats(self):
        """
        Get statistics about the LSM-Tree
        
        Returns:
            dict: Dictionary with statistics
        """
        return self.tree.get_stats()
    
    def clear(self):
        """
        Clear all data (for testing purposes)
        """
        self.tree.clear() 