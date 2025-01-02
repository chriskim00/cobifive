# Cobifive
Unified driver, user app, and binary for M.2 PCIe and quad PCIe cards

# Quad card write and read test user app instructions

1. Clone this repository to your local machine
2. Go to the quadpcie user app folder
3. Run 'g++ -o quadcobifive quadcobifive_user_app64.c'
4. Run './quadcobifive'
5. Run 'vi output_logs' to check results

# PCIe driver installation instructions

(First, go to the BIOS setting (hit F2 key when booting) and disable secure login option)

1. make clean
2. make all (fix compilation errors)
3. sudo cp pci_driver64.ko /lib/modules/$(uname -r)/kernel/drivers

note: You may have to run 'uname -r' first, and then copy the info to the above command line

4. sudo depmod -a
5. sudo vi /etc/modules
6. add the name of: pci_driver64.  
7. sudo chmod 644 /lib/modules/$(uname -r)/kernel/drivers/pci_driver64.ko
8. reboot computer (sudo shutdown -r +1)
9. verify that the driver is installed. on terminal: lsmod | grep pci_driver64
you should see: pci_driver64

# For a more permanent solution, you can create a udev rule to set the correct permissions when the device is created:

(Below steps may not be needed on a Ubuntu machine)

1. sudo vi /etc/udev/rules.d/99-cobi.rules

  Add the following line:

  KERNEL=="cobi_pcie_card*", MODE="0666"

2. Save and close the file, then reload the udev rules and trigger them:

  sudo udevadm control --reload-rules
  
  sudo udevadm trigger

# Driver Documentation
## Write
Writing to the PCIe FPGA bus is performed using the internal Linux ".write()" command that accesss the following function:

> pci_mmap_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)

*file: Pointer to PCIe device ID

*buf: pointer to the write_data_t data structure

count: The size of the data strucutre that is being passed i.e: sizeof(write_data_t)

## Read
Writing to the PCIe FPGA bus is performed using the internal Linux ".read()" command that accesss the following function:

> pci_mmap_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)

*file: Pointer to PCIe device ID

*buf: 32-bit uint32_t variable pointer that the driver will reuturn the read data too

count: The size of the data strucutre that is being passed i.e: sizeof(uint32_t) = 32 bits

A .write() is needed to corectly set the virtual memory pointer before a read can be performed. 

## Memory Map
A 32-bit x N offset is sent using the .write() command to set the correct address of the PCIe devices virtual memory to read and write from. Each address is 32bits apart as the PCI is a 32-bit bus. The offset doesn't necessairly need to be set using the .write() command, but is currently done so. The mapping of the virtual memory is shown in QUAD_COBIFIVE_CARD_Memory_Map.xlsx and starts from 1 * sizeof(uint32_t) (FWID or Firmware ID) and goes to 10 * sizeof(uint32_t) (COBI STATUS REG)

## Example Usage


