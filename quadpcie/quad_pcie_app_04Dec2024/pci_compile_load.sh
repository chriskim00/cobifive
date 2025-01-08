#!/bin/bash

# Clean the build environment
make clean

# Build the module
make all

# Check if the build was successful
if [ $? -ne 0 ]; then
    echo "Build failed. Please fix the compilation errors."
    exit 1
fi

# Copy the module to the appropriate directory
sudo cp pci_driver64.ko /lib/modules/$(uname -r)/kernel/drivers

# Remove the module if it is already loaded
sudo modprobe -r pci_driver64

# Update module dependencies
sudo depmod -a

# Set the correct permissions for the module
sudo chmod 644 /lib/modules/$(uname -r)/kernel/drivers/pci_driver64.ko

# Load the module
sudo modprobe pci_driver64

#remove old log file
sudo rm -rf /tmp/pci_driver.log 

echo "Module pci_driver64 loaded successfully."