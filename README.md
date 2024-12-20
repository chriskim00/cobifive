# cobifive
COBIFIVE driver, user app, and binary

M.2 PCIe card

Quad PCIe card

PCIe driver installation instructions

(First, go to the BIOS setting (hit F2 key when booting) and disable secure login option)
1. sudo cp pci_driver64.ko /lib/modules/$(uname -r)/kernel/drivers

note: You may have to run 'uname -r' first, and then copy the info to the above command line

2. sudo depmod -a
3. sudo vi /etc/modules
4. add the name of: pci_driver64.  
5. sudo chmod 644 /lib/modules/$(uname -r)/kernel/drivers/pci_driver64.ko
6. reboot computer (sudo shutdown -r +1)
7. verify that the driver is installed. on terminal: lsmod | grep pci_driver64
you should see: pci_driver64

For a more permanent solution, you can create a udev rule to set the correct permissions when the device is created:

(Below steps may not be needed on a Ubuntu machine)

1. sudo vi /etc/udev/rules.d/99-cobi.rules

  Add the following line:

  KERNEL=="cobi_pcie_card*", MODE="0666"

2. Save and close the file, then reload the udev rules and trigger them:

  sudo udevadm control --reload-rules
  sudo udevadm trigger

