#!/bin/bash

# Clean the build environment
make clean

# Build the module
make all
touch /tmp/pci_driver.log

# Check if the build was successful
if [ $? -ne 0 ]; then
    echo "Build failed. Please fix the compilation errors."
    exit 1
fi

# Copy the module to the appropriate directory
sudo cp vpci_driver64.ko /lib/modules/$(uname -r)/kernel/drivers
sudo cp pci_driver64.ko /lib/modules/$(uname -r)/kernel/drivers

# Remove the module if it is already loaded
sudo rmmod vpci_driver64.ko
sudo rmmod pci_driver64.ko


# Update module dependencies
sudo depmod -a

# Load the module
sudo insmod pci_driver64.ko
sudo insmod vpci_driver64.ko

sudo chmod 666 /dev/cobi_chip_vdriver64
sudo chmod 666 /dev/cobi_chip_testdriver640


#remove old log file
sudo rm -rf /tmp/pci_driver.log 
