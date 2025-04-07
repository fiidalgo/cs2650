#!/usr/bin/env python3
from setuptools import setup, find_packages

setup(
    name="lsm_tree",
    version="0.1.0",
    description="LSM-Tree implementation with different optimization strategies",
    author="CS2650 Student",
    package_dir={"": "python"},
    packages=find_packages(where="python"),
    python_requires=">=3.6",
) 