#!/bin/bash

MODULE="pci_driver"
DEVICE="/dev/cobi_pcie_card"
SYSFS_ENTRY="/sys/module/$MODULE"
DEVICE_CLASS="/sys/class/cobi_pcie_card"
VAR_LIB_ENTRY="/var/lib/$MODULE"
VAR_LOG_ENTRY="/var/log/$MODULE.log"
RUN_ENTRY="/run/$MODULE"

# Unload the module
sudo rmmod $MODULE

# Remove device node
if [ -e $DEVICE ]; then
    sudo rm -f $DEVICE
fi

# Remove sysfs entries
if [ -d $SYSFS_ENTRY ]; then
    sudo rm -rf $SYSFS_ENTRY
fi

if [ -d $DEVICE_CLASS ]; then
    sudo rm -rf $DEVICE_CLASS
fi

# Remove var entries
if [ -d $VAR_LIB_ENTRY ]; then
    sudo rm -rf $VAR_LIB_ENTRY
fi

if [ -f $VAR_LOG_ENTRY ]; then
    sudo rm -f $VAR_LOG_ENTRY
fi

# Remove run entry
if [ -d $RUN_ENTRY ]; then
    sudo rm -rf $RUN_ENTRY
fi

# Verify cleanup
lsmod | grep $MODULE

